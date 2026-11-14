# Handoff - Heroes Lore 3 Recomp

## Estado Atual

- **Última entrega concluída**: **Correções Android (loop de acelerômetro, escala Hi-DPI do `PortOptionsModal`/OSD, notch `LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES` e Widescreen/Auto no pré-jogo e gameplay) validadas ao vivo no emulador `Pixel_6_Pro_API_35` (`3120x1440`) e novo ciclo enviado ao GitHub Actions**
- **Próxima etapa recomendada**: Usuário testar os novos APKs (`arm64` / `universal`) da execução atual do GitHub Actions no celular Android físico (junto aos builds Windows, Linux `.AppImage` e Nintendo Switch `.nro`), e quando desejar publicar o Release formal, criar/enviar a tag `v1.0.3` (ou rodar `workflow_dispatch` com `create_release=true`).
- **Versão atual do Port**: `v1.0.3` (`versionCode 4` no Android, sincronizada entre `tools/hl3rt/version.py`, `project.json`, `src/core/common.h` e `android/app/build.gradle`)
- **Idioma padrão no boot**: Inglês (`en`), alternável em tempo real para Português (`pt-BR`) via `F7` ou `Port Options & Cloud` sem reiniciar nem alterar saves

## O que foi feito e validado na Sessão 24

1. **Bloqueio de Auto-Navegação em Loop no Android (`src/main.cpp`, `src/platform/input_controller.*`)**:
   - Configurados `SDL_HINT_ACCELEROMETER_AS_JOYSTICK="0"` e `SDL_HINT_TV_REMOTE_AS_JOYSTICK="0"` antes de `SDL_Init`, adicionada filtragem `InputController::should_ignore_joystick_device` em `handle_controller_added` para sensores sem botões/hats (`accelerometer`, `gyro`, `compass`, `fingerprint`), e `InputController::release_all_touches` ao entrar em background ou perder foco.
   - Validado no `emulator-5554` com injeção de evento `emu sensor set acceleration 8:2:4` e comparação de screenshots com intervalo de 3s sem qualquer deriva automática.
2. **Janela `PortOptionsModal` e OSD em Alta Resolução (`src/platform/options_modal.*`, `src/platform/input_controller.cpp`)**:
   - Refatorado `PortOptionsModal::compute_box` para dimensionamento proporcional (~88% da altura segura) e teto de escala elevado de `1.35f` para `2.8f` no modal (`px_s`) e no OSD (`draw_osd`).
   - Verificado ao vivo no `emulator-5554` em `3120x1440`: janela e textos grandes, centralizados e nítidos.
3. **Cutout Edge-to-Edge e Widescreen/Auto em Pré-Jogo & Gameplay (`HeroesLore3Activity.java`, `src/platform/display_config.*`, `src/hle/gl_bridge.*`)**:
   - Adicionado `LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES` (`Build.VERSION_CODES.P`+) e flags imersivas modernas em [`HeroesLore3Activity.java`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/android/app/src/main/java/com/davidbraga/heroeslore3/HeroesLore3Activity.java); superfície SDL confirmada em `3120x1440` (`Device size: 3120x1440`) sem faixa no notch.
   - Adicionados `hl3::compute_target_aspect_rect` e `GLBridge::present_widescreen_backdrop()` (cópia do framebuffer via `glCopyTexSubImage2D` com blur/vinheta opaca em `u_tex_mode == 2`) preenchendo visualmente as bordas em `16:9` e `Auto` no pré-jogo sem esticar HUD/menus, e gameplay inicial verificado em widescreen nativo `3120x1440`.
4. **Build & Testes Executados**:
   - `cmake --build build_cpp --parallel` + `ctest --test-dir build_cpp --output-on-failure -V` (`3/3`: `macho_boot`, `headless_runtime`, `platform_services`).
   - Gradle NDK `assembleRelease` local + `python tools/verify_android_package.py` (`PASS` em `heroes_lore_3_android_arm64.apk` e `heroes_lore_3_android_universal.apk`).
   - Suíte Python (`python -m unittest discover -s tests -p "test_*.py"`), `python tools/bump_version.py --check`, `python tools/project_status.py check` e `python tools/verify_workspace.py`.

## O que ficou pendente

- **Validação em hardware físico**: instalar os novos APKs (`arm64` / `universal`) gerados pelo Actions no celular Android real para confirmar fim do loop de navegação, legibilidade do modal e modo widescreen/Auto, além do teste físico no Switch (`0100F3A300030000`) e Linux AppImage.
- **Release formal**: após aprovação em aparelho real, criar tag `v1.0.3` / `workflow_dispatch` (`create_release=true`) e avançar para `S14` (`1.0.0` final).

## Próximo passo concreto ao retomar

Verificar os artefatos recém-publicados na última execução do GitHub Actions e validar no dispositivo Android físico; localmente, rodar:

```powershell
python tools/verify_workspace.py
python tools/bump_version.py --check
ctest --test-dir build_cpp --output-on-failure
```
