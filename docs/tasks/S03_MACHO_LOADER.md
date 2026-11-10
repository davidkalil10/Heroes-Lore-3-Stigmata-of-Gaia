# Etapa S03: Loader Mach-O ARMv6 e HLE Mínima Headless

## Objetivo

Implementar o carregador de binário Mach-O 32-bit ARMv6 (`Payload/Heroes Lore.app/Heroes Lore`), despacho de mensagens Objective-C 2.0 (`objc_msgSend`, `objc_msgSend_stret`, `objc_msgSendSuper2`), subsistemas mínimos de HLE (`Foundation`, `UIKit`, `EAGL`, `libc/libstdc++`, `OpenAL`, `sqlite3`) e executar o boot do jogo de forma determinística em modo headless até `didFinishLaunchingWithOptions`, inicialização da `GLView`/`TouchResponder` e execução dos primeiros ciclos de frame (`onTimer:`).

## Arquitetura Implementada

1. **Machine (`tools/hl3rt/machine.py`)**:
   - Mapeamento de memória baseado no contrato `docs/RUNTIME_CONTRACT.md`:
     - Imagem: `0x00001000..0x000EA000` (segmentos `__TEXT`, `__DATA`, `__LINKEDIT`)
     - Heap guest: `0x10000000..0x18000000` (128 MiB)
     - Região de Classes HLE: `0x38000000..0x38400000` (4 MiB)
     - Traps de Imports: `0x7F000000..0x7F100000` (1 MiB)
     - Stub de Retorno / Host Stop: `0x7F100000`
     - Pilha guest: `0xBFF00000..0xC0000000` (1 MiB)
   - Execução via CPU ARMv6 (Unicorn 1176 / ARMv6) com VFP ativado (`FPEXC=0x40000000`).
   - Resolução das 762 relocações externas (`extrel`) e bind das tabelas indiretas (`__nl_symbol_ptr`, `__la_symbol_ptr`, stubs).
   - Hook determinístico de traps e contadores por fase (`boot`, `launch`, `frame`).

2. **Runtime Objective-C 2.0 (`tools/hl3rt/objc.py`)**:
   - Registro de 22 classes compiladas no binário do jogo (`AppDelegate`, `GLView`, `TouchResponder`, `SocketBtDelegate`, `EAMTX_*`, `DMG*`, `SBJson*`).
   - Mapeamento de 405 métodos nativos com suporte tanto a instruções ARM quanto Thumb (bit 0 do IMP preservado).
   - Despacho transparente de `_objc_msgSend`, `_objc_msgSendSuper2` e `_objc_msgSend_stret` com forward para IMPs reais da imagem ou execução de handlers HLE nativos.
   - Gerenciamento de classes sintéticas de Foundation/UIKit (`NSObject`, `NSString`, `NSArray`, `NSDictionary`, `NSNumber`, `NSData`, `UIView`, `UIWindow`, etc.).

3. **HLE de Subsistemas (`tools/hl3rt/`)**:
   - `hle_libc.py`: funções de soft-float VFP (`___adddf3vfp`, `___muldf3vfp`, etc.), aritmética inteira 64-bit/32-bit (`___divdi3`, `___moddi3`, `___divsi3`, `___umodsi3`), strings e memória (`malloc`, `free`, `memcpy`, `memset`, `strcmp`, `strlen`, etc.), formatação `printf`/`sprintf`/`vsnprintf`, SJLJ unwinding stubs e I/O de arquivos (`fopen`, `fread`, `fwrite`, `fclose`, etc.) com mapeamento seguro de caminhos para o bundle e `Documents/`.
   - `hle_foundation.py`: hierarquia `NSObject`, strings `NSString`/`NSMutableString`/`CFString`, coleções `NSArray`/`NSMutableArray`, `NSDictionary`/`NSMutableDictionary`, `NSNumber`, `NSData`, `NSBundle` com leitura de `Info.plist`, `NSUserDefaults`, `NSFileManager`, `NSDate`, `NSLocale`, `NSThread`.
   - `hle_uikit.py`: ciclo de vida `UIApplicationMain` -> `UIApplicationDelegate`, orientação paisagem `statusBarOrientation`, métricas de tela `UIScreen` (320x480 lógico / 480x320 paisagem), `UIDevice`, hierarquia de views `UIWindow`, `UIView`, `GLView`, `TouchResponder`, contexto `EAGLContext` e buffers `CAEAGLLayer`.
   - `hle_gl.py`: shims de OpenGL ES 1.1 (`glGenTextures`, `glGenFramebuffersOES`, `glGenRenderbuffersOES`, queries de viewport e estado, contadores de chamadas).
   - `hle_audio.py`: shims de OpenAL e AudioSession.

4. **Boot e Execução Headless (`tools/boot_trace.py`)**:
   - Execução dos 4 inicializadores C++ de `__mod_init_func`.
   - Salto para `start` (`0x0000a438`), configuração da pilha de inicialização e captura de `UIApplicationMain`.
   - Invocação de `application:didFinishLaunchingWithOptions:`.
   - Ticks de frame via `onTimer:` em `AppDelegate`, gerando chamadas de desenho GLES1 e invocação de `presentRenderbuffer:`.
   - Simulação de toques via `TouchResponder touchesBegan:withEvent:`.

## Critérios de Aceitação

- [x] Binário Mach-O carregado e mapeado em memória sem erros de segmentação ou escrita unmapped.
- [x] 4 inicializadores `__mod_init_func` executados com sucesso.
- [x] Ponto de entrada `start` executa até a chamada de `UIApplicationMain`.
- [x] `AppDelegate application:didFinishLaunchingWithOptions:` instancia `UIWindow`, `GLView` e `TouchResponder`.
- [x] Arquivos de configuração `flags.ini` e `options.ini` criados/acessados via `fopen` em `Documents/`.
- [x] Execução de múltiplos frames com `onTimer:` apresentando frames via `presentRenderbuffer:`.
- [x] Teste automatizado determinístico adicionado à suite de testes.

