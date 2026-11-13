# Tarefa S08: FPS Configurável (30/60), True Widescreen e Expansão de Viewport

## Status e Metadados

- **ID da Etapa**: `S08`
- **Nome**: True Widescreen, Expansão de Viewport e FPS (`30`/`60`)
- **Fase**: `pc_playable_complete`
- **Data de Conclusão**: `2026-11-13`
- **Dependências Concluídas**: `S00` a `S07`
- **Objetivo**: Implementar alternância de FPS (`30` padrão calibrado vs `60`) e proporções (`Original 3:2`, `16:9`, `Auto`) com expansão real de cenário (`glOrthof`, câmera e culling em `GameCtx`), ancoragem de HUD e centralização intacta de menus/popups sem esticar sprites.

---

## 1. Descobertas de Engenharia Reversa no Mach-O (`Heroes Lore`)

1. **Câmera e Limites de Viewport (`GameCtx` em `0x000d8270`)**:
   - O getter `0x00039d28` retorna diretamente o endereço estático `0x000d8270` (`ldr r0, [pc]; bx lr`).
   - `ctx + 0x40 / 0x44` guardam largura/altura lógicas usadas no setup e `ctx + 0x5e60..0x5e7c` guardam janela de câmera e posição central, atualizados dinamicamente em `tools/hl3rt/widescreen.py`.
   - Imediatos `MOV rN, #480` / `#320` em rotinas de setup/rendering são ajustados deterministicamente com `encode_arm_rot_imm` para múltiplos de 4 alinhados ao layout ativo (`patch_w x patch_h`).
2. **Desalinhamento em Submenus e Batcher de Sprites (`0x0000c688`)**:
   - Controles como `LEVEL 1` (`0x6ee44`) e barras de HP/MP (`0x6f8a8`, `0x6f984`) leem coordenadas cruas dos nós ignorando deslocamentos em registradores.
   - Solução: centralizar `ui_main` diretamente na projeção/scissor OpenGL via `GLBridge.set_ui_offset(ui_dx, ui_dy)` mantendo dimmer/fundo expandidos e subtraindo `(ui_dx, ui_dy)` em `_on_ui_root_touch`.
   - Ao cruzar transições entre `ui_main`, dimmer e `main_popup`, um trampolim leve em `UI_FLUSH_RETURN_STUB` (`0x7F100020`) força o flush do batcher `0x0000c688(gfx)` antes de alterar `glOrtho`, eliminando deslocamento parcial de botões no pause e popups.
3. **Benchmark de FPS (`30` vs `60`) e Resize SDL/Win32**:
   - Em jogabilidade de campo, a emulação Unicorn/ARMv6 consome ~28,3–32,7 ms/frame (`30 FPS` como padrão estável sem oscilações, com `60 FPS` opcional via `F8` / `--fps 60`).
   - Redimensionamento ao vivo (`F9`) usa Win32 `AdjustWindowRectEx` + `SetWindowPos` no HWND SDL, evitando crash do `Window.from_display_module().size` no Windows e preservando o contexto OpenGL.

---

## 2. Entregáveis e Critérios de Conclusão

- [x] [`tools/hl3rt/display.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/display.py): normalização de `FPS` (`30`/`60`), modos `original`/`16:9`/`auto`, cálculo `ViewportLayout` sem distorção e persistência em `config/display.json`.
- [x] [`tools/hl3rt/widescreen.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/widescreen.py): patch ARM dinâmico de limites de câmera/culling em `0x000d8270`, suporte `UI_FLUSH_RETURN_STUB` (`0x7F100020`) para flush seguro e tradução de toque em `ui_main`.
- [x] [`tools/hl3rt/gl_bridge.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/gl_bridge.py) & [`tools/hl3rt/hle_gl.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/hle_gl.py): projeção `ortho_f` ciente de `ui_offset`, `glScissor` adaptado e preservação 3:2 centrada em telas pré-jogo.
- [x] [`tools/run_pc.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/run_pc.py) & [`tools/build_standalone_pc.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/build_standalone_pc.py): CLI `--aspect` / `--fps`, atalhos `F8` (FPS) / `F9` (Aspect) / `F11` (Fullscreen), janela redimensionável (`RESIZABLE`), foco/HUD ajustados e distribuição standalone atualizada.
- [x] [`tests/test_viewport_aspect.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/test_viewport_aspect.py): testes unitários de layout, encoding ARM, `GLBridge` e coordenadas do `PCRunner`.
- [x] Registro documental completo em `docs/STATUS.json`, `docs/STATUS.md`, `docs/HANDOFF.md`, `docs/RESEARCH_LOG.md`, `docs/DECISIONS.md` (`ADR-019`) e `research/sessions/2026-11-13-12.md`.
