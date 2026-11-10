# Heroes Lore 3: Stigmata of Gaia - Recomp / Native Multiplatform Port

Repositório de engenharia reversa, recompilação e runtime nativo multiplataforma de **Heroes Lore 3: Stigmata of Gaia** (`heroes_lore_3_recomp`).

## Visão Geral e Descoberta Arquitetural (`S01`)

A inspeção estática determinística do pacote de entrada (`Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk`, `61.755.432` bytes) revelou uma arquitetura distinta dos capítulos anteriores:

| Jogo | Formato de Entrada | Binário / Código do Jogo | API Gráfica / Áudio / Runtime |
| --- | --- | --- | --- |
| **HL1 / HL2 / Zero** | `.jar` (J2ME MIDP 2.0) | Bytecode Java (`.class`) | `javax.microedition.lcdui` + `mmapi` |
| **HL5: Covenant of Darkness** | `.apk` Android nativo | ELF 32-bit ARM (`libHeroesLore5.so`) + VFS (`data.vfs.mp3`) | JNI (`CletEntry`) + GLES1 + SDL2 host |
| **HL4: Knight of Soltia** | `.apk` Android com wrapper WIPI | ELF 32-bit ARM (`libbinary.so` / `binary.mod`) + `jarfile.mp3` | Imports WIPI (`MC_*`, `wipic_*`) + framebuffer 16-bit |
| **HL3: Stigmata of Gaia** | `.apk` wrapper touchHLE com **`assets/game.ipa`** | **Mach-O 32-bit ARMv6 (`Payload/Heroes Lore.app/Heroes Lore`, `915.824` bytes)** | **iOS 3.0 Objective-C (`UIKit`/`EAGLContext`) + OpenGL ES 1.1 + OpenAL / `AVAudioPlayer` + SQLite3** |

Principais fatos extraídos do `assets/game.ipa` (`43.686.130` bytes):
- **Bundle ID original**: `com.ea.heroesloresg.bv` (`CFBundleVersion` `1.0.1`, `MinimumOSVersion` `3.0`, orientação nativa `UIInterfaceOrientationLandscapeRight`).
- **Executável Mach-O ARMv6**: `Payload/Heroes Lore.app/Heroes Lore` (`915.824` bytes, SHA-256 `39c6ed1db25cbf6020c9edd2cfa145410ff391f3361a9150d9646b9a4fada849`).
  - Segmentos: `__PAGEZERO` (`0x00000000+0x1000`), `__TEXT` (`0x00001000+0xcb000`, código `__text` em `0x00002b60..0x0bdC2c`), `__DATA` (`0x000cc000+0x16000`), `__LINKEDIT` (`0x000e2000+0x8000`).
  - 30 classes Objective-C (`AppDelegate`, `GLView`, `TouchResponder`, `SocketBtDelegate`, `EAMTX_*`, `DMG*`, `SBJson*`), 679 seletores e 389 símbolos importados (`glOrthof`, `glDrawArrays`, `glDrawElements`, `glCompressedTexImage2D`, `alSourcePlay`, `sqlite3_*`, `_objc_msgSend`, etc.).
- **Recursos e Localização**:
  - `1.819` entradas no `.ipa`, incluindo `dat/` (`InGame_txt`, `char_dat`, `enemy_dat`, `chapter_hop`, `i0_dat`..`i18_dat`, `pe000_pa`..`pe016_pa`), `boss/` (`boss0_cif`..`boss4_cif`, `boss9000_bm`..`boss9004_bm`), `comm/`, `newres/sprites/*.pvr` (texturas PowerVR), `newres/fonts/*.rff` + `*.pvr`, `snd/bgm0_mf.mp3`..`bgm16_mf.mp3` (20 MP3s) e 28 arquivos `.wav`.
  - Localizações oficiais já presentes no bundle: `en`, `de`, `es`, `fr`, `it`, `ko`, `zh` (`en.lproj/Localizable.strings`, `help_text.txt`, `dmg_strings.txt`, `store_strings.txt`).

## Plataformas-Alvo e Melhorias Planejadas

- **Plataformas**:
  - Windows x64 (`windows-x64`)
  - Linux x86_64 (`linux-x86_64`)
  - Android ARM64 (`android-arm64`)
  - Nintendo Switch (`nintendo-switch`)
- **Melhorias de Qualidade de Vida (paridade com HL4 e HL5)**:
  - Suporte nativo a **Teclado, Mouse/Toque e Gamepad** (SDL2 GameController / HidNpad)
  - **True Widescreen** (expansão real de viewport/câmera em 16:9, 16:10, 20:9, 4:3 sem esticar HUD)
  - **Troca de idioma em tempo real (EN / PT-BR)** (Inglês nativo revisado como padrão + tradução completa para Português do Brasil sem reiniciar o jogo)
  - **Menu Nativo de Opções** acessível por botão na tela, `F10` e `R3`
  - **Save local isolado + Cloud Save (Google Drive)** com backup/restauração atômica
  - **Atualizador OTA via GitHub Releases** para todas as plataformas

## Estrutura do Repositório

- `AGENTS.md`: Regras obrigatórias de operação, isolamento e continuidade para agentes de IA.
- `PROMPT_NOVA_SESSAO.md`: Prompt pronto para iniciar novas conversas sem perda de contexto.
- `project.json`: Metadados e hashes SHA-256 verificáveis do APK, IPA e executável Mach-O.
- `docs/`:
  - `STATUS.json` / `STATUS.md`: Estado atual das etapas (`S00`..`S14`).
  - `HANDOFF.md`: Resumo executivo e instruções exatas para a próxima sessão.
  - `ROADMAP.md`: Plano completo de desenvolvimento (`S00` a `S14`).
  - `HL4_AND_HL5_LESSONS.md`: Lições consolidadas dos ports de HL4 e HL5 (performance ARM/AOT, áudio, Switch, widescreen, localização e serviços).
  - `DECISIONS.md`: Registro de decisões arquiteturais (ADRs).
  - `ARCHITECTURE.md`: Detalhamento da arquitetura Mach-O ARMv6 / iOS HLE e módulos do host.
  - `PORTING_MAP.md`: Mapa do que reutilizar dos snapshots de HL4/HL5 e o que construir para o HL3.
  - `PREVIOUS_PORTS.md`: Comparativo técnico entre HL1/2/Zero, HL5, HL4 e HL3.
  - `FEATURE_PARITY.md`: Matriz de recursos por plataforma.
  - `VALIDATION.md`: Critérios e comandos de verificação por etapa.
  - `SESSION_PROTOCOL.md`: Protocolo de abertura e fechamento de sessão.
  - `RESEARCH_LOG.md`: Histórico cronológico de descobertas técnicas.
  - `tasks/`: Especificações detalhadas das etapas (`S02_RUNTIME_CONTRACT.md`, etc.).
- `reference/`:
  - `analysis/`: Inventários estáticos determinísticos gerados por `tools/inspect_apk.py`.
  - `hl4_snapshot/` e `HL4_SNAPSHOT.json`: Snapshot congelado do `heroes_lore_4_recomp`.
  - `hl5_snapshot/` e `HL5_SNAPSHOT.json`: Snapshot congelado do `heroes_lore_5_recomp`.
  - `jar_ports_docs/` e `JAR_PORTS_DOCS.json`: Documentação de referência dos ports J2ME.
  - `extracted_apk/` e `extracted_ipa/`: Extração local (ignorada pelo Git) do APK e do `assets/game.ipa`.
- `tools/`:
  - `verify_workspace.py`: Validador completo de integridade do workspace, snapshots, status e análise estática.
  - `inspect_apk.py`: Inspetor e extrator determinístico do APK, do `assets/game.ipa` e do binário Mach-O ARMv6.
  - `project_status.py`: Gerenciador de `docs/STATUS.json` e gerador de `docs/STATUS.md`.

## Comandos Rápidos

```powershell
# Verificar integridade completa do workspace, snapshots e inventário estático
python tools/verify_workspace.py

# Consultar o status atual do roadmap
python tools/project_status.py show

# Reexecutar a inspeção estática e extrair APK + IPA para reference/extracted_*
python tools/inspect_apk.py --extract
```
