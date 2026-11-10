# Inglês no runtime HL4 — 2026-10-21

Inglês padrão; `--language ko` preserva desenho original. Catálogo local
`scratch/localization/catalog.ko-en.json`: 6.935 registros com `ko`, `en`, `ptbr`.
As duas traduções estão preenchidas, mas a maior parte é rascunho automático.
334 registros receberam correções editoriais locais: cenas e0000/e0001,
tutoriais, menus principais/opções, classes, prompts de equipamento e avisos.
Isso não representa revisão editorial de toda a campanha.

## Procedência e reconstrução

Tradução executada localmente com CTranslate2/NLLB-600M, diretamente de ko para
en e pt; textos não enviados a serviços. Arquivos/modelo e hashes em
`scratch/translation_models/nllb/download-provenance.json`. Pesos CC-BY-NC 4.0
nunca entram na distribuição. Primeiro modelo Argos ko-en foi descartado por
qualidade. NLLB também erra nomes, frases curtas e segmentos separados por tags.
Correções versionadas em `assets/locales/review_*.tsv`; termos de fantasia são
provisórios, não nomenclatura oficial confirmada.

`python tools/apply_locale_review.py` aplica as revisões por ID e propaga
duplicatas canônicas. `python tools/compile_locale.py` gera `dist/windows/hl4.lang`;
build_windows chama este compilador. Extras em runtime_extra.tsv vieram de
misses observados durante execução. inspect_text preserva traduções apenas
quando ID, hash e original coincidem; não substitui tradução de conteúdo alterado.

## ABI verificada nesta versão do ELF

Endereços abaixo são VA antes de BASE 0x02000000. ARM, argumentos AAPCS r0-r3
e demais na pilha. Guards recusam ELF diferente; nenhum endereço HL5 usado.

- HF_DrawTextAnchorH2Color 0x7588c: e92d4ff0, e59f8b80.
  `(frame,x,y,width,font,text,start,skip,end,anchor,color)`.
  **O quinto argumento é fonte, não altura.** Buffer localizado separado,
  args 6-9 transitoriamente adaptados; renderer ARM original desenha.
- Retornos conhecidos: BL 0x5afa4/eb006a38 e 0x764a0/ebfffcf9,
  retornos +4. Hooks registrados antes de executar ARM. Registro tardio dentro
  de um bloco já traduzido não restaurava a pilha; tentativa rejeitada.
  Guarda SP/LR restaura quatro palavras; RET do teste também restaura.
- HF_GetWidth2 0x756b0: e92d47f0, e1520001.
  `(text,end,start,maxWidth,font[sp])`. Medição usa os mesmos bytes ingleses
  do desenho; função original calcula a largura. R0-R2 são caller-saved.
  Teste compara largura do canônico interceptado com inglês direto, testa
  desenho, bytes canônicos, pilha e preservação de R4-R11.

Fonte latina nativa: avanço normal aproximadamente 6 px, fonte pequena 4 px;
layout não deve ser resolvido esticando framebuffer. Rótulos foram encurtados,
atributos usam STR/DEX/VIT/SPR e descrições têm quebras editoriais. Medição
corrigida resolve centralização que antes usava largura coreana.

## Limites que continuam abertos

Paginação/offsets parciais usam projeção proporcional do tamanho canônico;
não há mapeamento linguístico de cada caractere. GetLineCount e rotinas de
paginação ainda não foram adaptadas. Não afirmar encaixe de todo diálogo.
Imagens coreanas, itens/quests ainda não parseados e strings dinâmicas fora
do dicionário permanecem; misses são registrados no smoke em hex CP949.
PT-BR em UTF-8 no pacote/catálogo, mas runtime só en/ko até implementar acentos.
Há muitos rascunhos incorretos fora do trecho revisado. Próxima prioridade:
expansão de parser de quests/itens e revisão por cenas, com capturas de layout.

## Falha de zero bytes

Reprodução: cópia descartável do save do proprietário, script
scratch/cutscene-repro.txt, 105 s determinísticos. Original falhou com
`dlib_memcpy`, source=0, size=0, LR_VA=0x5da30 em Event_Get_QuestStatusList.
0x5da14 chama memset, 0x5da1c strlen do buffer vazio, 0x5da20 põe origem zero,
0x5da2c chama memcpy de comprimento zero. Corrigido contrato HLE vazio em
memcpy/memmove/memset/strncpy/strncmp/memcmp; limites de acesso não afrouxados.
Teste ainda rejeita cópia com origem nula e tamanho 1. Execução corrigida passou
pelo ponto da falha; proprietário também confirmou em teste manual.
Saves pessoais originais não foram editados; testes usam cópias em scratch.

## Avisos de missão — 2026-10-22

compile_locale remove apenas o padrão `;$...........{` de recuo invisível
das traduções. 344 registros (306 canônicos), 174 recebimentos/170 conclusões.
Os dois estados coreanos exatos agora têm tradução consistente EN/PTBR; títulos
ainda podem ser rascunhos. Original e tags de cor/quebra permanecem preservados.
HF_GetLineCount original: exemplo Find Kefnes/Quest received, largura130/font0,
3 linhas com recuo antigo contra2 sem recuo. --verify-core captura ambos com
HF_DrawTextAnchorH2Color, verifica canônico/pilha. Não resolve paginação geral.


## Nomes e composição dinâmica — 2026-10-22

Catálogo ampliado de6935 para8232registros:1297nomes em dados de equipamentos,
inimigos eNPCs. extract_dynamic_text.py confirma tabelas LE16 payload-length,
byte de comprimento do nome (offset0 ou1), paddingDESzero<8, CP949estrito e
roundtrip do container e DES via ARM original. Nenhum byte do recurso é alterado.
DYNAMIC_TEXT_INVENTORY.json registra formatos/contagens/hashes. Descrições
embutidas e outros formatos ainda não foram integralmente extraídos.

576nomes canônicos receberam glossário independente de frases NLLB; nomes de
fantasia são provisórios, compostosPT ainda exigem revisão gramatical. Exemplos:
패잔병->Straggler (curto paraHUD), 자수정->Amethyst, 롱소드->Longsword, 비스->Bis.
review_dynamic_names.py produz review_names.tsv; apply_locale_review reaplica
esse arquivo depois da revisão existente. Traduções EN/PTBR no catálogo;
runtime ainda EN/KO devido à fontePTBR pendente.

resolveText mantém lookup exato e compõe apenas trechos coreanos conhecidos e
ASCII literal (espaços, cores e contadores). Isso cobre Bis Longsword,
Straggler(1/10) e Amethyst com espaço final. Se restar qualquer byte coreano
não reconhecido, mantém o original e registra miss, sem inventar tradução.
Cache512/arena128KiB recicla só quando não há pilha de draw adaptada ativa.
Core testa600contadores diferentes, medidas/desenho originais e bytes/pilha
canônicos preservados. Cópia do save reproduziu cinco misses anteriores;
revisão intermediária traduziu todos. Relatório final registra o novo teste.
Não afirmar cobertura editorial de toda a campanha.
