# Handoff de Sessão - Heroes Lore 3 Recomp

## Estado Atual

- **Fase**: `pc_playable_complete`
- **Etapas concluídas**: `S00`, `S01`, `S02`, `S03`, `S04`, `S05`, `S06`, `S07`, `S08`
- **Etapa ativa (`current_step`)**: `S09` (`Extração de textos, fontes (.rff/.pvr) e base EN`)
- **Objetivo alcançado nesta sessão**: **Etapa `S08` concluída e entregue na build standalone ([`dist/HeroesLore3_PC/HeroesLore3.exe`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/dist/HeroesLore3_PC/HeroesLore3.exe)): proporção padrão `Auto (Full Wide)` expandindo automaticamente de borda a borda ao maximizar, redimensionar ou ativar tela cheia (alternável entre `Auto`, `16:9` e `Original 3:2` via `F9` ou `--aspect`), seleção de FPS (`30` padrão calibrado / `60` via `F8` ou `--fps`), aviso estilizado na tela (banner OSD no topo ao trocar FPS, proporção ou tela cheia), persistência em `config/display.json` e 38 testes unitários passando.**

## O que foi feito e validado nesta sessão

1. **Taxa de Quadros (`30` vs `60` FPS) e Otimizações Auxiliares (`fast_stubs`)**:
   - Medido o custo de emulação ARMv6 em campo livre (`28,33 ms/f` em `480x320` e `23,03–32,72 ms/f` em `16:9`); adotado **`30 FPS` como padrão estável** para evitar oscilação de ritmo, mantendo **`60 FPS` alternável ao vivo (`F8`)** ou via `--fps 60`, com persistência em [`config/display.json`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/config/display.json).
   - Adicionado [`tools/hl3rt/fast_stubs.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/fast_stubs.py) com atalhos nativos verificados contra oráculo (`tests/test_fast_stubs.py`) para `__divsi3`, `__modsi3`, `__udivsi3`, `__umodsi3`, `glGetError` e cadeia SJLJ.

2. **True Widescreen Sem Distorção (`Original 3:2` / `16:9` / `Auto`)**:
   - Criado [`tools/hl3rt/display.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/display.py) calculando `ViewportLayout` sem esticar pixels em qualquer tamanho de janela (`Original` centraliza `480x320`; `16:9` expande o viewport horizontal para ~`569x320`; `Auto` adapta o mundo à proporção da janela/monitor — ex.: `711x320` em `20:9` e `480x360` em `4:3`).
   - Descoberto que [`0x00039d28`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/widescreen.py) retorna diretamente o endereço estático `0x000d8270` (`GameCtx`); [`tools/hl3rt/widescreen.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/widescreen.py) sincroniza `+0x40/+0x44` e `+0x5e60..0x5e7c` além de aplicar imediatos ARM `MOV` múltiplos de 4 para expandir câmera e culling do mapa.
   - Mantidas telas pré-jogo (`app_state in (0, 1, 256)`) centralizadas em `480x320` em [`GLBridge`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/gl_bridge.py) enquanto Gameplay/Campo expande de borda a borda e a HUD permanece ancorada às margens.
   - Corrigidos os submenus (`LEVEL 1` em `0x6ee44`, barras HP/MP em `0x6f8a8`/`0x6f984`) e botões do Menu de Pausa centralizando `ui_main` via `GLBridge.set_ui_offset(ui_dx, ui_dy)` (`glOrtho`/`glScissor`) com drenagem prévia do batcher de sprites (`0x0000c688`) pelo trampolim `UI_FLUSH_RETURN_STUB` (`0x7F100020`).
   - Corrigido o crash de redimensionamento SDL2 no Windows (`F9`) em [`PCRunner._resize_window`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/run_pc.py) via Win32 `AdjustWindowRectEx` + `SetWindowPos`, preservando toques do mouse, teclado e gamepad em `_main_pt` e `_hud_pt`.

3. **Testes e Distribuição Standalone Validados**:
   - Capturas visuais em `16:9`, `4:3`, `20:9` e janela maximizada (`1536x793`) verificadas visualmente sem barras pretas em campo, com banner estilizado OSD (`_draw_osd` em [`tools/run_pc.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/run_pc.py)) exibido ao vivo nas trocas de `F8` (FPS), `F9` (Proporção) e `F11` (Tela Cheia).
   - `python -m unittest discover -s tests -v`: **38/38 testes `OK`** (incluindo [`tests/test_viewport_aspect.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/test_viewport_aspect.py) e [`tests/test_fast_stubs.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/test_fast_stubs.py)).
   - Recompilado [`dist/HeroesLore3_PC/HeroesLore3.exe`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/dist/HeroesLore3_PC/HeroesLore3.exe) com README atualizado (`F8`/`F9`/`--aspect`/`--fps`) e validado executando diretamente `--frames 30`, `--aspect 16:9 --fps 30 --frames 30` e `--aspect original --fps 60 --frames 30` (todos com saída `0`).

## O que ficou pendente

- **`S09`** (`Extração de textos, fontes (.rff/.pvr) e base EN`) e etapas seguintes (`S10`..`S14`).

## Próximo Passo Concreto (`S09`)

1. Ler `docs/tasks/S09_TEXT_EXTRACTION.md`.
2. Catalogar todas as strings de `dat/InGame_txt`, `en.lproj/Localizable.strings`, `help_text.txt`, `dmg_strings.txt` e `store_strings.txt` e mapear os glifos das fontes `.rff`/`.pvr` como base para a tradução PT-BR em tempo real (`S10`).

## Comandos de Retomada e Execução

Para rodar a versão standalone portátil no PC (sem precisar de Python):
```powershell
.\dist\HeroesLore3_PC\HeroesLore3.exe
# ou escolher proporcao / FPS inicial:
.\dist\HeroesLore3_PC\HeroesLore3.exe --aspect 16:9 --fps 30
.\dist\HeroesLore3_PC\HeroesLore3.exe --aspect auto --fps 60
```

Atalhos ao vivo novos de exibição:
- **`F8`**: alterna entre `30 FPS` (padrão) e `60 FPS`.
- **`F9`**: alterna entre `Original (3:2)`, `16:9` e `Auto` (preenche a janela/tela sem esticar; você também pode arrastar as bordas da janela livremente).

Para validar o workspace e rodar todos os testes:
```powershell
python tools/verify_workspace.py
python tools/project_status.py show
python -m unittest discover -s tests -v
```
