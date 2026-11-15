## Heroes Lore 3: Stigmata of Gaia Recomp `v1.0.7`

### What's New in `v1.0.7` (`Android versionCode 8`)
- **Nintendo Switch Automatic In-App Update & Relaunch (`.nro` + `envSetNextLoad`)**:
  - Replaced the manual staging fallback on Nintendo Switch with `install_portable_nro_update`: streams the downloaded `.nro` via `stdio` (`fread`/`fwrite`), validates the `NRO0` header magic, size, and SHA-256 before touching the active executable, flushes saves/configs and calls `romfsExit()` safely via `before_swap_cb_`, performs atomic swap with rollback protection, cleans up the staging/backup files, calls `envSetNextLoad` on the exact launched NRO path (`argv[0]`), and exits cleanly so Homebrew Loader relaunches the updated version automatically.
- **Test & CI Determinism**:
  - Added `test_switch_nro_installer` covering Switch NRO validation, pre-unmount rejection on corrupt magic/checksum, and atomic replacement.

## Heroes Lore 3: Stigmata of Gaia Recomp `v1.0.6`

### What's New in `v1.0.6` (`Android versionCode 7`)
- **Nintendo Switch Performance Optimization (`ArmFastCpu` + Pack A)**:
  - Introduced `ArmFastCpu`, a pre-decoded C++ ARMv6 + VFPv2 executor enabled on Nintendo Switch / Unicorn-TCI builds with deterministic RAII Unicorn fallback for Thumb-1 and uncovered instructions, validated lockstep against the Unicorn oracle with pixel-identical headless output.
  - Added host RAM mapping, zero-copy `libc` HLE fast paths, Objective-C selector/method caching, and OpenGL ES quad batching (`VBO`), reducing Unicorn-TCI steady-state frame cost by **2.37x** (`9.68 ms/frame` -> `4.09 ms/frame` over frames 120–360).
- **Nintendo Switch Face Button Swap (`A <-> B`, `X <-> Y`)**:
  - Inverted `A <-> B` and `X <-> Y` behavior on Nintendo Switch and Switch controllers across platforms to match native Nintendo ergonomics (`A` Confirm/Interact, `B` Cancel/Back).
- **Complete Gamepad HUD Slot Mapping + `README.md` Sync**:
  - Mapped all 3 skill slots and Port Options directly to the controller: `LT`/`ZL` (Skill 1), `RT`/`ZR` (Skill 2), `R3` (Right Stick Click — Skill 3), `LB`/`RB` (Potions 1/2 and Menu Tabs), `L3` (Left Stick Click — Port Options & Cloud), `X` (Primary Attack), and `Y` (Secondary Attack / Skip).

## Heroes Lore 3: Stigmata of Gaia Recomp `v1.0.5`

Native multiplatform C++17 recompilation/runtime port by **David Kalil Braga (2026)** built from the original iOS release `com.ea.heroesloresg.bv` (v1.0.1).

### What's New in `v1.0.5` (`Android versionCode 6`)
- **Fix for Transient Grid / Tile Seams in Menus & Map (GLES2 `highp` UV Precision)**: Raised OpenGL ES 2.0 vertex and fragment shader float precision to `highp` (`GL_FRAGMENT_PRECISION_HIGH` guard in fragment shader) so `1024x1024` UI/HUD/sprite atlas texture coordinates do not suffer FP16 quantization (`0.5` texel steps) or border bleeding under mobile fractional scaling. Also routed `GLBridge::tex_parameter_f` filter/wrap calls through `tex_parameter_i` to enforce crisp `GL_NEAREST` sampling consistently.
- **Instant Cloud Save & Updater UI Status Refresh**: Fixed `PortOptionsModal::poll()` failing to trigger a redraw on the final frame when a background Cloud Save or OTA updater worker transitioned from active (`busy` / `authorizing`) back to idle, preventing the menu from staying visually stuck on `"Verificando save na nuvem..."` or `"Fazendo backup..."` until re-opening. Added `CloudSaveManager::launch_worker` atomic initial status assignment and RAII busy-flag cleanup.
- **OTA & Over-Install Ready**: Signed `arm64-v8a` and `universal` APKs use the persistent release keystore (`SHA-256: 61ca6931c36cc8f1139b9922a07fd5e1e5479c935ed7c12728296b5f1dac4905`) with `versionCode 6` for direct over-install and in-app OTA upgrade from `v1.0.4`.

### Downloads Included in This Release

| Platform | Package | Notes |
| --- | --- | --- |
| **Windows x64** | `heroes_lore_3_windows_x64.zip` / `HeroesLore3.exe` | Standalone portable build, True Widescreen, XInput/Gamepad, OpenAL mixer, Google Drive Cloud Save & in-app self-updater |
| **Linux x86_64** | `heroes_lore_3_linux_x86_64.AppImage` | Portable AppImage (`chmod +x`), XDG persistent save storage, `libcurl` HTTP client, OpenAL mixer & desktop notifications |
| **Android** | `heroes_lore_3_android_arm64.apk` / `heroes_lore_3_android_universal.apk` | Sensor-landscape OpenGL ES 2.0 + SDL2 multi-touch & controller support, JNI HTTPS + Cloud Save + APK OTA updater |
| **Nintendo Switch** | `heroes_lore_3.nro` | `devkitA64` + `libnx` + Unicorn-TCI (Title ID `0100F3A300030000`), docked/handheld 1080p/720p + touch/Joy-Con + RomFS assets |

Verify package integrity with `SHA256SUMS.txt`.

