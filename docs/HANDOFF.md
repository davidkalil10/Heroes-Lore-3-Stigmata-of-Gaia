# Handoff - Heroes Lore 3 Recomp

## Estado Atual

- **Última etapa concluída**: `S11` (Menu Integrado, Cloud Save Google Drive, Créditos no Menu Sobre, OTA Updater GitHub Releases, README/Documentação e Release `v1.0.0` Windows x64)
- **Próxima etapa recomendada**: `S12` (Core nativo C++ / Linux x86_64 / Android ARM64 `com.davidbraga.heroeslore3` e CI Multiplataforma)
- **Versão atual do Port**: `v1.0.0` (desacoplada do jogo original iOS `1.0.1`)
- **Idioma padrão no boot**: Inglês (`en`), alternável em tempo real para Português (`pt-BR`) via `F7` ou `Port Options & Cloud` sem reiniciar nem alterar saves
- **Pacote base verificado**: `Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk` (`61.755.432` bytes), embarcando `assets/game.ipa` (`43.686.130` bytes) com Mach-O ARMv6 `Payload/Heroes Lore.app/Heroes Lore` (`915.824` bytes)

## O que foi feito e validado na Sessão 16

1. **Menu Sobre com Autoria e Versão do Port (`tools/hl3rt/version.py`, `tools/run_pc.py`)**:
   - Injetado em `slot 6632` de `TextUtils` (`0x000d8220`) logo abaixo de `Version: 1.0.1` / `Versão: 1.0.1`: `"Port by David Kalil Braga (2026) - v1.0.0||"` (EN) e `"Port por David Kalil Braga (2026) - v1.0.0||"` (PT-BR).
   - Corrigida alocação de `buf1` em `_patch_guest_about_credits()` para preservar capacidade `>= max_tbl[1]` (`alloc_sizes[buf1]`), eliminando sobrescrita ao alternar `EN` ↔ `PT-BR` ao vivo.
2. **Versionamento Canônico & Atualizador OTA GitHub Releases (`tools/hl3rt/updater.py`, `tools/bump_version.py`, `tools/hl3rt/options_menu.py`)**:
   - `UpdateManager` consulta `davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/releases/latest` em background, trata HTTP `404` graciosamente enquanto o repositório estiver privado, valida HTTPS + SHA-256, previne ZIP Slip e gera updater staging Windows preservando `config/` e `data/` com rollback.
   - Adicionadas preferência de checagem no startup (`config/services.json`), linha na lista principal (`6` itens + `Back`) e página responsiva `ModalPage.UPDATE` testada em paisagem e retrato.
3. **Documentação & Build Standalone Limpo (`README.md`, `docs/*`, `tools/build_standalone_pc.py`, `.gitignore`)**:
   - `README.md` reescrito com badges, destaques da `v1.0.0`, status honesto por plataforma, controles verificados, fluxo Cloud Save/OTA e instruções de build.
   - `docs/FEATURE_PARITY.md`, `docs/VALIDATION.md`, `docs/ARCHITECTURE.md`, `docs/PORTING_MAP.md`, `docs/DECISIONS.md` (`ADR-022`), `docs/RESEARCH_LOG.md`, `docs/STATUS.json` e `research/sessions/2026-11-13-16.md` atualizados.
   - `tools/build_standalone_pc.py` ajustado para omitir saves pessoais (`data/internal/Documents/`) e `config/cloud_auth.json` das builds padrão e gerar `dist/heroes_lore_3_windows_x64.zip`.
   - Validação: `62/62` testes passando em `python -m unittest discover -s tests -v`.

## O que ficou pendente (`S12`–`S14`)

- **`S12`**: Implementar core nativo C++/SDL2 em `src/`, alvo Linux x86_64 e app Android ARM64 (`com.davidbraga.heroeslore3`) + workflows GitHub Actions.
- **`S13`**: Port Nintendo Switch (`.nro`, Title ID `0100F3A300030000`) com controle Npad e gerenciamento de memória/cache.
- **`S14`**: Auditoria final multiplataforma entre Windows, Linux, Android e Nintendo Switch.

## Próximo passo concreto ao retomar

Iniciar `S12` verificando o estado do workspace e preparando a base C++/SDL2 e workflows multiplataforma:

```powershell
python tools/verify_workspace.py
python tools/bump_version.py --check
python -m unittest discover -s tests -v
```
