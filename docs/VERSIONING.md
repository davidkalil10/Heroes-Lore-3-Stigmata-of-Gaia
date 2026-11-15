# Versionamento e Release Multiplataforma (`tools/bump_version.py`)

O script [`tools/bump_version.py`](../tools/bump_version.py) centraliza e valida a versão do port em todas as plataformas compiladas do **Heroes Lore 3: Stigmata of Gaia Recomp**, evitando dessincronização entre o atualizador OTA (GitHub Releases), o crédito no menu **About**, os APKs do Android e o homebrew `.nro` do Nintendo Switch.

> [!NOTE]
> A versão do jogo original iOS (`ORIGINAL_GAME_VERSION = "1.0.1"`) permanece desacoplada e intocada; somente a versão do port (`PORT_VERSION` / `kPortVersion`) e o `versionCode` do Android avançam a cada release.

---

## Arquivos sincronizados automaticamente

| Arquivo | Campo | Plataformas afetadas |
| --- | --- | --- |
| [`tools/hl3rt/version.py`](../tools/hl3rt/version.py) | `PORT_VERSION` | Runtime Python, scripts de build e Switch (`nacptool` no CI) |
| [`src/core/common.h`](../src/core/common.h) | `kPortVersion` | Executáveis/bibliotecas C++17 (Windows `.exe`, Linux `AppImage`, Android `libmain.so`, Switch `.nro`) — alimenta UI do port, crédito no *About* e `UpdateManager` |
| [`project.json`](../project.json) | `port_version` | Manifesto canônico do workspace (`verify_workspace.py`) |
| [`android/app/build.gradle`](../android/app/build.gradle) | `versionName` + `versionCode` | APKs Android (`arm64-v8a` e `universal`): `versionName` recebe `X.Y.Z` e `versionCode` é incrementado monotonicamente (`N -> N+1`) para permitir atualização por cima / OTA |
| [`.github/workflows/build.yml`](../.github/workflows/build.yml) | `workflow_dispatch.inputs.release_tag` | Default e exemplo de `release_tag` para disparos manuais no GitHub Actions |
| [`README.md`](../README.md) | Badge, título de recursos, crédito e nota OTA | Mantém documentação alinhada à release atual sem tocar em `Original_iOS-v1.0.1` |

---

## Como subir a versão e publicar uma nova release

```powershell
# 1. Conferir versão atual e sincronização em todos os alvos
python tools/bump_version.py --check

# 2. Simular o bump (ex.: patch 1.0.3 -> 1.0.4, Android versionCode + 1)
python tools/bump_version.py patch --dry-run

# 3. Aplicar o bump (aceita: patch | minor | major | X.Y.Z; opcional: --android-code N)
python tools/bump_version.py patch
python tools/bump_version.py --check
```

Depois de aplicar o bump:

1. Atualize [`docs/RELEASE_NOTES.md`](RELEASE_NOTES.md) descrevendo as correções/melhorias da nova versão (usado como corpo da release no GitHub Actions).
2. Recompile/valide localmente (`ctest` + `python -m unittest discover -s tests -p "test_*.py"` + `python tools/verify_workspace.py`).
3. Faça o commit na branch `main`, crie e envie a tag SemVer com prefixo `v` para acionar o workflow [`Build Multiplatform & Release`](../.github/workflows/build.yml):

```powershell
git add -A
git commit -m "chore(release): bump port version to vX.Y.Z"
git push origin main
git tag vX.Y.Z
git push origin vX.Y.Z
```

O push da tag `vX.Y.Z` compila automaticamente **Windows x64** (`HeroesLore3.exe` + `.zip`), **Linux x86_64** (`AppImage`), **Android** (`arm64-v8a` + `universal` assinados com `android/app/heroes_lore_3.keystore`) e **Nintendo Switch** (`.nro` direto), calcula `SHA256SUMS.txt` e publica a Release no GitHub para o atualizador OTA do app.


## Nintendo Switch 2

O workflow multiplataforma tambem compila `heroes_lore_3_switch2.nro` com ounce-devkit fixado e exige startup OUN0 de Switch 2 e mapa sem drivers/libnx de Switch 1. Usa PORT_VERSION existente para NACP e runtime. O job de release aguarda as cinco plataformas e publica NRO, SHA individual e checksum agregado. `main`/PR geram artefatos; tags `v*` ou dispatch com `create_release=true` publicam.
