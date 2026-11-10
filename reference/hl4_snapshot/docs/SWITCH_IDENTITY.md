# Identidade do pacote Switch

Title ID escolhido para o homebrewHL4: **0100F4A400040000**. É uma escolha local
específica do projeto, não uma reserva oficial. Antes project.json e NACP
não definiam ID próprio. Workflow passa --titleid ao nacptool; verificador
confere presence/save-owner/play-log IDs no NACP embutido do NRO.

O proprietário abriu inicialmente por atalho instalado; após loading infinito,
passou a hbmenu com R (memória completa). Apagar NRO não apaga o registro do
atalho instalado. ID do forwarder/atalho é definido pela ferramenta que o cria;
NACP do NRO não troca nem desinstala um forwarder existente. Use o novo ID ao
criar outro atalho apontando ao mesmo NRO. Não foi gerado nem instalado NSP e
não foram alterados registros do console, pois este projeto não os controla.
Saves continuam em sdmc:/switch/heroes_lore_4/saves.

Formato e opção --titleid:
https://github.com/switchbrew/switch-tools/blob/master/src/nacptool.c
Diagnóstico nativoFS e próximos testes em PERFORMANCE.md.
