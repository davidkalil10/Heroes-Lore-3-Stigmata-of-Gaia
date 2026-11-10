# Tela e expansão do mundo — 2026-10-22

Auto é padrão na abertura. O host cria a janela antes do guest, consulta sua
área real com SDL_GetRendererOutputSize (antes da escala lógica) e amplia uma dimensão física do framebuffer: base240x400, landscape
400x240; 16:9 usa240x428, 20:9 usa240x534 e4:3 usa300x400. Dimensões pares,
limite1280 por dimensão. Escala SDL uniforme, sem deformar sprites. Arredondar
para pixels inteiros pode deixar até cerca de2pixels lógicos de diferença na
proporção. Limite1280 pode manter margens em proporções extremas.

`--aspect native` preserva240x400; `--aspect auto` é o padrão. `--window-size W H`
define a janela desktop/teste. Neste primeiro incremento o canvas é escolhido
na abertura: redimensionar depois mantém o canvas inicial e escala uniforme.
Reabrir na dimensão desejada para recalculá-lo. Resize vivo e seleção em menu
continuam pendentes. Android permanece horizontal e solicita FULLSCREEN SDL;
a SDLActivity controla barras, além do tema no Manifest. Reteste físico de
recortes, gestos, pausa/retorno e HUD ainda necessário.

## ABI própria do HL4

APK/ELF pinados em project.json; VAs antes de BASE0x02000000, ARM/AAPCS.
DM_Initialize0x56000 lê dimensões do screen: `_lcdG`0xedbe4 +0x64/+0x68
são dimensões físicas; +0x54/+0x58 são viewport lógico. Touch alterna/swap
conforme nTouchRotate e aloca dois buffers quadrados de lado altura física.
O host não modifica opcodes nem substitui desenho/culling do mapa.

`touch_setUiChange`0x35c14, r0=modo,r1=realocar, usa a largura do painel
`m_TouchDirectKeySize`0xf13f0. Em0x35d68 lê altura física;+0x35d70 subtrai
painel e0x35d78 escreve largura lógica; altura lógica recebe largura física.
A inicialização original atribui a sobra ao painel e mantém mundo320px.
Prova só com framebuffer maior revelou isso e foi rejeitada. Hook guardado
em0x35c14 (e92d45f0,e59f5468) conserva painel80px nas telas ampliadas antes
desse cálculo. Zero permanece zero: painel oculto não é ligado à força.
Não muda registradores/retorno/relógio/física/saves. Modo Native não instala hook.

O mapa original usa o viewport para câmera, culling e caches. Campos `_map`
0x1013b8 +0x84/+0x88 dão dimensões autorais; +0x9c/+0xa0 câmera, confirmados
pelos acessores Map_getScreenWidth/Height e touch_setUiChange0x36000..36074.
Cidade de teste400x640: viewport454x240 excede largura autoral400. Margens
nesse mapa pequeno são limite autoral; não inventar cenário para preenchê-las.
Mapas maiores e transições ainda precisam de cobertura física.

Textura e transformação inversa de mouse/toque usam dimensões reais do guest;
eventos SDL já lógicos são transformados só uma vez. Não altera blocos ARM
acelerados: leem os parâmetros dinâmicos e continuam sob guards/oracle.

## Evidência e reprodução

`python tools/run_aspect_tests.py dist/windows/heroes_lore_4.exe` gera capturas
com saves temporários isolados e chama verify_aspect_captures. Testes locais46s:
Native,4:3,16:10,16:9,20:9,21:9 e20:9
com oracle; todos1355flushes/23estados de gameplay idênticos e áudio não nulo.
17 recortes distintos16x16 têm bytes exatos sob translação rígida; amostra
adicional de cenário também verificada. Oracle20checks de R0-R15/CPSR e160MiB,
com imagens idênticas ao acelerado normal no mesmo canvas.

Toque SDL finger80s:106eventos, Skip1ms, ação, direções, menu/retorno,
releases balanceados e2488flushes. Fixture tests/pointer_gameplay_wide20.txt
move só posições de botões da direita134px, conservando roteiro e temporização.
Não é teste de tela tátil física. Capturas scratch/aspect-*/ e relatório
reference/analysis/VIEWPORT_EXPANSION_TESTS.json. Android/Switch e restante da
matriz (mapas/combate/transições/HUD/extremos) ainda em validação.
