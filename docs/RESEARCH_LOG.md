# Research Log - Heroes Lore 3 Recomp

Registro cronológico das descobertas técnicas sobre o pacote e o binário de **Heroes Lore 3: Stigmata of Gaia**.

---

## 2026-11-09 - Sessão 01 (`S00` + `S01`): Inspeção Estática do APK, `assets/game.ipa` e Executável Mach-O ARMv6

- **Relatório completo da sessão**: `research/sessions/2026-11-09-01.md`
- **Resumo das descobertas**:
  1. **Estrutura do APK externo (`Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk`)**:
     - Tamanho: `61.755.432` bytes | SHA-256: `2a96e02ab65e8c4180b91cfd7bb3c403f99bc239b8edd522af65278504458016`.
     - Contém `50` entradas e trata-se de um port Android baseado no emulador de iOS **touchHLE** (`lib/arm64-v8a/libtouchHLE.so`, `libSDL2.so`, `libc++_shared.so`).
     - O arquivo `assets/touchHLE_default_options.txt` define:
       `com.ea.heroesloresg.bv: --landscape-left`
  2. **Pacote iOS embutido (`assets/game.ipa`)**:
     - Tamanho: `43.686.130` bytes | SHA-256: `56dd27225077df0afa61082d5e40590a7dea730c16d47ed6b7ea93f9725558f3`.
     - Arquivo ZIP válido com `1.819` entradas sob `Payload/Heroes Lore.app/`.
     - `Payload/Heroes Lore.app/Info.plist`:
       - `CFBundleIdentifier`: `com.ea.heroesloresg.bv`
       - `CFBundleExecutable`: `Heroes Lore`
       - `CFBundleVersion`: `1.0.1`
       - `MinimumOSVersion`: `3.0` (`iphoneos4.1`)
       - `UIInterfaceOrientation`: `UIInterfaceOrientationLandscapeRight`
       - `CFBundleLocalizations`: `['en', 'de', 'es', 'fr', 'it', 'ko', 'zh']`
  3. **Binário Executável (`Payload/Heroes Lore.app/Heroes Lore`)**:
     - Tamanho: `915.824` bytes | SHA-256: `39c6ed1db25cbf6020c9edd2cfa145410ff391f3361a9150d9646b9a4fada849`.
     - Cabeçalho Mach-O 32-bit: `magic = 0xfeedface`, `cputype = 12` (ARM), `cpusubtype = 6` (ARMv6), `filetype = 2` (`MH_EXECUTE`), `ncmds = 31`, `sizeofcmds = 3768`.
     - Segmentos:
       - `__PAGEZERO`: `0x00000000..0x00001000`
       - `__TEXT`: `0x00001000..0x000cc000` (`__text` em `0x00002b60..0x000bdc2c`, `766.156` bytes de código)
       - `__DATA`: `0x000cc000..0x000e2000` (`__mod_init_func` em `0x000cc000` com 27 ponteiros; metadados Objective-C 2.0 e dados globais)
       - `__LINKEDIT`: `0x000e2000..0x000ea000`
     - Metadados Objective-C:
       - `30` classes declaradas em `__objc_classname` / `__objc_classlist` (incluindo `AppDelegate`, `GLView`, `TouchResponder`, `SocketBtDelegate`, `EAMTX_*`, `DMG*`, `SBJson*`).
       - `679` seletores em `__objc_methname` / `__objc_selrefs`.
     - Símbolos (`LC_SYMTAB`):
       - `394` símbolos na tabela (`5` definidos exportados/locais e `389` indefinidos importados de `UIKit`, `Foundation`, `CoreFoundation`, `QuartzCore`, `OpenGLES`, `OpenAL`, `AudioToolbox`, `AVFoundation`, `SystemConfiguration`, `libsqlite3.dylib`, `libz.1.dylib`, `libstdc++.6.dylib`, `libobjc.A.dylib`, `libSystem.B.dylib`).
  4. **Formatos de Recursos em `Payload/Heroes Lore.app/`**:
     - `dat/` (`606` arquivos): `InGame_txt`, `chapter_hop`, `char_dat`, `des_dat`, `drop_dat`, `enemy_dat`, `getitem_dat`, `i0_dat`..`i18_dat`, `pe000_pa`..`pe016_pa`, etc.
     - `boss/` (`66` arquivos): `boss0_cif`..`boss4_cif`, `boss9000_bm`..`boss9004_bm`, `boss_dat`, etc.
     - `comm/` (`102` arquivos): `attmsg_bm`, `inmenu_bm`, `item_bm`, `number_bm`, `shadow_bm`, `ui_bm`, etc.
     - `newres/sprites/*.pvr`: texturas PowerVR (`menu_bkg.pvr`, `hud.pvr`, `user_interface_with_alpha.pvr`, `end_screen_Kei.pvr`, `end_screen_Rits.pvr`, etc.).
     - `newres/fonts/`: métricas `.rff` e atlas `.pvr` (`font_hud`, `font_mainmenu`, `font_menu_medium`, `font_menu_small` e variantes `_cn`/`_ko`).
     - `snd/`: `20` arquivos `.mp3` (`bgm0_mf.mp3`..`bgm16_mf.mp3`) e `28` arquivos `.wav` na raiz/bundle.

---

## 2026-11-09 - Sessão 02 (`S02`): Contrato de Runtime Mach-O / iOS HLE e Formatos de Recursos

- **Relatório completo**: `research/sessions/2026-11-09-02.md`
- **Ferramenta**: `tools/probe_macho_contract.py` (`--check` integrado ao `tools/verify_workspace.py`).
- **Descobertas verificáveis**:
  1. `__mod_init_func`: **4** ponteiros em `0x000cc01c` (`0x2b60`, `0x5380`, `0x7bd8`, `0xa404`), não 27. Entry `pc=0x0000a438`.
  2. `__objc_classlist`: **22** classes / 405 métodos / 493 selrefs / 75 classrefs / 10 protocolos; todos os IMPs são ARM. Classes centrais: `AppDelegate` (`didFinishLaunching` `0xa6c4`, `onTimer:` `0xa5c4`), `GLView` (`initWithFrame:` `0xb3a0`), `TouchResponder` (`touchesBegan/Moved/Ended`).
  3. Tabelas indiretas: 753 entradas = 310 stubs (12 B) + 133 `__nl_symbol_ptr` + 310 `__la_symbol_ptr`; 117 entradas locais/abs; `UIApplicationMain` com 1 call site em `0xbbcc`. 762 relocações externas com `r_address` absoluto (relativo a `__PAGEZERO`).
  4. Grupos de imports: GLES1 158, libc/C++ 102, ObjC/Foundation/CF/CG 66, áudio (OpenAL/AVAudioPlayer/AudioSession) 24, UIKit/EAGL/QuartzCore 21, SQLite3 14, offline (GameKit/SystemConfiguration) 4, zlib 0.
  5. `.pvr`: 16 arquivos, PVR v2 (header 52 B), todos `RGBA8888` ou `AI88`, sem PVRTC; `.rff`: glifos `u16be` ordenados, 12 B/glifo restantes (`font_hud` diferente); `dat/InGame_txt`: `u16le` tamanho, `u16le` 243 strings, offsets absolutos; 28 WAV PCM 16-bit 44,1 kHz; 20 MP3 (`bgm0..18` + `battle_cry`).
  6. Exceções SJLJ e helpers VFP soft-float (`___adddf3vfp`…) exigem HLE/implementação nativa.
- **Hipóteses não confirmadas**: sequência exata de `main`/`start`; significado dos formatos `.spr`, `.bm`, `.cif`, `.pa`, `_dat`; métricas do `.rff`; regiões de toque do HUD.

---

## 2026-11-09 - Sessão 03 (`S03` + `S04` + `S05`): Loader Mach-O ARMv6, Host PC Playable (OpenGL ES 1.1 + Teclado/Gamepad)

- **Relatório completo**: `research/sessions/2026-11-09-03.md`
- **Descobertas verificáveis e correções críticas**:
  1. **Thumb vs ARM nos IMPs Objective-C**:
     - A análise estática de S02 supôs que métodos ObjC eram predominantemente ARM, mas a decodificação de baixo nível revelou que **368 de 405 métodos usam instruções Thumb (bit 0 = 1)**, enquanto apenas 37 usam ARM!
     - O despacho de `_objc_msgSend` foi corrigido para preservar o bit 0 do IMP, alternando a CPU Unicorn para modo Thumb (`CPSR bit 5`) transparentemente.
  2. **Inicializadores estáticos definidos pelo binário em `__nl_symbol_ptr`**:
     - O slot `0x000cc1dc` (`__ZZ18PVRTIsLittleEndianvE7bIsInit`) e `0x000cc1d8` são variáveis estáticas definidas pelo próprio binário (seção `__common` / `__data`). Ao carregar a imagem, estes slots devem ser pré-populados com seus valores de símbolo definidos, caso contrário uma leitura unmapped causava fault na decodificação de endianness do PVR.
  3. **Resolução de `CFBundleCopyResourceURL` e paths de áudio**:
     - O código C++ de reprodução de áudio (`0x00093154`) divide o caminho `/snd/sd000_mf.wav` com `strrchr` em subdiretório `snd` e nome `sd000_mf.wav`.
     - A implementação HLE de `_CFBundleCopyResourceURL` normalizou barras iniciais/finais, permitindo que todos os 28 efeitos sonoros WAV fossem encontrados e carregados corretamente com `fopen` / `alBufferData`.
  4. **Host Gráfico Desktop e Renderização de Tela**:
     - O jogo opera nativamente a 60 FPS com comandos OpenGL ES 1.1 (`glOrthof`, `glVertexPointer`, `glTexCoordPointer`, `glColorPointer`, `glDrawArrays`, `glTexImage2D`).
     - A captura de tela do host validou saída visual completa com **77.564 cores únicas** renderizadas (`reference/analysis/test_frame60.png` e `screenshot_*.png`).
  5. **Mapeamento de Entrada (Teclado + Gamepad + Mouse)**:
     - `TouchResponder` processa toques via `touchesBegan:withEvent:`, `touchesMoved:withEvent:`, `touchesEnded:withEvent:` consumindo objetos `UITouch` com `locationInView:` e `phase` via `_objc_msgSend_stret`.
     - Teclado (Setas/WASD + J/K/U/I/O/Space/Enter/Esc/P) e Gamepads (Analógico, D-Pad, A/B/X/Y/LB/RB/Start/Select) convertem comandos físicos em toques sintéticos contínuos.
     - Mouse atua como toque direto com coordenadas 1:1 mapeadas para 480x320.
     - Toque no centro da tela (`240, 160`) validou resposta com **82.6% de variação de pixels** entre a tela de título e a transição para menus.


---

## 2026-11-09 - Sessão 04 (`S06`): Subsistema de Áudio Avançado, OpenAL 64 Canais e AVAudioPlayer

- **Relatório completo**: `research/sessions/2026-11-09-04.md`
- **Descobertas verificáveis e correções críticas**:
  1. **Catálogo de Som do HL3 (`snd/`)**:
     - 28 arquivos WAV (`sd000_mf.wav` a `sd027_mf.wav`, 16-bit 44.1 kHz PCM) e 20 arquivos MP3 (`battle_cry.mp3` e `bgm0_mf.mp3` a `bgm18_mf.mp3`, ~26 MB) 100% íntegros e auditados.
  2. **Arquitetura de Som Firefly Engine**:
     - Mapeadas as classes C++ `Firefly::SimpleSoundService`, `GenericSoundLoader`, `WavLoader` e `MP3Loader`.
  3. **Mixagem em 64 Canais Virtuais e Rastreamento de Estado**:
     - Canais expandidos para 64 no Pygame/SDL para evitar corte de efeitos em combate denso.
     - `alGetSourcei(sid, AL_SOURCE_STATE)` consulta o canal ativo e retorna `AL_STOPPED` quando a reprodução encerra naturalmente.
  4. **Parâmetros OpenAL Suportados**:
     - `AL_GAIN`, `AL_PITCH`, `AL_MIN_GAIN`, `AL_MAX_GAIN`, `AL_LOOPING`, `AL_BUFFER` e vetores 3D operando fielmente.
  5. **Implementação Completa de `AVAudioPlayer`**:
     - Métodos `alloc`, `init`, `play`, `stop`, `pause`, `isPlaying`, `duration`, `currentTime`, `volume`, `numberOfLoops`, etc. com fading suave.
  6. **Persistência de Preferências e Atalhos no Host PC**:
     - Arquivo `config/audio.json` para volumes e mute; atalhos `+`, `-`, `M` no host nativo `run_pc.py`.


---

## 2026-11-09 - Sessão 05 (`S07`): Performance de CPU, Blocos Quentes AOT e Oráculo Unicorn

- **Relatório completo**: `research/sessions/2026-11-09-05.md`
- **Descobertas verificáveis e correções críticas**:
  1. **Volume de Instruções por Fase**:
     - `boot`: 9.903 insns | `launch`: 50.105 insns | `title`: 19.276.515 insns (30 frames, ~640k/frame) | `menu`: 3.281.466 insns (15 frames).
  2. **Hotspots Identificados**:
     - `0x0000f6a0`..`0x0000f720`: laço crítico de renderização de sprites e cálculos de coordenadas na CPU ARM (executado >170.000 vezes por segundo).
     - `0x0008bfe0`..`0x0008c340`: stubs e thunks C++ (`bx lr`, `mov r0, #0`).
  3. **Guarda de Opcodes e Oráculo Unicorn**:
     - 28 blocos AOT compilados com guardas explícitas de opcodes gerados em `tools/hl3rt/aot_blocks.py` e `src/arm_hot_blocks.inc`.
     - 28 blocos validados pelo oráculo contra estados de borda (0, 1, 31, 32, 0x7fffffff, 0x80000000, 0xffffffff) e determinísticos, com 100% de equivalência em R0..R12 e flags do CPSR.

---

## 2026-11-09 - Sessão 06: Correção de Distorção Gráfica, Toque UITouch Persistente e Compatibilidade Standalone PC (CFG)

- **Descobertas verificáveis e correções críticas**:
  1. **Distorção Gráfica em Degrau ("Stair-step Seams")**:
     - Causa raiz: O binário iOS emite 6 `glPushMatrix()` por frame sem correspondentes `glPopMatrix()`. Em drivers OpenGL desktop para PC, a pilha estourou a profundidade máxima (`GL_STACK_OVERFLOW`), acumulando matrizes antigas e gerando desalinhamento vertical contínuo entre quads vizinhos.
     - Solução: Implementada proteção de profundidade (`gl_bridge.push_matrix`/`pop_matrix`) e reinicialização para matriz identidade (`glLoadIdentity`) a cada frame em `begin_frame` nas pilhas de `GL_PROJECTION` e `GL_MODELVIEW`.
     - Também aplicado `GL_CLAMP_TO_EDGE` em texturas para eliminar sangramento de 1px nas bordas dos sprites e corrigido o mapeamento de coordenadas Y em `glScissor` (inversão da origem UIKit para OpenGL desktop).
  2. **Interação e Navegação de Menus ("Não consigo navegar nem clicar")**:
     - Causa raiz: A função do motor `TouchResponder touchesEnded:withEvent:` (`0xbe5c`) exige que o ponteiro `UITouch*` (`r3`) coincida com o ponteiro registrado em `touchesBegan:withEvent:` (`0xc0a4`). O host recriava uma instância nova de `UITouch` a cada evento, fazendo o jogo descartar `touchesEnded` e manter o botão eternamente pressionado em vermelho. Além disso, o alvo virtual de START estava apontando para o centro da tela em vez do botão "START GAME".
     - Solução: Implementado rastreamento de instâncias `UITouch` persistentes no host (`self.active_touches[tag] = (Touch, t_obj, evt_obj, set_obj)`), reutilizando rigorosamente o mesmo endereço guest e atualizando `phase` (0 = began, 3 = ended). Coordenadas do botão START ajustadas para `(240.0, 280.0)`. Validado avanço imediato para a tela de Seleção de Personagem (Kei / Rits).
  3. **Crash do Executável Standalone `HeroesLore3.exe` (0xC0000409 / STATUS_STACK_BUFFER_OVERRUN)**:
     - Causa raiz: O bootloader de 64-bit do PyInstaller no Windows possui a flag `IMAGE_DLLCHARACTERISTICS_GUARD_CF` (`0x4000`, Control Flow Guard) ativada. Durante a emulação ARMv6, a biblioteca nativa `unicorn.dll` invoca `longjmp()` em `VCRUNTIME140.dll` para gerenciar saídas de emulação e blocos de código JIT. O CRT do MSVC valida se o destino do `longjmp` está registrado na tabela de CFG; como o código gerado em JIT pelo Unicorn não consta na tabela, o Windows aciona encerramento imediato via `__fastfail(FAST_FAIL_FATAL_APP_EXIT)` (`0xc0000409`).
     - Solução: Implementada a rotina `strip_cfg` em `tools/build_standalone_pc.py` limpando o bit `0x4000` do cabeçalho PE de `HeroesLore3.exe`, igualando as características de DLL do `python.exe` (`0x8160`), permitindo execução perfeitamente estável e fluida do executável standalone com duplo clique direto.

---

## 2026-11-09 - Sessão 07: Correção de Linhas de Grade (Costuras de Textura) e Movimentação Fluida com D-Pad Contínuo

- **Descobertas verificáveis e correções críticas**:
  1. **Linhas de Grade / Costuras de Textura no Fundo da Tela Inicial ("ficou esquisito")**:
     - Causa raiz: O fundo da tela inicial é desenhado a partir de uma malha 10x5 de quads com textura `newres/sprites/menu_bkg.pvr`. As bordas dos blocos de 48x64 possuem pixels adjacentes transparentes no canal alfa com componentes RGB não-nulas (`255, 255, 255, 0`). Na amostragem `GL_LINEAR` padrão em resoluções aumentadas no PC (960x640), a GPU interpolava os texels úteis com os vizinhos transparentes fora dos limites do quad, gerando linhas de grade claras de 1 a 2 pixels ao longo de todas as bordas dos quads.
     - Solução: Forçado `GL_NEAREST` em `tools/hl3rt/gl_bridge.py` para `GL_TEXTURE_MIN_FILTER` e `GL_TEXTURE_MAG_FILTER`. O fundo e todas as fitas agora renderizam com superfície 100% lisa, sem linhas de emenda e com nitidez pixel-perfect idêntica ao jogo rodando no celular.
  2. **Movimentação do Personagem com Setas / WASD ("não vai nas direções corretamente")**:
     - Causa raiz:
       1. O centro do D-Pad virtual configurado no host estava em `(80.0, 240.0)`, enquanto a tabela real de controles do motor (`0x100090a0` / `0x10009330`) define o centro em `(60.0, 265.0)` com raio de hitbox `45.0`. A diferença de 20px no eixo X e 25px no eixo Y causava um desvio angular severo em todas as direções (pressionar CIMA ia para Cima-Direita; ESQUERDA ia para Esquerda-Cima, etc.).
       2. O motor calcula velocidade e direção acumulando o vetor contínuo em `onTouchMoved` (`0x16a04`): `dx = (touch.x - center.x) / radius` e `dy = (touch.y - center.y) / radius`. O host anterior emitia apenas `touchesBegan` pontual e nunca emitia `touchesMoved`, ignorando mudanças de direção durante o pressionamento contínuo das teclas.
     - Solução: Em `tools/run_pc.py`, alinhado o centro do stick em `(60.0, 265.0)` e implementado cálculo contínuo de vetor normalizado `(nx, ny)` com raio seguro de 35.0px. O host agora dispara `touchesBegan` no centro e despacha `touchesMoved` continuamente a cada frame para o ponto de arrasto, atualizando instantaneamente na mudança de direção e enviando `touchesEnded` ao soltar as teclas ou o analógico.
  3. **Desacoplamento de `previousLocationInView:` em `tools/hl3rt/hle_uikit.py`**:
     - Separado o tratamento de `locationInView:` e `previousLocationInView:` no shim UIKit, garantindo retorno exato de `(prev_x, prev_y)`.
  4. **Atualização da Distribuição Standalone `dist/HeroesLore3_PC/`**:
     - Recompilado e reempacotado o executável standalone `HeroesLore3.exe` com as novas correções incorporadas no bytecode e limpando a flag CFG `0x4000`. Validada inicialização, loop de 60 FPS e todos os 23 testes unitários.
