# Handoff de Sessão - Heroes Lore 3 Recomp

## Estado Atual

- **Fase**: `pc_playable_complete`
- **Etapas concluídas**: `S00`, `S01`, `S02`, `S03`, `S04`, `S05`, `S06`, `S07`
- **Etapa ativa (`current_step`)**: `S08` (`True Widescreen e expansão de viewport`)
- **Objetivo alcançado**: **Build PC Standalone 100% funcional e independente (`dist/HeroesLore3_PC/HeroesLore3.exe`), sem requerer Python ou ferramentas instaladas, com gráficos alinhados a 60 FPS, áudio completo (64 canais), suporte a mouse, teclado e gamepad!**

## O que foi feito e validado nesta sessão

1. **Correção de Distorção Gráfica ("Tudo torto / fatias desalinhadas")**:
   - Diagnóstico: O binário iOS emite 6 chamadas `glPushMatrix()` por frame sem `glPopMatrix()`. Nos drivers OpenGL desktop para PC, isso estourou a profundidade máxima da pilha de matrizes (`GL_STACK_OVERFLOW`), acumulando matrizes antigas e deslocando verticalmente quads e faixas da interface.
   - Solução: Implementada contenção de profundidade de matrizes em `tools/hl3rt/gl_bridge.py` e reinicialização para matriz identidade (`glLoadIdentity`) a cada frame em `begin_frame()` tanto na pilha `GL_PROJECTION` quanto `GL_MODELVIEW`.
   - Adicionado `GL_CLAMP_TO_EDGE` em texturas para eliminar sangramento de 1px nas emendas e corrigida a inversão de coordenadas Y do UIKit para OpenGL desktop em `glScissor`.
   - Resultado: Alinhamento pixel-perfect de todos os quads, ribbons e elementos visuais.

2. **Correção da Navegação de Toque e Menus ("Não consigo navegar nem clicar")**:
   - Diagnóstico: O motor do jogo em `TouchResponder touchesEnded:withEvent:` (`0xbe5c`) exige que o ponteiro `UITouch*` (`r3`) seja idêntico ao ponteiro registrado em `touchesBegan:withEvent:` (`0xc0a4`). O host recriava um novo objeto guest a cada evento, fazendo o jogo descartar `touchesEnded` e manter botões travados no estado pressionado (iluminados em vermelho).
   - Solução: Implementado rastreamento persistente de toques em `tools/run_pc.py` (`self.active_touches[tag] = (Touch, t_obj, evt_obj, set_obj)`), reutilizando o mesmo endereço guest e atualizando o campo `phase` (0 = began, 1 = moved, 3 = ended).
   - Coordenadas de clique do botão virtual START atualizadas para o centro do botão "START GAME" `(240.0, 280.0)`.
   - Validação: Ao clicar em "START GAME" ou pressionar Enter / Barra de Espaço / Botão A do gamepad, a tela avança imediatamente para a tela de Seleção de Personagem (Kei / Rits, nação Askra), comprovado em `reference/analysis/screenshot_20261109_195115.png`.

3. **Correção e Conclusão da Build Standalone para PC (`dist/HeroesLore3_PC/`)**:
   - Diagnóstico do fechamento da janela: A execução de `HeroesLore3.exe` fechava abruptamente com código `0xC0000409` (`STATUS_STACK_BUFFER_OVERRUN` / `__fastfail`) em `VCRUNTIME140.dll` na função `longjmp`. O bootloader de 64-bit do PyInstaller no Windows possui Control Flow Guard ativado (`IMAGE_DLLCHARACTERISTICS_GUARD_CF = 0x4000`). Como o Unicorn Engine emite JIT de ARMv6 e usa `setjmp`/`longjmp` internamente, o CRT do MSVC aborta o processo por violação de CFG.
   - Solução: Implementada rotina `strip_cfg` em `tools/build_standalone_pc.py` que limpa o bit `0x4000` do cabeçalho opcional PE de `HeroesLore3.exe`, igualando as características de DLL do `python.exe` (`0x8160`).
   - Validação: `dist/HeroesLore3_PC/HeroesLore3.exe` roda com sucesso total e código 0, abrindo a janela gráfica, executando a 60 FPS com áudio e controles funcionais.
   - O pacote distribuível contém: `HeroesLore3.exe`, APK original com assets, `project.json`, `config/audio.json`, pasta de saves `data/internal/Documents/` e manual de controles `README.txt`.

4. **Testes Automatizados e Governança**:
   - 23 testes unitários passando em ~8 segundos (`python -m unittest discover -s tests -v`).
   - `python tools/verify_workspace.py` passa com 100% de integridade.
   - Registrados `ADR-009` e `ADR-010` em `docs/DECISIONS.md`.
   - Atualizado `docs/RESEARCH_LOG.md`, `docs/STATUS.json` e `docs/STATUS.md`.

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
