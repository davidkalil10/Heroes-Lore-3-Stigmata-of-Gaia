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
