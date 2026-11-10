# S08 — aproveitar a tela com expansão real do cenário

## Pedido e estado atual

Em 2026-10-21, o proprietário relatou barras pretas laterais no Android e enviou
captura do título em horizontal. Pediu registrar o aproveitamento de 100% da área
útil em qualquer plataforma, mostrando mais conteúdo conforme a proporção da
tela, com a experiência do HL5 como referência. A captura também mostra barras
do sistema: verificar fullscreen separadamente do tamanho do framebuffer.
A imagem do título não comprova comportamento da câmera no mapa.

src/main.cpp cria framebuffer/textura 240×400, apresenta o jogo rotacionado em
400×240 e mantém a proporção com SDL_RenderSetLogicalSize. Esse era o estado inicial do pedido. A integração Auto inicial e evidências
estão na atualização abaixo; S08 agora está in_progress.

## Entrega esperada

Auto deve ser o padrão para ocupar a área de renderização disponível em Windows,
Linux/Steam Deck, Android e Switch. Expandir o mundo visível conforme a proporção
real, preservando geometria de sprites, física, relógio e lógica do jogo.
Native/Wide continuam como opções de compatibilidade; respeitar escolha explícita.
Esticar pixels, ampliar com corte ou preencher laterais com cópias/desfoque não
satisfaz o requisito de mostrar mais cenário.

Investigar dimensões WIPI, câmera, culling, clipping, caches e buffers do HL4.
Validar contratos/endereços próprios antes de alterar o guest. Ajustar HUD,
diálogos, menus, controles virtuais e transformação de toque/mouse. Preservar
horizontal no Android. Separar fullscreen imersivo, superfície real e áreas
seguras de recortes/gestos: fundo ocupa a superfície; controles ficam acessíveis.

Título, ilustrações e limites dos mapas exigem composição própria, sem deformar
arte nem ocultar conteúdo. Registrar limites autorais/técnicos; não alegar
cenário adicional onde não existe mapa.

Referências somente leitura: reference/hl5_snapshot/docs/DISPLAY.md,
src/display_layout.* e metodologia de tools/verify_aspect.py do snapshot.
Reaproveitar política e verificações, nunca endereços/offsets ou execução direta
de scripts HL5. O sucesso do HL5 não constitui teste de execução do HL4.

## Verificações para aceite

- Comparar Native e Auto no mesmo mapa/posição: provar cenário adicional
  desenhado, sem faixas causadas pelo canvas fixo. Comparar detalhes de sprites
  na mesma escala para detectar deformação ou simples zoom.
- Exercitar 4:3, 16:10, 16:9, 20:9 e ultrawide 21:9, incluindo a proporção real
  do aparelho do relato. Registrar tamanho da superfície/janela, canvas guest,
  retângulo de apresentação e áreas seguras.
- Testar centro/extremos de mapas, transições, exploração, combate, cutscenes,
  título, menus e diálogos longos. Conferir câmera, culling, camadas e ausência
  de resíduos; HUD e traduções sem cortes ou sobreposição.
- Repetir Skip, ação, movimento, menus e retorno por toque/mouse nas bordas e
  por teclado/gamepad. Cancelar gesto ativo ao mudar o canvas.
- Redimensionar e alternar fullscreen/modos com diálogo aberto. Android: barras
  do sistema, recortes, gestos, pausa/retorno e dois sentidos horizontais.
  Switch: portátil/dock; Steam Deck: 16:10; desktop: janela e fullscreen.
- Comparar estado de gameplay, áudio e saves temporários com Native; medir
  memória e tempo de quadro com área maior, sobretudo Android/Switch.
- Registrar capturas antes/depois, commit, proporção e dispositivo. Separar
  resultado automatizado de validação física nas quatro plataformas.

Próxima ação técnica: mapear tamanho/framebuffer/câmera do HL4 e criar prova
reproduzível de cenário adicional antes de integrar Auto. Este registro não
altera a build nem encerra o reteste de boot do Switch.

## Investigação de 2026-10-22

S08 em andamento. SDL_WINDOW_FULLSCREEN Android corrigido no host; teste
físico das barras/HUD pendente. Não confundir isso com câmera expandida.
Prova descartável em scratch/viewport-probe.cpp (não distribuída): tela240x534
produz _lcdG+0x54=240/+0x58=534 no boot e2222flushes em72s determinísticos.
Captura smoke-20 mostra área de mundo original e painel de controles ampliado:
aumentar tela física não aumenta sozinho o viewport lógico. ABI original:
DM_Initialize VA0x56000 lê screen/framebuffer; campos _lcdG0xedbe4 +54/+58.
Touch_Stand_Initialize VA0x360cc usa esses campos, mas desenho rotacionado
aplica composição própria. Próxima ação: rastrear seleção/alocação do buffer
320x240 do mundo e limites de composição da lateral na ativação nTouchRotate.
Não integrar a prova como Auto. Relatório ANDROID_LAYOUT_QUEST_TESTS.json.

## Integração inicial Auto — 2026-10-22

A investigação seguinte localizou m_TouchDirectKeySize0xf13f0. Preservar80px
em touch_setUiChange0x35c14 libera a largura extra para o mundo. Auto agora
usa proporção da janela na abertura; Native por CLI. Textura/input dinâmicos.
Prova46s em4:3/16:9/20:9:23estados idênticos,1355flushes e áudio;17recortes
16x16 exatos; oracle20checks memória/registradores. Toque80s/106eventos passou.
Detalhes/limites em DISPLAY.md e VIEWPORT_EXPANSION_TESTS.json. S08 segue
in_progress: resize vivo, outros mapas/HUD e hardware pendentes.
Suíte final também passou16:10/21:9. Dimensão vem de SDL_GetRendererOutputSize,
consultada antes de configurar escala lógica e inicializar o guest.
