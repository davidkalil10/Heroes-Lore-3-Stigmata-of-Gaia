# Handoff de Sessão - Heroes Lore 3 Recomp

## Estado Atual

- **Fase**: `pc_playable_complete`
- **Etapas concluídas**: `S00`, `S01`, `S02`, `S03`, `S04`, `S05`, `S06`, `S07`
- **Etapa ativa (`current_step`)**: `S08` (`True Widescreen e expansão de viewport`)
- **Objetivo alcançado**: **Build PC Standalone 100% autônoma (`dist/HeroesLore3_PC/HeroesLore3.exe`) com ícone oficial embutido (`assets/icon.ico` e `assets/icon.png`), sem requerer Python ou dependências externas. Controlador unificado de entrada orientado diretamente à leitura da memória do motor guest (`AppManager`, `UI_Mgr`, `GameCtx`). Todas as telas possuem navegação, confirmação e cancelamento completos via teclado e gamepad: seleção de herói e classe, tela de resumo sem moldura amarela espúria, pulo instantâneo de cutscenes e fechamento de popups de tutoriais, diálogo com NPCs no campo com 100% de confiabilidade (botão de fala a 450,258 em paz e 397,239 em combate via E/Enter/Space/Gamepad A), e menu de pausa com navegação limpa sem auto-clique.**

## O que foi feito e validado nesta sessão

1. **Controlador de Entrada Orientado ao Estado Real de Memória do Motor Guest**:
   - **Diagnóstico**: Heurísticas baseadas em passos desacoplados (`self.step 0..4`) perdiam a sincronia quando o usuário usava o mouse, fechava popups ou passava por tutoriais e cutscenes com múltiplos `sub_states` (`4` ou `22`).
   - **Solução**: Implementado `get_engine_state()` em `tools/run_pc.py` lendo diretamente a memória guest:
     - `AppManager*` obtido em `*(uint32_t*)0x000d7dd4`.
     - `app_state` (`+0x78`): `1` = Menus de Título e Seleção, `2` = In-Game/Cutscene, `8` = Menu de Pausa.
     - `ui_main` (`+0x40`) -> `main_form` (`+0x98`, `form_id` em `+0x14`), `main_popup` (`+0xa0`).
     - `ui_hud` (`+0x44`) -> `hud_form` (`+0x98`, `form_id = 70`), `hud_popup` (`+0xa0`, `form_id = 130` Tutorial).
     - `GameCtx` em `0x000d8270`: `sub_state` em `*(int8_t*)0x000de30c`, `can_talk` em `*(uint8_t*)0x000de0ad`, `combat_mode` em `*(uint8_t*)0x000de0ac`.
   - Adicionados métodos de conveniência em `tools/hl3rt/machine.py`: `rd16`, `rds16`, `rd8`, `rds8`, `rdf32`.

2. **Engenharia Reversa e Correção das Coordenadas da Árvore de Controles**:
   - **Fala com NPCs no Campo (`form_id = 70`)**:
     - No modo pacífico (`combat_mode == 0`), a HUD posiciona o botão de fala (id `7025`) exatamente em `(450.0, 258.0)` com bounding box `(421..479, 237..279)`. O código anterior clicava em `(445.0, 286.0)` (`y=286` é 7px fora da borda do botão).
     - No modo de combate (`combat_mode == 1`), o botão de ataque (id `7007`) assume `(450.0, 258.0)` e o botão contextual de fala (id `7026`) desloca-se para `(397.0, 239.0)`.
     - O motor avalia colisão com NPCs em `0x41550` (caixa 72x72) e aciona `0x90894` que escreve `1` na flag `can_talk` em `*(uint8_t*)0x000de0ad`. As teclas `E`, `Enter`, `Espaço` e Gamepad `A` acionam o alvo correto instantaneamente.
   - **Seleção de Classe (`form_id = 42`)**:
     - Botão `<` (id `4207`) localiza-se em `(73.0, 71.0)` e `>` (id `4206`) em `(400.0, 71.0)`. Setas do teclado `Esquerda`/`Direita`, `A`/`D` e números `1`, `2`, `3` agora alternam classes perfeitamente.
   - **Resumo de Personagem (`form_id = 43`) e Eliminação da Moldura Amarela**:
     - Confirmação em `(356.0, 295.0)` e voltar em `(28.0, 28.0)`.
     - O cursor dourado de menu de pausa (`_draw_menu_cursor`) foi restrito rigorosamente a `app_state == 8 and form_id == 2`, impedindo qualquer aparição indevida fora do pause.
   - **Pulo de Cutscenes (`form_id = 44`) e Popups**:
     - Botão SKIP em `(408.0, 28.0)` disparado via `S`, `Tab`, Gamepad `Y`/`Start`.
     - Popup de confirmação do Skip: Yes em `(395.0, 220.0)` disparado via `Enter`/`Espaço`.
     - Popups de tutoriais (`form_id = 130`): Fechar (botão X) em `(385.0, 74.0)` via `Esc`/`Backspace`, Avançar em `(385.0, 207.0)` via `Enter`/`Espaço`.
   - **Menu de Pausa (`app_state = 8`, `form_id = 2`)**:
     - Migração de ações para eventos discretos `KEYDOWN` do Pygame, eliminando cliques duplos ao teclar `Enter`. Navegação suave com setas `Cima`/`Baixo` entre os 6 itens.

3. **Substituição do Ícone pelo Asset Oficial**:
   - Extraído `Payload/Heroes Lore.app/Icon.png` do IPA original.
   - Gerados `assets/icon.png` e `assets/icon.ico` (multi-tamanho: 16x16, 32x32, 48x48, 64x64, 128x128, 256x256).
   - Aplicado no Pygame via `pygame.display.set_icon()` e embutido no executável standalone via PyInstaller (`HeroesLore3.spec` e `tools/build_standalone_pc.py`).

4. **Reempacotamento e Validação da Build Standalone (`dist/HeroesLore3_PC/`)**:
   - Compilado `dist/HeroesLore3_PC/HeroesLore3.exe` com PyInstaller e pós-processado com `strip_cfg` (limpeza da flag PE `0x4000`).
   - Executável testado com `--frames 30` (código de saída 0).
   - 24 testes unitários automatizados passando com 100% de sucesso (`python -m unittest discover -s tests -v`).
   - Registrados `ADR-016` em `docs/DECISIONS.md` e Sessão 09 em `docs/RESEARCH_LOG.md`.

## Próximo Passo (`S08`: True Widescreen e Expansão de Viewport)

1. Ler a especificação em `docs/tasks/S08_VIEWPORT_EXPANSION.md`.
2. Expandir a projeção ortográfica `glOrthof` (original 480x320) para proporções `16:9` (854x480 / 1280x720), `16:10` e `20:9`.
3. Ajustar matrizes e limites de culling mantendo os elementos de HUD ancorados nas bordas sem distorção.

## Comandos de Retomada e Execução

Para rodar a versão standalone portátil no PC (sem precisar de Python):
```powershell
.\dist\HeroesLore3_PC\HeroesLore3.exe
```

Para rodar diretamente via script Python:
```powershell
python tools/run_pc.py
```

Para validar a integridade do workspace e rodar testes:
```powershell
python tools/verify_workspace.py
python tools/project_status.py show
python -m unittest discover -s tests -v
```
