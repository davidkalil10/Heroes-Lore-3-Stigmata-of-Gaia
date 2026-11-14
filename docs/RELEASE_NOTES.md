## Heroes Lore 3: Stigmata of Gaia Recomp `v1.0.4`

Native multiplatform C++17 recompilation/runtime port by **David Kalil Braga (2026)** built from the original iOS release `com.ea.heroesloresg.bv` (v1.0.1).

### What's New in `v1.0.4` (`Android versionCode 5`)
- **BGM / Music Playback Fix**: Implemented `+[NSString stringWithCString:length:]`, `+[NSString stringWithCString:encoding:]`, and `-[NSString stringByAppendingFormat:]` in the HLE Objective-C Foundation layer so `AVAudioPlayer` resolves all 20 `.mp3` music tracks (`snd/bgm0_mf.mp3`..`bgm18_mf.mp3`, `battle_cry.mp3`) instead of loading the root directory path.
- **Independent In-Game & Port BGM Volume**: Decoupled `AVAudioPlayer`'s internal fades (`setVolume:`) from `AudioMixer`'s user music volume preference (`config/audio.json`), preserving both in-game fades and Port Options volume controls cleanly.
- **Persistent Android Signing & OTA Compatibility**: Signed `arm64-v8a` and `universal` APKs use the persistent release keystore (`SHA-256: 61ca6931c36cc8f1139b9922a07fd5e1e5479c935ed7c12728296b5f1dac4905`) for direct over-install and in-app OTA upgrades.
- **Android Gameplay & UI Fixes**: Disabled SDL accelerometer-as-joystick auto-scrolling, scaled **Port Options & Cloud** + OSD badges for Hi-DPI displays, enabled `LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES`, and added widescreen pre-game backdrop + gameplay aspect handling.
- **Cross-Platform Version Sync Tool**: Enhanced `tools/bump_version.py` (documented in `docs/VERSIONING.md`) to synchronize `PORT_VERSION`, `kPortVersion`, `project.json`, Android `versionName`/`versionCode`, Switch NACP metadata, CI workflow defaults, and `README.md`.

### Downloads Included in This Release

| Platform | Package | Notes |
| --- | --- | --- |
| **Windows x64** | `heroes_lore_3_windows_x64.zip` / `HeroesLore3.exe` | Standalone portable build, True Widescreen, XInput/Gamepad, OpenAL mixer, Google Drive Cloud Save & in-app self-updater |
| **Linux x86_64** | `heroes_lore_3_linux_x86_64.AppImage` | Portable AppImage (`chmod +x`), XDG persistent save storage, `libcurl` HTTP client, OpenAL mixer & desktop notifications |
| **Android** | `heroes_lore_3_android_arm64.apk` / `heroes_lore_3_android_universal.apk` | Sensor-landscape OpenGL ES 2.0 + SDL2 multi-touch & controller support, JNI HTTPS + Cloud Save + APK OTA updater |
| **Nintendo Switch** | `heroes_lore_3.nro` | `devkitA64` + `libnx` + Unicorn-TCI (Title ID `0100F3A300030000`), docked/handheld 1080p/720p + touch/Joy-Con + RomFS assets |

Verify package integrity with `SHA256SUMS.txt`.

