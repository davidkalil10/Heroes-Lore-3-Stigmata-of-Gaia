# Handoff - Heroes Lore 3 Recomp

## Estado Atual

- **Última entrega concluída**: **Release `v1.0.5` (`Android versionCode 6`) — correção dos artefatos quadriculados (GLES2 `highp` UV precision) e do travamento visual na tela de Cloud Save (`PortOptionsModal::poll` + `CloudSaveManager::launch_worker`)**, validada localmente com `ctest` (`3/3`), `unittest`, `bump_version.py --check` e `verify_workspace.py`, com commit e tag `v1.0.5` enviados ao GitHub para disparar o workflow multiplataforma.
- **Próxima etapa recomendada**: Testar atualização OTA de `v1.0.4` para `v1.0.5` no Android (validando ausência de linhas quadriculadas e resposta imediata na tela de Cloud Save) e testar `.nro` no Nintendo Switch físico.
- **Versão atual no repositório**: `v1.0.5` (`versionCode 6` no Android)
- **Idioma padrão no boot**: Inglês (`en`), alternável em tempo real para Português (`pt-BR`) via `F7` ou `Port Options & Cloud` sem reiniciar nem alterar saves

## O que foi feito e validado na Sessão 27

1. **Artefatos quadriculados intermitentes em menus e no mapa (`src/hle/gl_bridge.cpp`)**:
   - **Causa raiz**: No backend OpenGL ES 2.0 (`is_gles`), o *fragment shader* declarava `"precision mediump float;\n"` enquanto as principais texturas de interface e HUD (`user_interface_with_alpha.pvr`, `hud.pvr`, `menu_bkg.pvr`) têm `1024x1024`. Em FP16 (`mediump`), coordenadas UV em `[0.5, 1.0)` têm passo de `1/2048` (`0.5` texel), causando sangramento de borda (linhas `+` nos slots 9-slice e junções de tiles) sob escala fracionária mobile. Além disso, `tex_parameter_f` não delegava filtros para `tex_parameter_i`.
   - **Correção**: Atualizados `vs_src` e `fs_src` GLES2 para `precision highp float;` (`#ifdef GL_FRAGMENT_PRECISION_HIGH` no FS com fallback compatível) e roteado `GLBridge::tex_parameter_f` para `tex_parameter_i` preservando `GL_NEAREST`.
   - **Validação**: Coberto em `tests/test_viewport_aspect.py` (`test_gles2_shaders_request_highp_for_atlas_uvs`) e `tests/cpp/test_platform_services.cpp`.
2. **Feedback congelado na tela de Cloud Save / Updater (`src/platform/options_modal.h`, `src/platform/options_modal.cpp`, `src/platform/cloud_save.h`, `src/platform/cloud_save.cpp`)**:
   - **Causa raiz**: `PortOptionsModal::poll()` só marcava `dirty_ = true` enquanto `live` era `true` (`busy || authorizing`). Quando o worker em segundo plano concluía o refresh ou upload (`busy: true -> false`), `live` virava `false` naquele mesmo frame, deixando a textura em cache congelada em `"Verificando save na nuvem..."` ou `"Fazendo backup..."` até o usuário sair e entrar novamente no menu.
   - **Correção**: Implementado `build_live_state_signature(...)` + `was_live_` em `PortOptionsModal` para disparar `dirty_ = true` imediatamente em qualquer mudança de estado ou transição `live -> idle`. Em `CloudSaveManager::launch_worker`, a mensagem inicial (`cloud.msg.checking`, `cloud.msg.uploading`, `cloud.msg.downloading`) passou a ser definida atomicamente sob mutex antes do retorno, com `BusyResetGuard` RAII.
   - **Validação**: Novo teste `test_modal_live_poll_redraws` em `tests/cpp/test_platform_services.cpp` cobrindo refresh e upload assíncronos após limpeza de `dirty_`.

## O que ficou pendente

- Acompanhar o workflow `Build Multiplatform & Release` da tag `v1.0.5` no GitHub Actions, validar a atualização OTA de `v1.0.4` -> `v1.0.5` no Android (ausência de linhas quadriculadas + atualização imediata da tela de Cloud Save) e testar `.nro` em Nintendo Switch físico.

## Próximo passo concreto ao retomar

Verificar a execução do workflow da tag `v1.0.5` e validar os pacotes/testes locais caso novas alterações sejam feitas:

```powershell
python tools/bump_version.py --check
python tools/verify_workspace.py
```
