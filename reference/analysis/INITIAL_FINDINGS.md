# Initial Static Findings - Heroes Lore 3: Stigmata of Gaia (`S01`)

Este arquivo é gerado deterministicamente por `python tools/inspect_apk.py`.

## 1. Pacote Externo (`.apk`) e Wrapper touchHLE

- **Arquivo**: `Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk`
- **Tamanho**: `61755432` bytes
- **SHA-256**: `2a96e02ab65e8c4180b91cfd7bb3c403f99bc239b8edd522af65278504458016`
- **Entradas ZIP**: `50`
- **Natureza do APK**: Wrapper Android do emulador de iOS **touchHLE** (`lib/arm64-v8a/libtouchHLE.so`, `libSDL2.so`, `libc++_shared.so`) contendo o pacote iOS em `assets/game.ipa`.
- **`assets/touchHLE_default_options.txt`**: `com.ea.heroesloresg.bv: --landscape-left`

## 2. Pacote iOS Interno (`assets/game.ipa`)

- **Tamanho**: `43686130` bytes
- **SHA-256**: `56dd27225077df0afa61082d5e40590a7dea730c16d47ed6b7ea93f9725558f3`
- **Entradas ZIP**: `1819`
- **Bundle Identifier**: `com.ea.heroesloresg.bv`
- **Versão**: `1.0.1` (`MinimumOSVersion` `3.0`)
- **Orientação Inicial**: `UIInterfaceOrientationLandscapeRight` (`480x320` paisagem)
- **Localizações Inclusas**: `en, de, es, fr, it, ko, zh`

## 3. Executável Mach-O 32-bit ARMv6 (`Payload/Heroes Lore.app/Heroes Lore`)

- **Tamanho**: `915824` bytes
- **SHA-256**: `39c6ed1db25cbf6020c9edd2cfa145410ff391f3361a9150d9646b9a4fada849`
- **Header Mach-O**: `magic=0xfeedface`, `arch=ARMv6 32-bit`, `ncmds=31`, `uuid=a3b6b11d4ef7030052ad81c95b16f8d7`
- **Entrypoint (`LC_UNIXTHREAD` PC)**: `0x0000a438`
- **Inicializadores C++ (`__mod_init_func`)**: `4` funções (`0x000cc000`)
- **Classes Objective-C**: `30` (`AppDelegate`, `GLView`, `TouchResponder`, `SocketBtDelegate`, `EAMTX_*`, `DMG*`, `SBJson*`)
- **Seletores Objective-C**: `679`
- **Símbolos (`LC_SYMTAB`)**: `394` total (`5` definidos, `389` importados)
