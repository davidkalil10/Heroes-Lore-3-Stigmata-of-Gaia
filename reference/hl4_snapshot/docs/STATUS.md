# Status do projeto HL4

Atualizado: 2026-10-22. Próxima etapa: **S05**.

Consulte as evidências e a matriz de validação para saber o que foi realmente executado.

| Etapa | Entrega | Estado | Resumo | Evidência |
| --- | --- | --- | --- | --- |
| S00 | Preparação e continuidade | done | Workspace preparado e verificado | [AGENTS.md](../AGENTS.md), [HL5_SNAPSHOT.json](../reference/HL5_SNAPSHOT.json) |
| S01 | Inventário estático do APK | done | Inventário estático e extração verificados | [INITIAL_FINDINGS.md](../reference/analysis/INITIAL_FINDINGS.md) |
| S02 | Contrato WIPI/ARM | done | Contrato mínimo loader/ABI/eventos/recursos comprovado no runtime integrado | [RUNTIME_CONTRACT.md](../docs/RUNTIME_CONTRACT.md), [WINDOWS_RUNTIME.md](../docs/WINDOWS_RUNTIME.md) |
| S03 | CPU e loader PC | done | Loader ELF/REL/BSS e ARM/Thumb/ABI/contexto executados no PC | [WINDOWS_RUNTIME_TESTS.json](../reference/analysis/WINDOWS_RUNTIME_TESTS.json), [main.cpp](../src/main.cpp) |
| S04 | Boot e primeiro quadro | done | Boot original até título, novo jogo e cenário com loop responsivo | [WINDOWS_RUNTIME_TESTS.json](../reference/analysis/WINDOWS_RUNTIME_TESTS.json), [WINDOWS_RUNTIME.md](../docs/WINDOWS_RUNTIME.md) |
| S05 | Entrada e gameplay | in_progress | Usuario confirmou movimento/cliques e exploracao/combate; A/B,X/Y trocados no Switch aguardam reteste fisico; validacao formal segue parcial | [POINTER_TESTS.json](../reference/analysis/POINTER_TESTS.json), [POINTER_INPUT.md](../docs/POINTER_INPUT.md), [S05_GAMEPLAY_VALIDATION.md](../docs/tasks/S05_GAMEPLAY_VALIDATION.md) |
| S06 | Áudio e persistência | in_progress | Ogg real não nulo e arquivos temporários; save/load de partida e audição pendentes | [WINDOWS_RUNTIME_TESTS.json](../reference/analysis/WINDOWS_RUNTIME_TESTS.json) |
| S07 | Desempenho | in_progress | PC fluido confirmado; Android/Switch lentos em batalha. 10 regioes + watchdog persistente passaram combate57BMPs/55estados/20oracles no host; nova validacao fisica pendente | [PERFORMANCE.md](../docs/PERFORMANCE.md), [ARM_PROFILE.json](../reference/analysis/ARM_PROFILE.json), [ARM_FAST_TESTS.json](../reference/analysis/ARM_FAST_TESTS.json), [S07_SWITCH_PERFORMANCE.md](../docs/tasks/S07_SWITCH_PERFORMANCE.md), [ARM_FULL_FUNCTION_TESTS.json](../reference/analysis/ARM_FULL_FUNCTION_TESTS.json), [ARM_REMAINING_PROFILE.json](../reference/analysis/ARM_REMAINING_PROFILE.json), [ICON_FULL_AOT_PACKAGES.json](../reference/analysis/ICON_FULL_AOT_PACKAGES.json), [COMBAT_MOBILE_TESTS.json](../reference/analysis/COMBAT_MOBILE_TESTS.json), [ARM_COMBAT_PROFILE.json](../reference/analysis/ARM_COMBAT_PROFILE.json) |
| S08 | Viewport expandido | in_progress | Auto na abertura usa superficie real e expande cenario; cinco proporcoes com estados/pixels/audio/toque verificados no host. Android imersivo; resize vivo e hardware pendentes | [S08_VIEWPORT_EXPANSION.md](../docs/tasks/S08_VIEWPORT_EXPANSION.md), [ANDROID_LAYOUT_QUEST_TESTS.json](../reference/analysis/ANDROID_LAYOUT_QUEST_TESTS.json), [VIEWPORT_EXPANSION_TESTS.json](../reference/analysis/VIEWPORT_EXPANSION_TESTS.json), [DISPLAY.md](../docs/DISPLAY.md) |
| S09 | Texto e extração | in_progress | 8232registros, incluindo1297nomes de itens/inimigos/NPCs com DES/container/CP949 roundtrip; glossario576nomes, restante da campanha ainda em revisao | [TEXT_INVENTORY.json](../reference/analysis/TEXT_INVENTORY.json), [TEXT_LOCALIZATION.md](../docs/TEXT_LOCALIZATION.md), [DYNAMIC_TEXT_INVENTORY.json](../reference/analysis/DYNAMIC_TEXT_INVENTORY.json) |
| S10 | EN/PT-BR ao vivo | in_progress | Ingles padrao com nomes e composicao de equipamentos/contadores/coleta; zero misses no combate reproduzido. Campanha/editorial e fonte PTBR ainda parciais | [LOCALIZATION_RUNTIME.md](../docs/LOCALIZATION_RUNTIME.md), [LOCALIZATION_TESTS.json](../reference/analysis/LOCALIZATION_TESTS.json), [COMBAT_MOBILE_TESTS.json](../reference/analysis/COMBAT_MOBILE_TESTS.json) |
| S11 | Opções do port | pending | Ainda não iniciado | — |
| S12 | Builds multiplataforma | in_progress | f4e3ee0 / CI39807955246 concluida success: quatro plataformas; APKs ARM64/global e NRO verificados nos caminhos fixos. Novo IDNACP/logFS; hardware e atalho aguardam reteste | [WINDOWS_RUNTIME.md](../docs/WINDOWS_RUNTIME.md), [WINDOWS_RUNTIME_TESTS.json](../reference/analysis/WINDOWS_RUNTIME_TESTS.json), [CI_BUILDS.json](../reference/analysis/CI_BUILDS.json), [SWITCH_BOOT_WATCHDOG.json](../reference/analysis/SWITCH_BOOT_WATCHDOG.json), [ANDROID_LAYOUT_PACKAGES.json](../reference/analysis/ANDROID_LAYOUT_PACKAGES.json), [BRANDING.md](../docs/BRANDING.md), [ICON_FULL_AOT_PACKAGES.json](../reference/analysis/ICON_FULL_AOT_PACKAGES.json), [SWITCH_IDENTITY.md](../docs/SWITCH_IDENTITY.md), [COMBAT_MOBILE_PACKAGES.json](../reference/analysis/COMBAT_MOBILE_PACKAGES.json) |
| S13 | Atualizador e cloud save | pending | Ainda não iniciado | — |
| S14 | Validação e release | pending | Ainda não iniciado | — |

Fonte única: `docs/STATUS.json`. Editar com `python tools/project_status.py set ...`;
critérios em ROADMAP.md. DONE de pesquisa não significa que o port esteja pronto.
