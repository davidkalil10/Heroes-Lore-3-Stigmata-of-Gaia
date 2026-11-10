"""Audio subsystem HLE for Heroes Lore 3 (OpenAL + AVAudioPlayer via pygame.mixer).

Mixes 28 OpenAL sound effects and 20 MP3 BGM tracks cleanly outside the SDL audio callback.
Includes 64-channel virtual mixer, independent volume controls, smooth fading, and persistence.
"""

from __future__ import annotations

import io
import json
import struct
import wave
from pathlib import Path

from .objc import cm, func, im

# OpenAL constants
AL_PITCH = 0x1003
AL_POSITION = 0x1004
AL_DIRECTION = 0x1005
AL_VELOCITY = 0x1006
AL_LOOPING = 0x1007
AL_BUFFER = 0x1009
AL_GAIN = 0x100A
AL_MIN_GAIN = 0x100D
AL_MAX_GAIN = 0x100E
AL_SOURCE_STATE = 0x1010
AL_INITIAL = 0x1011
AL_PLAYING = 0x1012
AL_PAUSED = 0x1013
AL_STOPPED = 0x1014
AL_BUFFERS_QUEUED = 0x1015
AL_BUFFERS_PROCESSED = 0x1016
AL_SOURCE_RELATIVE = 0x202

AL_FORMAT_MONO16 = 0x1101
AL_FORMAT_STEREO16 = 0x1103


class AudioMixer:
    """SDL/Pygame-backed audio mixer with 64 channels, BGM streaming and configuration persistence."""

    def __init__(self, bundle, config_path: Path | None = None):
        self.bundle = bundle
        self.initialized = False
        self.buffers: dict[int, object] = {}  # buf_id -> pygame.mixer.Sound
        self.sources: dict[int, dict] = {}  # src_id -> source attributes dict
        self.current_bgm_path: str | None = None
        self.master_volume: float = 1.0
        self.bgm_volume: float = 1.0
        self.sfx_volume: float = 1.0
        self.muted: bool = False

        if config_path is None:
            # Default to ROOT/config/audio.json
            import sys
            if getattr(sys, "frozen", False):
                root = Path(sys.executable).resolve().parent
                if not (root / "project.json").exists() and hasattr(sys, "_MEIPASS"):
                    root = Path(sys._MEIPASS)
            else:
                root = Path(__file__).resolve().parents[2]
            self.config_path = root / "config" / "audio.json"
        else:
            self.config_path = Path(config_path)

        self._init_mixer()
        self.load_config()

    def _init_mixer(self):
        try:
            import pygame

            if not pygame.mixer.get_init():
                pygame.mixer.init(frequency=44100, size=-16, channels=2, buffer=1024)
            try:
                pygame.mixer.set_num_channels(64)
            except Exception:
                pass
            self.initialized = True
        except Exception:
            self.initialized = False

    # ------------------------------------------------------------- Persistence
    def load_config(self):
        if not self.config_path.exists():
            return
        try:
            with open(self.config_path, "r", encoding="utf-8") as f:
                cfg = json.load(f)
            self.master_volume = max(0.0, min(1.0, float(cfg.get("master_volume", 1.0))))
            self.bgm_volume = max(0.0, min(1.0, float(cfg.get("bgm_volume", 1.0))))
            self.sfx_volume = max(0.0, min(1.0, float(cfg.get("sfx_volume", 1.0))))
            self.muted = bool(cfg.get("muted", False))
            self._update_volumes()
        except Exception:
            pass

    def save_config(self):
        try:
            self.config_path.parent.mkdir(parents=True, exist_ok=True)
            cfg = {
                "master_volume": round(self.master_volume, 3),
                "bgm_volume": round(self.bgm_volume, 3),
                "sfx_volume": round(self.sfx_volume, 3),
                "muted": self.muted,
            }
            with open(self.config_path, "w", encoding="utf-8") as f:
                json.dump(cfg, f, indent=2)
        except Exception:
            pass

    # --------------------------------------------------------- Volume controls
    def set_master_volume(self, vol: float):
        self.master_volume = max(0.0, min(1.0, float(vol)))
        self._update_volumes()

    def set_bgm_volume(self, vol: float):
        self.bgm_volume = max(0.0, min(1.0, float(vol)))
        self._update_volumes()

    def set_sfx_volume(self, vol: float):
        self.sfx_volume = max(0.0, min(1.0, float(vol)))
        self._update_volumes()

    def set_muted(self, muted: bool):
        self.muted = bool(muted)
        self._update_volumes()

    def toggle_mute(self) -> bool:
        self.set_muted(not self.muted)
        return self.muted

    def _update_volumes(self):
        if not self.initialized:
            return
        import pygame

        bgm_eff = 0.0 if self.muted else (self.master_volume * self.bgm_volume)
        try:
            pygame.mixer.music.set_volume(max(0.0, min(1.0, bgm_eff)))
        except Exception:
            pass

        for sid, src in self.sources.items():
            ch = src.get("channel")
            if ch is not None:
                gain = src.get("gain", 1.0)
                sfx_eff = 0.0 if self.muted else (self.master_volume * self.sfx_volume * gain)
                try:
                    ch.set_volume(max(0.0, min(1.0, sfx_eff)))
                except Exception:
                    pass

    # ------------------------------------------------------------- SFX / OpenAL
    def buffer_data(self, buf_id: int, fmt: int, pcm_bytes: bytes, freq: int):
        if not self.initialized:
            return
        import pygame

        try:
            nchannels = 2 if fmt == AL_FORMAT_STEREO16 else 1
            wav_io = io.BytesIO()
            with wave.open(wav_io, "wb") as wf:
                wf.setnchannels(nchannels)
                wf.setsampwidth(2)  # 16-bit
                wf.setframerate(freq)
                wf.writeframes(pcm_bytes)
            wav_io.seek(0)
            sound = pygame.mixer.Sound(wav_io)
            self.buffers[buf_id] = sound
        except Exception:
            pass

    def play_source(self, src_id: int):
        if not self.initialized:
            return
        src = self.sources.get(src_id)
        if not src:
            return
        buf_id = src.get("buffer")
        sound = self.buffers.get(buf_id)
        if not sound:
            return

        import pygame

        ch_idx = (src_id - 1) % 64
        gain = src.get("gain", 1.0)
        eff_vol = 0.0 if self.muted else (self.master_volume * self.sfx_volume * gain)
        loops = -1 if src.get("looping", False) else 0

        try:
            ch = pygame.mixer.Channel(ch_idx)
            ch.set_volume(max(0.0, min(1.0, eff_vol)))
            ch.play(sound, loops=loops)
            src["channel"] = ch
            src["state"] = AL_PLAYING
        except Exception:
            try:
                sound.set_volume(max(0.0, min(1.0, eff_vol)))
                ch = sound.play(loops=loops)
                src["channel"] = ch
                src["state"] = AL_PLAYING
            except Exception:
                pass

    def stop_source(self, src_id: int):
        src = self.sources.get(src_id)
        if not src:
            return
        ch = src.get("channel")
        if ch is not None:
            try:
                ch.stop()
            except Exception:
                pass
        buf_id = src.get("buffer")
        sound = self.buffers.get(buf_id)
        if sound:
            try:
                sound.stop()
            except Exception:
                pass
        src["state"] = AL_STOPPED

    def pause_source(self, src_id: int):
        src = self.sources.get(src_id)
        if not src:
            return
        ch = src.get("channel")
        if ch is not None:
            try:
                ch.pause()
            except Exception:
                pass
        src["state"] = AL_PAUSED

    def get_source_state(self, src_id: int) -> int:
        src = self.sources.get(src_id)
        if not src:
            return AL_STOPPED
        if src.get("state") == AL_PLAYING:
            ch = src.get("channel")
            if ch is not None:
                try:
                    if not ch.get_busy():
                        src["state"] = AL_STOPPED
                except Exception:
                    pass
        return src.get("state", AL_STOPPED)

    def delete_buffers(self, buf_ids: list[int]):
        for bid in buf_ids:
            self.buffers.pop(bid, None)

    def delete_sources(self, src_ids: list[int]):
        for sid in src_ids:
            self.stop_source(sid)
            self.sources.pop(sid, None)

    # ------------------------------------------------------ BGM / AVAudioPlayer
    def play_bgm(self, path: str, loops: int = -1, fade_ms: int = 0):
        if not self.initialized:
            return
        import pygame

        try:
            pygame.mixer.music.load(path)
            bgm_eff = 0.0 if self.muted else (self.master_volume * self.bgm_volume)
            pygame.mixer.music.set_volume(max(0.0, min(1.0, bgm_eff)))
            if fade_ms > 0:
                pygame.mixer.music.play(loops=loops, fade_ms=fade_ms)
            else:
                pygame.mixer.music.play(loops=loops)
            self.current_bgm_path = path
        except Exception:
            pass

    def fade_out_bgm(self, duration_ms: int = 500):
        if not self.initialized:
            return
        import pygame

        try:
            pygame.mixer.music.fadeout(duration_ms)
        except Exception:
            pass

    def stop_bgm(self):
        if not self.initialized:
            return
        import pygame

        try:
            pygame.mixer.music.stop()
        except Exception:
            pass
        self.current_bgm_path = None

    def is_bgm_playing(self) -> bool:
        if not self.initialized:
            return False
        import pygame

        try:
            return bool(pygame.mixer.music.get_busy())
        except Exception:
            return False

    def get_bgm_pos(self) -> float:
        if not self.initialized:
            return 0.0
        import pygame

        try:
            pos = pygame.mixer.music.get_pos()
            return max(0.0, pos / 1000.0) if pos >= 0 else 0.0
        except Exception:
            return 0.0


# ---------------------------------------------------------------- OpenAL API
@func("_alcOpenDevice")
def _alc_open_device(rt):
    if not hasattr(rt, "audio_mixer"):
        rt.audio_mixer = AudioMixer(rt.bundle)
    return 0x2001


@func("_alcCreateContext")
def _alc_create_context(rt):
    return 0x2002


@func("_alcMakeContextCurrent|_alcCloseDevice|_alcDestroyContext")
def _alc_make_current(rt):
    return 1


@func("_alcGetCurrentContext")
def _alc_get_current(rt):
    return 0x2002


@func("_alcGetString")
def _alc_get_string(rt):
    cache = rt.state.setdefault("alc_strings", {})
    if "OpenAL" not in cache:
        cache["OpenAL"] = rt.m.new_cstr("OpenAL 1.1 HeroesLore3")
    return cache["OpenAL"]


@func("_alGetError")
def _al_get_error(rt):
    return 0


@func("_alGenBuffers")
def _al_gen_buffers(rt):
    m = rt.m
    n, ptr = m.reg(0), m.reg(1)
    ctr = rt.state.setdefault("al_ids", {})
    base = ctr.get("buf", 1)
    for i in range(n):
        bid = base + i
        m.wr32(ptr + 4 * i, bid)
    ctr["buf"] = base + n
    return 0


@func("_alBufferData")
def _al_buffer_data(rt):
    m = rt.m
    buf_id, fmt, data_ptr, size, freq = [m.reg(i) for i in range(4)] + [m.rd32(m.sp)]
    pcm = m.rdbytes(data_ptr, size)
    if hasattr(rt, "audio_mixer"):
        rt.audio_mixer.buffer_data(buf_id, fmt, pcm, freq)
    return 0


@func("_alDeleteBuffers")
def _al_delete_buffers(rt):
    m = rt.m
    n, ptr = m.reg(0), m.reg(1)
    if hasattr(rt, "audio_mixer") and ptr:
        bids = [m.rd32(ptr + 4 * i) for i in range(n)]
        rt.audio_mixer.delete_buffers(bids)
    return 0


@func("_alGenSources")
def _al_gen_sources(rt):
    m = rt.m
    n, ptr = m.reg(0), m.reg(1)
    ctr = rt.state.setdefault("al_ids", {})
    base = ctr.get("src", 1)
    for i in range(n):
        sid = base + i
        m.wr32(ptr + 4 * i, sid)
        if hasattr(rt, "audio_mixer"):
            rt.audio_mixer.sources[sid] = {
                "buffer": None,
                "state": AL_STOPPED,
                "gain": 1.0,
                "pitch": 1.0,
                "looping": False,
                "channel": None,
            }
    ctr["src"] = base + n
    return 0


@func("_alDeleteSources")
def _al_delete_sources(rt):
    m = rt.m
    n, ptr = m.reg(0), m.reg(1)
    if hasattr(rt, "audio_mixer") and ptr:
        sids = [m.rd32(ptr + 4 * i) for i in range(n)]
        rt.audio_mixer.delete_sources(sids)
    return 0


@func("_alSourcei")
def _al_source_i(rt):
    m = rt.m
    sid, pname, val = m.reg(0), m.reg(1), m.reg(2)
    if hasattr(rt, "audio_mixer"):
        src = rt.audio_mixer.sources.setdefault(sid, {})
        if pname == AL_BUFFER:
            src["buffer"] = val
        elif pname == AL_LOOPING:
            src["looping"] = bool(val)
        elif pname == AL_SOURCE_RELATIVE:
            src["relative"] = bool(val)
    return 0


@func("_alSourcef")
def _al_source_f(rt):
    m = rt.m
    sid, pname = m.reg(0), m.reg(1)
    raw_f = struct.unpack("<f", struct.pack("<I", m.reg(2)))[0]
    if hasattr(rt, "audio_mixer"):
        src = rt.audio_mixer.sources.setdefault(sid, {})
        if pname == AL_GAIN:
            src["gain"] = max(0.0, raw_f)
            ch = src.get("channel")
            if ch:
                vol = (
                    0.0
                    if rt.audio_mixer.muted
                    else (rt.audio_mixer.master_volume * rt.audio_mixer.sfx_volume * src["gain"])
                )
                try:
                    ch.set_volume(max(0.0, min(1.0, vol)))
                except Exception:
                    pass
        elif pname == AL_PITCH:
            src["pitch"] = max(0.1, raw_f)
        elif pname == AL_MIN_GAIN:
            src["min_gain"] = raw_f
        elif pname == AL_MAX_GAIN:
            src["max_gain"] = raw_f
    return 0


@func("_alSourcefv")
def _al_source_fv(rt):
    m = rt.m
    sid, pname, ptr = m.reg(0), m.reg(1), m.reg(2)
    if hasattr(rt, "audio_mixer") and ptr:
        src = rt.audio_mixer.sources.setdefault(sid, {})
        try:
            vals = struct.unpack("<fff", m.rdbytes(ptr, 12))
            if pname == AL_POSITION:
                src["pos"] = vals
            elif pname == AL_VELOCITY:
                src["vel"] = vals
            elif pname == AL_DIRECTION:
                src["dir"] = vals
        except Exception:
            pass
    return 0


@func("_alSourcePlay")
def _al_source_play(rt):
    sid = rt.m.reg(0)
    if hasattr(rt, "audio_mixer"):
        rt.audio_mixer.play_source(sid)
    return 0


@func("_alSourceStop")
def _al_source_stop(rt):
    sid = rt.m.reg(0)
    if hasattr(rt, "audio_mixer"):
        rt.audio_mixer.stop_source(sid)
    return 0


@func("_alGetSourcei")
def _al_get_source_i(rt):
    m = rt.m
    sid, pname, ptr = m.reg(0), m.reg(1), m.reg(2)
    val = 0
    if hasattr(rt, "audio_mixer"):
        if pname == AL_SOURCE_STATE:
            val = rt.audio_mixer.get_source_state(sid)
        elif pname == AL_BUFFER:
            val = rt.audio_mixer.sources.get(sid, {}).get("buffer", 0) or 0
        elif pname == AL_LOOPING:
            val = 1 if rt.audio_mixer.sources.get(sid, {}).get("looping") else 0
        elif pname == AL_BUFFERS_QUEUED:
            val = 1 if rt.audio_mixer.sources.get(sid, {}).get("buffer") else 0
        elif pname == AL_BUFFERS_PROCESSED:
            st = rt.audio_mixer.get_source_state(sid)
            val = 0 if st == AL_PLAYING else 1
    m.wr32(ptr, val)
    return 0


# ---------------------------------------------------------- AudioSession API
@func("_AudioSessionInitialize|_AudioSessionSetActive|_AudioSessionSetProperty")
def _audio_session_init(rt):
    return 0


@func("_AudioSessionGetProperty")
def _audio_session_get_prop(rt):
    m = rt.m
    prop_id, size_ptr, out_ptr = m.reg(0), m.reg(1), m.reg(2)
    if out_ptr and size_ptr:
        if prop_id == 0x6F746170:  # 'othp': OtherAudioIsPlaying
            m.wr32(out_ptr, 0)
            m.wr32(size_ptr, 4)
        elif prop_id == 0x61636174:  # 'acat': AudioCategory
            m.wr32(out_ptr, 0x736F6C6F)  # 'solo'
            m.wr32(size_ptr, 4)
        elif prop_id == 0x6368766C:  # 'chvl': CurrentHardwareOutputVolume
            m.wr32(out_ptr, 0x3F800000)  # 1.0f
            m.wr32(size_ptr, 4)
    return 0


# ---------------------------------------------------------- AVAudioPlayer API
@cm("AVAudioPlayer", "alloc")
def _av_alloc(rt, recv, a):
    return rt.objc.new_obj("AVAudioPlayer", {})


@im("AVAudioPlayer", "initWithContentsOfURL:error:|initWithData:error:")
def _av_init(rt, recv, a):
    url_obj = a[0]
    path = rt.objc.pystr(url_obj) or rt.objc.hobj.get(url_obj, "")
    rt.objc.hobj[recv] = {
        "path": path,
        "loops": -1,
        "volume": 1.0,
        "duration": 120.0,
        "playing": False,
        "delegate": 0,
    }
    return recv


@im("AVAudioPlayer", "prepareToPlay")
def _av_prep(rt, recv, a):
    return 1


@im("AVAudioPlayer", "play")
def _av_play(rt, recv, a):
    info = rt.objc.hobj.get(recv, {})
    path = info.get("path", "")
    r = rt.resolve_path(path)
    if r:
        kind, p = r
        if kind == "bundle":
            import tempfile

            cache = rt.state.setdefault("bgm_cache", {})
            if p not in cache:
                data = rt.bundle.read(p)
                tmp = tempfile.NamedTemporaryFile(suffix=".mp3", delete=False)
                tmp.write(data)
                tmp.close()
                cache[p] = tmp.name
                # Approximate duration based on MP3 size (approx 128 kbps = 16000 B/s)
                info["duration"] = max(1.0, round(len(data) / 16000.0, 2))
            host_path = cache[p]
        else:
            host_path = str(p)

        if hasattr(rt, "audio_mixer"):
            rt.audio_mixer.play_bgm(host_path, loops=info.get("loops", -1))
            info["playing"] = True
    return 1


@im("AVAudioPlayer", "stop|pause")
def _av_stop(rt, recv, a):
    info = rt.objc.hobj.get(recv, {})
    info["playing"] = False
    if hasattr(rt, "audio_mixer"):
        rt.audio_mixer.stop_bgm()
    return 0


@im("AVAudioPlayer", "isPlaying")
def _av_is_playing(rt, recv, a):
    if hasattr(rt, "audio_mixer"):
        return 1 if rt.audio_mixer.is_bgm_playing() else 0
    return 0


@im("AVAudioPlayer", "duration")
def _av_duration(rt, recv, a):
    info = rt.objc.hobj.get(recv, {})
    dur = float(info.get("duration", 120.0))
    lo, hi = struct.unpack("<II", struct.pack("<d", dur))
    rt.m.set_reg(0, lo)
    rt.m.set_reg(1, hi)
    return lo


@im("AVAudioPlayer", "currentTime")
def _av_curr_time(rt, recv, a):
    t = 0.0
    if hasattr(rt, "audio_mixer"):
        t = rt.audio_mixer.get_bgm_pos()
    lo, hi = struct.unpack("<II", struct.pack("<d", t))
    rt.m.set_reg(0, lo)
    rt.m.set_reg(1, hi)
    return lo


@im("AVAudioPlayer", "setCurrentTime:")
def _av_set_curr_time(rt, recv, a):
    return 0


@im("AVAudioPlayer", "setVolume:")
def _av_volume(rt, recv, a):
    vol = a.f32(0)
    info = rt.objc.hobj.get(recv, {})
    info["volume"] = vol
    if hasattr(rt, "audio_mixer"):
        rt.audio_mixer.set_bgm_volume(vol)
    return 0


@im("AVAudioPlayer", "volume")
def _av_get_volume(rt, recv, a):
    info = rt.objc.hobj.get(recv, {})
    vol = float(info.get("volume", 1.0))
    return struct.unpack("<I", struct.pack("<f", vol))[0]


@im("AVAudioPlayer", "setNumberOfLoops:")
def _av_loops(rt, recv, a):
    info = rt.objc.hobj.get(recv, {})
    info["loops"] = int(a[0])
    return 0


@im("AVAudioPlayer", "numberOfLoops")
def _av_get_loops(rt, recv, a):
    info = rt.objc.hobj.get(recv, {})
    return info.get("loops", -1)


@im("AVAudioPlayer", "delegate")
def _av_delegate(rt, recv, a):
    info = rt.objc.hobj.get(recv, {})
    return info.get("delegate", 0)


@im("AVAudioPlayer", "setDelegate:")
def _av_set_delegate(rt, recv, a):
    info = rt.objc.hobj.get(recv, {})
    info["delegate"] = a[0]
    return 0


@im("AVAudioPlayer", "dealloc")
def _av_dealloc(rt, recv, a):
    info = rt.objc.hobj.get(recv, {})
    if info.get("playing") and hasattr(rt, "audio_mixer"):
        rt.audio_mixer.stop_bgm()
    return 0
