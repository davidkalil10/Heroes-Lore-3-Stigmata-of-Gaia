# HANDOFF - Heroes Lore 3 Recomp

## Estado atual
- **Etapa ativa**: `S13` (Port Nintendo Switch `.nro` e validação de hardware/memória) — próxima recomendada `S14`.
- **Versão atual**: `v1.0.8` (`Android versionCode 9`), publicada via tag `v1.0.8`.
- **Objetivo desta release (`v1.0.8`)**:
  - Bump sincronizado multiplataforma (`v1.0.7` -> `v1.0.8`, `Android versionCode 8 -> 9`) para validar ao vivo o fluxo completo do auto-updater `.nro` (`install_portable_nro_update` + `romfsExit()` + `envSetNextLoad` + relançamento automático) diretamente da `v1.0.7` instalada no Nintendo Switch, bem como builds Windows, Linux e Android.

## O que ficou pendente
- Confirmar no Switch físico a atualização automática `v1.0.7 -> v1.0.8` pelo menu in-app e, estando validado, avançar para `S14` (auditoria final de paridade).

## Próximo passo concreto e comando de retomada
```powershell
python tools/project_status.py show
python tools/verify_workspace.py
```

## Experimento Switch 2 (2026-11-15)

- Branch `codex/switch2-action`, isolada da main e da pasta de desenvolvimento original.
- Novo workflow de NRO com SDK ounce-devkit 20261115 fixado; toolchain oficial CMake, OunceVK OpenGL ES, Unicorn TCI com limite de 32 MiB.
- Atualizador seleciona somente `heroes_lore_3_switch2.nro`; caminhos/formato de saves e Google Drive mantidos.
- Verificacao inicial encontrou header ignorado do snapshot HL5 ausente no checkout. Hidratado apenas na copia local com o arquivo identico de `src/vendor` (hash do manifesto conferido); sem alterar snapshots versionados.
- Testes locais: `verify_workspace.py`, `bump_version.py --check`, 6 testes `test_multiplatform_ci.py`. Build remota e teste em hardware pendentes.
- Retomada: abrir workflow Nintendo Switch 2 NRO, baixar o artefato e enviar ao Netloader. Depois de aprovacao em console, integrar ao workflow multiplataforma e publicar. Ver `docs/SWITCH2.md`.

- Primeira build `43692335843`: codigo do jogo compilado, Unicorn interrompida por `sys/mman.h` ausente. Corrigido include herdado em CMake antes de adicionar Unicorn; a toolchain oficial sobrescrevia as flags INIT locais. Nova compilacao pendente.

## Resultado da build Switch 2

- CI aprovado: https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/actions/runs/43692514440
- Fonte da build: `043b047f400fcfd69d649f2674e34686d8c9a503`. NRO: 55.572.693 bytes; SHA-256 `b4eeed7d1ac06941c4047275d5535accebfaec730b8241f083f776943756b93a`.
- Copia de teste: `D:/Games/NS2ToolKit-homebrew/Heroes-Lore-3-Switch2-043b047/heroes_lore_3_switch2.nro`.
- Transferencia ao Netloader 192.168.18.139:28280 aceita, nome remoto `heroes_lore_3_switch2_043b047.nro`. Confirmacao de recebimento nao comprova boot ou gameplay.
- Preservados codigo de saves, Google Drive, storage, runtime, toolchain Switch 1 e workflow multiplataforma (comparacao com main).
- Verificacoes locais: 6 testes multiplataforma, contrato de patch idempotente e guardado, YAML/bash, versao sincronizada e verify_workspace aprovados. CI passou contrato de memoria e verificacao NRO/RomFS.
- Proximo passo: aguardar teste no Switch 2 (incluindo save/load e restauracao) antes de integrar main/release.
