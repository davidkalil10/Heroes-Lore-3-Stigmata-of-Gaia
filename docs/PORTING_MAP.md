# Mapa de Reaproveitamento (Porting Map) - HL4 & HL5 -> HL3

Este documento define o que deve ser reaproveitado diretamente dos snapshots congelados (`reference/hl4_snapshot/` e `reference/hl5_snapshot/`) e o que precisa ser construído especificamente para o **Heroes Lore 3**.

## 1. Componentes Reutilizáveis (Adaptar de HL4 / HL5)

| Subsistema | Origem nos Snapshots | Como Reutilizar no HL3 |
| --- | --- | --- |
| **Decodificador e Emissor AOT ARM/Thumb + Oráculo** | `reference/hl4_snapshot/src/arm_hot_decoder.inc`, `arm_hot_bridge.inc`, `tools/arm_emit.py`, `tools/generate_arm_hot.py`, `tools/verify_arm_emit.py` | Reutilizar a infraestrutura de decodificação ARM32/Thumb, guarda de opcodes e validação de páginas sujas contra o Unicorn. |
| **Watchdog Persistente de Emulação** | `reference/hl4_snapshot/src/emulation_watchdog.inc` | Reutilizar diretamente para evitar criação/destruição de `std::thread` a cada fatia de frame. |
| **Patches e Toolchain de Nintendo Switch** | `reference/hl4_snapshot/platforms/switch/toolchain.cmake`, `unicorn_os.c`, `compat/sys/mman.h`, `tools/patch_unicorn_switch.py`, `src/switch_perf.inc` | Reutilizar o limite de 32 MiB do TCI cache, o wrapper de `mman.h`, a configuração de clock e o logger direto via `fsFileWrite`. |
| **Decodificador MP3 e Mixer SDL2** | `reference/hl5_snapshot/src/audio/dr_mp3.h`, `src/audio/audio.cpp`, `reference/hl4_snapshot/src/audio_runtime.inc` | Usar `dr_mp3.h` para decodificar os 20 arquivos `snd/bgm*_mf.mp3` do HL3 fora do callback da SDL. |
| **Layout de Display e Testes de Widescreen** | `reference/hl5_snapshot/src/display_layout.cpp`, `tools/display_layout_test.cpp`, `reference/hl4_snapshot/tools/verify_aspect_captures.py` | Adaptar para a resolução base paisagem do HL3 (`480x320`, `3:2`) expandindo para `16:9`, `16:10`, `20:9` e `4:3`. |
| **Menu Nativo, Cloud Save (Google Drive) e OTA Updater** | `reference/hl5_snapshot/src/native_settings.cpp`, `services.cpp`, `service_net.cpp`, `service_data.cpp`, `service_ui.cpp` | Reutilizar a UI do modal nativo, fluxo OAuth2/PKCE ou Device Flow do Google Drive e verificador de releases do GitHub, ajustando os caminhos de save e IDs do HL3. |
| **Workflow CI Multiplataforma** | `reference/hl4_snapshot/.github/workflows/build.yml` e `reference/hl5_snapshot/.github/workflows/build.yml` | Adaptar para compilar `heroes_lore_3` nos 4 alvos (`windows-x64`, `linux-x86_64`, `android-arm64`, `nintendo-switch`). |

## 2. Componentes Exclusivos do HL3 (Construir do Zero)

| Subsistema | Por que é exclusivo do HL3 | Etapa |
| --- | --- | --- |
| **Loader Mach-O 32-bit ARMv6 (`MH_EXECUTE`)** | O binário `Payload/Heroes Lore.app/Heroes Lore` é Mach-O iOS (`__TEXT` em `0x1000`, `__DATA` em `0xcc000`), não um `.so` ELF Android. | `S02` / `S03` |
| **Mini-Runtime Objective-C 2.0 + HLE iOS (`UIKit`/`Foundation`/`EAGL`)** | O jogo inicializa via `UIApplicationMain`, instancia `AppDelegate`, `GLView` e `TouchResponder`, e usa `_objc_msgSend`, `NSBundle`, `NSUserDefaults` e `NSTimer`. | `S02` / `S03` |
| **Bridge OpenGL ES 1.1 + Decodificador `.pvr`** | Diferente do framebuffer de software 16-bit do HL4, o HL3 desenha com `glOrthof`/`glDrawArrays`/`glDrawElements` e carrega texturas PowerVR (`newres/sprites/*.pvr`). | `S04` |
| **HLE de OpenAL + `AVAudioPlayer`** | Os efeitos sonoros `.wav` usam OpenAL (`alGenSources`, `alSourcePlay`) e as músicas `.mp3` usam `AVAudioPlayer`. | `S06` |
| **Parser de Fontes `.rff` e Tabelas `dat/InGame_txt`** | As fontes bitmap/métricas estão em `newres/fonts/*.rff` + `*.pvr` e os textos em `dat/InGame_txt` + `*.txt` + `Localizable.strings`. | `S09` / `S10` |
