#include "../core/common.h"
#include "../core/machine.h"
#include "../core/objc.h"
#include "../core/runtime.h"
#include "audio_mixer.h"

#include <algorithm>
#include <any>
#include <string>
#include <vector>

namespace hl3 {

namespace {

struct AlIdState {
    u32 buf = 1;
    u32 src = 1;
    u32 str_addr = 0;
};

struct AvPlayerState {
    std::string path;
    Bytes inline_data;
    int loops = -1;
    float volume = 1.0f;
    double duration = 120.0;
    bool playing = false;
    u32 delegate = 0;
};

AlIdState& get_al_ids(Runtime& rt) {
    auto it = rt.kv_state.find("al_ids");
    if (it == rt.kv_state.end()) {
        it = rt.kv_state.emplace("al_ids", AlIdState{}).first;
    }
    return *std::any_cast<AlIdState>(&it->second);
}

}  // namespace

void register_hle_audio(Runtime& rt, HleRegistry& reg) {
    // ---- OpenAL API ---------------------------------------------------------
    reg.add_func("_alcOpenDevice", [](Machine&) { return TrapResult::val(0x2001u); });
    reg.add_func("_alcCreateContext", [](Machine&) { return TrapResult::val(0x2002u); });
    reg.add_func("_alcMakeContextCurrent|_alcCloseDevice|_alcDestroyContext",
                 [](Machine&) { return TrapResult::val(1u); });
    reg.add_func("_alcGetCurrentContext", [](Machine&) { return TrapResult::val(0x2002u); });

    reg.add_func("_alcGetString", [&rt](Machine&) {
        auto& ids = get_al_ids(rt);
        if (ids.str_addr == 0) ids.str_addr = rt.machine().new_cstr("OpenAL 1.1 HeroesLore3");
        return TrapResult::val(ids.str_addr);
    });

    reg.add_func("_alGetError", [](Machine&) { return TrapResult::val(0u); });

    reg.add_func("_alGenBuffers", [&rt](Machine& m) {
        u32 n = m.reg(0);
        u32 ptr = m.reg(1);
        auto& ids = get_al_ids(rt);
        for (u32 i = 0; i < n; ++i) m.wr32(ptr + 4u * i, ids.buf + i);
        ids.buf += n;
        return TrapResult::val(0u);
    });

    reg.add_func("_alBufferData", [&rt](Machine& m) {
        u32 buf_id = m.reg(0);
        u32 fmt = m.reg(1);
        u32 data_ptr = m.reg(2);
        u32 size = m.reg(3);
        u32 freq = m.rd32(m.sp());
        if (auto* am = rt.audio_mixer(); am && data_ptr && size > 0) {
            Bytes pcm(size);
            if (m.rdbytes(data_ptr, size, pcm.data())) {
                am->buffer_data(buf_id, fmt, pcm.data(), pcm.size(), freq);
            }
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_alDeleteBuffers", [&rt](Machine& m) {
        u32 n = m.reg(0);
        u32 ptr = m.reg(1);
        if (auto* am = rt.audio_mixer(); am && ptr && n > 0) {
            std::vector<u32> bids(n);
            for (u32 i = 0; i < n; ++i) bids[i] = m.rd32(ptr + 4u * i);
            am->delete_buffers(bids);
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_alGenSources", [&rt](Machine& m) {
        u32 n = m.reg(0);
        u32 ptr = m.reg(1);
        auto& ids = get_al_ids(rt);
        auto* am = rt.audio_mixer();
        for (u32 i = 0; i < n; ++i) {
            u32 sid = ids.src + i;
            m.wr32(ptr + 4u * i, sid);
            if (am) am->create_source(sid);
        }
        ids.src += n;
        return TrapResult::val(0u);
    });

    reg.add_func("_alDeleteSources", [&rt](Machine& m) {
        u32 n = m.reg(0);
        u32 ptr = m.reg(1);
        if (auto* am = rt.audio_mixer(); am && ptr && n > 0) {
            std::vector<u32> sids(n);
            for (u32 i = 0; i < n; ++i) sids[i] = m.rd32(ptr + 4u * i);
            am->delete_sources(sids);
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_alSourcei", [&rt](Machine& m) {
        if (auto* am = rt.audio_mixer()) {
            am->set_source_i(m.reg(0), m.reg(1), m.reg(2));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_alSourcef", [&rt](Machine& m) {
        if (auto* am = rt.audio_mixer()) {
            am->set_source_f(m.reg(0), m.reg(1), bits_to_f32(m.reg(2)));
        }
        return TrapResult::val(0u);
    });

    reg.add_func("_alSourcefv", [](Machine&) { return TrapResult::val(0u); });

    reg.add_func("_alSourcePlay", [&rt](Machine& m) {
        if (auto* am = rt.audio_mixer()) am->play_source(m.reg(0));
        return TrapResult::val(0u);
    });

    reg.add_func("_alSourceStop", [&rt](Machine& m) {
        if (auto* am = rt.audio_mixer()) am->stop_source(m.reg(0));
        return TrapResult::val(0u);
    });

    reg.add_func("_alSourcePause", [&rt](Machine& m) {
        if (auto* am = rt.audio_mixer()) am->pause_source(m.reg(0));
        return TrapResult::val(0u);
    });

    reg.add_func("_alGetSourcei", [&rt](Machine& m) {
        u32 sid = m.reg(0);
        u32 pname = m.reg(1);
        u32 ptr = m.reg(2);
        u32 val = (pname == AL_SOURCE_STATE) ? AL_STOPPED : 0u;
        if (auto* am = rt.audio_mixer()) val = am->get_source_i(sid, pname);
        if (ptr) m.wr32(ptr, val);
        return TrapResult::val(0u);
    });

    // ---- AudioSession API ---------------------------------------------------
    reg.add_func("_AudioSessionInitialize|_AudioSessionSetActive|_AudioSessionSetProperty",
                 [](Machine&) { return TrapResult::val(0u); });

    reg.add_func("_AudioSessionGetProperty", [](Machine& m) {
        u32 prop_id = m.reg(0);
        u32 size_ptr = m.reg(1);
        u32 out_ptr = m.reg(2);
        if (out_ptr && size_ptr) {
            if (prop_id == 0x6F746170u || prop_id == 0x6F746870u) {  // 'otap'/'othp': OtherAudioIsPlaying
                m.wr32(out_ptr, 0u);
                m.wr32(size_ptr, 4u);
            } else if (prop_id == 0x61636174u) {  // 'acat': AudioCategory
                m.wr32(out_ptr, 0x736F6C6Fu);  // 'solo'
                m.wr32(size_ptr, 4u);
            } else if (prop_id == 0x6368766Cu) {  // 'chvl': CurrentHardwareOutputVolume
                m.wr32(out_ptr, 0x3F800000u);  // 1.0f
                m.wr32(size_ptr, 4u);
            }
        }
        return TrapResult::val(0u);
    });

    // ---- AVAudioPlayer ObjC Class ------------------------------------------
    reg.add_cm("AVAudioPlayer", "alloc", [](Runtime& r, u32, const ObjCArgs&) {
        return ObjCRet::val(r.objc().new_obj_with("AVAudioPlayer", AvPlayerState{}));
    });

    reg.add_im("AVAudioPlayer", "initWithContentsOfURL:error:", [](Runtime& r, u32 recv, const ObjCArgs& a) {
        AvPlayerState st;
        st.path = r.objc().get_str(a[0]);
        r.objc().set_payload(recv, std::move(st));
        return ObjCRet::val(recv);
    });

    reg.add_im("AVAudioPlayer", "initWithData:error:", [](Runtime& r, u32 recv, const ObjCArgs& a) {
        AvPlayerState st;
        if (const Bytes* b = r.objc().get_payload<Bytes>(a[0])) {
            st.inline_data = *b;
            st.path = "mem://avdata/" + std::to_string(b->size());
        }
        r.objc().set_payload(recv, std::move(st));
        return ObjCRet::val(recv);
    });

    reg.add_im("AVAudioPlayer", "prepareToPlay", [](Runtime&, u32, const ObjCArgs&) {
        return ObjCRet::val(1u);
    });

    reg.add_im("AVAudioPlayer", "play", [](Runtime& r, u32 recv, const ObjCArgs&) {
        auto* info = r.objc().get_payload<AvPlayerState>(recv);
        if (!info) return ObjCRet::val(1u);

        Bytes raw = info->inline_data;
        std::string key = info->path;
        if (raw.empty() && !info->path.empty()) {
            auto res = r.resolve_path(info->path);
            if (res.kind == Runtime::ResolvedPath::Kind::Bundle) {
                key = res.bundle_rel;
                r.bundle().read_asset(res.bundle_rel, raw);
            } else if (res.kind == Runtime::ResolvedPath::Kind::Docs) {
                key = res.host_path.u8string();
                read_file_bytes(res.host_path, raw);
            }
        }
        if (auto* am = r.audio_mixer(); am && !raw.empty()) {
            am->set_player_bgm_volume(info->volume);
            info->duration = am->play_bgm_data(key, raw, info->loops);
            info->playing = true;
        }
        return ObjCRet::val(1u);
    });

    reg.add_im("AVAudioPlayer", "stop|pause", [](Runtime& r, u32 recv, const ObjCArgs&) {
        if (auto* info = r.objc().get_payload<AvPlayerState>(recv)) {
            info->playing = false;
        }
        if (auto* am = r.audio_mixer()) am->stop_bgm();
        return ObjCRet::val(0u);
    });

    reg.add_im("AVAudioPlayer", "isPlaying", [](Runtime& r, u32, const ObjCArgs&) {
        if (auto* am = r.audio_mixer()) {
            return ObjCRet::val(am->is_bgm_playing() ? 1u : 0u);
        }
        return ObjCRet::val(0u);
    });

    reg.add_im("AVAudioPlayer", "duration", [](Runtime& r, u32 recv, const ObjCArgs&) {
        double dur = 120.0;
        if (auto* info = r.objc().get_payload<AvPlayerState>(recv)) dur = info->duration;
        u64 bits = f64_to_bits(dur);
        return ObjCRet::pair(static_cast<u32>(bits & 0xFFFFFFFFu), static_cast<u32>(bits >> 32));
    });

    reg.add_im("AVAudioPlayer", "currentTime", [](Runtime& r, u32, const ObjCArgs&) {
        double t = 0.0;
        if (auto* am = r.audio_mixer()) t = am->get_bgm_pos();
        u64 bits = f64_to_bits(t);
        return ObjCRet::pair(static_cast<u32>(bits & 0xFFFFFFFFu), static_cast<u32>(bits >> 32));
    });

    reg.add_im("AVAudioPlayer", "setCurrentTime:", [](Runtime&, u32, const ObjCArgs&) {
        return ObjCRet::val(0u);
    });

    reg.add_im("AVAudioPlayer", "setVolume:", [](Runtime& r, u32 recv, const ObjCArgs& a) {
        float vol = a.f32(0);
        if (auto* info = r.objc().get_payload<AvPlayerState>(recv)) info->volume = vol;
        if (auto* am = r.audio_mixer()) am->set_player_bgm_volume(vol);
        return ObjCRet::val(0u);
    });

    reg.add_im("AVAudioPlayer", "volume", [](Runtime& r, u32 recv, const ObjCArgs&) {
        float vol = 1.0f;
        if (auto* info = r.objc().get_payload<AvPlayerState>(recv)) vol = info->volume;
        return ObjCRet::val(f32_to_bits(vol));
    });

    reg.add_im("AVAudioPlayer", "setNumberOfLoops:", [](Runtime& r, u32 recv, const ObjCArgs& a) {
        if (auto* info = r.objc().get_payload<AvPlayerState>(recv)) {
            info->loops = static_cast<i32>(a[0]);
        }
        return ObjCRet::val(0u);
    });

    reg.add_im("AVAudioPlayer", "numberOfLoops", [](Runtime& r, u32 recv, const ObjCArgs&) {
        int loops = -1;
        if (auto* info = r.objc().get_payload<AvPlayerState>(recv)) loops = info->loops;
        return ObjCRet::val(static_cast<u32>(static_cast<i32>(loops)));
    });

    reg.add_im("AVAudioPlayer", "delegate", [](Runtime& r, u32 recv, const ObjCArgs&) {
        u32 d = 0;
        if (auto* info = r.objc().get_payload<AvPlayerState>(recv)) d = info->delegate;
        return ObjCRet::val(d);
    });

    reg.add_im("AVAudioPlayer", "setDelegate:", [](Runtime& r, u32 recv, const ObjCArgs& a) {
        if (auto* info = r.objc().get_payload<AvPlayerState>(recv)) info->delegate = a[0];
        return ObjCRet::val(0u);
    });

    reg.add_im("AVAudioPlayer", "dealloc", [](Runtime& r, u32 recv, const ObjCArgs&) {
        if (auto* info = r.objc().get_payload<AvPlayerState>(recv); info && info->playing) {
            if (auto* am = r.audio_mixer()) am->stop_bgm();
        }
        return ObjCRet::val(0u);
    });
}

}  // namespace hl3
