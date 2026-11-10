# Aprendizados comprovados no port anterior

Origem: snapshot HL5 1.1.6. O proprietário confirmou jogo/áudio/aspecto em PC,
Android/Switch, cloud restore sem sair do app, atualização Android e posteriormente
Switch, e melhora adicional da performance no Switch. Isso valida o HL5, não HL4.

## CPU, renderização e áudio

- Um port que usa SDL/GPU ainda pode gastar quase tudo interpretando ARM por
  pixel. Medir boot, mapa, diálogo, combate, menus e apresentação separadamente.
- HL5 começou com sete blocos ARM compilados e chegou a 14. Gerar do mesmo
  decoder; conferir opcodes; preservar fallback e comparar registradores, flags,
  memória completa e icount quando prometido. Não copiar os blocos para HL4.
- A abertura do menu incluía desfoque da tela toda com divisões emuladas por
  pixel. Um HLE guardado manteve o último pixel interpretado para restaurar o
  estado exato da linha. É um padrão de prova, não um patch universal.
- Profiling com otimizações desligadas pode esconder o gargalo restante. Medir
  também com acelerações ligadas. Separar cutscene de exploração/combate.
- Cachear opções de diagnóstico fora do caminho por pixel/instrução. Consulta
  de ambiente em loop chegou a contaminar o próprio benchmark de referência.
- Gravações de log sem buffer em SD podem causar pausas: tracing detalhado opt-in;
  guardar erros e agregados baratos. Não acumular tempos de quadro indefinidamente.
- Comparar PC com PC; cross-compile não prova FPS de Switch. Não forçar clocks
  nem reduzir qualidade automaticamente para encobrir um gargalo desconhecido.
- Áudio só funcionou ao substituir stubs silenciosos por mixer/decodificação
  reais. O arquivo com extensão .mp3 podia conter Ogg. Identificar por bytes.
- Preservar licenças de dr_mp3/stb_vorbis, não compilar stb_vorbis duas vezes,
  incluir src/audio na build Switch, manter I/O e decoding fora do callback.
- Relógio precisa acumular milissegundos fracionários e limitar deltas extremos;
  não transformar frames lentos em 16 ms fictícios. Evitar limitador duplicado
  quando vsync funciona. Verificar comportamento de alta taxa de atualização.

## Viewport e localização

- Widescreen real expande câmera, culling, cache e buffers. Esticar pixels não
  atende ao objetivo. Auto é a preferência padrão desejada, não sobrescrever uma
  escolha explícita salva. Testar resize/orientação ao vivo e diálogos abertos.
- Resolver idioma no draw preservando texto canônico, com buffers estáveis por
  idioma/contexto. Alterar bytes originais impede trocas completas ao vivo.
- Nomes PT-BR maiores exigem variantes contextuais, wrapping e ajuste de fonte
  dentro dos widgets; não usar abreviações globais destrutivas.
- Fontes no Switch precisam respeitar `romfs:/` e não depender de font do SO.
  Painel móvel deve escalar pelos pixels disponíveis, não só pela resolução guest.
- Configurações extras ficam em uma ação explícita das opções originais, com
  atalho F10/R3. Abrir um modal ao simplesmente pintar a tela original causou
  sobreposição e foco preso. Testar abrir/fechar/reabrir e toque.

## Serviços e distribuição

- SDL_RWFromFile no Android pode preferir arquivo interno em vez do APK e acabar
  lendo/truncando o mesmo destino. Extrair via AAssetManager, estágio, tamanho/hash
  e substituição só após validar; testar upgrade com arquivo corrompido existente.
- Restore deve estagiar, parar/destruir o estado guest anterior, aplicar com
  rollback e retornar ao título nativo; não fechar o processo nem retomar um
  mapa antigo sobre os novos saves. Manter formato e metadados de slots corretos.
- Backups incluem somente saves, nunca OAuth, idioma, preferências ou updater.
- Atualização precisa de release publicada e metadados crescentes em todas as
  plataformas. Artifacts do Actions não são releases. Preservar assinatura Android.
- Verificação ao iniciar deve ser visível, tolerar rede ainda indisponível e ter
  retry limitado. Download longo precisa mostrar bytes/% e manter a UI viva.
- No Switch, std::filesystem::copy_file retornou ENOSYS. Usar cópia fread/fwrite
  em blocos e rename com estágio/backup/rollback. Validar NRO/tamanho/hash antes
  de soltar RomFS; só então preparar relançamento. TLS continua verificado.
- Identificadores, caminhos e arquivos de nuvem devem ser específicos do jogo.
  Nunca publicar HL4 no repositório/updater nem na pasta de saves do HL5.
