# Arquitetura Técnica - Heroes Lore 3 Recomp

## 1. Alvo Original Descoberto em `S01`

A análise determinística de `Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk` (`61.755.432` bytes) demonstrou que ele atua como wrapper touchHLE sobre um pacote iOS embarcado:

- **Bundle iOS interno**: `assets/game.ipa` (`43.686.130` bytes, `1.819` entradas)
- **Executável real do jogo**: `Payload/Heroes Lore.app/Heroes Lore` (`915.824` bytes)
  - Formato: **Mach-O 32-bit ARMv6 (`MH_EXECUTE`, cputype 12, cpusubtype 6)**
  - Bundle ID / Versão original: `com.ea.heroesloresg.bv` / `1.0.1` (iPhone OS `3.0`, orientação nativa `UIInterfaceOrientationLandscapeRight`)
  - Segmentos: `__PAGEZERO`, `__TEXT` (`0x00001000..0x000cc000`), `__DATA` (`0x000cc000..0x000e2000`), `__LINKEDIT` (`0x000e2000..0x000ea000`)
  - Metadados Objective-C: **30 classes** (`AppDelegate`, `GLView`, `TouchResponder`, `SocketBtDelegate`, `EAMTX_*`, `DMG*`, `SBJson*`) e **679 seletores**
  - Dylibs importadas: `Foundation`, `UIKit`, `OpenGLES`, `QuartzCore`, `AudioToolbox`, `OpenAL`, `AVFoundation`, `CoreGraphics`, `libSystem.B.dylib`, `libstdc++.6.dylib`, `libsqlite3.dylib`, `libz.1.dylib`, `libobjc.A.dylib`, entre outras (com `389` símbolos externos mapeados)

## 2. Arquitetura Atual do Runtime PC (`tools/hl3rt/` + `tools/run_pc.py`)

O port operante no Windows x64 (`v1.0.1`) executa o binário Mach-O original diretamente em memória sobre uma arquitetura modular:

1. **Núcleo ARMv6 & Mach-O (`tools/hl3rt/machine.py`, `runtime.py`, `objc.py`, `aot_blocks.py`)**:
   - Mapeia segmentos Mach-O, inicializa `__mod_init_func`, resolve `__nl_symbol_ptr`/`__la_symbol_ptr` para stubs HLE na região `0x7F000000` e prepara seletores/ivars Objective-C.
   - Executa instruções ARMv6/Thumb via **Unicorn Engine** combinado com `28` blocos quentes acelerados (`aot_blocks.py`, gerados por `tools/generate_aot_blocks.py`), protegidos por guarda estrita de opcodes e validados (`48/48` comparações `PASS`) contra o oráculo Unicorn completo.
2. **Camada iOS HLE (`hle_foundation.py`, `hle_uikit.py`, `hle_libc.py`, `fast_stubs.py`)**:
   - Emula ciclos de `UIApplicationMain`, `AppDelegate`, `EAGLContext`, `NSTimer`, `NSBundle` (redirecionamento dinâmico de arquivos para PT-BR em `assets/`), `NSUserDefaults`, `fopen`/`fread` em `Documents/` e `SQLite3`/`zlib`.
3. **Gráficos e True Widescreen (`hle_gl.py`, `gl_bridge.py`, `display.py`, `widescreen.py`)**:
   - Traduz chamadas **OpenGL ES 1.1** (`glOrthof`, `glBindTexture`, `glDrawArrays`, `glDrawElements`, `glCompressedTexImage2D`, FBO OES) para **OpenGL Desktop** (`PyOpenGL`).
   - Decodifica texturas `.pvr` (PVRTC / RGBA / RGB565) mantendo aspectos originais de UI.
   - Aplica patches binários verificados em largura/altura (`480×320` base até `MAX_PATCH_DIM = 1020`) e centralização de formulários (`ui_main`) / ancoragem de HUD (`ui_hud`) para `Auto`, `16:9` e `Original (3:2)`.
4. **Áudio Desacoplado (`hle_audio.py`)**:
   - `AudioMixer` com `64` canais virtuais atendendo OpenAL (`28` efeitos `.wav`) e `AVAudioPlayer` (`20` trilhas `.mp3`), mixado fora do callback de áudio SDL e persistido em `config/audio.json`.
5. **UI Nativa do Port & Serviços (`options_menu.py`, `i18n.py`, `cloud_save.py`, `updater.py`, `version.py`)**:
   - Modal OpenGL responsivo (`PortOptionsModal`) injetado de forma transparente em `Help & About` (`form=113`) e `System` (`form=3`).
   - Troca de idioma ao vivo (`EN` ↔ `PT-BR`) via `TextUtils` (`0x000d8220`) + `vtable[0x3c]` + `_refresh_guest_banner_live` (`0x0002787c`) sem alterar saves, e linha de crédito injetada no menu `About` (`slot 6632`).
   - Cloud Save isolado no Google Drive (`Heroes Lore III - Stigmata of Gaia / heroes_lore_3_save.json`) com backup pré-restauração em `data/save-backups/`.
   - Atualizador OTA via GitHub Releases (`davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia`) com checagem startup opcional (`config/services.json`), validação HTTPS/SHA-256, prevenção de ZIP Slip e script de atualização staging no Windows.

## 3. Próxima Evolução Arquitetural (`S12`–`S13`: Base C++ Multiplataforma)

Os alvos nativos **Linux x86_64**, **Android ARM64** (`com.davidbraga.heroeslore3`) e **Nintendo Switch** (`0100F3A300030000`) utilizarão o núcleo C++17 planejado sob `src/` (incluindo blocos ARM gerados em `src/arm_hot_blocks.inc` + SDL2 + `dr_mp3`), reaproveitando os contratos binários, offsets Widescreen/TextUtils e formatos de Cloud Save / OTA validados no runtime PC.
