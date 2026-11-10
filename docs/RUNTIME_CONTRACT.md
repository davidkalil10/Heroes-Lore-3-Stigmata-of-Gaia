# Contrato de Runtime - Heroes Lore 3 (Mach-O ARMv6 + iOS HLE) - `S02`

Fonte: `python tools/probe_macho_contract.py` (determinístico, verificado por `--check` dentro de `tools/verify_workspace.py`).
Artefatos gerados em `reference/analysis/`:

| Arquivo | Conteúdo |
| --- | --- |
| `macho_import_bindings.json` | Os 389 imports com endereço de stub, `__nl_symbol_ptr`, `__la_symbol_ptr`, relocações externas de dados e call sites `bl`. |
| `objc_classes_detailed.json` | 22 classes ObjC 2.0 (`class_t` + `class_ro_t`), 405 métodos, ivars, protocolos, selrefs (493) e classrefs (75). |
| `resource_formats.json` | Cabeçalhos de `.pvr`, `.rff`, `.spr`, `dat/InGame_txt`, famílias de `dat/boss/comm` e áudio. |
| `RUNTIME_CONTRACT_IMPORTS.md` | Apêndice: lista completa de imports por subsistema. |

> [!IMPORTANT]
> Correções em relação aos documentos anteriores (valores medidos, não estimados):
> - `__mod_init_func` tem **4** ponteiros (`0x00002b60`, `0x00005380`, `0x00007bd8`, `0x0000a404`) em `0x000cc01c`, **não 27**.
> - `__objc_classlist` contém **22** classes (88 bytes); as "30" de S01 eram strings em `__objc_classname` (inclui nomes de protocolos).
> - As texturas `.pvr` são **todas não comprimidas** (`RGBA8888` ou `AI88`), **nenhuma PVRTC**. Não é necessário decodificador PVRTC.
> - Áudio: 20 MP3 = `bgm0_mf`..`bgm18_mf` (19) + `battle_cry.mp3`; 28 WAV `sd000_mf.wav`.. (todos em `snd/`).
> - Não há nenhum import de zlib (`libz` é linkada mas sem símbolos usados).

## 1. Imagem Mach-O

- Mach-O 32-bit, **não-PIE**, base fixa `0x1000`; sem relocações locais (`nlocrel=0`). Só há **762 relocações externas** (`extrel`) cujo `r_address` é relativo ao primeiro segmento (`__PAGEZERO` = 0, ou seja, endereço guest absoluto). Elas ligam 66 símbolos importados a palavras de dados (`__objc_classrefs`, `__objc_data` isa/superclass, `__cfstring` isa = `___CFConstantStringClassReference`, vtables C++).
- Entry: `LC_UNIXTHREAD` `pc=0x0000a438`, `sp=0`, demais registradores 0. O loader deve montar a pilha no estilo `crt1` (`[argc][argv…,0][envp…,0][apple…,0]`).
- Tabelas indiretas (753 entradas): `__symbol_stub4` (310 stubs de 12 bytes, `reserved1=0`), `__nl_symbol_ptr` (133 ponteiros, `reserved1=310`), `__la_symbol_ptr` (310 ponteiros, `reserved1=443`). 117 entradas são `INDIRECT_SYMBOL_LOCAL/ABS` (ponteiros para símbolos locais; já contêm o valor correto, não precisam de bind). 3 entradas de `__nl_symbol_ptr` apontam para símbolos **definidos** no próprio binário (`PVRTByteSwap`, statics de `PVRTIsLittleEndian`) e também não precisam de trap (ver `defined_symbol_pointers` no JSON).
- Todos os IMPs ObjC decodificados são **ARM** (bit 0 = 0). Nenhum Thumb identificado nos métodos ObjC; a varredura `bl` do probe é só ARM-mode (heurística). Thumb em `__text` não foi excluído: a camada de CPU deve suportar Thumb mesmo assim.
- Compilado com VFP soft-float helpers (`___adddf3vfp`, `___muldf3vfp`, …): precisam existir como funções HLE ou ser executados de uma libgcc fornecida.
- Exceções C++/ObjC são **SJLJ** (`__Unwind_SjLj_Register/Resume/Unregister`, `___gxx_personality_sj0`, `___objc_personality_v0`). Como o código só registra/desregistra contexto, o HLE pode manter uma pilha de contextos SJLJ ou transformar `Register/Unregister` em operações de lista encadeada em memória guest; `Resume` precisa de implementação real.

## 2. Mapa de memória guest proposto

| Região | Endereço | Tamanho | Uso |
| --- | --- | ---: | --- |
| Guard / NULL | `0x00000000` | `0x1000` | `__PAGEZERO` (fault em acesso) |
| Imagem `__TEXT` | `0x00001000` | `0xCB000` | código + cstrings (R-X) |
| Imagem `__DATA` | `0x000CC000` | `0x16000` | dados, `__objc_*`, `__common`, `__bss` (RW) |
| `__LINKEDIT` | `0x000E2000` | `0x8000` | opcional (só necessário para debug) |
| Heap guest (`malloc`) | `0x10000000` | 128 MiB (reserva; commit sob demanda) | `malloc`/`free`, `NSObject` raw |
| Objetos HLE ObjC | `0x30000000` | 32 MiB | `isa` + ivars de objetos Foundation/UIKit emulados (layout compatível com `instance_size`) |
| Classes HLE ObjC | `0x38000000` | 4 MiB | `class_t` sintéticos de classes importadas (`NSObject`, `UIView`, …) e metaclasses |
| Strings/constantes HLE | `0x39000000` | 4 MiB | selectors/CFString/NSString sintéticos |
| Trap de imports | `0x7F000000` | `8 * 389` (+ folga) | um slot de 8 bytes por import; `__la_symbol_ptr`/`__nl_symbol_ptr`/stubs resolvem para cá |
| Trap de retorno | `0x7F100000` | `0x1000` | `lr` de `__mod_init_func`, `main`, callbacks (`return-to-host`) |
| Pilha da thread principal | `0xBFF00000..0xC0000000` | 1 MiB | topo `0xBFFFF000` (convenção iOS) |

Política de traps: a execução chega ao slot de import → hook de fetch → o host lê `r0-r3`/pilha, executa a HLE, escreve `r0/r1` (e `s0/d0` se necessário) e emula `bx lr`. O mesmo mecanismo vale para `objc_msgSend`, que despacha por `(classe, seletor)` para (a) IMP real da imagem (22 classes do jogo) ou (b) handler HLE (classes importadas). Contadores de HLE por import e por fase são obrigatórios desde o primeiro commit do loader (`boot/title/field/combat/menu`).

## 3. Sequência de boot (derivada de seletores, ivars e call sites; **a confirmar em `S03`**)

1. Loader mapeia segmentos, aplica as 762 relocações externas, resolve `__nl_symbol_ptr`/`__la_symbol_ptr`/stubs para traps.
2. Executa os 4 inicializadores `__mod_init_func` (`0x2b60`, `0x5380`, `0x7bd8`, `0xa404`) com `lr` = trap de retorno.
3. Salta para `0x0000a438` (`start`) → `main` → `UIApplicationMain` (único call site: `0x0000bbcc`, stub `0x000bdd1c`, ponteiro lazy `0x000cc290`).
4. HLE de `UIApplicationMain`: cria `UIApplication`, `NSAutoreleasePool`, instancia `AppDelegate` (class_t em `objc_classes_detailed.json`), cria `UIWindow` e chama `application:didFinishLaunchingWithOptions:` (`0x0000a6c4`) / `applicationDidFinishLaunching:` (`0x0000a6f0`) / `startAppWithOptions:application:` (`0x0000aba0`).
5. O jogo cria `GLView` (`initWithFrame:` `0x0000b3a0`, `layerClass` `0x0000b5a4` → `CAEAGLLayer`, `createFramebuffer` `0x0000b6d0`) com `EAGLContext` (GLES1) e `TouchResponder` (`init` `0x0000bbfc`; `touchesBegan/Moved/Ended:withEvent:` `0x0000c0a4/0x0000bc1c/0x0000be5c`).
6. Loop de frame: `NSTimer scheduledTimerWithTimeInterval:target:selector:` com alvo `AppDelegate onTimer:` (`0x0000a5c4`), que chama o `Game` C++ (`game_instance`, `game_config_instance`). Cada tick termina com `presentRenderbuffer:` no `EAGLContext`; o host troca o buffer SDL nesse ponto.
7. Lifecycle: `applicationWillResignActive` (`0xa4e8`), `DidEnterBackground` (`0xa520`), `WillEnterForeground` (`0xa53c`), `DidBecomeActive` (`0xa504`), `WillTerminate` (`0xa668`) mapeados a eventos da janela (foco/minimizar/fechar) para suportar Switch/Android.

## 4. Imports por subsistema (389) e política de implementação

| Grupo | Qtd | Política |
| --- | ---: | --- |
| `opengles1` | 158 | HLE 1:1 sobre GL desktop (compat/ES1 shim) ou renderer próprio; inclui `kEAGLColorFormatRGBA8`, `kEAGLDrawableProperty*`. Estado de matriz, arrays de cliente, `glDrawArrays/Elements`, blend, textura 2D; FBO OES apenas para o framebuffer de apresentação. Tabela completa no apêndice. |
| `objc_foundation` | 66 | HLE mínima por **uso real**: `NSObject/NSString/NSMutableString/NSArray/NSMutableArray/NSDictionary/NSMutableDictionary/NSNumber/NSData/NSMutableData/NSBundle/NSFileManager/NSUserDefaults/NSTimer/NSAutoreleasePool/NSLocale/NSDate/NSURL…`, `CFBundle*`, `CFString*`, `NSHomeDirectory`, `NSSearchPathForDirectoriesInDomains`, `NSLog`, `objc_msgSend/_stret/Super2`, `objc_getProperty/setProperty`, `objc_sync_*`. `NSBundle`/`CFBundleCopyResourceURL` mapeiam para a pasta `Payload/Heroes Lore.app` extraída; `NSHomeDirectory`/`Documents` → diretório de saves do host (ID exclusivo do HL3). |
| `uikit_eagl_quartzcore` | 21 | HLE funcional apenas para `UIApplication(Main)`, `UIWindow`, `UIView`, `UIScreen` (480x320 lógico; escala no host), `UIDevice`, `UIViewController`, `CAEAGLLayer`, `EAGLContext`. `UIWebView/UIAlertView/UIButton/UILabel/UITextField/UIImage(View)/UIFont/UIColor/UIActivityIndicatorView`: stubs que respondem a seletores sem efeito (usados só por DMG/EAMTX e entrada de nome); entrada de texto (`myTextField`) via diálogo nativo do host. |
| `audio_openal_avfoundation` | 24 | `alc*/al*` → mixer próprio (28 WAV 44.1 kHz mono/estéreo 16-bit PCM, pré-decodificáveis); `AVAudioPlayer` → streaming de MP3 com `dr_mp3` em thread própria; `AudioSession*` → no-op bem-sucedido. **Nada de decodificação no callback SDL.** |
| `sqlite3` | 14 | Linkar SQLite3 real no host (amalgamation) e expor `sqlite3_open/prepare(_v2)/step/reset/bind_*/column_*/changes/last_insert_rowid/errmsg/finalize/close`; caminho em `Documents/`. |
| `libc_cxx` | 102 | libc: HLE direta (`malloc/free/mem*/str*/printf/fopen…` com paths mapeados ao bundle/saves); sockets (`socket/connect/send/recv/bind/select/gethostbyname/getifaddrs`), `sysctlbyname`, `host_statistics`, `mach_host_self`, `host_page_size`: falhar de forma determinística (offline). libgcc VFP helpers e `__divsi3/__modsi3/__umodsi3/__divdi3/__moddi3`: implementar nativamente. C++ ABI (`__cxa_atexit`, `__cxa_pure_virtual`, `__cxa_rethrow`, `_ZNSs*` = `std::string`, vtables `__cxxabiv1`, SJLJ personality): HLE própria. `CC_MD5`: implementar. |
| `offline_neutralized` | 4 | `GKSession/GKPeerPickerController`, `SCNetworkReachability*`: retornam "sem rede/ sem sessão". `EAMTX_*`, `DMG*`, `SBJson*` (código do jogo) rodam sobre `NSURLConnection` neutralizado (erro imediato). |
| `zlib` | 0 | Nenhum import. |

## 5. Entrada (impacto direto no objetivo "teclado + gamepad")

O jogo é 100% **touch**: a única entrada é `TouchResponder touches{Began,Moved,Ended}:withEvent:` (ivar `eventID`). Portanto teclado e gamepad serão implementados como **toques sintéticos** injetados nessas três IMPs (objetos `UITouch`/`NSSet`/`UIEvent` HLE com `locationInView:` em coordenadas 480x320). Para isso, `S04` precisa mapear, a partir do código do `Game` e de `hud.spr`/`user_interface_with_alpha.spr`, as regiões de toque de D-pad, botões de ação e menus. Esse mapeamento é o principal risco do plano de PC jogável e não é derivável só dos metadados ObjC.

## 6. Formatos de recursos (resumo; detalhes em `resource_formats.json`)

- **`.pvr`** (16 arquivos): header legado **PVR v2 de 52 bytes** (`tag` `PVR!`), `flags & 0xFF` = formato (`0x12` = `RGBA8888`, `0x17` = `AI88`), bit `0x8000` = alpha, bit `0x100` = mipmaps (`menu_bkg` e fontes `_cn/_ko` têm 8–10 mips), nenhum twiddled, `header_size + data_size == file_size` em todos. Upload direto com `glTexImage2D` (`GL_RGBA/UNSIGNED_BYTE` e `GL_LUMINANCE_ALPHA/UNSIGNED_BYTE`).
- **`.spr`** (6 arquivos junto aos `.pvr`): tabelas big-endian de retângulos/frames de atlas (ex.: `hud.spr` 10620 bytes). Estrutura ainda não decodificada → `S04`.
- **`.rff`** (10 fontes): `21 01 02` + 5 bytes de cabeçalho, `u16be` contagem de glifos em `@8`, `u16be` segunda contagem em `@10` (igual à primeira, exceto `font_hud` = 141/197), `u16be` code points ordenados a partir de `@12`; os bytes restantes são **12 bytes por glifo** (15,18 em `font_hud`, formato distinto). Campos de métrica não decodificados → `S09`.
- **`dat/InGame_txt`**: `u16le tamanho_total`, `u16le contagem (243)`, `u16le offsets[243]` absolutos (tabela termina em 490), strings NUL-terminadas em inglês (`MENU`, `STATUS`, `INVENTORY`, …). Base para o parser de `S09`.
- **`dat/*`**: famílias `_pa` (230 arquivos, 201 612 bytes), `_dat` (35), `_hop` (1), `_txt` (1). **`boss/`**: `_bm` (5), `_cif` (5), `_dat` (2). **`comm/`**: `_bm` (6; `ui_bm`, `item_bm`, …). Binários proprietários **não decodificados** em S02 (apenas cabeçalhos de 16 bytes amostrados); o significado de `.bm/.cif/.pa/_dat` fica para `S04`/`S05`, conforme o código do jogo os consumir.
- **Áudio**: 28 WAV PCM (`format_tag=1`, 44.1 kHz, 16-bit, mono ou estéreo), 20 MP3.
- **Localização**: `en.lproj/Localizable.strings` são **bplist** (não texto); strings de jogo em `dat/InGame_txt`.

## 7. Critérios de saída de `S02` → entrada de `S03`

- Todos os 389 imports têm stub/ptr/reloc catalogados (`verify`: nenhum import sem referência).
- Todas as classes do jogo têm IMPs ARM listadas; ponto de partida do HLE: `AppDelegate`, `GLView`, `TouchResponder`.
- `S03` deve começar por: loader + relocações externas, traps de imports com contadores, `objc_msgSend` despachando para IMPs reais, `main` até `didFinishLaunching`, e um trace de boot determinístico.
