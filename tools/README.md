# Diretório `tools/` - Heroes Lore 3 Recomp

Scripts determinísticos de inspeção, verificação e gerenciamento do projeto:

- `verify_workspace.py`: Verifica `project.json`, SHA-256 do APK original, snapshots `HL4_SNAPSHOT.json`, `HL5_SNAPSHOT.json`, `JAR_PORTS_DOCS.json`, status do roadmap e consistência da análise estática.
- `inspect_apk.py`: Inspeciona o APK externo, o `assets/game.ipa` embutido e o executável Mach-O ARMv6 (`Payload/Heroes Lore.app/Heroes Lore`), gerando `reference/analysis/*` e extraindo para `reference/extracted_apk/` e `reference/extracted_ipa/` quando chamado com `--extract`.
- `project_status.py`: Consulta, valida e renderiza `docs/STATUS.json` -> `docs/STATUS.md`.
