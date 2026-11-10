# S09 — textos, glossário e preparação da tradução EN

Pedido explícito do proprietário após jogar a primeira build: localizar os textos
coreanos para compreender o jogo em inglês. Trabalho em paralelo a S05/S06.
Ler TEXT_LOCALIZATION.md, TEXT_INVENTORY.json e o handoff atualizado.

Já extraído: 6.935 registros em 363 recursos, incluindo todos os 345 scenes,
menus, ajuda, dicas e opções. Roundtrip estrito CP949, estrutura e DES original.
Catálogo local scratch/localization/catalog.ko-en.json. Atualização: 6.935
registros preenchidos em EN/PTBR, 334 corrigidos editorialmente; inglês padrão
integrado. Ver LOCALIZATION_RUNTIME.md, que substitui o estado de preparação
das próximas ações abaixo. Recursos restantes/paginação/revisão pendentes.

Próximas ações:

1. Traduzir/revisar menus/consentimento/avisos e scenes e0000/e0001, preservando
   tags e placeholders; definir glossário de personagens/espécies/itens.
2. Mapear texto restante de quests/itens/ELF/imagens. Não promover heurística de
   string isolada a formato editável sem parser e roundtrip.
3. Confirmar ABI de getters/medição/desenho no ARM HL4. Usar buffers estáveis por
   idioma, preservar original e garantir largura/wrapping latinos. Não importar
   hooks, dicionários ou offsets HL5. Testar estado ARM antes/depois.
4. Validar inglês dentro dos menus e diálogos longos, com capturas e retorno ao
   coreano sem regravar scripts. Rótulos que forem imagens precisam de caminho
   próprio. Só então iniciar/concluir os critérios pertinentes de S10.

Não enviar o corpus a serviços externos. Cenas têm comprimentos de 1/2 bytes e
script compartilhado: não substituir no binário de forma cega. Arquivos originais
e saves ficam preservados; testes usam diretórios temporários.
