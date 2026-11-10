# Validação e Portões de Qualidade - Heroes Lore 3 Recomp

Nenhuma etapa do `docs/ROADMAP.md` deve ser marcada como concluída (`done`) sem passar pelos portões determinísticos abaixo.

## 1. Portão de Integridade do Workspace e Inventário (`S00` e `S01`)

Comandos obrigatórios:

```powershell
python tools/verify_workspace.py
python tools/project_status.py check
python tools/inspect_apk.py --check
```

O que é verificado:
- Existência e SHA-256 exato do APK original (`Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk`), do `assets/game.ipa` embutido e do executável Mach-O ARMv6 (`Payload/Heroes Lore.app/Heroes Lore`).
- Integridade completa dos snapshots congelados (`reference/HL4_SNAPSHOT.json`, `reference/HL5_SNAPSHOT.json` e `reference/JAR_PORTS_DOCS.json`).
- Sincronização entre `docs/STATUS.json` e `docs/STATUS.md`.
- Consistência entre os bytes do APK/IPA/Mach-O e os relatórios em `reference/analysis/`.

## 2. Portão de Contrato de Runtime e Boot Headless (`S02` e `S03`)

- `S02`: Script de probe do Mach-O validando os 27 inicializadores de `__mod_init_func`, as tabelas `__nl_symbol_ptr`/`__la_symbol_ptr`, os 30 objetos de classe Objective-C e os parsers de `.pvr`, `.rff` e `dat/InGame_txt`.
- `S03`: Execução headless determinística desde a entrada até a criação da `GLView` e primeiro disparo do loop de renderização sem acessos inválidos de memória.

## 3. Portão de Gameplay Desktop e Áudio (`S04`, `S05` e `S06`)

- `S04`/`S05`: Replay automatizado navegando pela tela título, iniciando novo jogo, movimentando o personagem em campo, executando ataques/skills, abrindo inventário/mapa e salvando/carregando progresso.
- `S06`: Reprodução contínua das 20 faixas MP3 (`snd/bgm0_mf.mp3`..`bgm16_mf.mp3`) e 28 efeitos WAV sem underruns nem bloqueio do thread principal.

## 4. Portão de Performance AOT + Oráculo (`S07`)

- Guarda de decodificação de opcodes (`verify_arm_emit.py`) com zero instruções silenciosamente ignoradas.
- Verificação de equivalência bit a bit contra o oráculo Unicorn (`verify_fast_runtime.py`) comparando `r0..r15`, `CPSR` e páginas de memória modificadas.
- Tempo de frame estável para 60 FPS em campo, combate e menus.

## 5. Portão de True Widescreen, Localização e Plataformas (`S08` a `S14`)

- `S08`: Capturas automatizadas em `480x320` (`3:2`), `16:9`, `16:10`, `20:9` e `4:3` comprovando ausência de estiramento da HUD.
- `S09`/`S10`: Auditoria de strings EN e PT-BR sem truncamento visual e troca de idioma ao vivo sem corromper saves.
- `S11`..`S14`: Testes de serviços (`Cloud Save` + `OTA`), pacotes verificados de Windows, Linux, Android ARM64 e Nintendo Switch (`.nro`).
