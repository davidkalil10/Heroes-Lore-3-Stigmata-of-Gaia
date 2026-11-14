# Handoff - Heroes Lore 3 Recomp

## Estado Atual

- **Última etapa concluída**: `S11` (Menu Integrado, Cloud Save Google Drive, Créditos no Menu Sobre, OTA Updater GitHub Releases, Releases `v1.0.0` e `v1.0.1` Windows x64)
- **Próxima etapa recomendada**: `S12` (Core nativo C++ / Linux x86_64 / Android ARM64 `com.davidbraga.heroeslore3` e CI Multiplataforma)
- **Versão atual do Port**: `v1.0.1` (desacoplada do jogo original iOS `1.0.1`)
- **Idioma padrão no boot**: Inglês (`en`), alternável em tempo real para Português (`pt-BR`) via `F7` ou `Port Options & Cloud` sem reiniciar nem alterar saves
- **Pacote base verificado**: `Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk` (`61.755.432` bytes), embarcando `assets/game.ipa` (`43.686.130` bytes) com Mach-O ARMv6 `Payload/Heroes Lore.app/Heroes Lore` (`915.824` bytes)

## O que foi feito e validado na Sessão 17 (`v1.0.1`)

1. **Bump de Versão (`v1.0.0 -> v1.0.1`) via `tools/bump_version.py patch`**:
   - Sincronizadas `PORT_VERSION = "1.0.1"` em `tools/hl3rt/version.py` e `"port_version": "1.0.1"` em `project.json`.
   - Menu Sobre (`form_id=114`, `slot 6632`), título da janela, modal de opções/atualizações e `README.txt` do bundle passam automaticamente a reportar `v1.0.1`.
2. **Documentação e Pacote Standalone `v1.0.1`**:
   - Atualizados `README.md`, `docs/FEATURE_PARITY.md`, `docs/ARCHITECTURE.md`, `docs/PORTING_MAP.md`, `docs/RESEARCH_LOG.md`, `docs/STATUS.json`, `docs/HANDOFF.md` e `research/sessions/2026-11-13-17.md`.
   - Recompilado `dist/HeroesLore3_PC/` + `dist/heroes_lore_3_windows_x64.zip`, validada a suíte completa (`62/62` testes) e smoke test do executável standalone, e publicada a tag/release `v1.0.1` no GitHub para teste de detecção e auto-atualização OTA a partir da `v1.0.0`.
   - *Observação*: a detecção pública dentro do jogo (`api.github.com/repos/.../releases/latest`) requer o repositório `davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia` em visibilidade **Pública**.

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
