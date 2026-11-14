# Handoff - Heroes Lore 3 Recomp

## Estado Atual

- **Última entrega concluída**: **Builds 4/4 verdes no GitHub Actions (`Run 43626533694`), artefatos individuais sem `.zip` extra (`upload-artifact@v7 archive: false`) e correção do crash Android testada ao vivo no emulador (`Pixel_6_Pro_API_35`)**
- **Próxima etapa recomendada**: Usuário testar os artefatos da Run [`43626533694`](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/actions/runs/43626533694) no celular Android físico, desktop Linux e Nintendo Switch físico (`0100F3A300030000`), e quando desejar publicar um GitHub Release formal, criar/enviar a tag `v1.0.3` (ou rodar `workflow_dispatch` com `create_release=true`).
- **Versão atual do Port**: `v1.0.3` (`versionCode 4` no Android, sincronizada entre `tools/hl3rt/version.py`, `project.json`, `src/core/common.h` e `android/app/build.gradle`)
- **Idioma padrão no boot**: Inglês (`en`), alternável em tempo real para Português (`pt-BR`) via `F7` ou `Port Options & Cloud` sem reiniciar nem alterar saves
- **Artefatos da Run GitHub Actions [`43626533694`](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/actions/runs/43626533694) (commit `8757fab`, publicados diretamente via `archive: false`)**:
  - `HeroesLore3.exe` (`6.752.256` bytes)
  - `heroes_lore_3_windows_x64.zip` (`45.497.323` bytes)
  - `heroes_lore_3_linux_x86_64.AppImage` (`53.352.952` bytes)
  - `heroes_lore_3_android_arm64.apk` (`50.497.979` bytes; local: `50.497.707` bytes, SHA-256 `a7f4075f73c2957e1dedaa329e7996145bd9574840260bae4cd4c902965bc0e4`)
  - `heroes_lore_3_android_universal.apk` (`60.178.568` bytes; local: `60.174.184` bytes, SHA-256 `85c4c2941eb1e2ca35f2fe0fd6ff8c205eede32a0fd8a50d4e197aeebc2071fa`)
  - `heroes_lore_3.nro` (`55.908.565` bytes)

## O que foi feito e validado na Sessão 23

1. **Diagnóstico e Correção do Crash Imediato no Android (`CMakeLists.txt`, `tools/verify_android_package.py`)**:
   - Reproduzido no emulador local `Pixel_6_Pro_API_35` (`emulator-5554`): `FATAL EXCEPTION: SDLActivity` -> `java.lang.UnsatisfiedLinkError: No implementation found for void org.libsdl.app.HIDDeviceManager.HIDDeviceRegisterCallback()`, causado por `set(SDL_HIDAPI OFF)` excluindo `src/hidapi/android/hid.cpp` de `libSDL2.so`.
   - Habilitado `SDL_HIDAPI ON` + `SDL_HIDAPI_LIBUSB OFF` em [`CMakeLists.txt`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/CMakeLists.txt#L31-L39) e adicionado check obrigatório das exportações JNI (`Java_org_libsdl_app_SDLActivity_nativeSetupJNI`, `Java_org_libsdl_app_HIDDeviceManager_HIDDeviceRegisterCallback`) em [`tools/verify_android_package.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/verify_android_package.py) e [`tests/test_multiplatform_ci.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/test_multiplatform_ci.py).
   - Validado ao vivo em `emulator-5554` com `dist/heroes_lore_3_android_universal.apk`: abertura fria (`1858 ms`), tela de título idêntica à versão PC (portrait e landscape `2975x1440`), processo mantido vivo (`PID=4082`) após `10 s` e responsivo a toque sintético.
2. **Desbloqueio Linux + Switch e Remoção de `.zip` Extra no Actions (`.github/workflows/build.yml`, `tools/prepare_platform_assets.py`, `CMakeLists.txt`)**:
   - **Actions sem `.zip`**: migrados uploads para `actions/upload-artifact@v7` com `archive: false` (um arquivo por upload) e job `release` para `actions/download-artifact@v8`.
   - **Linux `build-linux`**: substituído `verify_workspace.py` (que dependia de arquivos gitignored em `reference/hl5_snapshot/src/vendor/`) pelos verificadores versionados do repo; adicionado fallback `_pil_lanczos()` em [`tools/prepare_platform_assets.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/prepare_platform_assets.py) compatível com Pillow `<9.1` (`Image.Resampling` ausente no Ubuntu CI).
   - **Switch `build-switch`**: removido `dkp-pacman -Syu` redundante (HTTP `403` rate limit) em favor de `-S --noconfirm --needed` com fallback em backoff para `-Sy`; movido `platforms/switch/unicorn_os.c` de `libhl3_core.a` para `target_sources(HeroesLore3 PRIVATE ...)` em `CMakeLists.txt` eliminando `undefined reference to mmap/munmap/mprotect` no `ld` estático.
3. **Validações Locais Finais (após última edição de código em `8757fab`)**:
   - PC C++17: `cmake --build build_cpp -j` (`102/102`), `ctest --test-dir build_cpp --output-on-failure` (`3/3`: `macho_boot`, `headless_runtime`, `platform_services`).
   - Python: `python -m unittest discover -s tests -p "test_*.py" -v` (`68/68` OK em `36,115 s`), `python tools/bump_version.py --check` (`v1.0.3`) e `python tools/verify_android_package.py` nos 2 APKs reconstruídos.
   - CI: GitHub Actions Run [`43626533694`](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/actions/runs/43626533694) concluiu com `success` em todas as 4 plataformas.

## O que ficou pendente

- **Teste manual do usuário em hardware físico**:
  - Instalar e testar o novo `heroes_lore_3_android_arm64.apk` / `heroes_lore_3_android_universal.apk` (da Run `43626533694`) no celular Android real;
  - Testar `heroes_lore_3_linux_x86_64.AppImage` em desktop Linux e `heroes_lore_3.nro` no Nintendo Switch real (`0100F3A300030000`, inclusive conferindo `sdmc:/switch/heroes_lore_3/boot.log` e jogabilidade TCI).
- **Publicação de Tag/Release**: após aprovação dos testes em hardware, publicar GitHub Release via tag `v1.0.3` (ou `workflow_dispatch` com `create_release=true`), e evoluir para `S14` (`1.0.0` final/auditoria).

## Próximo passo concreto ao retomar

Testar os artefatos baixados da Run `43626533694` nos dispositivos físicos ou, caso queira rodar a verificação completa local do repositório:

```powershell
python tools/verify_workspace.py
python tools/bump_version.py --check
python -m unittest discover -s tests -p "test_*.py" -v
```
