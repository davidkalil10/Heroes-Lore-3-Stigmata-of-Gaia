# Tarefa S08: True Widescreen e Expansão de Viewport

## Status e Metadados

- **ID da Etapa**: `S08`
- **Nome**: True Widescreen e Expansão de Viewport
- **Fase**: `pc_playable_complete`
- **Data de Início**: 2026-11-09
- **Dependências Concluídas**: `S00` a `S07`
- **Objetivo**: Implementar suporte a múltiplas proporções de tela (`3:2`, `16:9`, `16:10`, `20:9`, `4:3`) e janela livre com letterbox/pillarbox automático por aceleração de hardware e True Widescreen (expansão horizontal do viewport ortográfico com ancoragem de HUD e botões sem distorção).

---

## 1. Diretrizes Técnicas (HL4 e HL5)

1. **Sem Esticar Elementos Gráficos (Aspect Ratio Integrity)**:
   - O jogo original foi projetado em `480x320` (`3:2` = 1.50).
   - Em telas `16:9` (`1.777`), `16:10` (`1.60`), `20:9` (`2.222`) ou `4:3` (`1.333`), a renderização nunca deve distorcer proporções visuais dos sprites, HUD ou textos.
2. **Dois Modos de Exibição**:
   - **Modo Integer / Aspect Fit (Letterbox / Pillarbox Seguro)**: preserva estritamente os `480x320` com barras pretas perfeitas em qualquer tamanho de janela ou monitor.
   - **Modo True Widescreen**: expande a largura ortográfica `glOrthof` (para 569 px em 16:9, 512 px em 16:10, 711 px em 20:9) e ancora dinamicamente a HUD (controles direitos na margem direita, controles esquerdos na margem esquerda, centro centralizado).
3. **Mapeamento de Entrada Inverso Fiel**:
   - As coordenadas de clique do mouse e toques sintéticos (`window_to_game_coord`) devem mapear com precisão matemática em ambos os modos, considerando o viewport de destino (`vx`, `vy`, `vw`, `vh`) e eventuais barras de letterbox/pillarbox.
4. **Comutação em Tempo Real**:
   - Atalho `F9` para alternar entre proporções (`3:2` -> `16:9` -> `16:10` -> `20:9` -> `4:3`).
   - Atalho `F10` para alternar modo True Widescreen vs Pillarbox.
   - Atalho `F11` para tela cheia com ajuste automático de viewport.

---

## 2. Entregáveis e Critérios de Conclusão

- [x] `tools/hl3rt/gl_bridge.py`: suporte a viewport dinâmico, letterbox/pillarbox, expansão ortográfica e ancoragem.
- [x] `tools/run_pc.py`: argumentos de linha de comando (`--aspect`, `--widescreen`), atalhos `F9`/`F10` e conversão de toque calibrada.
- [x] Capturas comparativas em `3:2`, `16:9`, `16:10`, `20:9` e `4:3` em `reference/analysis/aspect_*.png`.
- [x] Script de teste e verificação automatizada em `tests/test_viewport_aspect.py`.
- [x] Atualizações em `docs/STATUS.json`, `docs/STATUS.md`, `docs/HANDOFF.md`, `docs/RESEARCH_LOG.md` e `docs/DECISIONS.md` (ADR-009).
