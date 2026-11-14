# Mapa de Reuso e Adaptação (HL4 / HL5 / J2ME -> HL3)

Este documento mapeia o que foi adaptado com sucesso dos snapshots de referência (`reference/hl4_snapshot/`, `reference/hl5_snapshot/`, `reference/jar_ports_docs/`) e o que nasceu exclusivo da arquitetura **Mach-O ARMv6 / iOS** do **Heroes Lore 3**.

| Área | Origem de Referência | Status no HL3 | Adaptação Realizada no HL3 |
| --- | --- | --- | --- |
| **Loader do Binário** | Exclusivo HL3 (Mach-O 32-bit ARMv6) | ✅ Implementado (`tools/hl3rt/machine.py`, `objc.py`) | Carregador Mach-O (`__TEXT`, `__DATA`, `__LINKEDIT`), `__nl_symbol_ptr`/`__la_symbol_ptr` e runtime Objective-C 2.0 (`30` classes, `679` seletores). |
| **CPU ARMv6 & Oráculo** | HL4 (`ARM/Thumb + Unicorn oracle + guardas`) | ✅ Implementado (`aot_blocks.py`, `verify_aot_oracle.py`) | `28` blocos quentes acelerados com guarda estrita de opcodes e `48/48` comparações contra oráculo Unicorn. |
| **Gráficos & True Widescreen** | HL4 + HL5 (`projeção ortográfica + UI centrada`) | ✅ Implementado (`gl_bridge.py`, `widescreen.py`, `display.py`) | Tradução GLES 1.1 → OpenGL Desktop, decodificador `.pvr`, `Auto` / `16:9` / `Original 3:2`, `30`/`60` FPS (`F8`/`F9`). |
| **Áudio Não-Bloqueante** | HL5 / HL4 (`mixer fora do callback SDL`) | ✅ Implementado (`hle_audio.py`) | HLE OpenAL (`.wav`) + `AVAudioPlayer` (`.mp3`) em `64` canais virtuais fora do callback SDL. |
| **Sistema de Opções & UX** | HL1 / HL2 / HL Zero / HL5 (`modal responsivo`) | ✅ Implementado (`options_menu.py`, `tools/run_pc.py`) | Entrada dupla (`Help & About` + `System`), modal OpenGL adaptável (desktop/mobile), OSD estilizado (`F7`/`F8`/`F9`/`F11`). |
| **Localização `EN` / `PT-BR` ao Vivo** | HL4 / HL5 + Exclusivo `.rff`/`.pvr` | ✅ Implementado (`i18n.py`, `build_ptbr_*.py`, `run_pc.py`) | Extensão de fontes `.rff`/`.pvr` (`Ã`, `Ô`, `Õ`, `ã`, `õ`), `6.733/6.733` slots traduzidos e reload live via `TextUtils` + `GameWorld` banner. |
| **Cloud Save (Google Drive)** | HL1 / HL2 / HL Zero / HL5 (`OAuth Device Flow`) | ✅ Implementado (`cloud_save.py`) | Pasta isolada `Heroes Lore III - Stigmata of Gaia` (`heroes_lore_3_save.json`), backups locais em `data/save-backups/` e volta limpa ao Título. |
| **OTA Updater (GitHub Releases)** | HL5 / HL1 / HL2 (`updater + release check`) | ✅ Implementado (`updater.py`, `version.py`, `bump_version.py`) | Consulta GitHub API (`davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia`), download com progresso + SHA-256, ZIP Slip guard e updater staging Windows. |
| **Empacotamento Multiplataforma** | HL5 / HL4 (`Android` + `Switch` + `CI`) | ⏳ Próximas etapas (`S12`–`S14`) | Windows x64 standalone concluído (`v1.0.3`, via `assets/game.ipa` sem APK); próximos alvos: C++ native core (`src/`), Linux, Android ARM64 (`com.davidbraga.heroeslore3`) e Switch (`0100F3A300030000`). |
