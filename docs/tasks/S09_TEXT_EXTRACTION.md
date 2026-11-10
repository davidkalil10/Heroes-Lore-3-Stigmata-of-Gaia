# Etapa S09: Extração de Textos, Fontes (`.rff`/`.pvr`) e Base EN

## Objetivo

Catalogar 100% dos textos e métricas de fonte de **Heroes Lore 3: Stigmata of Gaia**, validando a base oficial em Inglês (EN) e preparando o pipeline de compilação de catálogo para a tradução PT-BR (`S10`).

## Fontes de Texto Identificadas no `.ipa` (`S01`)

1. **`Payload/Heroes Lore.app/en.lproj/Localizable.strings`** (e variantes `de`, `es`, `fr`, `it`, `ko`, `zh`).
2. **Arquivos de texto na raiz do `.app`**:
   - `help_text.txt` (`3.524` bytes)
   - `dmg_strings.txt` (`3.268` bytes)
   - `store_strings.txt` (`3.596` bytes)
   - `image_names.txt` (`184` bytes)
3. **Tabelas binárias em `Payload/Heroes Lore.app/dat/`**:
   - `dat/InGame_txt` (`9.409` bytes) e textos embutidos em scripts de capítulo/eventos (`chapter_hop`, `des_dat`, `char_dat`, `enemy_dat`, `getitem_dat`, `i0_dat`..`i18_dat`, `pe000_pa`..`pe016_pa`).
4. **Fontes em `Payload/Heroes Lore.app/newres/fonts/`**:
   - `font_hud.rff` + `font_hud.pvr`
   - `font_mainmenu.rff` + `font_mainmenu.pvr`
   - `font_menu_medium.rff` + `font_menu_medium.pvr`
   - `font_menu_small.rff` + `font_menu_small.pvr`
5. **Strings em `__cstring` e `__ustring` do binário Mach-O**.
