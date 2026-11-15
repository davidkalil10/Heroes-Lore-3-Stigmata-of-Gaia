# HANDOFF - Heroes Lore 3 Recomp

## Estado atual
- **Etapa ativa**: `S13` (Port Nintendo Switch `.nro` e validação de hardware/memória) — próxima recomendada `S14`.
- **Versão atual**: `v1.0.7` (`Android versionCode 8`), publicada via tag `v1.0.7`.
- **O que foi feito e validado nesta sessão (`v1.0.7`)**:
  1. **Auto-Updater Switch `.nro` completo (`install_portable_nro_update` + `envSetNextLoad`)**:
     - Extraído `Sha256Hasher` em [src/core/common.h](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/src/core/common.h) para validação em streaming (64 KiB) sem alocação extra de memória.
     - Implementados `normalize_switch_nro_path` e `install_portable_nro_update` em [src/platform/updater.h](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/src/platform/updater.h) e [src/platform/updater.cpp](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/src/platform/updater.cpp): copia via `std::FILE*` (`fread`/`fwrite`), valida magic `NRO0`, tamanho e SHA-256 **antes** de chamar `before_swap_cb_`, executa flush + `romfsExit()` ordenado em [src/main.cpp](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/src/main.cpp), faz swap atômico com rollback, limpa `.old`/pacote, configura `envSetNextLoad` para o caminho ativo (`argv[0]`) e sai de forma limpa para relançamento pelo Homebrew Loader.
  2. **Testes & CI**:
     - Adicionado `test_switch_nro_installer` em [tests/cpp/test_platform_services.cpp](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/cpp/test_platform_services.cpp).
     - Mantidas as correções de CI da sessão anterior (guard SDL `2.24.0` para Ubuntu 22.04 + diretórios temporários isolados em `test_macho_boot` e `test_headless_runtime`) + otimizações Switch da `v1.0.6`.
  3. **Documentação & Versionamento**:
     - `python tools/bump_version.py patch` (`1.0.6` -> `1.0.7`, `versionCode 7 -> 8`), [docs/RELEASE_NOTES.md](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/docs/RELEASE_NOTES.md), [docs/DECISIONS.md](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/docs/DECISIONS.md) (`ADR-030`), [docs/RESEARCH_LOG.md](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/docs/RESEARCH_LOG.md) (Sessão 29) e [research/sessions/2026-11-15-02.md](file:///C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/research/sessions/2026-11-15-02.md).

## O que ficou pendente
- Instalar o `heroes_lore_3.nro` da `v1.0.7` no Switch uma vez (via atualização manual ou substituindo o NRO sem auto-instalador da `v1.0.6`); a partir da `v1.0.7+`, futuras releases (`v1.0.8+`) atualizarão e reabrirão automaticamente pelo console.
- Validar atualização automática de ponta a ponta no Switch e avançar para `S14` (auditoria final de paridade).

## Próximo passo concreto e comando de retomada
```powershell
python tools/project_status.py show
python tools/verify_workspace.py
```
