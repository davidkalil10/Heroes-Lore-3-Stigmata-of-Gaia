# Handoff - Heroes Lore 3 Recomp

## Estado Atual

- **Última entrega concluída**: **Assinatura Android persistente garantida entre builds locais, CI e atualizações OTA** (`!android/app/heroes_lore_3.keystore` com certificado `SHA-256: 61ca6931c36cc8f1139b9922a07fd5e1e5479c935ed7c12728296b5f1dac4905`, `--require-existing-keystore` e `--require-signed --expected-cert-sha256`), além das correções Android já validadas em `3120x1440` (loop de acelerômetro, modal/OSD Hi-DPI, recorte notch e widescreen).
- **Próxima etapa recomendada**: Após instalar o novo APK desta build (removendo uma única vez o APK anterior da Run `43632103622` que usou chave efêmera), todas as próximas builds CI/locais e atualizações OTA in-app atualizarão por cima normalmente sem desinstalar. Em seguida, validar também Linux/Switch e publicar a tag `v1.0.3` quando desejar criar o GitHub Release.
- **Versão atual do Port**: `v1.0.3` (`versionCode 4` no Android, sincronizada entre `tools/hl3rt/version.py`, `project.json`, `src/core/common.h` e `android/app/build.gradle`)
- **Idioma padrão no boot**: Inglês (`en`), alternável em tempo real para Português (`pt-BR`) via `F7` ou `Port Options & Cloud` sem reiniciar nem alterar saves

## O que foi feito e validado na Sessão 25

1. **Keystore Persistente e Enforcement no CI (`.gitignore`, `android/app/build.gradle`, `.github/workflows/build.yml`, `tools/prepare_platform_assets.py`, `tools/verify_android_package.py`)**:
   - Corrigida a geração de chaves RSA aleatórias por runner no Actions: liberada `android/app/heroes_lore_3.keystore` no `.gitignore` (`!android/app/heroes_lore_3.keystore`), tornado `signingConfig signingConfigs.release` obrigatório no buildType `release` e adicionado suporte opcional a `HL3_KEYSTORE_BASE64` / `HL3_KEYSTORE_PATH` no workflow `build-android`.
   - Adicionado `--require-existing-keystore` em `tools/prepare_platform_assets.py` e verificação obrigatória de assinatura + SHA-256 do certificado via `apksigner verify --print-certs` em `tools/verify_android_package.py`.
2. **Validação Local Executada**:
   - `python tools/prepare_platform_assets.py android --require-existing-keystore` (`OK`).
   - `python tools/verify_android_package.py` com `--require-signed --expected-cert-sha256 61ca6931c36cc8f1139b9922a07fd5e1e5479c935ed7c12728296b5f1dac4905` aprovado nos APKs `arm64` e `universal`.
   - `tests/test_multiplatform_ci.py`, suíte Python completa, `project_status.py render/check`, `bump_version.py --check` e `verify_workspace.py`.

## O que ficou pendente

- **Substituição única no celular físico**: como o APK da Run `43632103622` ainda usava chave efêmera do Actions, o usuário precisa desinstalá-lo uma única vez antes de instalar o novo APK desta build assinada com a chave permanente (`61ca6931...`); a partir dela, todas as atualizações futuras (manuais ou via OTA in-app) instalarão por cima normalmente.
- **Testes em Switch/Linux e Release formal**: testar Switch `.nro` / Linux `.AppImage` e publicar a tag `v1.0.3` quando aprovado.

## Próximo passo concreto ao retomar

Testar a instalação sobreposta de builds com a nova chave persistente no Android (e o auto-updater quando houver nova versão/`versionCode`) ou verificar localmente:

```powershell
python tools/verify_workspace.py
python tools/bump_version.py --check
python -m unittest discover -s tests -p "test_*.py"
```
