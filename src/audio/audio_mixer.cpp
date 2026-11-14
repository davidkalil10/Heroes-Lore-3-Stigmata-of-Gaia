#include "audio_mixer.h"

#include "../core/bundle.h"
#include "../core/json_mini.h"

#define DR_MP3_IMPLEMENTATION
#include "../vendor/dr_mp3.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace hl3 {

namespace {

constexpr int kOutFreq = 44100;

float clampf01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

int16_t float_to_s16(float v) {
    v = std::max(-1.0f, std::min(1.0f, v));
    return static_cast<int16_t>(std::lrintf(v * 32767.0f));
}

std::vector<float> resample_s16_to_stereo44k(const int16_t* src, size_t src_frames, int channels, int src_rate) {
    if (!src || src_frames == 0 || channels <= 0 || src_rate <= 0) return {};
    if (src_rate == kOutFreq && channels == 2) {
        std::vector<float> out(src_frames * 2);
        for (size_t i = 0; i < src_frames * 2; ++i) {
            out[i] = static_cast<float>(src[i]) / 32768.0f;
        }
        return out;
    }

    size_t out_frames = std::max<size_t>(
        1, static_cast<size_t>((static_cast<double>(src_frames) * kOutFreq) / src_rate));
    std::vector<float> out(out_frames * 2);
    double step = static_cast<double>(src_rate) / static_cast<double>(kOutFreq);

    for (size_t i = 0; i < out_frames; ++i) {
        double pos = static_cast<double>(i) * step;
        size_t idx0 = std::min(src_frames - 1, static_cast<size_t>(pos));
        size_t idx1 = std::min(src_frames - 1, idx0 + 1);
        float frac = static_cast<float>(pos - static_cast<double>(idx0));

        auto sample = [&](size_t idx, int ch) -> float {
            int c = std::min(ch, channels - 1);
            return static_cast<float>(src[idx * static_cast<size_t>(channels) + static_cast<size_t>(c)]) / 32768.0f;
        };

        float l0 = sample(idx0, 0), l1 = sample(idx1, 0);
        float r0 = sample(idx0, 1), r1 = sample(idx1, 1);
        out[i * 2 + 0] = l0 + (l1 - l0) * frac;
        out[i * 2 + 1] = r0 + (r1 - r0) * frac;
    }
    return out;
}

bool decode_mp3_bytes(const Bytes& raw, std::vector<float>& stereo_44k, double& duration_sec) {
    drmp3_config cfg{};
    drmp3_uint64 total_pcm_frames = 0;
    drmp3_int16* pcm = drmp3_open_memory_and_read_pcm_frames_s16(
        raw.data(), raw.size(), &cfg, &total_pcm_frames, nullptr);
    if (!pcm || total_pcm_frames == 0 || cfg.channels == 0 || cfg.sampleRate == 0) {
        if (pcm) drmp3_free(pcm, nullptr);
        return false;
    }
    stereo_44k = resample_s16_to_stereo44k(
        pcm, static_cast<size_t>(total_pcm_frames), static_cast<int>(cfg.channels), static_cast<int>(cfg.sampleRate));
    drmp3_free(pcm, nullptr);
    duration_sec = static_cast<double>(stereo_44k.size() / 2) / static_cast<double>(kOutFreq);
    return !stereo_44k.empty();
}

}  // namespace

AudioMixer::AudioMixer(const std::filesystem::path& config_path) {
    config_path_ = config_path.empty() ? (find_writable_data_root() / "config" / "audio.json") : config_path;
    load_config();
}

AudioMixer::~AudioMixer() {
    shutdown_device();
}

void AudioMixer::init_device() {
    if (dev_id_ != 0) return;
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            initialized_ = false;
            return;
        }
    }
    SDL_AudioSpec desired{};
    SDL_AudioSpec obtained{};
    desired.freq = kOutFreq;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = &AudioMixer::sdl_callback;
    desired.userdata = this;

    dev_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (dev_id_ != 0) {
        SDL_PauseAudioDevice(dev_id_, 0);
        initialized_ = true;
    } else {
        initialized_ = false;
    }
}

void AudioMixer::shutdown_device() {
    if (dev_id_ != 0) {
        SDL_CloseAudioDevice(dev_id_);
        dev_id_ = 0;
    }
    initialized_ = false;
}

void AudioMixer::load_config() {
    Bytes raw;
    if (!read_file_bytes(config_path_, raw)) return;
    JsonValue doc;
    if (!JsonValue::parse(std::string(raw.begin(), raw.end()), doc) || !doc.is_object()) return;
    std::lock_guard<std::mutex> lk(mutex_);
    master_volume_ = clampf01(static_cast<float>(doc.get_number("master_volume", 1.0)));
    bgm_volume_ = clampf01(static_cast<float>(doc.get_number("bgm_volume", 1.0)));
    sfx_volume_ = clampf01(static_cast<float>(doc.get_number("sfx_volume", 1.0)));
    muted_ = doc.get_bool("muted", false);
}

void AudioMixer::save_config() const {
    JsonValue doc = JsonValue::make_object();
    {
        std::lock_guard<std::mutex> lk(mutex_);
        doc.obj_val["master_volume"] = JsonValue::make_number(std::round(master_volume_ * 1000.0) / 1000.0);
        doc.obj_val["bgm_volume"] = JsonValue::make_number(std::round(bgm_volume_ * 1000.0) / 1000.0);
        doc.obj_val["sfx_volume"] = JsonValue::make_number(std::round(sfx_volume_ * 1000.0) / 1000.0);
        doc.obj_val["muted"] = JsonValue::make_bool(muted_);
    }
    std::string text = doc.dump(2) + "\n";
    write_file_bytes(config_path_, text.data(), text.size());
}

void AudioMixer::set_master_volume(float vol) {
    std::lock_guard<std::mutex> lk(mutex_);
    master_volume_ = clampf01(vol);
}

void AudioMixer::set_bgm_volume(float vol) {
    std::lock_guard<std::mutex> lk(mutex_);
    bgm_volume_ = clampf01(vol);
}

void AudioMixer::set_sfx_volume(float vol) {
    std::lock_guard<std::mutex> lk(mutex_);
    sfx_volume_ = clampf01(vol);
}

void AudioMixer::set_muted(bool m) {
    std::lock_guard<std::mutex> lk(mutex_);
    muted_ = m;
}

bool AudioMixer::toggle_mute() {
    std::lock_guard<std::mutex> lk(mutex_);
    muted_ = !muted_;
    return muted_;
}

void AudioMixer::create_source(u32 src_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    sources_[src_id] = Source{};
}

void AudioMixer::set_source_i(u32 src_id, u32 pname, u32 val) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto& src = sources_[src_id];
    if (pname == AL_BUFFER) src.buffer = val;
    else if (pname == AL_LOOPING) src.looping = (val != 0);
    else if (pname == AL_SOURCE_RELATIVE) src.relative = (val != 0);
}

void AudioMixer::set_source_f(u32 src_id, u32 pname, float val) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto& src = sources_[src_id];
    if (pname == AL_GAIN) src.gain = std::max(0.0f, val);
    else if (pname == AL_PITCH) src.pitch = std::max(0.1f, val);
}

u32 AudioMixer::get_source_i(u32 src_id, u32 pname) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sources_.find(src_id);
    if (it == sources_.end()) return (pname == AL_SOURCE_STATE) ? AL_STOPPED : 0u;
    const Source& src = it->second;
    if (pname == AL_SOURCE_STATE) return src.state;
    if (pname == AL_BUFFER) return src.buffer;
    if (pname == AL_LOOPING) return src.looping ? 1u : 0u;
    if (pname == AL_BUFFERS_QUEUED) return src.buffer ? 1u : 0u;
    if (pname == AL_BUFFERS_PROCESSED) return (src.state == AL_PLAYING) ? 0u : 1u;
    return 0u;
}

void AudioMixer::buffer_data(u32 buf_id, u32 fmt, const u8* pcm_bytes, size_t size, u32 freq) {
    if (!pcm_bytes || size < 2 || freq == 0) return;
    int channels = (fmt == AL_FORMAT_STEREO16) ? 2 : 1;
    size_t sample_count = size / 2;
    size_t frames = sample_count / static_cast<size_t>(channels);
    if (frames == 0) return;

    std::vector<int16_t> s16(sample_count);
    std::memcpy(s16.data(), pcm_bytes, sample_count * 2);

    auto track = std::make_shared<PcmTrack>();
    track->stereo_44k = resample_s16_to_stereo44k(s16.data(), frames, channels, static_cast<int>(freq));
    track->frames = track->stereo_44k.size() / 2;
    track->duration_sec = static_cast<double>(track->frames) / static_cast<double>(kOutFreq);

    std::lock_guard<std::mutex> lk(mutex_);
    buffers_[buf_id] = std::move(track);
}

void AudioMixer::play_source(u32 src_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sources_.find(src_id);
    if (it == sources_.end()) return;
    auto bit = buffers_.find(it->second.buffer);
    if (bit == buffers_.end() || bit->second->frames == 0) return;
    it->second.cursor = 0;
    it->second.state = AL_PLAYING;
}

void AudioMixer::stop_source(u32 src_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sources_.find(src_id);
    if (it == sources_.end()) return;
    it->second.state = AL_STOPPED;
    it->second.cursor = 0;
}

void AudioMixer::pause_source(u32 src_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sources_.find(src_id);
    if (it == sources_.end()) return;
    it->second.state = AL_PAUSED;
}

u32 AudioMixer::get_source_state(u32 src_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sources_.find(src_id);
    return (it == sources_.end()) ? AL_STOPPED : it->second.state;
}

void AudioMixer::delete_buffers(const std::vector<u32>& buf_ids) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (u32 bid : buf_ids) buffers_.erase(bid);
}

void AudioMixer::delete_sources(const std::vector<u32>& src_ids) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (u32 sid : src_ids) sources_.erase(sid);
}

double AudioMixer::play_bgm_data(const std::string& cache_key, const Bytes& encoded_bytes, int loops) {
    std::shared_ptr<const PcmTrack> track;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (auto it = bgm_cache_.find(cache_key); it != bgm_cache_.end()) {
            track = it->second;
        }
    }

    if (!track && !encoded_bytes.empty()) {
        std::vector<float> pcm;
        double dur = 0.0;
        if (decode_mp3_bytes(encoded_bytes, pcm, dur)) {
            auto t = std::make_shared<PcmTrack>();
            t->frames = pcm.size() / 2;
            t->stereo_44k = std::move(pcm);
            t->duration_sec = dur;
            track = std::move(t);
            std::lock_guard<std::mutex> lk(mutex_);
            bgm_cache_[cache_key] = track;
        }
    }

    if (!track || track->frames == 0) {
        return std::max(1.0, static_cast<double>(encoded_bytes.size()) / 16000.0);
    }

    std::lock_guard<std::mutex> lk(mutex_);
    current_bgm_ = track;
    current_bgm_key_ = cache_key;
    bgm_frame_ = 0;
    bgm_loops_remaining_ = loops;
    bgm_playing_ = true;
    bgm_fade_total_ = 0;
    bgm_fade_rem_ = 0;
    return track->duration_sec;
}

void AudioMixer::fade_out_bgm(int duration_ms) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!bgm_playing_) return;
    int samples = std::max(1, (duration_ms * kOutFreq) / 1000);
    bgm_fade_total_ = samples;
    bgm_fade_rem_ = samples;
}

void AudioMixer::stop_bgm() {
    std::lock_guard<std::mutex> lk(mutex_);
    bgm_playing_ = false;
    bgm_frame_ = 0;
    bgm_fade_total_ = 0;
    bgm_fade_rem_ = 0;
    current_bgm_key_.clear();
}

bool AudioMixer::is_bgm_playing() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return bgm_playing_;
}

double AudioMixer::get_bgm_pos() const {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!bgm_playing_ || !current_bgm_) return 0.0;
    return static_cast<double>(bgm_frame_) / static_cast<double>(kOutFreq);
}

void SDLCALL AudioMixer::sdl_callback(void* userdata, Uint8* stream, int len) {
    auto* self = static_cast<AudioMixer*>(userdata);
    if (!self || !stream || len <= 0) return;
    size_t frame_count = static_cast<size_t>(len) / (sizeof(int16_t) * 2);
    self->mix_frames(reinterpret_cast<int16_t*>(stream), frame_count);
}

void AudioMixer::mix_frames(int16_t* out_stereo, size_t frame_count) {
    std::lock_guard<std::mutex> lk(mutex_);

    if (muted_) {
        std::memset(out_stereo, 0, frame_count * sizeof(int16_t) * 2);
        // Advance cursors deterministically even when muted.
        if (bgm_playing_ && current_bgm_ && current_bgm_->frames > 0) {
            for (size_t i = 0; i < frame_count; ++i) {
                ++bgm_frame_;
                if (bgm_frame_ >= current_bgm_->frames) {
                    if (bgm_loops_remaining_ < 0) bgm_frame_ = 0;
                    else if (bgm_loops_remaining_ > 0) { --bgm_loops_remaining_; bgm_frame_ = 0; }
                    else { bgm_playing_ = false; bgm_frame_ = 0; break; }
                }
            }
        }
        for (auto& kv : sources_) {
            Source& s = kv.second;
            if (s.state != AL_PLAYING) continue;
            auto bit = buffers_.find(s.buffer);
            if (bit == buffers_.end() || bit->second->frames == 0) { s.state = AL_STOPPED; continue; }
            s.cursor += frame_count;
            if (s.cursor >= bit->second->frames) {
                if (s.looping) s.cursor %= bit->second->frames;
                else { s.cursor = 0; s.state = AL_STOPPED; }
            }
        }
        return;
    }

    float bgm_gain = master_volume_ * bgm_volume_;
    float sfx_base_gain = master_volume_ * sfx_volume_;

    for (size_t i = 0; i < frame_count; ++i) {
        float mix_l = 0.0f;
        float mix_r = 0.0f;

        if (bgm_playing_ && current_bgm_ && current_bgm_->frames > 0) {
            float fade = 1.0f;
            if (bgm_fade_total_ > 0) {
                fade = static_cast<float>(bgm_fade_rem_) / static_cast<float>(bgm_fade_total_);
                if (bgm_fade_rem_ > 0) --bgm_fade_rem_;
                if (bgm_fade_rem_ == 0) {
                    bgm_playing_ = false;
                    bgm_frame_ = 0;
                    bgm_fade_total_ = 0;
                }
            }
            if (bgm_playing_) {
                size_t idx = bgm_frame_ * 2;
                mix_l += current_bgm_->stereo_44k[idx] * bgm_gain * fade;
                mix_r += current_bgm_->stereo_44k[idx + 1] * bgm_gain * fade;
                ++bgm_frame_;
                if (bgm_frame_ >= current_bgm_->frames) {
                    if (bgm_loops_remaining_ < 0) {
                        bgm_frame_ = 0;
                    } else if (bgm_loops_remaining_ > 0) {
                        --bgm_loops_remaining_;
                        bgm_frame_ = 0;
                    } else {
                        bgm_playing_ = false;
                        bgm_frame_ = 0;
                    }
                }
            }
        }

        for (auto& kv : sources_) {
            Source& s = kv.second;
            if (s.state != AL_PLAYING) continue;
            auto bit = buffers_.find(s.buffer);
            if (bit == buffers_.end() || bit->second->frames == 0) {
                s.state = AL_STOPPED;
                continue;
            }
            const auto& track = *bit->second;
            float g = sfx_base_gain * s.gain;
            size_t idx = s.cursor * 2;
            mix_l += track.stereo_44k[idx] * g;
            mix_r += track.stereo_44k[idx + 1] * g;
            ++s.cursor;
            if (s.cursor >= track.frames) {
                if (s.looping) {
                    s.cursor = 0;
                } else {
                    s.cursor = 0;
                    s.state = AL_STOPPED;
                }
            }
        }

        out_stereo[i * 2 + 0] = float_to_s16(mix_l);
        out_stereo[i * 2 + 1] = float_to_s16(mix_r);
    }
}

}  // namespace hl3

