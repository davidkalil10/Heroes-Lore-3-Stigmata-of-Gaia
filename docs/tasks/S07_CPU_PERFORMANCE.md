# Tarefa S07: Performance de CPU / Blocos Quentes AOT + Oráculo Unicorn

## Status e Metadados

- **ID da Etapa**: `S07`
- **Nome**: Performance de CPU / Blocos Quentes AOT + Oráculo Unicorn
- **Fase**: `pc_playable_complete`
- **Data de Início**: 2026-11-09
- **Dependências Concluídas**: `S00`, `S01`, `S02`, `S03`, `S04`, `S05`, `S06`
- **Objetivo**: Instrumentar a medição de instruções e blocos básicos executados por fase (`boot`, `title`, `menu`, `field`, `combat`), identificar os blocos e laços mais quentes do binário Mach-O ARMv6/Thumb do Heroes Lore 3, gerar implementações AOT compiladas com guarda explícita de opcodes (sem no-op silencioso) e validar a equivalência matemática completa de registradores (`R0..R15`), `CPSR` e memória contra o oráculo Unicorn.

---

## 1. Escopo Técnico e Regras Críticas (Lições de HL4 e HL5)

1. **Medição por Fase Antes de Otimizar**:
   - Rastrear instruções e blocos básicos por fase de execução (`boot`, `title`, `menu`).
   - Identificar os endereços PC que acumulam a maior porcentagem de ciclos.
2. **Guarda Explícita de Decodificação de Opcodes (Lição do HL4)**:
   - Todo bloco compilado AOT deve ter validação dos opcodes originais esperados na memória guest antes de executar (`guard(block_id)`).
   - O emissor AOT **nunca** deve ignorar instruções não suportadas como `no-op` silencioso. Qualquer instrução não modelada deve provocar falha explícita na compilação ou manter a execução interpretada no Unicorn.
   - Instruções de borda (`mvn`, `bic`, `tst`, `cmn`, `ldr` literal pool, `push`/`pop`) devem ser tratadas com semântica estrita de flags `N`, `Z`, `C`, `V`.
3. **Oráculo de Teste Rigoroso**:
   - Comparar o estado pós-execução do bloco AOT contra a execução de 1 bloco/instrução no emulador Unicorn, testando contra valores de borda (`0`, `1`, `31`, `32`, `0x7fffffff`, `0x80000000`, `0xffffffff`) e valores pseudo-aleatórios determinísticos.
4. **Sem `std::thread` por Chamada**:
   - Manter execução síncrona sem overhead de criação e destruição de threads de sistema por bloco ou chamada HLE.

---

## 2. Entregáveis da Etapa

1. `tools/profile_phases.py`: profiling de blocos quentes e distribuição de instruções por fase.
2. `reference/analysis/cpu_profile_phases.json`: relatório determinístico de hotspots do HL3.
3. `tools/generate_aot_blocks.py`: extrator e gerador de blocos AOT com guardas de opcodes.
4. `tools/verify_aot_oracle.py`: ferramenta de oráculo comparando blocos AOT com o emulador Unicorn.
5. `tests/test_aot_hot_blocks.py`: testes unitários automatizados cobrindo profiling, guardas de opcodes e oráculo.
6. Atualizações em `docs/STATUS.json`, `docs/STATUS.md`, `docs/HANDOFF.md`, `docs/RESEARCH_LOG.md` e `docs/DECISIONS.md` (ADR-008).

