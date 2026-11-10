# Protocolo de Sessão - Heroes Lore 3 Recomp

Para manter continuidade perfeita entre diferentes conversas e agentes, toda sessão deve seguir este protocolo de 3 fases:

## 1. Abertura da Sessão

1. Ler `AGENTS.md`.
2. Consultar `docs/STATUS.json` (`python tools/project_status.py show`) e `docs/HANDOFF.md`.
3. Ler a especificação da tarefa ativa em `docs/tasks/` correspondente ao `current_step`.
4. Executar `python tools/verify_workspace.py` para garantir que o ambiente e os snapshots estão íntegros antes de fazer qualquer alteração.

## 2. Execução Durante a Sessão

- Trabalhar exclusivamente na etapa ativa (`current_step`), mantendo mudanças atômicas e testáveis.
- Sempre criar ou atualizar ferramentas determinísticas em `tools/` para provar descobertas de engenharia reversa ou validar comportamento de runtime.
- Nunca modificar `heroes_lore_4_recomp`, `heroes_lore_5_recomp` ou outros diretórios fora de `heroes_lore_3_recomp`.

## 3. Fechamento da Sessão

1. Rodar todos os testes e scripts de validação aplicáveis à etapa.
2. Atualizar `docs/STATUS.json` e regenerar `docs/STATUS.md`:
   ```powershell
   python tools/project_status.py render
   python tools/project_status.py check
   ```
3. Atualizar `docs/HANDOFF.md` com o estado exato deixado e o próximo comando.
4. Registrar descobertas técnicas em `docs/RESEARCH_LOG.md` (e em `research/sessions/YYYY-MM-DD-NN.md`) e decisões em `docs/DECISIONS.md`.
5. Executar `python tools/verify_workspace.py` e confirmar saída `OK`.
