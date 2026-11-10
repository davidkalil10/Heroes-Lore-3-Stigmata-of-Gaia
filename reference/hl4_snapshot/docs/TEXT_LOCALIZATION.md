# Textos coreanos e preparação de inglês — 2026-10-21

Pedido do proprietário: investigar os textos para começar a jogar entendendo
menus e história. Extração feita inteiramente offline, sem enviar conteúdo a
serviços. Não houve alteração do APK, saves ou recursos usados pelo runtime.
Atualização: inglês padrão integrado em buffers separados. Ver
[LOCALIZATION_RUNTIME.md](LOCALIZATION_RUNTIME.md) para ABI, revisão, testes
e limitações atuais. As seções de preparação abaixo registram a investigação
anterior à integração.

## Localização comprovada

6.935 registros em 363 recursos foram extraídos, com IDs de recurso/índice,
offsets, comprimentos, hash e campos ko/en/review. Catálogo local ignorado no Git:
`scratch/localization/catalog.ko-en.json`. Na extração inicial eram 13 rascunhos;
nomes próprios e termos precisam de glossário/revisão. Não é tradução completa.
Resumo sem corpus: `reference/analysis/TEXT_INVENTORY.json`.

| Conteúdo | Recurso/formato |
| --- | --- |
| História e diálogos | 345 arquivos `res/MAP/SC/eNNNN_scn`, DES; seção de strings CP949 |
| Menus principais e prompts | `res/tdf/_tdf_MENU_TXT`, tabela de offsets u16 LE |
| Ajuda | `res/tdf/_tdf_HELP`, mesma tabela |
| Inventário/menus/NPC | `res/GMenu/_GMENU_TXT`, `_GMENU_SUB_TXT`, `_NPCUI_TXT` |
| Popups de evento | `res/NPC/_EVENT_POP_TXT` |
| Layouts de toque | `res/TOUCH/txm02_t_kt`, `_lgt`, `_skt` |
| Opções, submenu e dicas | `_tdf_OPT`, `_tdf_SMENU`, `_tdf_TIP`, contagem u8 e comprimentos BE16 |
| Descrições de habilidades | `res/ITM/_ITM_SD0`, `_ITM_SD1`, `_ITM_SD2`, mesmo formato BE16 |

Há ainda quests, itens, nomes de NPCs e texto no ELF em formatos misturados.
O catálogo não pretende cobrir tudo. `_QUEST_0_DAT` foi decodificado em prova
isolada, mas seu esquema completo ainda precisa de parser antes de exportar.
Texto em imagens, como alguns rótulos, deve ser inventariado separadamente.

## Evidência da imagem enviada

A fala exibida pelo usuário foi encontrada exatamente em
`res/MAP/SC/e0000_scn:11`, offset 1704 no recurso decodificado, 55 bytes incluindo
NUL, SHA-256 ac548fd92ac0cde14371457e973324157957ee3fb74f02e4aff0d75210534529.
Rascunho: “Don't be stupid. They've seen our faces. If they report us...”
Isso confirma a origem do diálogo e o codec por bytes, sem depender de OCR.

## Contratos e roundtrip

- CP949: decodificação estrita e recodificação idêntica em todos os registros.
  Não assumir UTF-8/UTF-16. O jogo já possui fontes latinas em res/FT/.
- Tabelas indexadas: u16 LE tamanho total, u16 LE quantidade, offsets absolutos
  u16 LE, strings terminadas em NUL. Validar monotonicidade, limites e terminador.
- MergedText: quantidade u8; para cada registro comprimento BE16 e bytes sem NUL
  implícito. ABI em MrgText_load 0x9e5c4: LDRB/LSL #8/add para o comprimento;
  Text_get 0x9e4e4 e Text_getSize 0x9e4fc. Não trocar por LE.
- Scene: três bytes iniciais definem largura das tabelas (1/2 bytes); seção
  começa no offset 12. Dois blocos de script com contagem u8, comprimentos LE e
  payload; depois contagem u8, comprimentos das strings, payload e padding.
  Script_Init 0x81f38 e IT_Initialize 0x820ac confirmam ordem/leituras; SCENE_Change
  0x5f0bc passa buffer, offset 12 e as três larguras ao inicializador.
- DES: SaveDataInit 0xbee74 instala a chave pelo MX_desInit 0x2ad4c; wrappers
  MX_desDecrypt 0x27f88 e MX_desEncrypt 0x28030 recebem (buffer,bytes), trabalham
  em blocos de 8 e gerenciam ordem das subchaves. São funções ARM originais,
  executadas em Unicorn; nenhum algoritmo/chave de HL5 foi copiado.
- Todos os 345 scenes passaram por decrypt/encrypt idêntico e reconstrução exata
  do contêiner, preservando scripts/padding; zero scenes rejeitados. As demais
  tabelas passaram por reserialização e roundtrip do codec. Isto não prova que
  traduções de tamanho diferente caibam no layout ou na largura do campo.

Reprodução: `python tools/inspect_text.py` (dependências da análise em scratch).
O script verifica o SHA do APK via Wipi e registra somente metadados em Git.
`python -m unittest discover -s tests -v` inclui limites/corrupção/endianness e
preservação de script/padding com fixtures sintéticas.

## Próxima implementação

Priorizar menus/consentimento/avisos e e0000/e0001. Preservar original coreano,
IDs, `%` placeholders, marcadores de cor e controles. HF_IsEscape 0x74db8 e
HF_GetWidth2 0x756b0 reconhecem ASCII especial; `;` pode ser quebra de linha,
`{`/`}` e outros símbolos têm função de formatação. Não removê-los globalmente.
Não usar busca/substituição cega nos arquivos de cena: scripts e strings dividem
o mesmo recurso e os campos de 1 byte têm limite de 255 bytes por registro.

Recomendação técnica, ainda não implementada: buffers separados por idioma,
resolução nos pontos de texto/medição próprios do HL4, wrapping usando métricas
latinas originais. Confirmar ABI e instruções antes de interceptar desenho;
getter/HF_DrawTextAnchorH2Color 0x7588c são candidatos, não hooks aprovados.
Validar menus e diálogos extensos por capturas, preservando estado ARM/canônico.
