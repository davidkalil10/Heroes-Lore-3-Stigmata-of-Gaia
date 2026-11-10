# Arquitetura do Heroes Lore 3 Recomp

## 1. Anatomia do Artefato de Entrada (`S01`)

O arquivo de entrada `Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk` (`61.755.432` bytes) possui duas camadas:

1. **Camada Externa (APK Android wrapper touchHLE)**:
   - Contém `lib/arm64-v8a/libtouchHLE.so`, `libSDL2.so`, `libc++_shared.so`, `assets/touchHLE_dylibs/*`, `assets/touchHLE_fonts/*` e `assets/touchHLE_default_options.txt` (`com.ea.heroesloresg.bv: --landscape-left`).
   - Embute o pacote iOS original intacto em `assets/game.ipa` (`43.686.130` bytes).
2. **Camada Interna (`assets/game.ipa` -> `Payload/Heroes Lore.app/`)**:
   - **Metadados (`Info.plist`)**:
     - `CFBundleIdentifier`: `com.ea.heroesloresg.bv`
     - `CFBundleExecutable`: `Heroes Lore`
     - `CFBundleVersion`: `1.0.1`
     - `MinimumOSVersion`: `3.0` (`iphoneos4.1` SDK, Xcode `0324`)
     - `UIInterfaceOrientation`: `UIInterfaceOrientationLandscapeRight` (480x320 base)
     - `CFBundleLocalizations`: `['en', 'de', 'es', 'fr', 'it', 'ko', 'zh']`
   - **Binário Executável (`Payload/Heroes Lore.app/Heroes Lore`)**:
     - Formato: **Mach-O 32-bit ARMv6 (`MH_EXECUTE`)**, `915.824` bytes, SHA-256 `39c6ed1db25cbf6020c9edd2cfa145410ff391f3361a9150d9646b9a4fada849`.
     - Segmentos de memória:
       - `__PAGEZERO`: `0x00000000` (`0x001000` bytes)
       - `__TEXT`: `0x00001000` (`0x0cb000` bytes, arquivo `0x000000..0x0cb000`)
         - `__text`: `0x00002b60` (`0x0bb0cc` bytes de código ARMv6/Thumb)
         - `__symbol_stub4`: `0x000bdc2c` (`0x0015d0` bytes)
         - `__cstring`: `0x000bf1fc` (`0x004ec5` bytes)
         - `__objc_methname`, `__objc_methtype`, `__objc_classname`, `__const`, `__ustring`, `__gcc_except_tab`
       - `__DATA`: `0x000cc000` (`0x016000` bytes em VM, `0x00d000` bytes em arquivo `0x0cb000..0x0d8000`)
         - `__mod_init_func`: `0x000cc000` (`0x00006c` bytes = 27 inicializadores estáticos C++)
         - `__nl_symbol_ptr` e `__la_symbol_ptr`: ponteiros de símbolos importados
         - `__cfstring`, `__objc_classlist` (30 classes), `__objc_const`, `__objc_selrefs` (679 seletores), `__objc_classrefs`, `__objc_data`
         - `__data`, `__common`, `__bss`
       - `__LINKEDIT`: `0x000e2000` (`0x008000` bytes em VM, arquivo `0x0d8000..0x0df970`)

## 2. Subsistemas do Jogo Original (Imports e Classes Objective-C)

- **Classes de Ciclo de Vida e Renderização (`UIKit` / `QuartzCore` / `OpenGLES`)**:
  - `AppDelegate`: gerencia o ciclo de vida do aplicativo iOS, timers e transições.
  - `GLView` e `TouchResponder`: configuram `CAEAGLLayer` / `EAGLContext`, framebuffers (`glBindFramebufferOES`, `glBindRenderbufferOES`) e despacham eventos de toque (`touchesBegan:withEvent:`, `touchesMoved:withEvent:`, `touchesEnded:withEvent:`).
- **Gráficos (`OpenGL ES 1.1`)**:
  - Usa pipeline fixo 2D ortográfico (`glOrthof`, `glMatrixMode`, `glLoadIdentity`, `glTranslatef`, `glScalef`, `glRotatef`, `glColor4f`, `glColor4ub`, `glEnableClientState`, `glVertexPointer`, `glTexCoordPointer`, `glColorPointer`, `glDrawArrays`, `glDrawElements`, `glTexImage2D`, `glCompressedTexImage2D`).
- **Áudio (`OpenAL` + `AVFoundation` / `AudioToolbox`)**:
  - `OpenAL`: efeitos sonoros `.wav` (`alcOpenDevice`, `alcCreateContext`, `alGenBuffers`, `alBufferData`, `alGenSources`, `alSourcei`, `alSourcef`, `alSourcePlay`, `alSourceStop`).
  - `AVAudioPlayer`: reprodução das 20 faixas de música `.mp3` em `snd/bgm0_mf.mp3`..`bgm16_mf.mp3`.
- **Persistência e Dados (`Foundation` / `SQLite3` / `zlib` / `libc`)**:
  - Arquivos de dados do jogo em `dat/`, `boss/`, `comm/`, `newres/sprites/*.pvr`, `newres/fonts/*.rff`.
  - Banco SQLite (`trackingdb.sqlite` via `sqlite3_open`, `sqlite3_prepare_v2`, `sqlite3_step`, `sqlite3_Column_*`) e preferências/saves em `Documents/` e `NSUserDefaults`.
- **Rede / Telemetria Legada (`EAMTX_*`, `DMG*`, `SocketBtDelegate`)**:
  - Módulos de microtransação/rede da EA Mobile (`EAMTX_Network`, `DMGController`, `SCNetworkReachability*`) que devem ser neutralizados/bypassados de forma offline segura na nossa HLE.

## 3. Organização Modular do Nosso Runtime (`src/`)

- `src/core/`:
  - `macho_loader`: mapeamento determinístico de `__TEXT`, `__DATA`, `__LINKEDIT`, execução de `__mod_init_func` e tabela de traps para `__symbol_stub4` / `__la_symbol_ptr` / `__nl_symbol_ptr`.
  - `cpu`: motor de execução ARMv6/Thumb (Unicorn + blocos quentes AOT em C++ com guarda de opcodes e watchdog persistente).
  - `guest_mem`: gerenciador de memória guest 32-bit, alocador (`malloc`/`free`/`operator new`) e proteção de páginas.
- `src/hle/`:
  - `objc_runtime`: despachante `_objc_msgSend` / `_objc_msgSendSuper2`, registro de classes/seletores, `NSString`/`CFString`, `NSArray`, `NSDictionary`, `NSBundle`, `NSFileManager`, `NSUserDefaults`, `NSTimer`.
  - `uikit_eagl`: HLE de `UIApplicationMain`, `UIView`, `CAEAGLLayer`, `EAGLContext` e entrega de eventos de toque/teclado/gamepad para `GLView` / `TouchResponder`.
  - `gles1_hle`: implementação host das chamadas OpenGL ES 1.1, decodificador de texturas `.pvr` (PVRTC/RGBA) e expansão de viewport True Widescreen.
  - `sqlite_zlib_libc`: ponte para `libc`, `libstdc++`, `zlib` e `sqlite3`.
- `src/audio/`:
  - Mixer de áudio SDL2 desacoplado do callback, com suporte às fontes OpenAL (`.wav`) e ao `AVAudioPlayer` (`dr_mp3` para `.mp3`).
- `src/platform/`:
  - Caminhos de plataforma (`Windows`, `Linux`, `Android`, `Switch`), Menu Nativo de Opções (`F10`/`R3`/botão), Localização em tempo real (`EN` / `PT-BR`), Cloud Save (`Google Drive`) e Atualizador (`GitHub Releases`).
