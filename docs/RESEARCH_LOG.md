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

## 2026-11-10 - Sessão 08: Máquina de Estados de Criação de Personagem (5 Passos), Resolução da Tela de Resumo, Pulo de Cutscenes e Toque Multiframe

- **Descobertas verificáveis e correções críticas**:
  1. **Descoberta do Fluxo Completo de Criação de Personagem (4 Telas Distintas)**:
     - `Step 0`: Tela de Título com Carrossel (`START GAME` / `CONTINUE`) em `(240.0, 280.0)`.
     - `Step 1`: Seleção de Herói (Kei em `(150, 100)` ou Rits em `(330, 100)`), confirmação em `(415, 284)`.
     - `Step 2`: Seleção de Classe com setas `<` `(99, 88)` e `>` `(389, 86)` e chibis `(160, 80)`, `(244, 80)`, `(338, 80)`, confirmação em `(415, 284)`.
     - `Step 3`: Resumo do Personagem (`KEI`, `NATION NEO SOLTIA`, `CLASS BERSERKER`). A ilustração de corpo inteiro de Kei ocupa a metade direita da tela (`X: 240..480`), deslocando o botão verde de confirmação para o canto inferior direito da caixa de informações em `(355.0, 285.0)` (60 pixels à esquerda do botão de classe!).
     - `Step 4`: Cutscenes de abertura (Penhasco de batalha) e Gameplay na Vila de Gaia.
  2. **Causa Raiz da Moldura Amarela Indevida ao Teclar Esc**:
     - O código tratava `game` logo após `class_select`, ignorando o `Step 3`. Ao teclar `Esc` na tela de resumo, caía na condição de pause menu (`self.menu_mode = True`), desenhando a moldura dourada do cursor de pause sobre a arte de Kei e travando a entrada do teclado.
     - Correção: Implementado `step = 3` (`summary`) onde `Esc` estritamente recua para a seleção de classe via `BTN_BACK (27, 17)`. Adicionada guarda estrita em `_draw_menu_cursor`: o cursor só pode ser desenhado se `self.menu_mode and self.step == 4`.
  3. **Mecânica de Pulso Multiframe em `tap_once`**:
     - A rotina anterior de `tap_once` enviava `touchesBegan` no frame $N$ e `touchesEnded` no frame $N+1$ (16ms). Como teclas físicas permanecem pressionadas por 50..120ms, o código enviava Began-Ended-Began-Ended em rajadas desordenadas a cada frame, causando descarte de eventos pelo motor iOS.
     - Correção: Implementado `self.pending_tap_ups: dict[str, int]` com contagem regressiva de sustentação configurável (`hold_frames=2` ou `3`). O toque agora é mantido ativamente por 2 a 3 ciclos completos de `onTimer:`, simulando com perfeição um toque físico na tela capacitiva.
  4. **Alinhamento do Botão de Fala com NPCs e Pulo de Cutscenes**:
     - O botão de conversa com NPCs (balão azul medido em `media_1794324014624_479f1886.jpg`) tem centro em `(445.0, 286.0)` / `(435.0, 275.0)` e responde imediatamente a `E`, `Enter` e Gamepad `A`.
     - O botão SKIP de cutscene (círculo marrom com "X" medido em `media_1794325928187_84a8c6e0.jpg`) tem centro em `(410.0, 28.0)` com texto em `(410.0, 45.0)` e responde imediatamente a `S`, `Tab`, Gamepad `Y` e `Start`.
  5. **Atualização da Distribuição Standalone `dist/HeroesLore3_PC/HeroesLore3.exe`**:
     - Recompilado e testado com sucesso (30 frames apresentados, código 0). Todos os 24 testes unitários passando.

---

## 2026-11-10 - Sessão 09: Controlador Orientado à Memória do Motor Guest (AppManager / UI_Mgr / GameCtx), Coordenadas Exatas da Árvore de Controles e Ícone Oficial

- **Descobertas verificáveis e correções críticas**:
  1. **Estruturas de Dados do Motor em Memória Guest (Ground Truth)**:
     - `AppManager*` ponteiro global estático em `*(uint32_t*)0x000d7dd4` (`0x10008e80`).
     - `app_state` no deslocamento `+0x78`: `1` = Menus de Título e Criação de Personagem, `2` = In-Game (Campo, Diálogo, Cutscenes), `8` = Menu de Pausa In-Game.
     - `ui_main` (`UI_Mgr*`) no deslocamento `+0x40`:
       - `main_form` em `+0x98`, com `form_id` em `+0x14` (`41` = Seleção de Personagem, `42` = Seleção de Classe, `43` = Resumo, `44` = Cutscenes).
       - `main_popup` em `+0xa0`: popups de confirmação sobrepostos.
     - `ui_hud` (`UI_Mgr*`) no deslocamento `+0x44`:
       - `hud_form` em `+0x98`: formulário de HUD (`id=70`).
       - `hud_popup` em `+0xa0`: tutoriais pop-up (`id=130`).
     - `GameCtx` em `0x000d8270`:
       - `sub_state` em `*(int8_t*)0x000de30c`: `0` = Menus, `3` = Campo Aberto / Exploração, `4`/`22` = Cutscene e Diálogo com NPCs, `7` = Mensagem de Campo.
       - `can_talk` em `*(uint8_t*)0x000de0ad`: flag definida pela rotina de colisão `0x90894` quando o herói colide com a hitbox 72x72 de um NPC (`0x41550`).
       - `combat_mode` em `*(uint8_t*)0x000de0ac`: `0` = Vila Pacífica (sem armas empunhadas), `1` = Modo de Combate / Campo com Inimigos.
  2. **Causa Raiz Exata do Botão de Fala com NPCs**:
     - No modo pacífico (`combat_mode == 0`), o botão de fala (id `7025`) tem centro em `(450.0, 258.0)` com bounding box `(421..479, 237..279)`. O código anterior disparava em `(445.0, 286.0)` (`y=286` é 7 pixels abaixo da borda inferior do botão, caindo em espaço vazio!).
     - No modo de combate (`combat_mode == 1`), o botão de ataque (id `7007`) assume `(450.0, 258.0)` e o botão de fala contextual (id `7026`) desloca-se para `(397.0, 239.0)`.
  3. **Causa Raiz da Falha nas Setas de Seleção de Classe**:
     - O botão `<` (id `4207`) localiza-se em `(73.0, 71.0)` e o botão `>` (id `4206`) em `(400.0, 71.0)`. As coordenadas anteriores `(99, 88)` e `(389, 86)` caíam fora da área ativa dos botões.
  4. **Cursor de Pause e Eventos Discretos de Teclado**:
     - Eliminada a variável desincronizada `self.step`. O cursor do menu de pausa agora verifica rigorosamente `app_state == 8 and form_id == 2`.
     - Ações de menu e navegação foram migradas de `pygame.key.get_pressed()` para eventos discretos `KEYDOWN`, eliminando cliques duplos simultâneos ao teclar `Enter`.
  5. **Ícone Oficial do Jogo**:
     - Extraído `Payload/Heroes Lore.app/Icon.png` do IPA original e gerados `assets/icon.png` e `assets/icon.ico`.
     - Aplicado como ícone da janela Pygame (`pygame.display.set_icon`) e embutido no executável standalone `dist/HeroesLore3_PC/HeroesLore3.exe` via PyInstaller.

---

## 2026-11-10 - Sessão 10: Resolução da Tela Preta (Vazamento de GL_TEXTURE_2D e Ordem de Ícone SDL2) e Descoberta dos Centros Reais do Mapa e Poções

- **Descobertas verificáveis e correções críticas**:
  1. **Causa Raiz da Tela Preta**:
     - `_draw_menu_cursor()` desativava a textura 2D via `glDisable(GL_TEXTURE_2D)` para desenhar linhas douradas, mas omitia a restauração de `glEnable(GL_TEXTURE_2D)`. Qualquer quadro em que o cursor de pause fosse desenhado deixava a GPU em modo untextured permanentemente, impedindo a renderização de todos os sprites e tiles. Solução: Envolvido o cursor em `glPushAttrib(GL_ALL_ATTRIB_BITS)` ... `glPopAttrib()` com restauração explícita de `glEnable(GL_TEXTURE_2D)` e garantia de textura habilitada em `GLBridge.begin_frame()`.
     - No Pygame 2 / SDL2 em Windows, `pygame.display.set_icon()` invocado antes de `pygame.display.set_mode()` com superfície não padronizada (57x57) falhava com `No video mode has been set` e interferia na criação do contexto OpenGL. Solução: Invocação movida para após `set_mode()` com imagem suavizada para 32x32 RGBA.
  2. **Estrutura Real de Controles da HUD (`form_id = 70`)**:
     - Inspecionados diretamente na memória guest os retângulos de toque de todos os nós:
       - `id=7011`: Minimap / Bússola em `[9..38, 7..37]`, centro `(23.5, 22.0)`.
       - `id=7002`: Poção Superior (Slot 1) em `[4..54, 86..128]`, centro `(29.0, 107.0)`.
       - `id=7003`: Poção Inferior (Slot 2) em `[4..54, 136..178]`, centro `(29.0, 157.0)`.
       - `id=7006`: Skill 1 em `[426..476, 163..205]`, centro `(451.0, 184.0)`.
       - `id=7005`: Skill 2 em `[426..476, 113..155]`, centro `(451.0, 134.0)`.
       - `id=7004`: Skill 3 em `[426..476, 63..105]`, centro `(451.0, 84.0)`.
       - `id=7007`: Ataque Primário em `[425..482, 232..284]`, centro `(450.0, 258.0)`.
       - `id=7008`: Ataque Secundário em `[380..437, 263..315]`, centro `(408.5, 289.0)`.
  3. **Mapeamento de Teclas no Host PC**:
     - Teclado: Mapa em `M` / `Tab`; Poção 1 em `Q` / `1`; Poção 2 em `K` / `2`; Skills em `U`/`3`, `I`/`4`, `O`/`5`; Ataque em `J`/`Espaço`; Guarda/Ataque 2 em `L`/`H`.
     - Gamepad: Poção 1 em `LB`; Poção 2 em `RB`; Mapa em `Select`; Ataque em `X`; Skills/Skip em `Y`; Confirmar em `A`; Cancelar em `B`; Menu em `Start`.
  4. **Distribuição Standalone Recompilada**:
     - `dist/HeroesLore3_PC/HeroesLore3.exe` reempacotado com CFG desativado e validado rodando com mais de 102.000 cores.

---

## 2026-11-13 - Sessão 11: Ícone Win32 HD (`iTunesArtwork` 512x512 + `WM_SETICON`/`AppUserModelID`) e Mapeamento Completo da Árvore `UIControl` dos 6 Submenus

- **Descobertas verificáveis e artefatos adicionados**:
  1. **Ícone Win32 Multi-Resolução e Substituição da "Cobrinha"**:
     - Extraído `iTunesArtwork` (PNG 512x512) de `assets/game.ipa` para `assets/icon.png` e convertido para `assets/icon.ico` com 7 resoluções (`16, 24, 32, 48, 64, 128, 256`).
     - Confirmado via `PCRunner` e `dist/HeroesLore3_PC/HeroesLore3.exe` que `SetCurrentProcessExplicitAppUserModelID("DavidBraga.HeroesLore3.Recomp.PC")` + `WM_SETICON` / `SetClassLongPtrW` (`LoadImageW`) vinculam `HICON` não-nulos na janela SDL/OpenGL e console, além de `ExtractIconExW == 2` no `.exe` embutido.
  2. **Estrutura Real da Árvore `UIControl` nos Submenus e Overlays (`app_state == 8`)**:
     - Flag de visibilidade/habilitação: `(vis & 0x0003) == 0x0003` indica nó visível e clicável; containers com `(vis & 0x0003) != 0x0003` ocultam subárvores inativas (`ITEMS`: `836`/`837` vs `835`; `QUESTS`: `4708` detalhe) e controles `vis=0x0002` representam botões desabilitados (`DROP 823`, `USE 883`, seta `<` na pág. 1).
     - `CHARACTER (form=5)`: cards `533..540` (`ctype=4`) sempre ativos e botões `+`/`-` (`501..508`) em `vis=0x0003` quando há pontos livres.
     - `EQUIPMENT (form=6)`: grade equipada 3x3 (`619..627`), grade mochila 3x3 (`628..636`), 8 pílulas de status inferiores (`533..540`) e paginação (`640`/`641` quando ativa).
     - `SKILLS (form=10)`: abas `1005` (PASSIVE), `1003` (PRIMARY), `1004` (SECONDARY), 6 slots (`1006..1011`), 5 quickslots (`1016..1020`) e `+`/`-` (`1040..1051`).
     - `ITEMS (form=8)`: grade 8x2 (`801..816`), setas `< 818` / `> 817`, 5 quickslots (`825..829`), e botões contextuais `DROP 823` / `USE 883` que passam para `vis=0x0003` ao selecionar item consumível (ex.: slot `807`).
     - `QUESTS (form=46)`: canto superior esquerdo `4709` em `(28,28)` (filtrado fora do foco quando há missões/scroll), `MAIN QUEST 4601`, lista `4614` (`4602..4610`) e painel de detalhe `4708` (`ctype=10` rolável + botão fechar `4709`).
     - `SYSTEM (form=3)` / `PREFERENCES (form=112)` / `HELP (form=118)` / popups `4` (`SAVE`) e `106` (`MAIN MENU`): sliders `ctype=13` (`11205..11207`) com filhos `< 198` e `> 197`; `HELP` com `< 11809` / `> 11810` e `ctype=10` rolável; popups `4` (`401`/`199`) e `106` (`10601`/`10602`) com alternância Sim/Não por setas.
  3. **Validação**:
     - Roteiro vivo ponta a ponta percorrendo Campo -> Pausa -> 6 submenus -> `PREFERENCES`/`HELP`/`SAVE`/`MAIN MENU` -> Campo executado com saída 0 e capturas verificadas visualmente.
     - Suíte unitária ampliada para 26 testes em `tests/` (todos `OK` em 10.0s) e `dist/HeroesLore3_PC/HeroesLore3.exe --frames 30` validado com saída 0.

---

## 2026-11-13 - Sessão 12 (`S08`): FPS Configurável (`30`/`60`), True Widescreen (`Original`/`16:9`/`Auto`), `GameCtx` (`0x000d8270`) e Trampolim de Flush GL

- **Relatório completo**: `research/sessions/2026-11-13-12.md`
- **Descobertas verificáveis e correções críticas**:
  1. **Benchmark de FPS (`30` padrão vs `60` opcional)**:
     - Em campo livre, a emulação ARMv6 consumiu `28,33 ms/frame` em `480x320` e `23,03–32,72 ms/frame` em `16:9`. Definido `DEFAULT_FPS_TARGET = 30` para estabilidade contínua de pacing, com alternância ao vivo para `60 FPS` via `F8` / `--fps 60`, apoiada pelos shims nativos em `tools/hl3rt/fast_stubs.py` (`__divsi3`, `__modsi3`, `__udivsi3`, `__umodsi3`, `glGetError`, `_Unwind_SjLj_*`).
  2. **Endereços Exatos de Viewport/Câmera no Mach-O (`0x00039d28` e `GameCtx` `0x000d8270`)**:
     - Desmontagem de `0x00039d28` confirmou `ldr r0, [pc]; bx lr; .word 0x000d8270`: o getter devolve o próprio endereço estático de `GameCtx`, não um ponteiro indireto.
     - Atualizar diretamente `0x000d8270 + 0x40/0x44` e `+0x5e60..0x5e7c` expande imediatamente os limites de câmera e culling do mapa sem faixas pretas laterais ou verticais em `16:9`, `20:9` e `4:3`.
  3. **Centralização de `ui_main` via Projeção OpenGL e Trampolim `UI_FLUSH_RETURN_STUB` (`0x0000c688`)**:
     - Desenhos diretos de `LEVEL 1` (`0x6ee44`) e barras HP/MP (`0x6f8a8`, `0x6f984`) usam posição crua do nó; mover o deslocamento para `GLBridge.set_ui_offset(ui_dx, ui_dy)` (`glOrtho` e `glScissor`) alinhou todas as barras/textos pixel a pixel.
     - Como o renderizador acumula quads em lote e só esvazia via `0x0000c688(gfx)`, redirecionar brevemente via `UI_FLUSH_RETURN_STUB` (`0x7F100020`) antes de alternar `ui_offset` em `0x74e1c`/`0x74e54`/`0x74e8c`/`0x74f00`/`0x51f90`/`0x52140` eliminou o deslocamento de `ITEMS`/`QUESTS`/`SYSTEM` no menu de pausa e popups.
  4. **Redimensionamento, Maximização SDL2/Win32 e Banner OSD Estilizado**:
     - Em Windows 11/Pygame 2.6.1, `pygame.display.get_surface().get_size()` permanece no tamanho inicial após maximizar via barra de título, enquanto `pygame.display.get_window_size()` retorna imediatamente a área cliente real (`1536x793`). Definido `DEFAULT_ASPECT_MODE = "auto"` (preenchendo Full Wide automaticamente ao maximizar/tela cheia sem barras pretas), leitura contínua de tamanho + eventos SDL2 (`WINDOWSIZECHANGED`/`WINDOWMAXIMIZED`) e verificação `IsZoomed` no `F9`.
     - Implementado aviso estilizado OSD no topo da tela (`show_osd` / `_draw_osd`) renderizado em OpenGL com textura cacheada, borda dourada/safira e fade-in/fade-out ao vivo nos atalhos `F8` (FPS), `F9` (Proporção) e `F11` (Tela Cheia).
  5. **Validação**:
     - Capturas em `16:9`, `4:3`, `20:9` e janela maximizada (`1536x793`) inspecionadas visualmente com cenário de borda a borda e OSD estilizado legível.
     - `38/38` testes unitários passando (`OK`) e `dist/HeroesLore3_PC/HeroesLore3.exe` validado em modo padrão (`Auto 30 FPS`), `--aspect 16:9 --fps 30` e `--aspect original --fps 60` com saída `0`.

---

## 2026-11-13 - Sessão 13: Menu Nativo de Opções (`FPS`/`Proporção`/`Idioma`), Acesso Duplo (`form=113`/`form=3`) e Cloud Save Google Drive Isolado

- **Relatório completo**: `research/sessions/2026-11-13-13.md`
- **Descobertas verificáveis e entregas**:
  1. **Compactação de `Help & About` (`form_id=113`) e Botão em `System` (`form_id=3`)**:
     - Compactados os 6 botões `11301..11306` em `form_id=113` (`cy=[33, 69, 105, 141, 177, 213]`) liberando espaço limpo para o 7º botão virtual `cid=99113` (`[93..393, 234..264]`) acima de `VERSION 1.0.1`.
     - Adicionado o 5º botão virtual `cid=99003` (`[90..390, 256..286]`) em `System` (`form_id=3`).
     - Reconstruída [`assets/ui_button_ribbon.png`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/assets/ui_button_ribbon.png) (`608x66`) diretamente com os ornamentos azuis extraídos de `user_interface_with_alpha.pvr` (`344,208..393,238`) e rampa de cor amostrada dos botões originais, com encaixe idêntico aos botões nativos.
  2. **Modal Responsivo OpenGL e Idioma ao Vivo (`EN` / `PT-BR`)**:
     - Criados [`tools/hl3rt/options_menu.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/options_menu.py) e [`tools/hl3rt/i18n.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/i18n.py) com layout adaptável retrato/paisagem validado em `960x640`, `1138x640` (`16:9`) e janelas verticais estreitas, sem vazamento de toque ao motor ARMv6.
  3. **Cloud Save Google Drive Isolado para Heroes Lore 3**:
     - Criado [`tools/hl3rt/cloud_save.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/hl3rt/cloud_save.py) usando OAuth Device Flow + Drive v3 isolado em `"Heroes Lore III - Stigmata of Gaia"` / `"heroes_lore_3_save.json"`, verificação SHA-256, staging atômico + rollback em `data/save-backups/<stamp>` e reinício limpo para a tela título (`_reload_runtime_to_title()` -> `app_state=1, form_id=100`).
     - Empacotamento `tools/build_standalone_pc.py` configurado para excluir `config/cloud_auth.json` e suíte ampliada validada (`48/48` testes `OK` e `dist/HeroesLore3_PC/HeroesLore3.exe --frames 15` validado com saída `0`).

---

## 2026-11-13 - Sessão 14 (`S09` + `S10`): Auditoria EN, Fontes `.rff`/`.pvr` Acentuadas, Tradução Integral PT-BR e Reload em Tempo Real de `TextUtils`

- **Relatório completo**: `research/sessions/2026-11-13-14.md`
- **Descobertas verificáveis e entregas**:
  1. **Base Determinística `S09` (`tools/extract_texts.py`)**:
     - Verificado layout binário exato de `hdr` (`244` bytes, nomes UTF-16BE a partir do byte `115`), `t_en` (`6.733` slots, `6.648` não vazios, anomalias `[6496]`/`[6607]` resolvidas via `t_ko`), `dat/InGame_txt_2` (`27` bytes em UTF-8 no endereço Mach-O `0x00048ee4`) e métricas dos 4 pares `.rff`/`.pvr`.
  2. **Extensão Nativa de `.rff`/`.pvr` (`tools/build_ptbr_fonts.py`)**:
     - Adicionados os 5 glifos faltantes para Português (`Ã`, `Ô`, `Õ`, `ã`, `õ`) nos atlas de `assets/fonts/`: `font_mainmenu` (`142`, `used_y=289/512`), `font_menu_medium` (`142`, `used_y=210/512`) e `font_menu_small` (`151`, `used_y=182/256`), mantendo `sorted_codepoints == True` e `ptbr_missing == []`.
  3. **Tradução Integral PT-BR (`6.733/6.733` slots) + Recarregamento ao Vivo**:
     - Todos os 12 chunks (`localization/chunks/*.json`) e `InGame_txt_2` compilados via `tools/build_ptbr_texts.py` em `assets/texts/t_pt-br` (`696.466` bytes, blob `669.530` bytes), `hdr_pt-br` (`244` bytes) e `InGame_txt_2_pt-br` (`27` bytes).
     - Descoberta crítica de memória em `TextUtils` (`0x000d8220`): `chunk_cnt = 2`, onde `chunk 0` armazena apenas os 7 nomes de idioma do cabeçalho (`max_sz=0`) e `chunk 1` armazena `t_en` (`max_sz=769.296` bytes); corrigido `_apply_guest_language_live()` para gravar em `[sizes_tbl[0] + chunk_idx * 4]` (chunk 1), limpar `tu+0x34` via `Machine.wr8` e invocar `0x0001230c(0)` + `vtable[0x3c]`, trocando textos na tela no mesmo instante (`CONTINUE` <-> `CONTINUAR`, `CONNECTION ERROR!` <-> `ERRO DE CONEXÃO!`) com integridade SHA-256 intacta em `data/`.
  4. **Validação Completa**:
     - `python tools/extract_texts.py --check` PASS, `python tools/build_ptbr_texts.py` PASS, `52/52` testes `OK` em `unittest` (`4` novos testes em `tests/test_s09_s10_localization.py`) e build standalone `dist/HeroesLore3_PC/HeroesLore3.exe` atualizada.

---

## 2026-11-13 - Sessão 15 (`S10` Fix): Recálculo ao Vivo do Banner de Diálogo (`GameWorld`) e Estabilização de `strip_cfg`

- **Relatório completo**: `research/sessions/2026-11-13-15.md`
- **Descobertas verificáveis e entregas**:
  1. **Diagnóstico em `GameWorld` (`0x000d8270`) / `banner_init` (`0x0002787c`)**:
     - Confirmada paridade integral de `^[Speaker]# ` e tags `#|{}^` em todos os `6.733` slots de `t_pt-br` (`slot 1798` / `tid=1799` íntegro: `'^[Guerreiro Mascarado]# Saia da frente, Bruxa Vermelha! '`).
     - Identificado que o banner ativo em diálogo/cutscene cacheia índices de cor e quebras em `[app + 0x28]` via `0x0002787c`; sem recalcular ao trocar idioma, `0x027344`/`0x010b9c` aplicava os offsets/tags do inglês (`[0, 17, 45, 0]`) ao texto em português, omitindo o `'r'` na posição `17`, desenhando `'#'` literal e cortando na coluna `45`.
  2. **Correção ao Vivo e Build Standalone**:
     - Adicionado `_refresh_guest_banner_live` em [`tools/run_pc.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/run_pc.py#L1001-L1021) com guardas para `app_state in (2, 8)`, `gw + 0x40 > 0x20`, buffers não nulos e `banner_tid >= 0`, rechamando `0x0002787c` e ajustando `gw + 0x611d` com segurança.
     - Adicionado retry para bloqueios transitórios de Windows em `strip_cfg` ([`tools/build_standalone_pc.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/build_standalone_pc.py#L125-L155)) e adicionados `3` testes de regressão em [`tests/test_s09_s10_localization.py`](file:///c:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tests/test_s09_s10_localization.py) (`55/55` testes `OK`, `HeroesLore3.exe --frames 15` com saída `0`).

---

## 2026-11-13 - Sessão 16 (`S11` Concluída): Créditos no Menu Sobre, Atualizador OTA via GitHub Releases, Documentação e Release `v1.0.0`

- **Relatório completo**: `research/sessions/2026-11-13-16.md`
- **Descobertas verificáveis e entregas**:
  1. **Crédito do Port e Versão no Menu Sobre (`form_id=114`, `slot 6632`)**:
     - Descoberta da montagem do menu Sobre por concatenação de `slot 6731` (`Version: ` / `Versão: `) + `"1.0.1||"` + `slot 6632` em `TextUtils` (`0x000d8220`).
     - Implementado `PCRunner._patch_guest_about_credits()` injetando em linha única `"Port by David Kalil Braga (2026) - v1.0.0||"` / `"Port por David Kalil Braga (2026) - v1.0.0||"` logo acima do bloco de atendimento ao cliente, ajustando `offs1[6633..6733]`, reaproveitando capacidade real do buffer em `m.alloc_sizes` (evitando estouro quando `max_tbl[1]` excede o blob EN) e validado visualmente em EN e PT-BR sem recorte no pergaminho.
  2. **Versionamento Canônico & Atualizador OTA (`tools/hl3rt/version.py`, `tools/hl3rt/updater.py`, `tools/bump_version.py`)**:
     - Definida versão inicial do port `v1.0.0` desacoplada da versão original `1.0.1` e sincronizada entre `version.py` e `project.json` via `bump_version.py --check`.
     - Implementado `UpdateManager` (consulta GitHub Releases `davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia`, tratamento gracioso de HTTP `404` em repo privado, filtro de drafts/prereleases, validação HTTPS e SHA-256, proteção ZIP Slip e script staging no Windows preservando `config/` e `data/` com rollback) + página responsiva `ModalPage.UPDATE` em `options_menu.py` (`config/services.json`).
  3. **Documentação, Standalone Clean Build e Validação**:
     - Reescrita completa de `README.md`, atualização de `docs/FEATURE_PARITY.md`, `docs/VALIDATION.md`, `docs/ARCHITECTURE.md`, `docs/PORTING_MAP.md`, `docs/DECISIONS.md` (`ADR-022`), `docs/STATUS.json` (`S11` -> `done`) e `docs/HANDOFF.md`.
     - `tools/build_standalone_pc.py` ajustado para excluir saves pessoais de `data/internal/Documents/` e `config/cloud_auth.json` por padrão, empacotando `dist/heroes_lore_3_windows_x64.zip`.
     - Suíte completa com `62/62` testes `OK` (`tests/test_updater.py` + `tests/test_options_menu.py` + demais suítes).

---

## 2026-11-13 - Sessão 17 (Release `v1.0.1`): Bump SemVer para Teste de Atualização OTA (`v1.0.0 -> v1.0.1`)

- **Relatório completo**: `research/sessions/2026-11-13-17.md`
- **Entregas e validações**:
  - Executado `python tools/bump_version.py patch`, avançando a versão sincronizada do port de `1.0.0` para `1.0.1` em `tools/hl3rt/version.py` e `project.json`.
  - Atualizados `README.md`, `docs/` e recompilado o pacote standalone `dist/heroes_lore_3_windows_x64.zip` com `v1.0.1` para habilitar teste ponta a ponta de detecção, download, verificação SHA-256 e auto-instalação a partir de instalações `v1.0.0` (requer repositório público no GitHub para consulta não autenticada).





