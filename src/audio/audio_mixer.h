#pragma once

#include "../core/common.h"

#include <SDL.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace hl3 {

// OpenAL constants used by Heroes Lore 3.
inline constexpr u32 AL_PITCH = 0x1003u;
inline constexpr u32 AL_POSITION = 0x1004u;
inline constexpr u32 AL_DIRECTION = 0x1005u;
inline constexpr u32 AL_VELOCITY = 0x1006u;
inline constexpr u32 AL_LOOPING = 0x1007u;
inline constexpr u32 AL_BUFFER = 0x1009u;
inline constexpr u32 AL_GAIN = 0x100Au;
inline constexpr u32 AL_MIN_GAIN = 0x100Du;
inline constexpr u32 AL_MAX_GAIN = 0x100Eu;
inline constexpr u32 AL_SOURCE_STATE = 0x1010u;
inline constexpr u32 AL_INITIAL = 0x1011u;
inline constexpr u32 AL_PLAYING = 0x1012u;
inline constexpr u32 AL_PAUSED = 0x1013u;
inline constexpr u32 AL_STOPPED = 0x1014u;
inline constexpr u32 AL_BUFFERS_QUEUED = 0x1015u;
inline constexpr u32 AL_BUFFERS_PROCESSED = 0x1016u;
inline constexpr u32 AL_SOURCE_RELATIVE = 0x0202u;

inline constexpr u32 AL_FORMAT_MONO16 = 0x1101u;
inline constexpr u32 AL_FORMAT_STEREO16 = 0x1103u;

class AudioMixer {
public:
    explicit AudioMixer(const std::filesystem::path& config_path = {});
    ~AudioMixer();

    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;

    void init_device();
    void shutdown_device();
    bool is_initialized() const { return initialized_; }
    void set_paused(bool paused) {
        if (dev_id_ != 0) SDL_PauseAudioDevice(dev_id_, paused ? 1 : 0);
    }

    // Persistence
    void load_config();
    void save_config() const;

    // Volume controls
    void set_master_volume(float vol);
    void set_bgm_volume(float vol);
    void set_sfx_volume(float vol);
    void set_muted(bool muted);
    bool toggle_mute();

    float master_volume() const { return master_volume_; }
    float bgm_volume() const { return bgm_volume_; }
    float sfx_volume() const { return sfx_volume_; }
    bool muted() const { return muted_; }

    // OpenAL SFX (64 virtual sources; PCM pre-resampled outside callback)
    void create_source(u32 src_id);
    void set_source_i(u32 src_id, u32 pname, u32 val);
    void set_source_f(u32 src_id, u32 pname, float val);
    u32 get_source_i(u32 src_id, u32 pname);
    void buffer_data(u32 buf_id, u32 fmt, const u8* pcm_bytes, size_t size, u32 freq);
    void play_source(u32 src_id);
    void stop_source(u32 src_id);
    void pause_source(u32 src_id);
    u32 get_source_state(u32 src_id);
    void delete_buffers(const std::vector<u32>& buf_ids);
    void delete_sources(const std::vector<u32>& src_ids);

    // BGM (MP3/WAV pre-decoded via dr_mp3 outside SDL audio callback)
    double play_bgm_data(const std::string& cache_key, const Bytes& encoded_bytes, int loops = -1);
    void fade_out_bgm(int duration_ms = 500);
    void stop_bgm();
    bool is_bgm_playing() const;
    double get_bgm_pos() const;

private:
    struct PcmTrack {
        std::vector<float> stereo_44k;  // L, R interleaved at 44100 Hz
        size_t frames = 0;
        double duration_sec = 0.0;
    };

    struct Source {
        u32 buffer = 0;
        u32 state = AL_STOPPED;
        float gain = 1.0f;
        float pitch = 1.0f;
        bool looping = false;
        bool relative = false;
        size_t cursor = 0;
    };

    static void SDLCALL sdl_callback(void* userdata, Uint8* stream, int len);
    void mix_frames(int16_t* out_stereo, size_t frame_count);

    std::filesystem::path config_path_;
    SDL_AudioDeviceID dev_id_ = 0;
    bool initialized_ = false;

    float master_volume_ = 1.0f;
    float bgm_volume_ = 1.0f;
    float sfx_volume_ = 1.0f;
    bool muted_ = false;

    mutable std::mutex mutex_;
    std::unordered_map<u32, std::shared_ptr<const PcmTrack>> buffers_;
    std::unordered_map<u32, Source> sources_;

    std::unordered_map<std::string, std::shared_ptr<const PcmTrack>> bgm_cache_;
    std::shared_ptr<const PcmTrack> current_bgm_;
    std::string current_bgm_key_;
    size_t bgm_frame_ = 0;
    int bgm_loops_remaining_ = 0;  // <0 = infinite, 0 = last play, >0 = remaining loops after current
    bool bgm_playing_ = false;
    int bgm_fade_total_ = 0;
    int bgm_fade_rem_ = 0;
};

}  // namespace hl3

