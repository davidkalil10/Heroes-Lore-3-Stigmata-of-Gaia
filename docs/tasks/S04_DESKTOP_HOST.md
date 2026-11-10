# Etapa S04: Host Desktop (Windows/Linux) + Renderizador GLES1 + Input (Teclado e Gamepad)

## Objetivo

Implementar o host nativo para execução em desktop (PC / Windows / Linux), provendo janela interativa, bridge de tradução direta de chamadas OpenGL ES 1.1 para o hardware gráfico host via OpenGL desktop, subsistema de áudio funcional (OpenAL + AVAudioPlayer) e mapeamento completo de teclado, gamepads/controles e mouse para entrada do jogo.

## Arquitetura Implementada

1. **Host Executável (`tools/run_pc.py`)**:
   - Criação de janela gráfica SDL2 / OpenGL com escala configurável (padrão 2x: 960x640, permitindo 1x: 480x320 ou fullscreen `F11`).
   - Loop principal cravado a 60 FPS com `pygame.time.Clock.tick(60)`.
   - Captura de screenshots em alta qualidade (`F12`) salvas em `reference/analysis/`.
   - Suporte a execução automatizada para testes contínuos (`--frames N`).

2. **Bridge OpenGL ES 1.1 (`tools/hl3rt/gl_bridge.py` + `tools/hl3rt/hle_gl.py`)**:
   - Tradução direta de comandos GLES 1.1 para a GPU:
     - Projeção ortográfica `glOrthof` (0 a 480 horizontal, 320 a 0 vertical).
     - Upload de texturas via `glTexImage2D` (RGBA8888 e AI88 descompactados) com suporte a mipmaps completos.
     - Pointers de vértices (`glVertexPointer`), coordenadas de textura (`glTexCoordPointer`) e cores (`glColorPointer`) lidos da memória guest no momento do desenho.
     - Desenho geométrico por `glDrawArrays` com blending alfa (`glBlendFunc`, `glAlphaFunc`).
     - Proteção contra overflow de pilha de matrizes.

3. **Mapeamento de Entrada (Teclado + Gamepad + Mouse)**:
   - O jogo opera exclusivamente via toques lógicos 480x320 na view `TouchResponder`. O runner converte inputs físicos em toques sintéticos contínuos (`touchesBegan:`, `touchesMoved:`, `touchesEnded:`):
     - **D-Pad / Movimentação**:
       - Teclado: Setas direcionais ou W / A / S / D (incluindo diagonais).
       - Gamepad: D-Pad Hat ou Analógico Esquerdo (com deadzone de 25%).
     - **Ação / Ataque / Confirmar**:
       - Teclado: `J` ou `Barra de Espaço`.
       - Gamepad: Botão `A` (Button 0).
     - **Habilidades (Skills)**:
       - Skill 1: Tecla `U` / `1` ou Gamepad `X` (Button 2).
       - Skill 2: Tecla `I` / `2` ou Gamepad `Y` (Button 3).
       - Skill 3: Tecla `O` / `3` ou Gamepad `RB` (Button 5).
     - **Item / Poção**:
       - Teclado: `K` ou `Q`.
       - Gamepad: `LB` (Button 4).
     - **Toque Central (Iniciar / Diálogos / Pular)**:
       - Teclado: `Enter` ou `Z`.
       - Gamepad: Botão `A` (ou analógico central).
     - **Menu / Pausa**:
       - Teclado: `Escape` ou `Tab`.
       - Gamepad: Botão `Start` (Button 7) ou `B` (Button 1).
     - **Status / Perfil**:
       - Teclado: `P`.
       - Gamepad: Botão `Select` (Button 6).
     - **Mouse / Toque Direto**:
       - Clique e arraste com o botão esquerdo do mouse em qualquer ponto da janela é mapeado diretamente para as coordenadas 480x320 do jogo, permitindo controle tátil exato de menus, inventário, equipamentos e diálogos.

4. **Subsistema de Áudio (`tools/hl3rt/hle_audio.py`)**:
   - Inicialização do mixer de áudio SDL (`44.1 kHz, 16-bit estéreo`).
   - Carregamento e reprodução dos 28 efeitos sonoros WAV via OpenAL (`alGenBuffers`, `alBufferData`, `alSourcePlay`).
   - Decodificação e streaming dos arquivos BGM MP3 via `AVAudioPlayer` (`snd/bgm0_mf.mp3`..`bgm18_mf.mp3`) fora do callback de áudio.

## Critérios de Aceitação

- [x] Janela interativa SDL2/OpenGL abre no PC e renderiza gráficos nítidos a 60 FPS.
- [x] Renderização de texturas e quads validada visualmente (screenshot com 77.000+ cores únicas).
- [x] Teclado mapeado e funcional para movimentação, ataque, habilidades e menus.
- [x] Gamepads/controles suportados e auto-detectados via SDL.
- [x] Mouse funciona como toque direto de precisão sobre a tela.
- [x] Áudio SFX e BGM inicializado e integrado.
- [x] Teste automatizado determinístico adicionado à suite de testes (`tests/test_run_pc.py`).

