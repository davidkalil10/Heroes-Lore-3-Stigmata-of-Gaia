# Contrato WIPI/ARM do HL4 — S02 em andamento

Evidência de 2026-10-21. **Não existe runtime jogável, boot ou entrada de host
implementados.** Os testes abaixo executam funções ARM isoladas em Unicorn,
com fixtures explícitas para serviços externos. Não equivalem a rodar o jogo.

## Reprodução e procedência

Inputs: APK fixado em project.json, ELF SHA-256
`c31276b0d6578c8d5b1eb77572f066994718829d9660117a966852c920963d27`,
DEX `23115392cb1e627898b33d8546248edee3314d05956e1b53e47fff100f37a246`.
Ferramentas novas deste projeto; nenhum script do snapshot foi executado.

```powershell
python -m pip install --target scratch/analysis_deps -r tools/requirements-analysis.txt
python tools/inspect_runtime.py
python tools/probe_abi.py
python tools/inspect_runtime.py --check
python tools/probe_abi.py --check
python -m unittest discover -s tests -v
```

Capstone pacote 5.0.9 (binding declara 5.0.7), pyelftools 0.33,
Androguard 4.1.4 e Unicorn 2.1.4. Dependências ficam em scratch, ignorado.
Os verificadores originais continuam usando apenas Python padrão.

Relatórios portáveis: [inventário](../reference/analysis/runtime_inventory.json),
[DEX decodificado](../reference/analysis/dex_contract.json),
[instruções de ABI](../reference/analysis/abi_disassembly.txt),
[provas ARM](../reference/analysis/ABI_PROBES.json),
[relocations](../reference/analysis/runtime_relocations.csv),
[imports](../reference/analysis/runtime_imports.csv) e
[headers de recursos](../reference/analysis/resource_headers.csv).
Desassembly ARM completo é regenerado em reference/disassembly/hl4_arm.txt,
ignorado pelo Git. Os endereços neste documento são **virtuais, sem rebasing**.

## Wrapper Android: fatos do DEX

Activity: `android.lgt.wipi.App00035956.App00035956`, conforme manifesto.
`onCreate` chama `runService` porque o identificador ARM `A900102715` não é vazio.
`runService`, instrução +0x32, faz bind da action `com.lgt.arm.ArmInterface`
com flag 1. `onServiceConnected` +0x30/+0x34 passa aquele identificador para
`executeArm(String): int`; não passa caminho do ELF nem do JAR.
O Proxy usa Binder transaction 1, token `com.lgt.arm.ArmInterface`, string e
retorno inteiro. Esse serviço **não está implementado neste APK**.

Com retorno 1, chama `runWipiPlayer`. Esse método abre raw resource 0x7f040000,
escreve `00035956.jar` no diretório privado histórico do APK e lança action
`android.intent.action.LAUNCH_WIPI`, categoria DEFAULT, flag 0x10000000,
extra `AID_PATH="00035956.jar"` (+0x98 a +0xa6). O raw resource é o JAR
identificado no inventário. Esses IDs/caminhos pertencem ao original.
O player externo também não é fornecido. Portanto, copiar o wrapper/JNI
do HL5 não fornece um bootstrap de PC. Não foi executado Binder ou Android.

## Loader e memória

ELF32 little-endian, ET_DYN, ARM, flags 0x04000002 (EABI4).

| Segmento | Offset no arquivo | VA inicial | Bytes de arquivo | Bytes em memória | Fim exclusivo |
| --- | --- | --- | --- | --- | --- |
| LOAD RX | 0x0 | 0x0 | 0xde62c | 0xde62c | 0xde62c |
| LOAD RW | 0xde62c | 0xdf62c | 0x2a0c | 0x6eb74 | 0x14e1a0 |

O segundo LOAD tem **VA diferente do offset**, por 0x1000. Não indexar o arquivo
diretamente por VA para dados/GOT/BSS. BSS: [0xe2038,0x14e1a0), 442.728 bytes;
deve iniciar zerada. O intervalo entre segmentos não é conteúdo carregado.
PT_DYNAMIC está em VA 0xe00d0, offset 0xdf0d0. EXIDX existe; não executá-lo.

1.689 R_ARM_RELATIVE (23): escrever `B + addend` de 32 bits no destino.
1 R_ARM_GLOB_DAT (21): VA 0xe0774, `MC_netSocketWrite`.
105 R_ARM_JUMP_SLOT (22): resolver símbolo e gravar endereço guest/trap.
São tabelas REL; o addend vem da memória carregada. DT_TEXTREL está presente:
aplicar relocations antes da proteção final das páginas RX. Não há DT_INIT,
DT_INIT_ARRAY ou DT_FINI nesta tabela dinâmica. `e_entry=init=0x220c4` é uma
entrada WIPI, não uma main de processo. Não inferir construtores do HL5.

As 105 entradas PLT foram verificadas pelas três instruções de cada slot e
pelo endereço GOT resultante, contra a respectiva relocation. Imports não
resolvidos devem gerar traps identificadas; não chamar as bibliotecas host
pelos nomes DT_NEEDED. A lista inclui liblgt_system, dvm e bibliotecas Android.

As provas carregam/relocalizam em bases **de teste** 0x02000000 e 0x02800000,
verificam os bytes de ambos LOAD e BSS, e executam `_init_module`/`init`.
Essas bases não definem o layout do runtime final.

ARM.attributes bruto: `4118000000616561626900010e000000053554450006040801`.
Contém CPU `5TE`, Tag_CPU_arch=4 e Tag_ARM_ISA_use=1. Nenhum símbolo FUNC
tem bit Thumb; há 6 mapping symbols $a, 9 $d e nenhum $t. Isso favorece ARM
para o primeiro caminho; não prova ausência de Thumb em chamadas indiretas.
Capstone encontra BLX register em android_c_trampoline. O decoder deverá
preservar interworking ou falhar explicitamente. Chamadas __aeabi e o uso de
pares de registradores sustentam ABI com passagem inteira/soft-float; o ABI
numérico completo ainda exige testes de floats, flags e retornos de 64 bits.

## Entrada e tabelas: observado em execução isolada

`_init_module` 0x22134 recebe r0, r1, r2. Em 0x22158,
bytes `145280e5` (`str r5,[r0,#0x214]`), instala `_export_table` em
`[r0+0x214]`. Em 0x22164/68/6c, grava os argumentos em:

| Argumento | Global | VA |
| --- | --- | --- |
| r0 | _module_ptr | 0xe2048 |
| r1 | _module_intf_ptr | 0xe204c |
| r2 | _dylus_log_mask_ptr_ | 0xe2050 |

O objeto em r0 precisa admitir pelo menos a escrita até +0x217. Semântica do
restante desse objeto e da interface em r1 ainda aberta. A prova confirma as
escritas em ambas as bases, sem implementar esses serviços. `_export_table`
tem 24 bytes em 0xe0c04: word 3, ponteiro para init, ponteiro para string
`init` e três zeros. O significado do word 3 permanece aberto.

`init` faz três chamadas `dlet_set_property`: (0,100,*dummy_global_variable2,0),
(0,100,240,0), (0,101,400,0). O primeiro valor é zero com BSS limpa.
Os dois últimos sugerem dimensões 240×400; significado de properties e ordem
de consumo são inferências. Em 0x22124, bytes `6effffea`, tail-call para
`wipic_event_handler`, r0=`cfunc`, r1=0. r2/r3 remanescentes não constituem
argumentos comprovados. As fixtures de property retornam zero e a prova
**para antes do event handler**, sem fingir que ele foi implementado.

`cfunc` tem 24 bytes em 0xe0bec; seis ponteiros relocados, verificados:

| Slot | Função | VA |
| --- | --- | --- |
| 0 | startClet | 0x31f34 |
| 1 | pauseClet | 0x268a0 |
| 2 | resumeClet | 0x26718 |
| 3 | destroyClet | 0x268e8 |
| 4 | paintClet | 0x266bc |
| 5 | handleCletEvent | 0x32bb4 |

Leitura estática: startClet → backlight/mute → CUSTOM_onStartClet →
clsidAuth → auth_startApp → gamerating_close → onStartApp. O serviço externo
é quem consumiria cfunc; sua ordem completa de callbacks não foi observada.
CUSTOM_onStartClet consulta PHONEMODEL e TIMEZONE e contém um caminho
específico de LG-LU9400. Não inventar properties só para escolher um ramo.

onStartApp 0x336d8 chama SaveDataInit, FR_Load, Item_dataPreLoad,
MP_Initialize, nFPS_Initialize, DM_Initialize, SM_Initialize, HF_Initialize,
DM_initLCDEffect, loadCommonMenuImage, Touch_Initialize, touch_frameTouchInit,
touch_setTouchGroup(2), UM_SetMenuXY, MainFrame_Initialize e timers.
Portanto arquivos/áudio aparecem já no caminho de inicialização; prioridade
P1 no catálogo não autoriza ignorá-los quando atingidos.

`paintClet`: apenas `1eff2fe1`/`bx lr`; prova executada sem imports.
A pintura relevante está no timerCB, não nesse callback vazio.

## Eventos e relógio

`handleCletEvent(r0=evento,r1=subtipo,r2=dados)` traduz evento 0x708:
subtipos 0/1/2 geram 0x5064/0x5065/0x5066 com dois words lidos de dados
para r1/r2 e r3=-1. As três provas param em clsidAuthHandle. Interpretação
como toque pressionado/solto/movido é sustentada pelos consumidores, mas
nenhum toque de host foi enviado. Outros eventos seguem pelo dispatcher.

`onHandleEvent(502,code,...)` e `(503,code,...)` gravam o código pendente em
mainFrame+0x30 e +0x34 no ramo normal. Provas executadas com code=0x32 e
LoadFrame_isLoading retornando zero. O significado das teclas numéricas e
dos demais códigos ainda deve ser mapeado pelo HL4; não importar tabela HL5.
MainFrame_KeyPressed/Released consomem esses campos durante timerCB.

MyTimer_setTimer 0xa7938 usa `_mTimer+0x18`, registra timerCB e solicita delay
50 por CUSTOM_extSetTimer. Esse wrapper passa `(timer,0,delay_low,delay_high)`
e quinto argumento em `[sp]` para MC_knlSetTimer no ramo normal. A prova usa
delay 50 e quinto argumento sentinela; para antes da API. O alinhamento do
par de 64 bits r2:r3 é evidência concreta contra tratar todos os argumentos
como quatro inteiros independentes. Unidade/epoch do tempo ainda abertas;
milissegundos são hipótese. MC_knlCurrentTime é consumido como r0:r1.

timerCB 0xa7994 chama KeyPressed, KeyReleased, TouchNotify, Process, Render,
EndProcess, áudio, flush e MyTimer_setTimerAfterPaint. O último rearma o
timer com o período de 64 bits. Preservar essa cadência e medir antes de
introduzir loop de host; não executar paintClet a 60 Hz presumindo equivalência.

## Recursos e memória

JAR: 1.935 entradas = 41 diretórios, binary.mod e 1.893 outros arquivos.
Todos esses arquivos usam ZIP Deflate. Nenhum ID numérico persistente foi
descoberto: host deverá criar handles próprios ligados a nomes exatos.

CUSTOM_knlGetResourceID → tempLocalGetExactFilename tira **uma** barra inicial
`/` ou `\` e formata `res/%s` antes de MC_knlGetResourceID(r0=nome,r1=size_out).
Quatro provas confirmam o prefixo e preservação de case com fixtures de
memória/formatação. Não tornar a busca case-insensitive nem usar caminhos de host.

UM_LoadRes 0xca200 usa o tamanho retornado, MC_knlCalloc(size), depois
MC_knlGetResource(id,buffer,size). O chamador é dono da alocação. Trata lookup
-12 como falha e read -18 com free e retorno nulo/tamanho zero. Há três
provas dessas ramificações com lookup/leitura/alocação simulados. Isso prova
o consumidor, não todos os códigos de erro do SDK. GetLoadResBinary tem
tratamento diferente e não deve ser substituído por comportamento inventado.

43 arquivos têm assinatura OggS; 42 começam com MMMD (contêiner SMAF).
Restantes 1.808 ainda não classificados; `_BM` não é PNG por extensão/nome.
Exemplo CM/GRADE_BM começa `010000003b0800000b3c0045003b000e`.
Investigar os decoders do próprio jogo antes de substituir imagem proprietária.
Nenhum texto foi traduzido/enviado a serviços externos; nenhum áudio foi tocado.

binary.mod é o ELF completo mais trailer de 80 bytes, offset 1.034.442.
Trailer começa `RAPT`, contém ASCII `00035956`, `kernel dlet wipic` e `VL`.
Hex e 20 words estão no inventário. A word após RAPT e campos numéricos não
têm semântica comprovada. O harness de ABI lê a biblioteca, preservando o
módulo/trailer; isso não decide que o player original ignora o trailer.

## Framebuffer: contrato parcial comprovado

DM_GET_FRAME_BUFFER_INFO 0x50500 recebe r0=descritor de saída e r1=handle.
Consulta wipic_get_frame_width/height/pointer(handle). A prova executa
instruções originais com fixtures 240×400 e 320×240 e confirma:

| Offset | Escrita |
| --- | --- |
| +0x00 | width × 2 (stride em bytes) |
| +0x04 | word 2 |
| +0x08 | width |
| +0x0c | height |
| +0x10 | ponteiro guest de pixels |
| +0x14 | byte 2; bytes seguintes permanecem intactos |

DM_Initialize recebe handle de MC_grpGetScreenFrameBuffer(0), inicializa
contexto e cria offscreens. Máscaras 0xf800/0x07e0/0x001f no código sustentam
RGB565 no caminho investigado. Formato completo dos contextos, semântica dos
valores 2 e ownership dos handles precisam de investigação. Flush observado
em __knlGR_draw: `(0,handle,x,y,width,height)`, últimos argumentos na stack.
Não foi criado framebuffer real nem produzido um quadro do jogo.

## Fronteiras HLE e reaproveitamento

Catálogo cobre todos os 108 imports: 22 aeabi, 19 gráficos, 16 kernel,
13 libc/dlib, 10 rede, 10 áudio, 7 arquivos, 7 plataforma/legado e 4 bridge.
15 têm contrato parcial de consumidor; 93 seguem sem assinatura verificada.
Prioridades são propostas de investigação: P0 CPU/memória/recursos/gráficos,
P1 áudio/arquivos, P2 legado/rede. Reachability pode promover qualquer import.
Nenhum contrato completo de SDK ou stub de sucesso foi implementado.
Unwind pode ser metadata-only no caminho inicial; não generalizar para C++.

Snapshot HL5 consultado: guest.*, elf_loader.* e cpu.cpp. Layout ELF32/REL e
aritmética ARM são referências úteis, com nova validação de bounds, segmentos,
símbolos e falhas. Remover integralmente isHook/switch por endereços,
localização/trans, viewport/menu/settings, fastVfsLookup, menu_blur,
hot_blocks, profiling e syscalls com retorno zero. Não carregar suas constantes
de memória, JNI/GLES, IDs ou saves. O decoder atual do snapshot rejeita Thumb:
isso permanece lacuna se o HL4 atingir esse estado. Licenças continuam aplicáveis.

## Menor harness PC proposto e pendências de aceite

O probe Python é instrumento de pesquisa, não o runtime S03. O próximo núcleo
deve carregar o ELF com bounds/REL/BSS verificados, disponibilizar estado ARM
completo, traps e limite de instruções, reproduzir os probes como testes e
executar a cadeia original com log de primeiro contrato desconhecido.
Pode usar o oráculo Unicorn para comparação; não copiar hooks do HL5.

**S02 permanece em andamento**: faltam o objeto de módulo/interface consumido
pelo trampoline, contrato de scheduler/callbacks de wipic_event_handler,
contextos gráficos/handles e respostas de properties/recursos/arquivos exigidas
na cadeia de start. Ainda não é possível afirmar um contrato mínimo suficiente
para o boot. Próxima investigação: trace limitado de startClet até onStartApp
com cada fixture explícita, mapear MC_grpInitContext/SetContext e atingir
DM_Initialize sem sucesso fictício. Só então reavaliar S02 e iniciar S03/S04.

## Runtime integrado e correções posteriores — 2026-10-21

As tabelas/catálogo acima descrevem o inventário S02 e seus probes isolados.
O host integrado está em src/main.cpp; contratos efetivamente exercitados e
limites em [WINDOWS_RUNTIME.md](WINDOWS_RUNTIME.md). Este documento não deve ser
usado para afirmar que ainda não existe runtime Windows.

Correção: MC_knlSetTimer tem delay uint64 alinhado em r2:r3; r1 é padding AAPCS,
não modo. Quinto argumento [sp] é userdata e callback recebe (timer,userdata).
Também verificados media '-file ', fsOpen 1/4/8 e Printk já formatado.
Rede indisponível usa falha assíncrona; -19 era interpretado incorretamente pelo
wrapper original como pronto. Não houve pedido externo nem sucesso de rede simulado.
