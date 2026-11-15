# Nintendo Switch 2

`heroes_lore_3_switch2.nro` foi aprovado no console em 15/11/2026, a partir da build `49b6629` (CI `43693074812`). O alvo agora integra o workflow multiplataforma. Pushes na main e PRs geram builds; tags `v*` ou publicação manual geram a release com as cinco plataformas.

## Arquitetura e dependências

O runtime C++17, `ArmFastCpu` e Unicorn TCI do HL3 permanecem os mesmos. TCI conserva o limite de 32 MiB e o adaptador de memória de dados do Switch. O alvo `HL3_SWITCH2` usa a toolchain oficial do [ounce-devkit](https://github.com/ouncebrew/ounce-devkit), libounce e OpenGL ES via [OunceVK](https://github.com/ouncebrew/OunceVK). SDL2 e bibliotecas C compatíveis vêm dos portlibs devkitPro. A busca prioriza `portlibs/switch2` e libounce, evitando os drivers Mesa de Switch 1.

SDK: `20261115`, instalador `0f38ba80d7ee768c5749278f127749d14ece2036`, container `devkitpro/devkita64:20260219`. Unicorn TCI continua fixado no commit existente no projeto. Apenas os probes de configuração passam a usar libounce.

## Saves e atualizador

Mantidos o formato e a pasta `sdmc:/switch/heroes_lore_3`, além dos serviços Google Drive existentes. A build procura exclusivamente `heroes_lore_3_switch2.nro` nas releases; nunca baixa o NRO de Switch 1. O fluxo de restauração não foi alterado. Nenhum save pessoal ou credencial é incluído no pacote.

## Compilar e testar

Faça push na main e abra o workflow **Build Multiplatform & Release**, ou execute **Nintendo Switch 2 NRO** manualmente. O artefato inclui NRO com RomFS, ícone, NACP, SHA-256 e `switch2-build.json` com o commit de origem. A verificação de empacotamento não substitui o teste em hardware.

No ounce-hbmenu, abra Netloader. Envie o NRO com nxlink para o IP e porta exibidos. Verifique título, controles, áudio, gameplay, save/load, backup/restauração Google Drive, idiomas e proporções. Se falhar, preserve `sdmc:/switch/heroes_lore_3/boot.log` e os demais logs do jogo.

A release inclui o NRO de Switch 2 e seu SHA-256 junto aos demais arquivos. A versão vem da mesma fonte do port, sem edição duplicada.

## Armadilha de linkagem SDL2

A primeira build compilou, mas falhou no console: o pacote `sdl2.pc` do Switch 1 contem caminhos absolutos para libnx e Mesa de Switch 1, que o CMake transforma em dependencias absolutas. O alvo Switch 2 usa bibliotecas explicitas em vez desse arquivo. `verify_switch2_package.py` exige o registro MOD0 `OUN0` (plataforma 2, versao 1) emitido pelo startup real da libounce e verifica o mapa de linkagem. A build antiga 043b047 deve ser descartada. Um marcador isolado adicionado ao arquivo nao corrige binarios de Switch 1.
