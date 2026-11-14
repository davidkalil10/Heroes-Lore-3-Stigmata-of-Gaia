# Handoff - Heroes Lore 3 Recomp

## Estado Atual

- **Última etapa concluída**: `S11` (Menu Integrado, Cloud Save Google Drive, Créditos no Menu Sobre, OTA Updater GitHub Releases corrigido para Windows, pacote sem APK redundante, Release `v1.0.2` Windows x64)
- **Próxima etapa recomendada**: Testar atualização OTA `v1.0.2 -> v1.0.3` quando o usuário confirmar o download da `v1.0.2`, e seguir para `S12` (Core nativo C++ / Linux x86_64 / Android ARM64 `com.davidbraga.heroeslore3` e CI Multiplataforma)
- **Versão atual do Port**: `v1.0.2` (desacoplada do jogo original iOS `1.0.1`)
- **Idioma padrão no boot**: Inglês (`en`), alternável em tempo real para Português (`pt-BR`) via `F7` ou `Port Options & Cloud` sem reiniciar nem alterar saves
- **Pacote base verificado**: `Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk` (`61.755.432` bytes), embarcando `assets/game.ipa` (`43.686.130` bytes) com Mach-O ARMv6 `Payload/Heroes Lore.app/Heroes Lore` (`915.824` bytes) — pacote Windows standalone embarca apenas `assets/game.ipa`

## O que foi feito e validado na Sessão 18 (`v1.0.2`)

1. **Correção da Falha do Auto-Updater Windows (`tools/hl3rt/updater.py`)**:
   - Identificado e comprovado experimentalmente em pacote frozen que `CREATE_NO_WINDOW | DETACHED_PROCESS` no `subprocess.Popen` fazia `powershell.exe` encerrar com código `0` sem executar `update_windows.ps1`.
   - Alterado para `CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW` com `stdin=DEVNULL`, `stdout=DEVNULL`, `stderr=DEVNULL` e `close_fds=True`.
   - Gravado `update_windows.ps1` com UTF-8 BOM (`utf-8-sig`), retries (`Invoke-WithRetry`, 15 tentativas x 300 ms) para troca/rollback contra bloqueios temporários de antivírus/DLL e limpeza da pasta `staged/` ao concluir.
2. **Remoção do APK Redundante do Pacote Windows (`tools/hl3rt/machine.py`, `tools/build_standalone_pc.py`)**:
   - `Bundle.__init__` carrega `assets/game.ipa` diretamente com verificação SHA-256 (`nested_ipa.sha256`), mantendo fallback para o `.apk` no workspace de desenvolvimento.
   - `build_standalone_pc.py` extrai `assets/game.ipa` (`43.686.130` bytes) para `dist/HeroesLore3_PC/assets/game.ipa` sem copiar os `61.755.432` bytes do `.apk` nem vazar `tools/scratch_*.py` em `_internal/`.
3. **Bump `v1.0.1 -> v1.0.2`, Testes e Release Build 3**:
   - `python tools/bump_version.py patch` sincronizou `v1.0.2` em `tools/hl3rt/version.py` e `project.json`.
   - `tests/test_updater.py` ampliado para `8` testes (`63` testes na suíte total) cobrindo UTF-8 BOM, flags do `Popen` e boot standalone via `assets/game.ipa` sem APK.
   - Publicada a release `v1.0.2` (Build 3) no GitHub para o usuário baixar e em seguida gerarmos a 4ª build (`v1.0.3`) para testar o update ao vivo.
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
