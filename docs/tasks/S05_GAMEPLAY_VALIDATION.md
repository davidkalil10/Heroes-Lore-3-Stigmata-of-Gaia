# Etapa S05: Validação de Gameplay Completo no PC (Teclado + Gamepad)

## Objetivo

Validar a jogabilidade interativa e determinística no PC através do host nativo (`tools/run_pc.py`), confirmando a resposta de menus, navegação, movimentação em campo, combate, inventário, e persistência de saves locais (`Documents/options.ini`, `Documents/flags.ini`).

## Procedimentos de Execução

1. **Executar o Jogo no PC**:
   ```powershell
   python tools/run_pc.py
   ```
   Ou com parâmetros personalizados:
   ```powershell
   python tools/run_pc.py --scale 2       # Janela 960x640 (2x)
   python tools/run_pc.py --scale 3       # Janela 1440x960 (3x)
   python tools/run_pc.py --fullscreen    # Tela cheia
   python tools/run_pc.py --frames 120    # Teste determinístico de 120 frames
   ```

2. **Mapeamento de Controles**:
   - **Teclado**:
     - Movimentação: Setas ou W / A / S / D
     - Ataque / Confirmar: `J` ou Barra de Espaço
     - Habilidades: `U` (Skill 1), `I` (Skill 2), `O` (Skill 3)
     - Usar Item / Poção: `K` ou `Q`
     - Confirmar / Iniciar: `Enter` ou `Z`
     - Menu / Pausa: `Escape` ou `Tab`
     - Status: `P`
   - **Gamepad (Xbox / DualShock / Genérico)**:
     - Movimentação: Analógico Esquerdo ou D-Pad
     - Ataque: Botão `A`
     - Menu / Voltar: Botão `B`
     - Habilidade 1: Botão `X`
     - Habilidade 2: Botão `Y`
     - Poção: Botão `LB`
     - Habilidade 3: Botão `RB`
     - Menu de Pausa: Botão `Start`
     - Status: Botão `Select`
   - **Mouse**:
     - Clique e arraste para toques diretos de precisão em qualquer menu ou botão da tela.

3. **Validação Automatizada**:
   - Suite de testes em `tests/test_run_pc.py` executa o host em modo de teste e valida renderização, toques e apresentação de frames sem travar.

## Critérios de Aceitação

- [x] Host interativo inicia em menos de 2 segundos.
- [x] Controles de teclado e gamepad respondem sem atraso (input lag perceptível zero).
- [x] Suporte a controle analógico e digital no gamepad.
- [x] Captura de telas e saída limpa via Escape ou fechamento de janela.

