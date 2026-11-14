# Handoff - Heroes Lore 3 Recomp

## Estado Atual

- **Última entrega concluída**: **Infraestrutura Multiplataforma Completa (`Windows x64`, `Linux AppImage`, `Android ARM64/Universal APK`, `Nintendo Switch NRO`) + `.github/workflows/build.yml` (`v1.0.3`)**
- **Próxima etapa recomendada**: Fazer push para o GitHub e validar no Actions os runners Linux (`AppImage`) e Switch (`devkitA64`), além de testar os APKs em dispositivo/emulador Android e `.nro` no Switch (`0100F3A300030000`).
- **Versão atual do Port**: `v1.0.3` (`versionCode 4` no Android, sincronizada entre `tools/hl3rt/version.py`, `project.json`, `src/core/common.h` e `android/app/build.gradle`)
- **Idioma padrão no boot**: Inglês (`en`), alternável em tempo real para Português (`pt-BR`) via `F7` ou `Port Options & Cloud` sem reiniciar nem alterar saves
- **Pacotes verificados localmente no Windows host**:
  - **Windows x64**: `dist/HeroesLore3_PC/HeroesLore3.exe` (`6.723.584` bytes, SHA-256 `205a2102e717cf40e6977dea3d4f1c55bd49507b8dbc54e09dffda5054da722a`) e `dist/heroes_lore_3_windows_x64.zip` (`45.482.126` bytes, SHA-256 `2a686c55acf74ef052a260eedcce80e9f1847a021c3f18f3304353cac6d05c8b`)
  - **Android `arm64-v8a`**: `dist/heroes_lore_3_android_arm64.apk` (`50.316.995` bytes, SHA-256 `a496abbd9f2370d9b3aa97dd74f2c6963d01579e8194fd859446f4ce0868ce04`)
  - **Android `universal` (`arm64-v8a` + `armeabi-v7a`)**: `dist/heroes_lore_3_android_universal.apk` (`54.029.329` bytes, SHA-256 `792d84514c795345512633da5f7478a9e838a9a2a65f28b3d2f0c822d53158e1`)

## O que foi feito e validado na Sessão 22 (`S12` + `S13` Multiplataforma + CI)

1. **Adaptação Multiplataforma do Core C++17 (`CMakeLists.txt`, `src/`)**:
   - Padronizado `<SDL.h>` em toda a base; adicionados armazenamento gravável portável (`find_writable_data_root()`), pausa em segundo plano (`AudioMixer::set_paused`), multi-touch `SDL_FINGER*` + desduplicação `SDL_TOUCH_MOUSEID` + botão voltar Android (`SDLK_AC_BACK -> SDLK_ESCAPE`), fonte Horizon `plGetSharedFontByType` + fallback TTF Android/Linux em `src/platform/ui_renderer.cpp`.
   - Em `src/platform/net_http.{h,cpp}` e `src/platform/updater.cpp`: implementada ponte JNI Android (`SDL_AndroidGetActivity()` + `GetObjectClass()`, âncora `volatile` em `nativeDownloadProgress` contra `--gc-sections` e `install_android_apk()` via `ApkFileProvider`), cliente `libcurl` + `romfs:/cacert.pem` para Linux/Switch e `xdg-open` protegido por `#if defined(__linux__) && !defined(__ANDROID__)`.
2. **Android Gradle + NDK Build & Verificação (`android/`, `tools/verify_android_package.py`)**:
   - Configurados Gradle `8.1.1`, AGP `8.1.1`, NDK `26.3.11579264`, SDL2 `2.30.12` dinâmica + `libmain.so` (`ANDROID_STL=c++_shared`), `com.davidbraga.heroeslore3`, `versionCode 4`, `versionName '1.0.3'`, permissões `INTERNET` + `REQUEST_INSTALL_PACKAGES` e assets preparados via `tools/prepare_platform_assets.py android`.
   - Compilados e assinados localmente ambos os APKs de release (`assembleRelease`, `4m 18s`), corrigido bug `sh[1]` (`sh_type`) + detecção de XML binário otimizado `adaptive-icon` em `tools/verify_android_package.py` e validados ambos os APKs com `verify_android_package.py`, `apksigner verify` e inspeção ELF confirmando `SDL_main=True` e `nativeDownloadProgress=True` nas duas ABIs (`arm64-v8a` e `armeabi-v7a`).
3. **Linux AppImage, Nintendo Switch (`0100F3A300030000`) & GitHub Actions (`.github/workflows/build.yml`)**:
   - Configurados `platforms/switch/{toolchain.cmake,unicorn_os.c,compat/sys/mman.h}`, `tests/switch_memory_contract.c`, `tools/patch_unicorn_switch.py` (refatorado com tabela `REPLACEMENTS`, `-DUNICORN_INTERPRETER=ON` + buffer TCI `32 MiB`), `tools/fetch_build_deps.py`, `tools/prepare_platform_assets.py` e `tools/verify_switch_package.py`.
   - Workflow `.github/workflows/build.yml` criado com os jobs `build-windows`, `build-linux` (`xvfb-run` + AppImage), `build-android`, `build-switch` (`devkitpro/devkita64:latest` + `elf2nro`) e `release` (`SHA256SUMS.txt` + `docs/RELEASE_NOTES.md`).
   - Validação local: `cmake --build build_cpp --parallel` (`90/90`) + `ctest` (`3/3`), `tools/build_pc_cpp.py`, `tools/bump_version.py --check` (`v1.0.3`), e `unittest` (`68/68` incluindo `tests/test_multiplatform_ci.py` e correção de hermeticidade em `tests/test_s09_s10_localization.py`).

## O que ficou pendente

- **Execução real em Linux x86_64 e Nintendo Switch (`devkitA64`)**: o host desta máquina é Windows; os builds/testes reais de `build-linux` (`AppImage`) e `build-switch` (`.nro`) estão prontos no `.github/workflows/build.yml` para validação na primeira execução do GitHub Actions.
- **Smoke-test em hardware/emulador**: instalar e testar o APK (`arm64-v8a` / `universal`) em aparelho Android real/AVD e o `.nro` no console Nintendo Switch (inclusive HUD/touch/controllers e OTA).

## Próximo passo concreto ao retomar

Verificar `.github/workflows/build.yml` após o `git push` no GitHub Actions (ou disparar manualmente via `workflow_dispatch`), além de manter os checks locais verdes:

```powershell
python tools/verify_workspace.py
python tools/bump_version.py --check
python -m unittest discover -s tests -p "test_*.py" -v
```
