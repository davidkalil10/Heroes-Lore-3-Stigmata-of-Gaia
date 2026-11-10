# Correção de mouse/toque — 2026-10-21

Relato: Skip e outros botões virtuais não correspondiam aos cliques na janela.
Causa confirmada no host: SDL_RenderSetLogicalSize já filtra mouse e toque para
resolução lógica (contrato em SDL_render.h, junto à função). O host chamava
SDL_RenderWindowToLogical novamente, aplicando escala/barras duas vezes.
Na janela 800x480, um clique lógico 380,235 virava aproximadamente 190,117;
com resize a divergência aumentava. A rotação inversa em si estava correta.

Correção: usar coordenadas lógicas do evento uma vez e inverter só a rotação
gx=ly, gy=399-lx. Toque SDL_FINGER tem caminho próprio e um dedo ativo; eventos
sintéticos mouse↔toque são ignorados para não duplicar ações. Cliques nas barras
não iniciam interação. CaptureMouse mantém release fora da janela; perder foco
cancela/solta. Motion consecutivo é coalescido. Down/up ficam em fila, entregues
um por timerCB original para evitar consumir um clique inteiro antes do Process.
Nenhum código do jogo ou sua cadência foi modificado.

## Validação integrada

`tests/pointer_boot.txt` inicializa com teclado; depois
`tests/pointer_gameplay.txt` usa exclusivamente pixels para Skip/ação/movimento/
menu/retorno. Primeiro Skip dura 1 ms, de modo que down/up chegam no mesmo polling.
Eventos são inseridos na SDL com windowID correto e coordenadas físicas obtidas
por SDL_RenderLogicalToWindow; passam pelo filtro real de eventos do renderer.
Um primeiro teste sintético sem windowID não passava pelo filtro: fixture corrigida.

```powershell
$env:SDL_VIDEODRIVER='dummy'
$env:SDL_AUDIODRIVER='dummy'
dist/windows/heroes_lore_4.exe --smoke 80 --deterministic --script tests/pointer_boot.txt --pointer-script tests/pointer_gameplay.txt --pointer-kind mouse --window-size 800 480 --capture-dir scratch/pointer-mouse-final --save-dir scratch/pointer-mouse-final-saves
dist/windows/heroes_lore_4.exe --smoke 80 --deterministic --script tests/pointer_boot.txt --pointer-script tests/pointer_gameplay.txt --pointer-kind finger --window-size 1000 700 --capture-dir scratch/pointer-finger-final --save-dir scratch/pointer-finger-final-saves
python tools/verify_pointer_runs.py
```

Usar saves novos em cada repetição. Terceiro percurso em janela 1000x700 verifica
mouse com letterbox. Evidência em reference/analysis/POINTER_TESTS.json: Skip
atinge key35; ação -5, direita -4, esquerda -3 e menu -16 no código original;
posições mudam e menu retorna; todos os presses têm release. Mouse/toque geram
mesmos estados do guest. Toque é injetado via SDL; touchscreen físico não testado.
O relato do usuário substitui a alegação anterior de clique funcionando, que
não tinha uma prova integrada correspondente.

Distribuição corrigida em dist/HeroesLore4-Windows-inputfix.zip, com DLLs.
Pasta distinta preserva a build aberta e seus saves. Fechar a anterior antes de
usar a nova; para continuar progresso, copiar sua pasta saves para a nova pasta.
