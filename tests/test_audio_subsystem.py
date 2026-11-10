"""Automated unit tests for S06: Heroes Lore 3 Audio Subsystem (OpenAL + AVAudioPlayer)."""

from __future__ import annotations

import io
import json
import struct
import sys
import tempfile
import unittest
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.hle_audio import (
    AL_BUFFER,
    AL_FORMAT_MONO16,
    AL_FORMAT_STEREO16,
    AL_GAIN,
    AL_LOOPING,
    AL_PLAYING,
    AL_SOURCE_STATE,
    AL_STOPPED,
    AudioMixer,
)
from hl3rt.machine import Bundle
from hl3rt.runtime import Runtime


class S06AudioSubsystemTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bundle = Bundle()
        cls.rt = Runtime()

    def test_all_snd_assets_exist_and_valid(self):
        """All 28 WAV files and 20 MP3 files in snd/ must exist and be valid audio."""
        snd_names = [n for n in self.bundle.names if n.startswith("Payload/Heroes Lore.app/snd/")]
        wav_names = sorted([n for n in snd_names if n.endswith(".wav")])
        mp3_names = sorted([n for n in snd_names if n.endswith(".mp3")])

        self.assertEqual(len(wav_names), 28, f"Expected 28 WAV files, found {len(wav_names)}")
        self.assertEqual(len(mp3_names), 20, f"Expected 20 MP3 files, found {len(mp3_names)}")

        # Validate each WAV file header and PCM parameters
        for wn in wav_names:
            rel = wn.split("Payload/Heroes Lore.app/")[1]
            data = self.bundle.read(rel)
            self.assertGreater(len(data), 44, f"WAV file {rel} too small")
            with wave.open(io.BytesIO(data), "rb") as wf:
                self.assertIn(wf.getnchannels(), (1, 2))
                self.assertEqual(wf.getsampwidth(), 2)  # 16-bit
                self.assertEqual(wf.getframerate(), 44100)

        # Validate each MP3 file size
        for mn in mp3_names:
            rel = mn.split("Payload/Heroes Lore.app/")[1]
            data = self.bundle.read(rel)
            self.assertGreater(len(data), 50000, f"MP3 file {rel} suspiciously small")

    def test_audio_mixer_volume_and_persistence(self):
        """AudioMixer volume controls, mute toggle and config save/load must work cleanly."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            cfg_path = Path(tmp_dir) / "audio_test.json"
            mixer = AudioMixer(self.bundle, config_path=cfg_path)

            self.assertEqual(mixer.master_volume, 1.0)
            self.assertEqual(mixer.bgm_volume, 1.0)
            self.assertEqual(mixer.sfx_volume, 1.0)
            self.assertFalse(mixer.muted)

            # Update volumes
            mixer.set_master_volume(0.75)
            mixer.set_bgm_volume(0.60)
            mixer.set_sfx_volume(0.85)
            mixer.set_muted(True)
            self.assertEqual(mixer.master_volume, 0.75)
            self.assertEqual(mixer.bgm_volume, 0.60)
            self.assertEqual(mixer.sfx_volume, 0.85)
            self.assertTrue(mixer.muted)

            # Save configuration
            mixer.save_config()
            self.assertTrue(cfg_path.exists())

            # Load into new instance
            mixer2 = AudioMixer(self.bundle, config_path=cfg_path)
            self.assertAlmostEqual(mixer2.master_volume, 0.75)
            self.assertAlmostEqual(mixer2.bgm_volume, 0.60)
            self.assertAlmostEqual(mixer2.sfx_volume, 0.85)
            self.assertTrue(mixer2.muted)

            # Test unmuting
            muted = mixer2.toggle_mute()
            self.assertFalse(muted)
            self.assertFalse(mixer2.muted)

    def test_openal_buffer_and_source_lifecycle(self):
        """OpenAL buffers and sources can be allocated, bound, played and stopped."""
        mixer = self.rt.audio_mixer
        if not mixer.initialized:
            self.skipTest("SDL audio mixer not initialized in headless environment")

        # Generate 0.1s of 440 Hz sine wave PCM (mono 16-bit 44.1 kHz)
        import math

        num_samples = int(44100 * 0.1)
        samples = [int(32767 * 0.5 * math.sin(2 * math.pi * 440 * i / 44100)) for i in range(num_samples)]
        pcm_bytes = struct.pack(f"<{num_samples}h", *samples)

        # Buffer data
        buf_id = 9001
        mixer.buffer_data(buf_id, AL_FORMAT_MONO16, pcm_bytes, 44100)
        self.assertIn(buf_id, mixer.buffers)

        # Create source
        src_id = 101
        mixer.sources[src_id] = {
            "buffer": buf_id,
            "state": AL_STOPPED,
            "gain": 1.0,
            "pitch": 1.0,
            "looping": False,
            "channel": None,
        }

        # Play source
        mixer.play_source(src_id)
        self.assertEqual(mixer.sources[src_id]["state"], AL_PLAYING)

        # Stop source
        mixer.stop_source(src_id)
        self.assertEqual(mixer.sources[src_id]["state"], AL_STOPPED)

        # Delete source and buffer
        mixer.delete_sources([src_id])
        self.assertNotIn(src_id, mixer.sources)
        mixer.delete_buffers([buf_id])
        self.assertNotIn(buf_id, mixer.buffers)

    def test_openal_traps_execution(self):
        """OpenAL guest traps correctly handle alGen, alBufferData, alSourcei/f and alGetSourcei."""
        m = self.rt.m

        # alGenBuffers(1, ptr)
        ptr_buf = m.malloc(4)
        m.set_reg(0, 1)
        m.set_reg(1, ptr_buf)
        self.rt.call_import("_alGenBuffers")
        buf_id = m.rd32(ptr_buf)
        self.assertGreater(buf_id, 0)

        # alGenSources(1, ptr)
        ptr_src = m.malloc(4)
        m.set_reg(0, 1)
        m.set_reg(1, ptr_src)
        self.rt.call_import("_alGenSources")
        src_id = m.rd32(ptr_src)
        self.assertGreater(src_id, 0)

        # alSourcei(src_id, AL_BUFFER, buf_id)
        m.set_reg(0, src_id)
        m.set_reg(1, AL_BUFFER)
        m.set_reg(2, buf_id)
        self.rt.call_import("_alSourcei")
        self.assertEqual(self.rt.audio_mixer.sources[src_id]["buffer"], buf_id)

        # alSourcei(src_id, AL_LOOPING, 1)
        m.set_reg(0, src_id)
        m.set_reg(1, AL_LOOPING)
        m.set_reg(2, 1)
        self.rt.call_import("_alSourcei")
        self.assertTrue(self.rt.audio_mixer.sources[src_id]["looping"])

        # alSourcef(src_id, AL_GAIN, 0.5f)
        m.set_reg(0, src_id)
        m.set_reg(1, AL_GAIN)
        gain_bits = struct.unpack("<I", struct.pack("<f", 0.5))[0]
        m.set_reg(2, gain_bits)
        self.rt.call_import("_alSourcef")
        self.assertAlmostEqual(self.rt.audio_mixer.sources[src_id]["gain"], 0.5)

        # alGetSourcei(src_id, AL_LOOPING, ptr)
        ptr_val = m.malloc(4)
        m.set_reg(0, src_id)
        m.set_reg(1, AL_LOOPING)
        m.set_reg(2, ptr_val)
        self.rt.call_import("_alGetSourcei")
        self.assertEqual(m.rd32(ptr_val), 1)

        # Cleanup
        m.set_reg(0, 1)
        m.set_reg(1, ptr_src)
        self.rt.call_import("_alDeleteSources")
        m.set_reg(0, 1)
        m.set_reg(1, ptr_buf)
        self.rt.call_import("_alDeleteBuffers")

    def test_av_audio_player_methods(self):
        """AVAudioPlayer HLE methods (alloc, init, isPlaying, duration, volume, loops) must behave correctly."""
        rt = self.rt

        # Alloc
        av_cls = rt.objc.by_name["AVAudioPlayer"]
        player = rt.send(av_cls.addr, "alloc")
        self.assertGreater(player, 0)

        # Init with URL
        url_obj = rt.objc.new_obj("NSURL", "/var/mobile/Applications/HL3/Heroes Lore.app/snd/bgm0_mf.mp3")
        init_res = rt.send(player, "initWithContentsOfURL:error:", url_obj, 0)
        self.assertEqual(init_res, player)

        # Verify initial properties
        is_playing = rt.send(player, "isPlaying")
        self.assertEqual(is_playing, 0)

        # Set & Get volume
        vol_bits = struct.unpack("<I", struct.pack("<f", 0.8))[0]
        rt.send(player, "setVolume:", vol_bits)
        ret_vol_bits = rt.send(player, "volume")
        ret_vol = struct.unpack("<f", struct.pack("<I", ret_vol_bits))[0]
        self.assertAlmostEqual(ret_vol, 0.8, places=3)

        # Set & Get loops
        rt.send(player, "setNumberOfLoops:", 3)
        ret_loops = rt.send(player, "numberOfLoops")
        self.assertEqual(ret_loops, 3)

        # Check prepareToPlay
        prep = rt.send(player, "prepareToPlay")
        self.assertEqual(prep, 1)

        # Stop
        stop_res = rt.send(player, "stop")
        self.assertEqual(stop_res, 0)

    def test_audio_session_get_property(self):
        """_AudioSessionGetProperty must return sane values for other audio and volume."""
        m = self.rt.m

        # Query other audio playing ('othp' = 0x6f746170)
        ptr_size = m.malloc(4)
        ptr_out = m.malloc(4)
        m.wr32(ptr_size, 4)

        m.set_reg(0, 0x6F746170)
        m.set_reg(1, ptr_size)
        m.set_reg(2, ptr_out)
        res = self.rt.call_import("_AudioSessionGetProperty")
        self.assertEqual(res, 0)
        self.assertEqual(m.rd32(ptr_out), 0)

        # Query current hardware volume ('chvl' = 0x6368766c)
        m.wr32(ptr_size, 4)
        m.set_reg(0, 0x6368766C)
        m.set_reg(1, ptr_size)
        m.set_reg(2, ptr_out)
        res = self.rt.call_import("_AudioSessionGetProperty")
        self.assertEqual(res, 0)
        vol_f = struct.unpack("<f", struct.pack("<I", m.rd32(ptr_out)))[0]
        self.assertAlmostEqual(vol_f, 1.0)


if __name__ == "__main__":
    unittest.main()

