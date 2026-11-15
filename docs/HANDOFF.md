# HANDOFF - Heroes Lore 3 Recomp

## Estado atual
- **Etapa ativa**: `S13` (Port Nintendo Switch `.nro` e validação de hardware/memória) — próxima recomendada `S14`.
- **Versão atual**: `v1.0.6` (`Android versionCode 7`), publicada via tag `v1.0.6`.
- **O que foi feito e validado nesta sessão (`v1.0.6`)**:
  1. **Nintendo Switch Button Swap (`A <-> B`, `X <-> Y`)**: implementado em [src/platform/input_controller.h](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/src/platform/input_controller.h) e [src/platform/input_controller.cpp](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/src/platform/input_controller.cpp) para Switch nativo (`#ifdef __SWITCH__`) e controles Switch em PC/Android.
  2. **Mapeamento completo de slots HUD + sincronia no `README.md`**: `LT`/`ZL` (Skill 1), `RT`/`ZR` (Skill 2), `R3` (Right Stick Click — Skill 3), `LB`/`RB` (Poções 1/2 e Abas), `L3` (Left Stick Click — Port Options & Cloud), `X` (Ataque Primário) e `Y` (Ataque Secundário / Skip), validado por `test_gamepad_switch_and_slots` em [tests/cpp/test_platform_services.cpp](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/cpp/test_platform_services.cpp).
  3. **Otimizações Pack A + Pack B (`ArmFastCpu`) para Nintendo Switch / Unicorn-TCI**:
     - Pack A: acesso direto à RAM host em `Machine`, caminhos zero-copy em `hle_libc.cpp`, cache ObjC em `src/core/objc.{h,cpp}`, batching de quads via VBO em `gl_bridge.{h,cpp}` e ajuste de VSync/Sleep em `src/main.cpp`.
     - Pack B: [src/core/arm_fast_cpu.h](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/src/core/arm_fast_cpu.h) e [src/core/arm_fast_cpu.cpp](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/src/core/arm_fast_cpu.cpp) (`HL3_USE_FAST_CPU=1` em `HL3_TCI` / `HL3_SWITCH`), validado lockstep contra Unicorn (`fast=0` vs `fast=1`) em `macho_boot` e bit/pixel-a-pixel em `headless_runtime`.
     - Benchmark Unicorn-TCI (`--headless`): `120` frames `4.969s` -> `3.7743s`; `360` frames `7.293s` -> `4.7552s`; delta steady-state (`360f - 120f`) acelerado **2.37x** (`2.324s` -> `0.981s`, `9.68 -> 4.09 ms/frame`).
  4. **Versionamento e documentação**: `python tools/bump_version.py patch` (`1.0.5` -> `1.0.6`, `versionCode 6 -> 7`), `docs/DECISIONS.md` (`ADR-029`), `docs/RESEARCH_LOG.md` (Sessão 28), [research/sessions/2026-11-15-01.md](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/research/sessions/2026-11-15-01.md) e `docs/RELEASE_NOTES.md`.
  5. **Correções de CI para `v1.0.6`**:
     - `SDL_VERSION_ATLEAST(2, 24, 0)` nos tipos `SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_*` em [src/platform/input_controller.cpp](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/src/platform/input_controller.cpp) para compilar no SDL `2.0.20` do Ubuntu 22.04.
     - Diretórios temporários isolados (`host_docs_override`) por pass `fast=0` / `fast=1` em [tests/cpp/test_macho_boot.cpp](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/cpp/test_macho_boot.cpp) e [tests/cpp/test_headless_runtime.cpp](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/cpp/test_headless_runtime.cpp), garantindo paridade exata de heap (`5676880` vs `5676880`) e framebuffer mesmo em checkouts limpos.

## O que ficou pendente
- Testar o novo `heroes_lore_3.nro` (`v1.0.6`) no Nintendo Switch físico (botões `A/B`/`X/Y`, slots `LT/RT/R3/LB/RB/L3` e ganho de fluidez em jogo).
- Concluir validação pós-release da `v1.0.6` e avançar para `S14` (auditoria final de paridade).

## Próximo passo concreto e comando de retomada
Acompanhar o workflow `Build & Release Multi-Platform` da tag `v1.0.6` no GitHub Actions e, após download/atualização no Switch, executar em sessão futura:
```powershell
python tools/project_status.py show
python tools/verify_workspace.py
```
