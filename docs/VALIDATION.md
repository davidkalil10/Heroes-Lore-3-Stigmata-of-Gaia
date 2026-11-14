# Critérios e Comandos de Validação - Heroes Lore 3 Recomp

Este documento consolida todas as verificações determinísticas de integridade, comportamento em runtime, localização e empacotamento standalone.

## 1. Validação Estática e Integridade do Workspace

```powershell
# Verifica arquivos obrigatórios, SHA-256 de APK/IPA/Mach-O, manifestos de snapshots e status.json
python tools/verify_workspace.py

# Confirma sincronização entre docs/STATUS.json e docs/STATUS.md
python tools/project_status.py check

# Confirma sincronização da versão do port (tools/hl3rt/version.py <-> project.json)
python tools/bump_version.py --check

# Reexecuta inspeção estática e compara byte a byte com reference/analysis/
python tools/inspect_apk.py --check
python tools/probe_macho_contract.py --check
```

## 2. Validação de CPU ARMv6, Oráculo Unicorn e Fases

```powershell
# Confere equivalência de registradores/CPSR e memória dos 28 blocos AOT contra Unicorn (48 comparações)
python tools/verify_aot_oracle.py

# Gera métricas determinísticas de execução por fase (boot, title, menu)
python tools/profile_phases.py
```

## 3. Validação de Textos EN e Tradução PT-BR

```powershell
# Garante extração idêntica dos 6.733 slots de texto EN e métricas das 4 fontes .rff/.pvr
python tools/extract_texts.py --check

# Compila t_pt-br, hdr_pt-br e InGame_txt_2_pt-br validando prefixos, [Speaker]#, tags #|{}^, printf e glifos
python tools/build_ptbr_texts.py
```

## 4. Suíte de Testes Automatizados (`tests/`)

```powershell
python -m unittest discover -s tests -v
```

Cobertura atual da suíte (`62` testes):
- `tests/test_runtime_boot.py`: loader Mach-O, runtime ObjC, boot, New Game, combate e save/load local.
- `tests/test_audio_and_aot.py`: mixer OpenAL/WAV + MP3 fora do callback SDL, guarda de opcodes e blocos AOT.
- `tests/test_viewport_aspect.py`: projeção ortográfica OpenGL, True Widescreen (`Auto`, `16:9`, `Original`), centralização de popups/menus e ancoragem de HUD.
- `tests/test_s09_s10_localization.py`: catálogo EN (`6.733` slots), extensão de glifos `.rff`/`.pvr`, integridade dos 12 chunks PT-BR, reload live via `TextUtils` + `GameWorld` banner e linha de crédito no menu `About` (`slot 6632`).
- `tests/test_cloud_save.py`: autenticação Google Drive OAuth, isolamento de pasta/arquivo HL3, backups de segurança e detecção de corrupção.
- `tests/test_options_menu.py`: navegação, hitboxes responsivas (paisagem e retrato), 6 linhas de opções, páginas Cloud Save e Updates.
- `tests/test_updater.py`: parser SemVer e API do GitHub Releases, filtro HTTPS/domínio, verificação de versão sincronizada, download streaming com SHA-256, proteção ZIP Slip e geração de script Windows com preservação de `config/`/`data/`.

## 5. Validação de Build Standalone (Windows x64)

```powershell
# Gera dist/HeroesLore3_PC/ e dist/heroes_lore_3_windows_x64.zip sem dados pessoais/credenciais
python tools/build_standalone_pc.py

# Smoke test headless do executável compilado
.\dist\HeroesLore3_PC\HeroesLore3.exe --frames 15
```
