# Handoff de Sessão - Heroes Lore 3 Recomp

## Estado Atual

- **Fase**: `pc_playable_complete`
- **Etapas concluídas**: `S00`, `S01`, `S02`, `S03`, `S04`, `S05`, `S06`, `S07`, `S08`
- **Etapa em andamento (`S11`)**: Menu Personalizado de Opções & Cloud Save (Google Drive) concluído e entregue; OTA Updater (GitHub Releases) permanece pendente.
- **Etapa ativa (`current_step`)**: `S09` (`Extração de textos, fontes (.rff/.pvr) e base EN`)
- **Objetivo alcançado nesta sessão**: **Menu personalizado `PORT OPTIONS & CLOUD` integrado nativamente ao jogo em dois acessos (`Help & About` no título e `System` no menu de pausa), com ajuste ao vivo de `FPS` (`30`/`60`), `Proporção` (`Auto`/`16:9`/`Original 3:2`), `Idioma` (`English (US)` padrão / `Português (BR)`), modal adaptável a telas desktop/widescreen/mobile, Cloud Save Google Drive isolado para HL3 (`Heroes Lore III - Stigmata of Gaia` / `heroes_lore_3_save.json`) e build standalone recompilada em [`dist/HeroesLore3_PC/HeroesLore3.exe`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/dist/HeroesLore3_PC/HeroesLore3.exe).**

## O que foi feito e validado nesta sessão

1. **Acesso Duplo com Aparência Nativa (`Help & About` + `System`)**:
   - Em [`tools/run_pc.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/run_pc.py), implementado `_patch_help_about_layout()` compactando verticalmente os 6 botões originais de `Help & About` (`form_id == 113`, centros `cy=[33, 69, 105, 141, 177, 213]`) para acomodar o 7º botão (`cid=99113`, `[93..393, 234..264]`) acima de `VERSION 1.0.1`, além do 5º botão (`cid=99003`, `[90..390, 256..286]`) no submenu `System` (`form_id == 3`).
   - Produzida a textura [`assets/ui_button_ribbon.png`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/assets/ui_button_ribbon.png) (`608x66`) composta diretamente sobre os ornamentos azuis extraídos de `user_interface_with_alpha.pvr`, com gradiente prateado/azul e foco dourado alinhados aos botões originais do jogo.
   - Estendido `handle_cancel` para formulários pré-jogo `112..118` e garantido retorno/navegação 100% operáveis por teclado, gamepad e mouse.

2. **Modal Responsivo e Internacionalização Bilíngue (`EN` / `PT-BR`)**:
   - Criado [`tools/hl3rt/options_menu.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/options_menu.py) com [`compute_modal_layout()`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/options_menu.py) e [`PortOptionsModal`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/options_menu.py): adapta automaticamente cartões empilhados e grade `2x2` em modo retrato/mobile compacto ou painéis lado a lado e linha única em modo paisagem/widescreen, sem vazamento de toque ou teclado para o motor ARMv6 enquanto o modal estiver aberto.
   - Criado [`tools/hl3rt/i18n.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/i18n.py) e expandido [`tools/hl3rt/display.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/display.py) com `language` (`"en"` padrão no boot / `"pt-BR"` ao vivo), refletindo instantaneamente em botões injetados, modal e banners OSD.

3. **Cloud Save Google Drive Isolado para Heroes Lore 3**:
   - Criado [`tools/hl3rt/cloud_save.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/cloud_save.py) implementando [`CloudSaveManager`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/cloud_save.py): OAuth 2.0 Device Flow (RFC 8628), isolamento completo na pasta `"Heroes Lore III - Stigmata of Gaia"` e pacote único `"heroes_lore_3_save.json"` com hash `SHA-256`, sanitização de caminhos relativos, migração automática da raiz, staging atômico + backup local de rollback em `data/save-backups/<stamp>` e reinício limpo para a tela título via `_reload_runtime_to_title()`.
   - Atualizados [`.gitignore`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/.gitignore) e [`tools/build_standalone_pc.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/build_standalone_pc.py) para jamais versionar ou incluir `config/cloud_auth.json` e backups locais no pacote standalone.

4. **Validação Completa e Build Standalone**:
   - `python -m unittest discover -s tests -v`: **48/48 testes `OK`** (incluindo [`tests/test_cloud_save.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/test_cloud_save.py) e [`tests/test_options_menu.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/test_options_menu.py)).
   - Capturas visuais ao vivo verificadas no `Help & About` (`960x640`, 7 botões), `System` (`1138x640` widescreen `16:9`, 5 botões), Modal de Opções (`EN`), Modal Cloud Save (`PT-BR` PC e retrato mobile) e reinício limpo `_reload_runtime_to_title()` retornando a `app_state=1, form_id=100`.
   - Recompilado [`dist/HeroesLore3_PC/HeroesLore3.exe`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/dist/HeroesLore3_PC/HeroesLore3.exe) com configurações limpas (`Auto`, `30 FPS`, `en`, sem `cloud_auth.json`) e validado diretamente com `--frames 15` (saída `0`).

## O que ficou pendente

- **`S09`** (`Extração de textos, fontes (.rff/.pvr) e base EN`).
- **`S10`** (`Tradução PT-BR dos textos do jogo conectada ao seletor ao vivo EN / PT-BR já pronto`).
- **`S11` (parte restante)**: OTA Updater via GitHub Releases.
- **`S12` .. `S14`**: Empacotamento Android ARM64 (`com.davidbraga.heroeslore3`), Nintendo Switch (`0100F3A300030000`) e polimento final.

## Próximo Passo Concreto (`S09`)

1. Ler `docs/tasks/S09_TEXT_EXTRACTION.md`.
2. Extrair e auditar todas as strings de `dat/InGame_txt`, `en.lproj/Localizable.strings`, `help_text.txt`, `dmg_strings.txt` e `store_strings.txt`, bem como os glifos `.rff`/`.pvr` preparando o hook de tradução PT-BR em tempo real (`S10`).

## Comandos de Retomada e Execução

Para rodar a versão standalone portátil no PC (sem precisar de Python):
```powershell
.\dist\HeroesLore3_PC\HeroesLore3.exe
```

Para validar o workspace e rodar todos os testes:
```powershell
python tools/verify_workspace.py
python tools/project_status.py show
python -m unittest discover -s tests -v
```
