# Revisão de combate e plataformas móveis — 2026-10-22

Usuário confirma PC fluido; Android e Switch perdem velocidade nas batalhas.
A revisão anterior não resolveu a fluidez e não criou performance.log no
aparelho. S07 continua em andamento. Evidências de host não são FPS físicos.

Perfil reproduzido com cópia descartável do save: 110 s simulados, 3525flushes,
55estados. Com sete regiões anteriores ligadas, os bytes ARM ainda interpretados
se concentram em DM_DrawRotate565Ex28.44%, CHAR_checkCollision9.34%,
DM_FillRectBlendingRGB7.60%, SPR_drawSpriteNPC4.92%, HF_DrawTextAnchorH2Color4.81%.
É volume executado, não tempo. Perfil durante combate difere do percurso cidade.

Agora dez regiões: as sete anteriores + UM_Collision_Two_Rect VA0xc901c,
ramo de rotação/mistura VA0x4a7c4 e preenchimento RGB565 VA0x524c0. CFG próprios,
sem BL/retorno LDM-PC nos trechos nativos, guardas de todos os opcodes e fallback.
Android passa a usar as acelerações por padrão; desktop mantém padrão JIT.

Unicorn2.1.4 cria/junta uma thread a cada uc_emu_start com timeout e faz polling
TIMEOUT_STEP=2us (scratch/unicorn-source/uc.c1012–1040). O host agora mantém uma
thread SDL por engine, dormindo em condição até o deadline. Mesmos budgets
5s desktop,120s boot/30s gameplay TCI; não altera relógio/física/temporizadores
do jogo. Parada serializada com arm/disarm; após expirar continua enviando stop
até disarm, cobrindo suspensão entre arm e entrada. --legacy-watchdog preserva
o mecanismo anterior para comparação. Oracle usa Unicorn independente.

Core verifica três loops infinitos limitados20ms, incluindo suspensão30ms antes
de emu_start, e retorno imediato na chamada seguinte. Core --verify-fast também
cria sprite indexado16x16 e overlay de dano: quatro comparações integrais das
regiões8/9, sem save pessoal. O smoke normal cobre regiões0–7; tool agrega os
dois percursos. ALU46formas/1472estados compara emissão compilada com Unicorn.

Uma amostra Windows isolando somente o watchdog (mesmo EXE, ambas --fast):
35.140s legado,33.890s persistente,1.037x;38BMPs/36estados iguais. Ganho pequeno
nesse host, sem extrapolar para Android/Switch. Uma tentativa anterior com
outros testes concorrentes foi descartada como benchmark. Não comparar esses
números com runnerCI nem chamar de fluidez física resolvida.

Tentativas de navegar do início até a batalha pararam no tutorial de Graham;
não foram tratadas como combate. Verificação local real usa cópias do save
explicitamente fornecido a tools/verify_combat_replay.py. Save nunca vai ao Git
ou insumosCI. CI usa os kernels sintéticos, mais percurso cidade e oracle.

Log Switch v3 abre o cartão diretamente com fsOpenSdCardFileSystem e escreve
FsFile com FsWriteOption_Flush; independe de stdio/devoptab/dup2. Cabeçalho e
etapas do boot são gravados antes do SDL, métricas seguem a cada5s. Caminho
/switch/heroes_lore_4/performance.log; falha de abertura exibe códigoFS no
alerta SDL. Causa da falha anterior permanece incerta até reteste físico.
API: https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/fs.h

---
Histórico das revisões anteriores (resultados não equivalem à revisão atual):

# HL4 — investigação de desempenho no Switch

O proprietário confirmou boot/jogo no Switch após 9263dc57, mas relatou abertura
demorada e lentidão geral comparada ao PC/Android. Isso confirma o boot, não
fluidez, combate completo ou persistência. S07 está em andamento.

## Referência e medição

Consultado o chat `Identifique o projeto Flutter`, inclusive o relato de menu
levando 4–5 s no HL5 e a confirmação posterior de melhora. Código/documentação
HL5 v1.1.6 foram lidos sem executar scripts ou alterar o projeto. Soluções:
blocos ARM compilados, desfoque nativo guardado, menos I/O na SD e métricas por
fase. Nenhum endereço/offset ou hook de jogo do HL5 foi transplantado.

O novo --profile-arm coleta um histograma de blocos em arrays fixos, sem I/O,
alocações ou consultas de ambiente por instrução. É diagnóstico opt-in de PC;
desativá-lo ao medir desempenho. O peso conta bytes de blocos executados,
não tempo de CPU. O percurso original de 72 s simulados atribuiu aproximadamente
90% desse volume a DM_DrawScreen565Ex/DM_DrawRotate565/DM_DrawRotate565Ex/
DM_DrawScreen565/DM_CopyFrameBufRotate. DES também pesa na abertura.
Relatório reproduzível: tools/profile_arm_report.py e ARM_PROFILE.json.

## Aceleração e contratos

Dez regiões HL4 geradas por tools/generate_arm_hot.py a partir dos opcodes do
APK pinado, especializando src/arm_hot_decoder.inc. O decoder puro foi adaptado
seletivamente do snapshot HL5 src/core/cpu.cpp: condições, shifts, aritmética e
transferências ARM. Sem camada de hooks, syscalls ou endereços HL5. A parte SWI
que devolvia sucesso no outro projeto foi excluída. Contratos desconhecidos
continuam falhando explicitamente. Procedência do snapshot em HL5_SNAPSHOT.json.

Regiões: rotação/cópia RGB565, sprites com paleta/transparência/mistura, retângulo
alpha e rodadas DES. O gerador rejeita chamadas, coprocessadores e retorno por
LDM-PC; epílogos permanecem originais. Confere todos os opcodes antes de instalar
hooks. Em divergência, mantém execução original. Limite finito de instruções por
entrada, verificações de memória e preservação de R0–R15/CPSR. Operações usam
a mesma memória convidada e a mesma ordem do decoder, inclusive sobreposição.
Nenhuma mudança na física, relógio, viewport, assets ou clocks do aparelho.

Padrão: ativo em Switch/TCI; PC/Android JIT mantêm caminho anterior. --fast força
o caminho nativo para diagnóstico no PC, --no-fast mantém o oracle original.
Não expor opções de diagnóstico como substituto de validação do produto.

Na ponte Unicorn, escrever PC com uc_reg_write individual depois dos demais
registradores. Na versão pinada, o batch não ativa skip_sync_pc_on_exit; usá-lo
para redirecionar dentro de hook repetia a entrada com contadores já avançados.
Outra falha de desenvolvimento: incluir epílogo LDM-PC no intervalo gerado;
agora esse retorno fica fora e o gerador o rejeita. Ambos detectados antes de
publicação pelos testes, sem aceitar resultado incorreto.

## Verificação e desempenho

--verify-fast compara duas entradas reais de cada região com outro motor Unicorn
sem hooks: todos os 17 registradores R0–R15/CPSR e os 167.772.160 bytes da memória
convidada. Oracle preserva execução original; comparação pesada é só diagnóstico
de PC/CI, nunca padrão no Switch. tools/verify_fast_runtime.py compara também
execuções inteiras com/sem aceleração: estados, quadros, BMPs e áudio não nulo.

Com --benchmark, a ferramenta registra uma comparação pareada no mesmo host,
sem profiler nem oracle no trecho cronometrado. Inclui boot/capturas e registra
uma amostra; não equivale a FPS no console nem a benchmark estatístico final.
Medir novamente no Switch com o mesmo mapa, movimento, ataque e abertura do menu.

Switch usa O3 para os blocos nativos e registra uma linha [hl4-perf] por 5 s:
FPS do laço, flushes do jogo/s, entrada, guest e apresentação/espera, além de
instruções aceleradas. Acumuladores fixos, sem vetor crescente. Logs de eventos
recorrentes de controle foram retirados da execução normal; erros/boot continuam.
Enviar boot.log após explorar/mover/atacar/abrir menu para comparar o aparelho.

## Reprodução local

python tools/generate_arm_hot.py --check
python tools/verify_fast_runtime.py dist/windows/heroes_lore_4.exe build/perf/accepted --benchmark
python tools/profile_arm_report.py build/perf/blocks.csv --output reference/analysis/ARM_PROFILE.json

Usar somente saves temporários. Builds futuras permanecem nas mesmas pastas.


## Resultado de CI em 2026-10-22

Commit35280b7, run39715834752: seis jobs concluidos com sucesso. Windows/Linux/TCI
confirmaram20comparacoes de registradores/memoria cada,36estados e38BMPs iguais,
2222flushes e audio nao nulo. NRO21911934bytes, icone/RomFS verificados e pacote
apenasNRO. Arquivo local fixo: dist/heroes_lore_4.nro.

Uma amostra pareada no mesmo runner Linux com backend TCI:304.48s original,
82.95s acelerado, razao3.67x. Sem profiler nem oracle na cronometragem, incluindo
boot/capturas. Nao corresponde a FPS medidos no Switch. Amostra Windows JIT
separada109.15s/75.45s; nao comparar tempos absolutos entre hosts. Dados completos
em ARM_FAST_TESTS.json. Reteste fisico de fluidez continua pendente.

## Revisão: funções completas e especialização direta (2026-10-22)

O proprietário confirmou melhora pequena, mas lentidão ainda extrema. Perfil
com as dez regiões anteriores ativas atribui 40.107% e27.526% dos bytes ainda
interpretados a DM_DrawScreen565Ex/DM_DrawScreen565 (não são percentuais de
tempo). ARM_REMAINING_PROFILE.json registra o escopo correto.

Agora essas duas funções são geradas a partir do CFG alcançável das entradas
HL4 VA0x3fdec e0x3de54. Epílogos LDM-PC0x3fe94/0x3df90 permanecem originais.
Pools de literais não são instruções. Páginas nativas de1024bytes limitam a
complexidade do compilador, sem sincronizar Unicorn a cada linha de sprite.
ALU/flags e transferências comuns são emitidas diretamente por arm_emit.py,
mesma ordem do decoder; casos incomuns mantêm template/fallback. PC relativo
usa endereço original; contagem agrupada por bloco, limite nos saltos para trás.

Sete regiões: cópia, duas rotações, duas funções completas de desenho, alpha e
DES. Menos regiões representa maior cobertura. Guardas conferem cada opcode;
sem relógio/física alterados nem redução de qualidade. Primeira tentativa de
expandir templates genéricos nas funções inteiras excedeu recursos do
compilador local e foi abortada antes de publicação; páginas e emissão direta
resolveram a compilação. Teste independente compara38formas ALU/1216estados
com Unicorn, incluindo flags e extremos; esse teste isolado não cobre memória.
Oracle integrado compara memória inteira e registradores nas entradas reais.

Amostra pareada Windows, ambas revisões --fast sem profiler/oracle:
55.258s anterior contra34.503s candidata (1.602x). Mesmos38BMPs/36estados e
2222flushes, áudio real não nulo. Não é medida de FPS ou ganho no Switch.
A build local usa JIT no trecho restante, enquanto o console usa TCI.

Log físico revelou output800x480, mesmo após gameplay. Switch passa a pedir
SDL_GetCurrentDisplayMode em fullscreen, fallback1280x720. Auto usa a superfície
resultante. Também troca A/B eX/Y na tabela SDL apenas Switch; físicoA deve
atacar/confirmar. Confirmar dimensões/HUD/mapeamento e fluidez no hardware.

## CI da revisão completa

4c9d405 / run39774383459: seis jobs success. Windows/Linux/TCI confirmam
14checks integrais,38BMPs/36estados iguais,2222flushes e áudio não nulo.
Probe ALU passa em Windows/Linux. Uma amostra pareadaTCI no mesmo runner:
208.524s original sem aceleração contra32.729s acelerado (6.371x). Não é FPS
Switch nem comparação direta com a amostra histórica de outro runner.
ICON_FULL_AOT_PACKAGES.json contém relatórios/artefatos/hashes. NRO atualizado
no caminho fixo, verificando arte nova e dados. S07 ainda requer fluidez física.
