# Diário de pesquisa

## 2026-10-21 — inglês padrão, revisão e cutscene

Sessão research/sessions/2026-10-21-05.md; detalhes em LOCALIZATION_RUNTIME.md,
evidência LOCALIZATION_TESTS.json. Crash dlib_memcpy de zero bytes com src nula
reproduzido e corrigido no contrato HLE, sem aceitar acesso nulo não vazio.
EN/PTBR preenchidos offline; 334 correções editoriais, restante rascunho.
Desenho e medição ARM HL4 adaptados com guards e restauração de pilha. Menus
encurtados; duas primeiras cenas/tutoriais revistos. Pacote English-review.
105 s com cópia de save e trajetória igual ao coreano; menu/status por cliques.
Paginação/cobertura completa/PTBR com acentos continuam pendentes.

## 2026-10-21 — S02: DEX, loader e primeiras provas de ABI

Fatos novos e comandos completos em RUNTIME_CONTRACT.md; sessão em
research/sessions/2026-10-21-02.md. ELF/DEX analisados localmente com hashes
preservados. Binder executeArm recebe identificador, não o ELF; após retorno 1
o wrapper escreve JAR e lança player WIPI externo com AID_PATH. Nenhum desses
serviços é entregue no APK. Confirmada a necessidade de bootstrap próprio.

Loader: dois LOAD com diferença de 0x1000 entre offset/VA no segmento RW;
1.689 RELATIVE, 1 GLOB_DAT e 105 JUMP_SLOT; BSS 442.728 bytes. Ausência de
DT_INIT/INIT_ARRAY; DT_TEXTREL presente. PLT verificada contra seus destinos GOT.
ARM mapping symbols são escassos, nenhum FUNC tem bit Thumb; não concluir
ausência de Thumb em destinos indiretos. Nenhum hook/endereço HL5 reutilizado.

21 probes ARM aprovados: _init_module instala _export_table (não trampoline)
e salva três argumentos; init entrega cfunc ao event handler; paintClet retorna;
toques 0x708/subtipos 0/1/2 viram 0x5064/65/66; eventos 502/503 armazenam código
pendente. Descriptor de framebuffer usa stride width*2 e ponteiro guest.
Timer usa delay r2:r3 e quinto argumento na stack. Quatro casos de nomes
confirmam res/ e case preservado; três caminhos de UM_LoadRes confirmam as
ramificações -12/-18 com alocação/free simulados. Isso não testa o SDK real.

Catálogo: 108 imports classificados/priorizados; 15 com contrato parcial e 93
sem assinatura verificada. Recursos: 43 OggS, 42 contêineres MMMD, 1.808 não
classificados. Trailer RAPT de 80 bytes preservado; campos numéricos abertos.

Resultados negativos/correções: tentativa inicial de instalar dependências na
rede restrita falhou com WinError 10013. Instalação autorizada completou e as
permissões das dependências em scratch foram reintegradas à pasta para execução
restrita. A primeira versão do desassembly propagava um $d até funções sem
mapping symbols: corrigida para iniciar STT_FUNC como código, com regressão.
Literal pools sem símbolos continuam explicitamente ambíguos. A hipótese de
callback direto no offset +0x214 foi rejeitada pela execução: é _export_table.
Nenhum resultado inicial incorreto foi mantido como contrato confirmado.

Validação: verificações originais, 21 probes, check de relatórios e 3 regressões
(mapping esparso, PLT alterada e relocation desconhecida). Não há boot, pixels
reais, áudio tocado, teclado ou gamepad de host. S02 segue em andamento por
contratos de scheduler/contextos/properties/interface ainda não mapeados.

Diretriz do proprietário nesta sessão: primeiro marco é jogo executando neste
PC, com teclado e mapeamento automático/equivalente para gamepad. Critério
registrado em VALIDATION.md; nenhuma plataforma foi promovida a validada.

## 2026-10-21 — inventário inicial (fatos)

- APK raiz preservado, 8.407.877 bytes, SHA-256 em project.json.
- ZIP externo: 10 entradas; classes.dex tem 9.948 bytes.
- Biblioteca lib/armeabi/libbinary.so: 1.034.442 bytes, ELF32 little-endian,
  máquina ARM (40), entrada virtual 0x220c4. Endereço ainda não rebased.
- Dynsym: 3.013 símbolos com nome; 108 indefinidos/importados; sem Java_* exports.
- Símbolos startClet, paintClet, onStartApp, onHandleEvent, MainFrame_Process,
  MainFrame_Render e funções de menu/save/mapa preservados. Endereços/catalogação
  completos em reference/analysis/elf_symbols.csv.
- res/raw/jarfile.mp3 é ZIP/JAR: 1.935 entradas, zero .class. Recursos incluem
  res/H4, MAP, OBJ, SND, NPC, CM, ITM, TOUCH e outros diretórios.
- binary.mod tem 1.034.522 bytes: prefixo idêntico à biblioteca inteira, mais
  80 bytes finais. O significado desses bytes ainda não foi determinado.
- Strings DEX incluem android.intent.action.LAUNCH_WIPI e com.lgt.arm.ArmInterface.

## Hipóteses ainda abertas

1. O DEX delega a execução a um serviço WIPI/LGT externo. Confirmar o fluxo por
   disassembly DEX e binder, sem precisar executar o APK no sistema atual.
2. Entradas Clet/eventos podem fornecer um bootstrap menor que o JNI do HL5.
   Ler as instruções e ABI; nomes não provam ordem/parâmetros.
3. Recursos do JAR podem ser servidos por índice/nome via MC_knlGetResource*.
   Mapear IDs, compressão e ownership antes de criar filesystem virtual.
4. Mixer/loader/decoder do HL5 podem ser parcialmente reaproveitados. Verificar
   ISA, relocations e contratos; não assumir compatibilidade binária.

Próximo experimento: docs/tasks/S02_RUNTIME_CONTRACT.md. O inventário não é um
teste de execução. Nenhum título, mapa ou áudio do HL4 foi executado aqui.


## Informação do proprietário — ports 1, 2 e Zero

Durante a preparação, o proprietário reforçou que os três primeiros ports eram
jogos .jar. Registrado em PREVIOUS_PORTS.md, com cópias de documentação para consulta.
Não generalizar isso ao JAR de recursos do APK HL4, cuja estrutura já foi inspecionada.
O manifesto Android do HL4 informa package android.lgt.wipi.App00035956,
versionName 01.00.00, versionCode 1 e minSdkVersion 4; esses são dados do input,
não os identificadores/versionamento que o novo port deverá adotar.


## Verificação das ferramentas de preparação

12 checks de fixtures descartáveis e 15 links internos aprovados. O teste de
nomes ZIP revelou que o Python normaliza barras invertidas no Windows: a extração
passou a validar orig_filename e a fixture preserva o nome bruto. Repetição da
extração preserva arquivos iguais e rejeita sobrescrita de conteúdo diferente.
Os testes também recusam caminhos externos/symlinks e DONE sem evidência.
A descoberta diz respeito ao tooling, não a uma falha no APK original.

## 2026-10-21 — runtime integrado e primeira distribuição Windows

Loader/HLE próprios C++/SDL/Unicorn executam código ARM original até criação de
personagem, primeiro mapa, movimento e menu. Áudio Ogg não nulo no dispositivo
real. Paisagem solicitada implementada na apresentação, sem esticar framebuffer.
Pacote standalone inclui três DLLs, dados verificados, fontes e licenças.
Teclado e gamepad virtual entregaram estados e 40 BMPs idênticos. Pacote rodou
80 s com relógio real neste PC: 1.429 flushes, 1.988.608 amostras não nulas.

Resultados negativos: lib Unicorn do wheel é estática; gerar import library do
DLL resolveu link. Timer r1 é padding, não modo; callback precisa de userdata.
MC_netConnect(-19) era interpretado como sucesso pelo wrapper: substituir por
callback assíncrono(-1,userdata), que aciona erro original e permite continuar.
Formato original '%s%0' é inválido: manter prefixo e retornar -1. Popups de quest
impediam movimento nos primeiros scripts; usar input original para encerrar cenas.

Hipóteses abertas: outros imports/formatos podem ser alcançados após o primeiro
mapa; ataque/dano, habilidades e save/load de partida exigem nova evidência.
MMMD não está validado. Gamepad físico e audição humana continuam pendentes.
Sessão research/sessions/2026-10-21-03.md; contratos e reprodução WINDOWS_RUNTIME.md.

Checagem final: encontrados 10 caches/relatórios Gradle gerados pelo editor dentro
do snapshot (originais ainda com hashes corretos). Preservados em
scratch/snapshot-generated-files com manifesto, restaurando inventário exato.
.vscode/settings.json apontava CMake ao HL5; agora aponta ao workspace e desativa
configuração/importação automática. Nenhum script de build HL5 foi executado pelo host.

## 2026-10-21 — correção de pointer e início S09 por pedido do proprietário

Relato do usuário invalidou a alegação anterior de mouse validado. SDL LogicalSize
já escala eventos; RenderWindowToLogical aplicava escala/barras pela segunda vez.
Host corrigido, down/up enfileirados por timer e SDL_FINGER separado. Testes
integrados verificam Skip 1 ms, ação/direção/menu/retorno em 800x480 e 1000x700,
com mesmos estados por mouse/finger. Primeiro teste sintético omitia windowID,
contornando filtro SDL: fixture corrigida, sem aceitar seu falso diagnóstico.

Textos encontrados via análise de ARM e recursos: UI em CP949/indexed LE16;
MergedText count+BE16; cenas MAP/SC em DES com scripts e strings no mesmo recurso.
SaveDataInit + DES original decodificam 345 scenes; todas passaram encrypt/decrypt
e reconstrução exata. 6.935 registros em 363 recursos, 13 rascunhos EN no catálogo
local ignorado. Fala exata da screenshot encontrada em e0000_scn:11 (offset1704).
Nenhum conteúdo enviado a serviço, nenhum recurso da build alterado por tradução.
Quests/itens misturados, texto de imagens e inserção EN no renderer são pendências.
Ver TEXT_LOCALIZATION.md, POINTER_INPUT.md e sessão 2026-10-21-04.


## 2026-10-21 - main privada, quatro plataformas, icone e Switch

S12 implementado em main propria, estrutura HL5 somente leitura. Insumos APK e
catalogo autorizados em release privado fora do Git; nenhum save/chave enviado.
81a98d3/run39690370966: seis jobs concluidos, APKs ARM64/global (3 ABIs), fontes
em artifact separado. Android instalado e jogado pelo proprietario. Switch
anterior fecha antes do boot; investigacao encontrou reserva TCI64 de 1 GiB.
07ac588/run39691502454 limita a 32 MiB antes de lazy init e grava boot.log na SD.
Hipotese de causa ainda requer reteste de console. Icone original extraido do
APK pinado e convertido offline. Evidencias: CI_BUILDS.json e sessao 06.


## 2026-10-21 - requisito de viewport multiplataforma

Relato e captura Android com barras laterais registrados em S08. Auto deve
mostrar mais cenario conforme a proporcao, com HUD/toque e fullscreen corretos.
Referencia HL5 DISPLAY.md consultada somente leitura. Matriz cobre quatro
plataformas, areas seguras, proporcoes, estados/audio e memoria/desempenho.
Apenas planejamento; nenhuma mudanca na build ou validacao de viewport.


## 2026-10-21 - log Switch: desRound interrompido pelo watchdog

Log do proprietario comprova boot do host e entrada no codigo original. PC
0x02024d54 corresponde a desRound VA0x24d54 no APK pinado. Watchdog anterior
5s interrompe chamada; erro code_gen_buffer e munmap na limpeza, nao alocacao.
9263dc57 amplia orcamento TCI sem mudar relogio guest, consulta timeout real e
libera cache heap com contrato explicito; guard opcional desativado so Switch.
CI39693888307 e evidencia SWITCH_BOOT_WATCHDOG.json; hardware novo pendente.


## 2026-10-22 - Switch executa, primeira aceleracao medida

Usuario confirmou boot/jogo, relatou lentidao geral. Historico HL5 consultado:
chat Identifique o projeto Flutter, v1.1.6 graficos/menu/SD e feedback positivo.
Perfil HL4 atribuiu89.74% do volume ARM a5rotinas graficas (nao tempo de CPU).
10regioes geradas do decoder puro adaptado; 20checks memoria167772160bytes e
R0-R15/CPSR,36estados e38BMPs iguais. Amostra Windows109.15s vs75.45s, naoSwitch.
35280b7 em CI. BootDES tambem acelerado; O3 e log10s para medicao no aparelho.
Ver sessao2026-10-22-01 e ARM_FAST_TESTS.json. S07 segue parcial ate reteste fisico.

## 2026-10-22 - barras Android e aviso de missao

Fotos mostram barras do sistema e HUD cortado. Janela SDL sem FULLSCREEN
solicita barras visíveis ao SDLActivity apesar do tema fullscreen no Manifest.
Solicitado SDL_WINDOW_FULLSCREEN no Android; precisa de reteste no aparelho.
Aviso de missão tinha onze pontos invisíveis depois de `$` para recuo;
inglês deslocado quebra em3linhas. Retirada do recuo e estados EN/PTBR
consistentes em344registros. Original ARM comprova3->2linhas e captura correta.
Prova experimental240x534 inicializa _lcdG+54/+58 e roda2222flushes/áudio,
mas amplia painel de controle, não cenário. Não integrada como widescreen.
Evidências ANDROID_LAYOUT_QUEST_TESTS.json.

## 2026-10-22 - expansão inicial comprovada

Resolvido o limite do primeiro protótipo: touch_setUiChange0x35c14 subtrai
m_TouchDirectKeySize da altura física para compor a largura lógica. Conservar
painel80px (zero oculto preservado) libera largura extra, em vez de atribuí-la
ao painel. Hook guardado, sem opcodes alterados. Auto na abertura e Native CLI.
4:3/16:9/20:9:1355flushes/23estados iguais,17patches16x16 exatos, áudio real.
Oracle20checks e160MiB iguais. Toque106eventos/Skip1ms/menu/movimento passou.
Cidade400x640 limita cenário autoral: não confundir margem de mapa pequeno
com framebuffer fixo. DISPLAY.md e VIEWPORT_EXPANSION_TESTS.json. Ainda sem
retorno físico desta expansão e sem resize vivo.

Suíte final: acrescentadas16:10 e21:9, sete execuções46s e toque80s
com saves temporários isolados; todos passaram. Renderer criado antes do
guest para medir saída física em SDL_GetRendererOutputSize antes do logical
size. Não depender da dimensão nominal solicitada, especialmente Switch.
Core,7unittests, gerador ARM --check, workspace/APK passaram.

cec054c/run39767050237: quatro builds jogáveis success. APKs ARM64/global
eNRO finais baixados e verificados, mesmos caminhos dist/. Windows portable
reempacotado com deps. Referência TCI ainda executando Verify no último check.
Hashes em ANDROID_LAYOUT_PACKAGES.json. Sem teste físico desta revisão.


## 2026-10-22 — Renderizadores completos e nova identidade

Perfil remanescente com fast ativo: DM_DrawScreen565Ex40.107% e
DM_DrawScreen56527.526% dos bytes interpretados. CFG próprio HL4 gerado inteiro,
epílogos originais, páginas1024 e emissão direta; contadores agrupados.
Expansão genérica inicial excedeu recursos de compilação e foi abortada;
nova emissão compila em segundos. Validado7regiões/14oracles integrais,
38BMPs/36estados/2222flushes iguais;9013163392instruções nativas antes/depois
de agrupar contador. ALU38formas/1216estados Unicorn. Host55.258s->34.503s,
um par sem profiler/oracle, não SwitchFPS.

Usuário jogou/combateu/abriu menus, boot.log completo não contém métricas.
Criado performance.log independente com fflush5s. Log físico output800x480:
Switch solicita modo do console/fullscreen; A/B eX/Y invertidos. Arte2048 do
proprietário pinada no insumo privado; Android adaptativo opaco, todos os
formatos derivados offline. Build Windows local recebe recurso ICO também.
S07 e validação física continuam abertos. Fonte/relatórios em sessão03.


## Entrega e próximos testes

Código4c9d405, run39774383459: seis jobs success. Windows/Linux/TCI validam
14checks completos,38BMPs/36estados e2222flushes iguais. AmostraTCI pareada
208.524s original contra32.729s fast,6.371x no mesmo runner; não é FPS físico
nem comparação direta com runner anterior. APKs ARM64/global eNRO baixados,
conteúdo/ícones/deps verificados e atualizados nos caminhos dist/ habituais.
Hashes/artifactIDs/jobs/relatórios em ICON_FULL_AOT_PACKAGES.json.

Próximo: reteste físico do dono no Switch (fluidez, A/B,X/Y, resolução/HUD),
performance.log independente e máscara de ícone Android. S07 continua parcial.


## 2026-10-22 — combate, nomes e móvel

Reprodução com cópia do save confirmou misses Bis/Longsword,Amethyst e
패잔병(1/10). Expandido catálogo+1297nomes para8232, roundtripDES/container/CP949.
NLLB em nomes curtos gerou frases sem sentido; todas as576chaves novas únicas
passaram a glossário/compostos offline, fantasia ePTgramática provisórios.
translate_catalog teve assert em tradução antiga já revisada: corrigido para
validar só campos novos, preservando traduções/metadados anteriores.

Perfil do percurso110s com fast anterior: rotateEx28.44%,collision9.34%,
fillRectBlend7.60% dos bytes restantes, não walltime. Três novas regiões do próprio
HL4; Android agora acelera por padrão. Watchdog antigo cria/junta thread/poll2us
por chamada. Substituído por thread SDL persistente, orçamentos intactos;
três loops infinitos, suspensão antes da entrada e chamadas seguintes passaram.
Tentativas frescas de chegar ao combate pararam em tutorial; não usadas como
prova de combate. CI recebe sprite/overlay sintéticos no core, sem save pessoal.

Resultado final host: combate57BMPs/55estados/3525flushes iguais e20comparações
GP/CPSR/160MiB; áudio não nulo, zero misses, saves originais inalterados.
Cidade38BMPs/36estados/2222flushes mais4checks kernels; ALU46formas/1472casos,
10tests Python, cache600contadores e cinco proporções/finger passaram.
Host watchdog isolado35.140s->33.890s (1par), pequeno, não prometeFPS móvel.
Primeira cronometragem concorrente descartada. COMBAT_MOBILE_TESTS.json.

Log inexistente informado pelo usuário ao usar hbmenu/R: API FsFile nativa
com flush e cabeçalho préSDL, diagnósticoFS se abrir falha; causa antiga incerta.
TitleIDNACP0100F4A400040000 e docs explicam independência do forwarder instalado;
saves mantidos. Novo catálogo em insumo privado666280257 autorizado, sem saves.
Próxima ação: main/CI, verificar/baixar pacotes nos caminhos fixos e pedir
reteste físico de combate/log/atalho. S07,S10,S12 continuam parciais.


## Entrega final CI

f4e3ee0 / run39807955246: workflow completed/success, pacotes das4plataformas.
Windows/Linux/TCI20checks,38BMPs/36estados/2222flushes iguais; ALU1472casos.
TCI1par127.207s semfast /19.518s fast,6.517x, nãoFPS nem comparação com outrohost.
Endpointjobs manteve Linux in_progress/null mesmo com todas as etapas success
incluindoCompletejob; preservar resposta bruta e workflowfinalsuccess no relatório.

APKs ARM64/global eNRO baixados/validados, mesmos nomesdist; NACPnovo e
markerFSv3 conferidos. COMBAT_MOBILE_PACKAGES.json com hashes/artifactIDs/reports.
Windows local standalone atualizado. Próximo é teste físico de combate/log/textos
nos aparelhos e novo forwarder com ID0100F4A400040000. Não remover save/atalho
pessoal nem declarar fluidez resolvida. Fonte .vscode permanece intocada.
