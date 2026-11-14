# Etapa S10: Tradução PT-BR e Troca de Idioma em Tempo Real (`EN` / `PT-BR`)

## Objetivo

Entregar tradução integral para **Português do Brasil (PT-BR)** mantendo **Inglês (EN)** como idioma padrão no boot, estender os glifos `.rff`/`.pvr` originais para acentuação completa do português sem cortes e conectar a troca ao vivo no motor Mach-O (`TextUtils` + árvore `UI`) sem reiniciar o app nem alterar saves.

## Entregáveis Implementados

1. **Extensão Nativa de Fontes `.rff`/`.pvr` ([`tools/build_ptbr_fonts.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/build_ptbr_fonts.py))**:
   - Injetados `['Ã', 'Ô', 'Õ', 'ã', 'õ']` com avanço, linha de base e contorno alinhados pixel a pixel nos três atlas de UI/diálogo em `assets/fonts/`:
     - `font_mainmenu`: `137 -> 142` glifos (`used_y=289/512`, `missing PT-BR=[]`)
     - `font_menu_medium`: `137 -> 142` glifos (`used_y=210/512`, `missing PT-BR=[]`)
     - `font_menu_small`: `146 -> 151` glifos (`used_y=182/256`, `missing PT-BR=[]`)
   - Tabela de codepoints mantida estritamente ordenada em UTF-16BE (`c1 == c2`) para compatibilidade com a busca binária `0x0000f710`.

2. **Glossário, Chunks Validados e Compilador (`6.733/6.733` slots)**:
   - [`localization/glossary.json`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/localization/glossary.json): terminologia unificada (`Núcleo Sagrado`, `Núcleo Nix`, `Cara de Lata`, `Estigma de Gaia`, classes, cidades e status `NV`/`ATQ`/`DEF`/`FOR`/`DES`).
   - [`tools/prepare_translation_chunks.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/prepare_translation_chunks.py) + 12 arquivos em `localization/chunks/*.json` revisados com apoio das strings originais coreanas (`t_ko`) para corrigir inversões e artefatos da tradução inglesa da EA (ex.: slots `[6496]`, `[6607]`, `[915]`, `[1540]`, `[1542]`, `[5962]`).
   - [`tools/validate_ptbr_chunk.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/validate_ptbr_chunk.py): valida prefixos de cor líderes, tags `[Speaker]#`, contagem de `{`, `}`, `|`, `#`, `;`, `||`, especificadores `printf` e cobertura exata do charset `.rff`.
   - [`tools/build_ptbr_texts.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/build_ptbr_texts.py): compila `assets/texts/t_pt-br` (`696.466 B`, blob `669.530 B`), `assets/texts/hdr_pt-br` (`244 B`) e `assets/texts/InGame_txt_2_pt-br` (`27 B`, `["Mochila cheia.", "Sim"]`).

3. **Troca em Tempo Real sem Reinício nem Alteração de Save**:
   - [`tools/hl3rt/machine.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/machine.py) (`Bundle` + `Machine.wr8`): interceptação dinâmica de `newres/texts/t_en`, `newres/texts/hdr` e `dat/InGame_txt_2` conforme `get_active_language()`, e entrega contínua das fontes `assets/fonts/*.{rff,pvr}` em qualquer idioma.
   - [`tools/run_pc.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/run_pc.py) (`_apply_guest_language_live` chamado por `set_language`): atualiza `[sizes_tbl[0] + chunk_idx*4]` (chunk 1 = `t_en`) no singleton `TextUtils` (`0x000d8220`), força reload via `0x0001230c(0)` e dispara `vtable[0x3c]` em `ui_main+0x90` e `ui_hud+0x90`, atualizando imediatamente os textos na tela (ex.: `CONTINUE` -> `CONTINUAR`, `CONNECTION ERROR!` -> `ERRO DE CONEXÃO!`) mantendo `data/` intacto bit a bit.

4. **Testes Automatizados (`tests/test_s09_s10_localization.py`)**:
   - Cobertura completa para reprodutibilidade de `S09`, invariantes de `.rff`/`.pvr`, validação estrita dos 12 chunks, leitura de `t_pt-br`/`hdr_pt-br`/`InGame_txt_2_pt-br` e alternância ao vivo `en` <-> `pt-BR` com checagem SHA-256 de `data/`.

