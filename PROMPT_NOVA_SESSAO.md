# Prompt para Iniciar Nova Sessão no Heroes Lore 3 Recomp

Copie e cole o bloco abaixo ao abrir uma nova conversa na pasta `C:\Users\david\Desktop\Projetos_Flutter\heroes_lore_3_recomp`:

```text
Você está assumindo o projeto Heroes Lore 3 Recomp neste workspace (`C:\Users\david\Desktop\Projetos_Flutter\heroes_lore_3_recomp`).

Antes de qualquer ação:
1. Leia `AGENTS.md`
2. Leia `docs/STATUS.json` (ou rode `python tools/project_status.py show`)
3. Leia `docs/HANDOFF.md`
4. Leia `docs/ROADMAP.md`, `docs/HL4_AND_HL5_LESSONS.md`, `docs/DECISIONS.md` e `reference/analysis/INITIAL_FINDINGS.md`
5. Leia a especificação da etapa atual em `docs/tasks/` (atualmente `docs/tasks/S02_RUNTIME_CONTRACT.md`)
6. Rode `python tools/verify_workspace.py`

Regras:
- Trabalhe somente dentro de `heroes_lore_3_recomp`; nunca altere `heroes_lore_4_recomp`, `heroes_lore_5_recomp` ou os ports J2ME.
- Use `reference/hl4_snapshot/`, `reference/hl5_snapshot/` e `reference/jar_ports_docs/` para consultar padrões já validados.
- Lembre-se que o APK de entrada do HL3 é um wrapper touchHLE contendo `assets/game.ipa`, cujo executável real é `Payload/Heroes Lore.app/Heroes Lore` (Mach-O 32-bit ARMv6 com Objective-C, OpenGL ES 1.1, OpenAL/AVAudioPlayer e SQLite3).
- Ao terminar a sessão, atualize `docs/STATUS.json`, `docs/STATUS.md`, `docs/HANDOFF.md`, `docs/RESEARCH_LOG.md` e valide com `python tools/verify_workspace.py`.

Execute a etapa atual (`current_step`) indicada em `docs/STATUS.json`.
```
