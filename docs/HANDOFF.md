# Handoff de Sessão - Heroes Lore 3 Recomp

## Estado Atual

- **Fase**: `pc_playable_complete`
- **Etapas concluídas**: `S00`, `S01`, `S02`, `S03`, `S04`, `S05`, `S06`, `S07`
- **Etapa ativa (`current_step`)**: `S08` (`True Widescreen e expansão de viewport`)
- **Objetivo alcançado**: **Build PC Standalone 100% funcional e independente (`dist/HeroesLore3_PC/HeroesLore3.exe`), sem requerer Python ou ferramentas instaladas, com gráficos alinhados a 60 FPS, todos os textos e artes de menus renderizados perfeitamente, fluxo de criação de personagens em 5 etapas (título, herói, classe, resumo em (355, 285) e gameplay) com suporte completo a setas e números 1/2/3, eliminação da moldura amarela no Esc, pulo instantâneo de cutscenes (SKIP em (410, 28)) e conversa com NPCs no centro exato (445.0, 286.0 / 435.0, 275.0 via E/Enter/Gamepad A) com pulso multiframe em tap_once!**

## O que foi feito e validado nesta sessão

1. **Descoberta da Tela de Resumo de Personagem (Step 3) e Correção da Confirmação**:
   - Diagnóstico: A criação de personagem do HL3 possui 4 etapas sequenciais antes do gameplay:
     - `step 0`: Carrossel de Título (`START GAME` em `(240, 280)`).
     - `step 1`: Seleção Kei vs Rits (`KEI_CARD (150, 100)`, `RITS_CARD (330, 100)`, confirmação em `(415, 284)`).
     - `step 2`: Seleção de Classe (`CLASS_LEFT (99, 88)`, `CLASS_RIGHT (389, 86)`, chibis `(160, 80)`, `(244, 80)`, `(338, 80)`, confirmação em `(415, 284)`).
     - `step 3`: Resumo do Herói com Nação e Classe. A ilustração de Kei fica à direita e o botão verde de confirmação fica no canto inferior da caixa de informações em `(355.0, 285.0)` (60 pixels à esquerda do botão de classe).
     - `step 4`: Cutscenes de Abertura e Gameplay na Vila de Gaia.
   - Solução: O código anterior saltava diretamente de `class_select` para `game`, de modo que na tela de resumo o `Enter` tentava disparar o botão em `(415, 284)` (cinto de Kei) em vez do checkmark real em `(355, 285)`. Implementado o `step = 3` com alvo `SUMMARY_CONFIRM = (355.0, 285.0)`, permitindo avançar instantaneamente com `Enter`, `Espaço` ou Gamepad `A`.

2. **Eliminação da Moldura Amarela ao Teclar Esc e Navegação com Teclado na Seleção de Classe**:
   - Diagnóstico: Quando o jogo estava na tela de resumo mas o runner acreditava estar em `game`, teclar `Esc` caía no `else` que ativava `self.menu_mode = True`, abrindo indevidamente o menu de pausa e desenhando o cursor dourado sobre a tela de criação.
   - Solução:
     - No `step 1`, `step 2` e `step 3`, a tecla `Esc` e `Backspace` estritamente recuam de tela via `BTN_BACK (27.0, 17.0)` e decrementam `self.step`, sem jamais abrir o menu de pausa.
     - Adicionada guarda estrita em `_draw_menu_cursor`: o cursor só pode ser desenhado se `self.menu_mode and self.step == 4`.
     - Na seleção de classe (`step 2`), as setas Esquerda/Direita e teclas `A`/`D` acionam as setas `<` `(99.0, 88.0)` e `>` `(389.0, 86.0)`, e as teclas numéricas `1`, `2`, `3` selecionam os chibis diretamente `(160, 80)`, `(244, 80)` e `(338, 80)`.

3. **Pulo Instantâneo de Cutscenes (SKIP) e Conversa com NPCs com Pulso Multiframe**:
   - Diagnóstico:
     - Pulo de cutscenes: O botão circular marrom com "X" (`BTN_SKIP`) medido na captura real fica em `(410.0, 28.0)` com texto SKIP em `(410.0, 45.0)`. As teclas `S`, `Tab` e botões de Gamepad `Y`/`Start` agora disparam toques no centro do botão e da confirmação.
     - Fala com NPCs: O botão azul de conversa medido na captura de tela do usuário (`media_1794324014624_479f1886.jpg`) tem centro em `(445.0, 286.0)` / `(435.0, 275.0)`.
     - Pulso multiframe: `tap_once` anterior liberava o toque em 1 único frame (16ms), causando oscilações e rejeição de eventos pelo motor iOS. Implementado dicionário `self.pending_tap_ups: dict[str, int]` com contagem regressiva de frames (`hold_frames=2` ou `3`), garantindo reconhecimento impecável pelo `TouchResponder`.
     - Teclas `E`, `Enter` e Gamepad `A` acionam a conversa com o NPC e avançam falas de diálogos sem falhas.

4. **Reempacotamento e Validação da Build Standalone (`dist/HeroesLore3_PC/`)**:
   - Recompilado `dist/HeroesLore3_PC/HeroesLore3.exe` com PyInstaller e pós-processamento `strip_cfg`.
   - Execução standalone testada com `--frames 30` (código de saída 0).
   - 24 testes unitários automatizados passando com 100% de sucesso (`python -m unittest discover -s tests -v`).
   - Registrados `ADR-014` e `ADR-015` em `docs/DECISIONS.md`.

## Próximo Passo (`S08`: True Widescreen e Expansão de Viewport)

1. Ler a especificação em `docs/tasks/S08_VIEWPORT_EXPANSION.md`.
2. Expandir a projeção ortográfica `glOrthof` (original 480x320) para proporções `16:9` (854x480 / 1280x720), `16:10` e `20:9`.
3. Ajustar matrizes e limites de culling mantendo os elementos de HUD ancorados nas bordas sem distorção.

## Comandos de Retomada e Execução

Para rodar a versão standalone portátil no PC (sem precisar de Python):
```powershell
.\dist\HeroesLore3_PC\HeroesLore3.exe
```

Para rodar diretamente via script Python:
```powershell
python tools/run_pc.py
```

Para validar a integridade do workspace e rodar testes:
```powershell
python tools/verify_workspace.py
python tools/project_status.py show
python -m unittest discover -s tests -v
```

## Próximo Passo (`S08`: True Widescreen e Expansão de Viewport)

1. Ler a especificação em `docs/tasks/S08_VIEWPORT_EXPANSION.md`.
2. Expandir a projeção ortográfica `glOrthof` (original 480x320) para proporções `16:9` (854x480 / 1280x720), `16:10` e `20:9`.
3. Ajustar matrizes e limites de culling mantendo os elementos de HUD ancorados nas bordas sem distorção.

## Comandos de Retomada e Execução

Para rodar a versão standalone portátil no PC (sem precisar de Python):
```powershell
.\dist\HeroesLore3_PC\HeroesLore3.exe
```

Para rodar diretamente via script Python:
```powershell
python tools/run_pc.py
```

Para validar a integridade do workspace e rodar testes:
```powershell
python tools/verify_workspace.py
python tools/project_status.py show
python -m unittest discover -s tests -v
```
