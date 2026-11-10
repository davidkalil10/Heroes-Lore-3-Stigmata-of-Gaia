# Etapa S12: Port Android ARM64 Nativo e CI Multiplataforma

## Objetivo

Configurar o empacotamento Android ARM64 nativo (`com.davidbraga.heroeslore3`) usando nosso próprio runtime C++/SDL2 (substituindo o wrapper `libtouchHLE.so` do APK de entrada) e configurar o workflow `.github/workflows/build.yml` para compilar os 4 alvos:

- `windows-x64`
- `linux-x86_64`
- `android-arm64`
- `nintendo-switch` (`0100F3A300030000`)

## Referências nos Snapshots

- `reference/hl4_snapshot/.github/workflows/build.yml`
- `reference/hl5_snapshot/.github/workflows/build.yml`
- `reference/hl4_snapshot/android/`
