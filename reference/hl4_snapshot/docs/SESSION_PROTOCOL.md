# Protocolo de continuidade

## Ao iniciar

Ler AGENTS/status/handoff/decisões; verificar alterações locais antes de editar;
executar verify_workspace e inspect_apk --check; selecionar a próxima tarefa
pendente e registrar o início. Preservar mudanças de outras sessões/IA.

## Durante o trabalho

- Atualizar STATUS.json com o helper e anexar caminhos de evidência.
- Em RESEARCH_LOG.md registrar fato/hipótese/teste/resultado, incluindo falhas.
- Em DECISIONS.md registrar escolhas que mudam arquitetura, ABI, formato ou fluxo.
- Criar documentos de subsistema conforme a implementação cresce; evitar que
  só uma conversa ou log ignorado contenha um contrato essencial.
- Preferir passos pequenos e verificáveis. Uma hipótese pode orientar teste,
  mas não vira fato antes de evidência. Não repetir experimento já refutado sem
  indicar o que mudou.

## Ao encerrar ou transferir

Atualizar docs/HANDOFF.md com: último estado comprovado, etapa atual, alterações,
comandos/testes, limitações, arquivos importantes e próxima ação executável.
Criar `research/sessions/AAAA-MM-DD-NN.md` usando TEMPLATE.md. Atualizar status,
executar verificadores e registrar resultado. Se houver Git, deixar commits de
escopo claro quando adequado; não incluir APK, saves, chaves ou artefatos.

## Estados

`pending`: ainda não começou. `in_progress`: existe trabalho ativo.
`blocked`: dependência identificada e registrada. `done`: critérios atendidos,
evidência existente e limitações explicitadas. DONE de pesquisa não significa
produto terminado. Corrigir status quando nova evidência invalidar uma conclusão.

Exemplo:

```powershell
python tools/project_status.py set S02 in_progress --summary "Mapeando callbacks WIPI"
python tools/project_status.py set S02 done --summary "ABI inicial documentada" --evidence docs/RUNTIME_CONTRACT.md
python tools/project_status.py check
```

Não marcar S02 done apenas com a lista de símbolos: ler o critério completo.

As referências congeladas mantêm os bytes originais por .gitattributes (-text), para que os hashes continuem válidos em checkouts Windows/Linux. Não normalizar esses arquivos; documentos ativos usam LF.
