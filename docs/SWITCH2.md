# Nintendo Switch 2 — build experimental

A branch `codex/switch2-action` compila `heroes_lore_3_switch2.nro` em um workflow separado. O workflow multiplataforma e as releases existentes continuam inalterados até a validação no console.

## Arquitetura e dependências

O runtime C++17, `ArmFastCpu` e Unicorn TCI do HL3 permanecem os mesmos. TCI conserva o limite de 32 MiB e o adaptador de memória de dados do Switch. O alvo `HL3_SWITCH2` usa a toolchain oficial do [ounce-devkit](https://github.com/ouncebrew/ounce-devkit), libounce e OpenGL ES via [OunceVK](https://github.com/ouncebrew/OunceVK). SDL2 e bibliotecas C compatíveis vêm dos portlibs devkitPro. A busca prioriza `portlibs/switch2` e libounce, evitando os drivers Mesa de Switch 1.

SDK: `20261115`, instalador `0f38ba80d7ee768c5749278f127749d14ece2036`, container `devkitpro/devkita64:20260219`. Unicorn TCI continua fixado no commit existente no projeto. Apenas os probes de configuração passam a usar libounce.

## Saves e atualizador

Mantidos o formato e a pasta `sdmc:/switch/heroes_lore_3`, além dos serviços Google Drive existentes. A build procura exclusivamente `heroes_lore_3_switch2.nro` nas releases; nunca baixa o NRO de Switch 1. O fluxo de restauração não foi alterado. Nenhum save pessoal ou credencial é incluído no pacote.

## Compilar e testar

Faça push nesta branch e abra o workflow **Nintendo Switch 2 NRO**. O artefato inclui NRO com RomFS, ícone, NACP, SHA-256 e `switch2-build.json` com o commit de origem. A verificação de empacotamento não substitui o teste em hardware.

No ounce-hbmenu, abra Netloader. Envie o NRO com nxlink para o IP e porta exibidos. Verifique título, controles, áudio, gameplay, save/load, backup/restauração Google Drive, idiomas e proporções. Se falhar, preserve `sdmc:/switch/heroes_lore_3/boot.log` e os demais logs do jogo.

Após confirmação no console: integrar o job reutilizável no workflow multiplataforma e publicar o NRO de Switch 2 junto aos arquivos existentes.
