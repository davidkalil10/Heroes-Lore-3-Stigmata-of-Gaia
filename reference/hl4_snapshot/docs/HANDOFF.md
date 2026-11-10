# Entrega final mobile/combate — f4e3ee0 / run39807955246

Workflow concluído com success. Quatro pacotes gerados; Windows/Linux/TCI
passaram20checks integrais (cidade16 + kernels4),38BMPs/36estados iguais,
2222flushes/áudio real. ProbeALU1472casos. ReferênciaTCI127.207s semfast e
19.518s fast,1par no mesmo runner; não é FPS físico nem comparação de hosts.
APIjobs ainda informou Linux in_progress/null apesar de TODAS as etapas,
incluindo Complete job, com success e workflow completed/success. Reportar
workflow e passos, preservar resposta bruta em COMBAT_MOBILE_PACKAGES.json.

NRO24183832bytes, APK17261067bytes ARM64/25661488bytes global baixados,
validados e substituídos nos mesmos nomes dist/. Payloads/data/lang/arte/ABIs
/deps e IDNACP conferidos; marker native-FS v3 presente. Windows English-review
local atualizado com trêsDLLs e saves preservados. Fontes separadas dos pacotes.

Próxima ação: reteste físico do proprietário em batalhas Android eSwitch;
conferir textos(Straggler/Bis Longsword/Amethyst e mensagens de coleta), novo
performance.log viaFSnativo e criar atalho com ID0100F4A400040000. ApagarNRO
não apaga o forwarder instalado. Não alegar remoção do fantasma nem fluidez
resolvida. S07/S10/S12 ainda parciais; saves/pastas mantidos, .vscode do usuário.

# Revisão mobile/combate — 2026-10-22

Usuário confirma quase todos os itens anteriores, mas Android/Switch lentos
principalmente em batalha; PC fluido. Nenhum performance.log apareceu, mesmo
hbmenu via R. Atalho instalado ficou em loading infinito/registro fantasma;
apagar NRO não o remove. Não assumir causa do atalho sem ferramenta/console.

Novas10regiões cobrem colisões, rotação com mistura e overlayRGB565. Android
agora liga fast por padrão. Watchdog SDL persistente evita thread nova/polling2us
a cada chamada, mesmos budgets; tests interrompem loops infinitos e retomam.
--legacy-watchdog mantém diagnóstico anterior. PERF não encerra sem hardware.

Local final: combate em cópias do save,57BMPs/55estados/3525flushes iguais,
20oracles integrais160MiB, áudio real e zero misses; saves originais preservados.
Cidade38BMPs/36estados/2222flushes iguais mais4checks de kernels sintéticos;
ALU46formas/1472estados e10unittests. Cinco proporções/finger passaram de novo.
COMBAT_MOBILE_TESTS.json e ARM_COMBAT_PROFILE.json. Ganho isolado watchdog noPC
foi pequeno35.140s->33.890s, não extrapolar Switch. Teste concorrente descartado.

Catálogo8232: +1297nomes,576canônicos com glossário. Straggler/Bis Longsword/
Amethyst, composição de espaços/cores/contadores. Cache512/128KiB recicla fora
de draw ativo;600contadores testados. Nomesfantasia/PTcompostos ainda provisórios.
DYNAMIC_TEXT_INVENTORY e LOCALIZATION_RUNTIME.md. DES/APK e scripts intactos.

Novo IDNACP **0100F4A400040000**, próprio de homebrew, não reserva oficial.
Forwarder instalado exige usar esse ID ao recriar; NRO não muda registro antigo.
Saves continuam iguais. Logv3 usa FsFileWrite+Flush no cartão diretamente,
começa ANTES do SDL em /switch/heroes_lore_4/performance.log; erroFS gera alerta.
Não prometer que logs ou atalho foram validados fisicamente.

Próximo: publicar main e acompanhar CI, corrigir falhas reais, baixar/verificar
APKs ARM64/global eNRO para dist/ habitual. Windows English-review deve ser
reempacotado com DLLs/saves preservados. CI usa kernels sintéticos, não save
pessoal. Relatório final de jobs/hashes será adicionado. .vscode é do usuário.

# Entrega final — 4c9d405 / run39774383459

Seis jobs concluídos com sucesso: Windows/Linux/Android/Switch, fonte separada e
referência TCI. Windows/Linux/TCI confirmam14oracles de registradores/160MiB,
38BMPs/36estados/2222flushes iguais e áudio não nulo. Probe ALU1216estados
passou em Windows/Linux. TCI: amostra pareada208.524s original sem aceleração
vs32.729s acelerada (6.371x) no mesmo runner; não é FPS do Switch nem comparação
estatística com runner anterior. Relatório ICON_FULL_AOT_PACKAGES.json.

APKs17195023bytes ARM64/25512146bytes global eNRO24105064bytes baixados,
verificados e substituídos nos mesmos dist/heroes_lore_4_* e .nro. Verificação
exige pixels dos ícones adaptativos, XML, payloads, ABIs/SDL_main/deps e JPEG
NRO exatos. Pacotes jogáveis sem código-fonte interno. Windows English-review
standalone local já atualizado com três DLLs, íconePE e saves preservados.

Próxima ação: proprietário retesta fluidez Switch no mesmo cenário, A/B,X/Y,
dimensões fullscreen/HUD e ícone Android. Coletar performance.log independente
após gameplay; não pedir novamente métricas ausentes do boot.log antigo.
S07 permanece em andamento. Resize vivo, outras telas/mapas/UI e revisão da
tradução continuam pendentes. Árvore só deve manter .vscode do usuário após
commit documental final [skip ci]. Não incluir .vscode no Git.

# Prioridade atual — funções gráficas completas, arte e Switch (2026-10-22)

Usuário confirma Switch ainda extremamente lento após ganho pequeno anterior.
Nova implementação usa7regiões de maior cobertura com duas funções completas;
14oracles R0-R15/CPSR/160MiB,38BMPs/36estados/2222flushes iguais. ALU1216estados
passaram. Amostra Windows pareada55.258s->34.503s, não mede Switch. Fonte e
relatórios ARM_FULL_FUNCTION_TESTS/ARM_REMAINING_PROFILE. Física/relógio intactos.

Novo ícone2048 do proprietário pinado e publicado no release privado de
insumos, sem binário no Git. Android adaptativo opaco sem placa branca;
Windows recursoICO inclusive build local, LinuxPNG, SwitchJPEG. BRANDING.md.
Botões A/B eX/Y trocados apenas Switch. Modo de tela Switch consulta console
em fullscreen (log físico anterior800x480). Reteste físico dos três pendente.

Usuário confirma que enviou TODO boot.log após explorar/combater/abrir menus;
nenhuma métrica estava lá. Não pedir de novo o trecho ausente. Novo arquivo
independente sdmc:/switch/heroes_lore_4/performance.log com cabeçalho imediato
e flush5s; fallback sdmc:/hl4-performance.log. Causa do stdout truncado incerta.

Windows standalone reempacotado na mesma pasta English-review, saves preservados.
Próxima ação: publicar main e acompanhar CI, corrigir erros reais, baixar APKs
ARM64/global e NRO aos mesmos caminhos. CI ANTERIOR39767050237 concluiu todos
os seis jobs success, incluindo TCI (corrige nota antiga abaixo). Não alegar
que resultado automatizado prova fluidez física. .vscode é do usuário.

# Pacotes disponíveis — cec054c / run39767050237

Quatro jobs jogáveis success: Windows/Linux/Android/Switch. Referência TCI
ainda executa o passo Verify; não alegar seis jobs verdes nesta revisão.
APKs13.30MB ARM64/20.93MB global eNRO21.91MB baixados, conteúdo/ícone/deps
verificados e substituídos nos mesmos nomes de dist/. Windows English-review
reempacotado com DLLs e saves preservados. Hashes/artifactIDs em
ANDROID_LAYOUT_PACKAGES.json. Ícones em dist/HeroesLore4-icon-original.png
(72x72 exato) e HeroesLore4-icon-256.jpg (ampliação existente paraSwitch).

Próxima ação: consultar conclusão do job Interpreter reference no run
39767050237 e registrar resultado; usuário retesta APKs/NRO, barras/HUD,
quests, mapas/combate/transições. Resize vivo continua pendente, não fazer
claim de cobertura física/total da UI. Árvore só tem .vscode do usuário além
dos relatórios/docs desta atualização; não incluir .vscode no Git.

# Suíte final: superfície real e cinco proporções (2026-10-22)

Consulta de tamanho agora usa SDL_GetRendererOutputSize antes da escala lógica
e antes do guest, cobrindo diferenças janela/superfície. Suíte final repetida
com saves temporários: Native+4:3/16:10/16:9/20:9/21:9 e oracle20:9,
23estados iguais/1355flushes cada.17patches16x16 iguais,106eventos finger e
20checks de registradores/160MiB. Core e7testes Python passaram. Reproduzir
com python tools/run_aspect_tests.py dist/windows/heroes_lore_4.exe.
Próximos testes físicos: barras/recortes/HUD e exploração/combate/transições
Android e Switch. Canvas só na abertura; resize vivo ainda pendente.

# Atualização: Auto inicial integrado (2026-10-22)

Após a prova rejeitada citada abaixo, identificado painel m_TouchDirectKeySize
0xf13f0 e touch_setUiChange0x35c14. Auto na abertura agora expande mundo,
conservando painel80 e zero oculto. Native CLI preservado.4:3/16:9/20:9,
23estados/1355flushes iguais,17patches16x16 exatos;20checks oracle160MiB.
Toque SDL finger106eventos/Skip1ms/menu/movimento passou. DISPLAY.md e
VIEWPORT_EXPANSION_TESTS.json. Ainda parcial: resize vivo e hardware pendentes.
Primeiro commit873d828 (fullscreen/quest) teve seis jobs verdes no run
39763904662. Próxima build deve incluir Auto; não entregar o protótipo scratch.

# Atualização 2026-10-22 — Android e notificações

Ícone entregue dist/HeroesLore4-icon-original.png (72x72 exato, maior fonte
original disponível) e dist/HeroesLore4-icon-256.jpg (ampliado para Switch).
Janela Android agora solicita SDL_WINDOW_FULLSCREEN: tema sozinho era
sobrescrito pelo SDLActivity. Aparelho precisa confirmar barras/HUD.
compile_locale corrige recuo e estados de344avisos EN/PTBR (306canônicos);
--verify-core demonstra aviso3->2linhas, captura antes/depois e preservação
de original/pilha. Títulos continuam com qualidade editorial variável.
Truewide NÃO concluído: prova240x534 roda mas amplia painel e não mundo;
não distribuir essa prova. Próxima ação ABI em tasks/S08_VIEWPORT_EXPANSION.md.
Relatório reference/analysis/ANDROID_LAYOUT_QUEST_TESTS.json.

# Handoff — 2026-10-21 — inglês em revisão e crash corrigido

## Prioridade atual: desempenho Switch (2026-10-22)

Usuario confirmou que NRO9263dc57 abre e roda, mas boot/jogo lentos comparados
a PC/Android. S07 em andamento. Consultado historico do chat "Identifique o
projeto Flutter" e HL5 atual/snapshot somente leitura; v1.1.6 usou AOT graficos,
desfoque nativo e menos I/O SD, com melhora confirmada pelo proprietario.

35280b7 implementa 10 regioes ARM proprias HL4, geradas do APK pinado por
specializacao do decoder puro copiado seletivamente do snapshot. Switch/TCI
ativos por padrao; JIT desktop/Android preservado. --no-fast e fallback original,
--verify-fast oracle independente de R0-R15/CPSR e memoria inteira160MiB.
Local:20comparacoes,36estados/38BMPs iguais,2222flushes/audio nao nulo; core e7tests
passaram. Amostra pareada Windows109.15s original vs75.45s nativo (1.45x), nao FPS
Switch. Perfil ponderado por bytes de blocos (~90% em5rotinas) nao e tempo real.

Switch O3 e [hl4-perf] cada10s separando entrada/guest/apresentacao, sem vetores
crescentes. Logs repetidos de tecla fora da execucao normal. NRO novo gerado,
verificado e substituido em dist/heroes_lore_4.nro. Run39715834752 terminou
com seis jobs success. Windows/Linux/TCI confirmam20checks por host,36estados e
38BMPs iguais. TCI pareado no mesmo runner:304.48s original vs82.95s acelerado,
3.67x em uma amostra sem profiler/oracle na cronometragem; NAO e FPS Switch.
Proxima acao: usuario retesta fluidez no console e envia [hl4-perf] do boot.log
se ainda houver lentidao. Codigo e pacotes enviados main; NRO na pasta habitual.
Registrar tempos TCI sem profiler/oracle no benchmark. Docs PERFORMANCE.md,
ARM_FAST_TESTS.json e tasks/S07_SWITCH_PERFORMANCE.md detalham contratos/limites.

## Atualizacao: log real do Switch e watchdog (prioridade)

Usuario enviou boot.log: SDL/RomFS/ARM/idioma/renderer/controles inicializam,
mas original boot para em PC 33705300 = guest 0x02024d54 = VA0x24d54 desRound.
Nao e falha de nova alocacao: `fail code_gen_buffer` vem de free_code_gen_buffer
na destruicao do Runtime, pois munmap antigo tentou svcSetMemoryPermission e
retornou ENOTSUP. Primeiro erro e ausencia de retorno no watchdog antigo de 5s.
9263dc57/run39693888307: TCI120s boot/30s outras chamadas; JIT5s preservado;
UC_QUERY_TIMEOUT e logs entry/PC/budget/elapsed. Optional guard TCG omitido
explicitamente apenas Switch TCI; adapter heap RW libera mappings reais sem
transicao de permissao. mprotect nao suportado continua falhando, sem sucesso
ficticio. Teste Linux do adapter real e teste core de timeout/recuperacao.
Evidencia: reference/analysis/SWITCH_BOOT_WATCHDOG.json. NRO gerado e validado,
na mesma dist/heroes_lore_4.nro. TCI passou core/timeout/recuperacao, contrato de
memoria e smoke72s: 2.222 flushes, audio nao nulo, 36 estados/38 BMPs iguais a
referencia nativa 07ac588. Windows/Linux passaram; Android em pos-build no ultimo
check. Reteste de console ainda obrigatorio; coletar novo boot.log se falhar.

Pedido adicional registrado: barras laterais no Android; ampliar cenário para
ocupar a área útil em qualquer plataforma, com HL5 como referência. Critérios e
próxima ação em tasks/S08_VIEWPORT_EXPANSION.md, ROADMAP.md e VALIDATION.md.
S08 permanece pending: planejamento, sem alteração de renderização nesta
atualização. Manter também o reteste de boot do Switch pendente.

## Trabalho ativo: ícone original e fechamento no Switch

Main privada: davidkalil10/Heroes-Lore-4-Phantasmal-Mask-Recomp.
Run 39690370966 / 81a98d3 terminou com todos os seis jobs verdes: quatro
plataformas, referência TCI e fontes separadas. Android ARM64 e global
(ARM64/ARMv7/x86_64), sem fontes dentro dos pacotes jogáveis. Proprietário
confirmou instalação e jogo no Android; não informou commit exato. Switch
anterior fecha imediatamente, portanto não está validado em hardware.

Follow-up 07ac588, run 39691502454: ícone original res/drawable/icon.png do APK
pinado extraído offline em build/, Android manifest, ICO Windows, PNG AppImage,
BMP janela e JPG 256 no NRO. Nenhum binário original incluído no Git.
Switch/TCI limitam cache de tradução a 32 MiB ANTES do primeiro uc_mem_map_ptr;
fonte do fork usa 1 GiB por padrão em host64 e nosso mmap zera essa reserva.
É causa provável do encerramento, ainda sem log do console que a confirme.
Boot log antes de SDL_Init: sdmc:/switch/heroes_lore_4/boot.log (fallback
sdmc:/hl4-boot.log); estágios de recursos/ARM/vídeo/boot, stdout e stderr.
Loop agora consulta appletMainLoop. Referência Linux TCI roda core e smoke72s
com o mesmo limite de cache, além dos testes nativos Windows/Linux.

Run 39691502454 / 07ac588 terminou com seis jobs verdes. APKs baixados e
verificados: assets, conjunto ABI, dependencias e pixels exatos do icone.
NRO: icone JPEG exato, RomFS exato e artifact apenas NRO. Windows: quatro imagens
do ICO dentro do PE, nenhum arquivo de fontes no ZIP. Windows/Linux/TCI32:
2.222 flushes, audio nao nulo; 36 estados e 38 BMPs iguais entre os tres motores.
Nao prova boot/desempenho de console. Proxima acao: proprietario retesta NRO em
title override/full RAM; se fechar, analisar boot.log. Manter S12 in_progress.
Arquivos locais estaveis: dist/heroes_lore_4.nro e os dois APKs em dist/;
Windows continua na mesma pasta English-review, saves preservados.

Acompanhar APIs com tools/github_ci.py: logs/artifact zip usam binary=True,
accept='application/vnd.github+json'. Rede/Git exigem execucao elevada. Nao
imprimir credenciais. EN/PTBR ainda tem ampla revisao editorial pendente.

APK e catálogo autorizados em release PRIVADO ci-inputs, hashes em ci-inputs.json.
Nenhum save/chave local enviado. Chave Android de teste gerada/cacheada no CI.
Snapshot/HL5 consultados somente leitura. Próximas builds locais reutilizam
sempre dist/HeroesLore4-Windows-English-review, preservando saves. Não sobrescrever
EXE se o proprietário estiver jogando. .vscode/ é configuração local, não commitar.

---

Pedido: EN/PTBR, inglês padrão e crash ao terminar cutscene. Usuário confirmou
crash corrigido, mas relatou coreano restante, tradução ruim e menus sobrepostos.

Build: dist/HeroesLore4-Windows-English-review.zip, pasta homônima; dist/windows
atualizada. DLLs + hl4.lang; ZIP sem saves. Copiar saves da build anterior para
continuar. Originais pessoais não alterados. Inglês padrão, --language ko para
original. PTBR preenchido, ainda não selecionável até integrar glifos acentuados.

Leia LOCALIZATION_RUNTIME.md e reference/analysis/LOCALIZATION_TESTS.json.
6.935 registros EN/PTBR preenchidos offline; 334 corrigidos editorialmente:
duas primeiras cenas, tutoriais, menus/opções/classes, equipamento e avisos.
A maioria ainda é rascunho automático com erros. Recursos não parseados e
imagens podem continuar coreanos. Nomes/termos são provisórios.

src/localized_text.inc adapta desenho e medição ARM originais, com guards HL4.
Quinto argumento de desenho é fonte, não altura. Restaura pilha nos retornos
verificados, instalados antes de executar (hooks tardios falharam em TB cache).
Core testa largura traduzida vs inglês direto, desenho, R4-R11, pilha e bytes
canônicos. Paginação/offsets parciais ainda proporcionais; não afirmar layout
completo. GetLineCount/GetByteLenPrintable requerem investigação e adaptação.

Crash: dlib_memcpy(src=0,n=0), retorno 0x5da30 em quest status; corrigido contrato
vazio sem relaxar ptr. Teste não vazio nulo rejeitado. 105 s com cópia de save:
3.358 quadros, 4.088 desenhos/restaurações, zero misses nesse percurso; 52
amostras de trajetória/callback idênticas ao coreano, áudio não nulo. Menus/status
abertos por pointer e capturas conferidas. Um composto Human/Warrior acrescentado
após esses runs; core standalone repetido com pack final. Controle físico pendente.

Próximas ações:
1. Rever cenas após e0001 e menus NPC/lojas/ajuda. Fonte editorial em
   assets/locales/review_*.tsv, aplicada por tools/apply_locale_review.py. Não
   confiar no NLLB: Tyr virou frase, holy war virou temple nos rascunhos.
2. Parser com roundtrip para quests/itens/nomes dinâmicos. Misses do smoke em
   localization-missing.tsv são hex CP949; extras em runtime_extra.tsv.
3. Confirmar ABI da paginação/contagem e mapear offsets por palavras e tags;
   conferir caixas por capturas. Não modificar scripts/saves para traduzir.
4. Integrar glifos PTBR e seleção de idioma; repetir capturas dos dois idiomas.

Rebuild: cmd /c tools\build_windows.cmd. Catálogo ignorado em
scratch/localization/catalog.ko-en.json. inspect_text preserva traduções só
quando ID/hash/original coincidem. Inferência foi inteiramente local; não enviar
corpus a serviços. Modelos, cache, saves e capturas locais não entram no Git.

Repro: copiar scratch/cutscene-repro-original/kickass para nova pasta em scratch;
executar --smoke 105 --deterministic --script tests/cutscene_save_repro.txt
--save-dir <cópia> --capture-dir <nova pasta>. Nunca sobrescrever saves em dist.
Pointer menu/status: tests/locale_menu_keys.txt + tests/locale_menu_pointer.txt.

Passaram verify_workspace, inspect_apk --check, sete regressões, core nativo e
core standalone em PATH System32/cwd separado. Evidências antigas de input e
runtime pertencem às versões anteriores e ficam preservadas. S05/S10 parciais.
Sem commit/remoto/publicação. Não declarar campanha traduzida nem gameplay pleno.

CI final39693888307: seis jobs concluidos com sucesso. Novo NRO ainda
aguarda validacao fisica no Switch; manter S12 in_progress.
