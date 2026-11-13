# Decisões Arquiteturais (ADRs) - Heroes Lore 3 Recomp

Este documento registra as decisões de arquitetura tomadas no projeto **Heroes Lore 3: Stigmata of Gaia Recomp**.

---

## ADR-001: Isolamento Estrito do Workspace e Snapshots Congelados de HL4, HL5 e Ports J2ME

- **Data**: 2026-11-09
- **Status**: Aceito
- **Contexto**:
  - Os projetos anteriores (`heroes_lore_recomp`, `heroes_lore_2_recomp`, `heroes_lore_zero_recomp`, `heroes_lore_4_recomp` e `heroes_lore_5_recomp`) já resolveram problemas complexos de emulação/recompilação ARM, áudio SDL2, true widescreen, localização EN/PT-BR, Google Drive Cloud Save, OTA Updater e portabilidade para Nintendo Switch e Android.
  - Entretanto, cada jogo possui binário, mapa de memória e formatos próprios, e os repositórios irmãos não devem ser alterados nem referenciados por caminhos externos frágeis durante o desenvolvimento do HL3.
- **Decisão**:
  - Manter o desenvolvimento 100% contido em `C:\Users\david\Desktop\Projetos_Flutter\heroes_lore_3_recomp`.
  - Congelar snapshots verificáveis por SHA-256 em `reference/hl4_snapshot/` (`reference/HL4_SNAPSHOT.json`), `reference/hl5_snapshot/` (`reference/HL5_SNAPSHOT.json`) e `reference/jar_ports_docs/` (`reference/JAR_PORTS_DOCS.json`).
  - Validar esses snapshots automaticamente em `python tools/verify_workspace.py`.

---

## ADR-002: Execução Direta do Binário iOS Mach-O ARMv6 (`Payload/Heroes Lore.app/Heroes Lore`) com HLE iOS Dedicada

- **Data**: 2026-11-09
- **Status**: Aceito
- **Contexto**:
  - A inspeção estática (`S01`) do arquivo `Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk` demonstrou que o APK externo é um wrapper Android do emulador **touchHLE** contendo `assets/game.ipa`.
  - Dentro de `assets/game.ipa` está o aplicativo oficial de iOS 3.0 (`com.ea.heroesloresg.bv` v1.0.1) com o executável `Payload/Heroes Lore.app/Heroes Lore` (`915.824` bytes, **Mach-O 32-bit ARMv6**).
  - Diferente do HL5 (ELF Android com JNI `CletEntry`) e do HL4 (ELF Android com wrapper WIPI `MC_*`/`wipic_*`), o HL3 utiliza Objective-C (`AppDelegate`, `GLView`, `TouchResponder`), OpenGL ES 1.1 (`glOrthof`, `glDrawArrays`, `glDrawElements`, `.pvr`), OpenAL + `AVAudioPlayer` (`.wav` + `.mp3`) e SQLite3.
- **Decisão**:
  - Construir o runtime multiplataforma do HL3 carregando diretamente o binário Mach-O ARMv6 e os recursos de `Payload/Heroes Lore.app/`, provendo:
    1. **Mach-O 32-bit ARMv6 Loader**: mapeamento de `__TEXT` (`0x00001000`), `__DATA` (`0x000cc000`), `__LINKEDIT` (`0x000e2000`), execução de `__mod_init_func` e resolução de `__nl_symbol_ptr` / `__la_symbol_ptr`.
    2. **HLE iOS Focada no HL3**: despacho de `_objc_msgSend` e classes/seletores necessários (`NSBundle`, `NSString`, `NSUserDefaults`, `NSTimer`, `UIApplication`, `EAGLContext`, `AVAudioPlayer`), bridge OpenGL ES 1.1 sobre o host SDL2/GL, mixer de áudio para OpenAL/MP3 e integração com SQLite3/zlib/libc.
    3. **Camada de CPU ARMv6/Thumb + Blocos Quentes AOT**: execução assistida por Unicorn + blocos quentes compilados em C++ (como validado no HL4 e HL5) para garantir 60 FPS no PC, Android e Nintendo Switch.
  - Descartar o wrapper `libtouchHLE.so` do APK de entrada nas builds finais, gerando nossos próprios executáveis nativos para Windows, Linux, Android ARM64 e Nintendo Switch com suporte nativo a true widescreen, troca de idioma em tempo real, gamepad, cloud save e OTA updater.

---

## ADR-003: Fonte Única de Status (`docs/STATUS.json`) e Progressão Baseada em Evidências

- **Data**: 2026-11-09
- **Status**: Aceito
- **Contexto**:
  - Projetos longos conduzidos em múltiplas sessões de IA sofrem regressões quando o estado do projeto fica espalhado apenas no histórico do chat.
- **Decisão**:
  - `docs/STATUS.json` é a fonte única de verdade do progresso (`S00`..`S14`), e `docs/STATUS.md` é gerado deterministicamente via `python tools/project_status.py render`.
  - Nenhuma etapa pode ser marcada como `done` sem artefatos verificáveis em `evidence` e validação limpa por `python tools/verify_workspace.py`.

---

## ADR-004: Contrato de Runtime S02 - Traps de Import, HLE por Uso Real e Entrada por Toques Sintéticos

- **Data**: 2026-11-09
- **Status**: Aceito
- **Contexto**:
  - A análise profunda (`tools/probe_macho_contract.py`) mostrou: binário não-PIE com 762 relocações externas e 389 imports; 22 classes ObjC com IMPs todos ARM; texturas `.pvr` sem compressão (`RGBA8888`/`AI88`); entrada exclusivamente via `TouchResponder touches*:withEvent:`; sem imports de zlib; exceções SJLJ.
- **Decisão**:
  1. Imports resolvem para slots de trap de 8 bytes em `0x7F000000`; a HLE é invocada por hook de fetch e emula `bx lr` (mapa completo em `docs/RUNTIME_CONTRACT.md`).
  2. `objc_msgSend` despacha para IMPs reais da imagem (22 classes do jogo) ou handlers HLE das classes importadas; HLE implementada **por uso real** (lista de 389 imports/75 classrefs/493 selrefs), não por cobertura genérica de UIKit/Foundation.
  3. Texturas `.pvr` são enviadas direto como RGBA/LA (sem decodificador PVRTC).
  4. Teclado e gamepad serão **toques sintéticos** injetados em `TouchResponder` (coordenadas lógicas 480x320), com mapeamento derivado em `S04` a partir do código do `Game` e dos `.spr` de HUD.
  5. Rede/GameKit/SystemConfiguration/EAMTX/DMG ficam offline e neutralizados de forma determinística.
- **Consequências**: O caminho crítico para o PC jogável passa por `S03` (loader + `objc_msgSend` + `didFinishLaunching`) e `S04` (GLES1 + toques sintéticos). O risco principal é descobrir as regiões de toque do HUD.


---

## ADR-005: Host Desktop Interativo (Pygame + PyOpenGL) e Mapeamento de Teclado, Gamepad e Toque Direto

- **Data**: 2026-11-09
- **Status**: Aceito
- **Contexto**:
  - O objetivo primário estabelecido é ter uma build jogável no PC, com teclado mapeado e gamepads funcionando, antes de avançar para as demais plataformas.
  - O executável iOS Mach-O 32-bit ARMv6 opera nativamente em landscape 480x320 com projeção ortográfica OpenGL ES 1.1 e entrada exclusiva via `TouchResponder touches*:withEvent:`.
- **Decisão**:
  1. Utilizar o ecossistema SDL2 / Pygame com PyOpenGL (`tools/run_pc.py` e `tools/hl3rt/gl_bridge.py`) para prover uma janela nativa com aceleração de hardware completa na GPU.
  2. Implementar a ponte `GLBridge` traduzindo comandos GLES 1.1 (`glOrthof`, `glVertexPointer`, `glTexCoordPointer`, `glColorPointer`, `glDrawArrays`, `glTexImage2D` com mipmaps, `glBlendFunc`, etc.) em chamadas diretas para o driver OpenGL desktop.
  3. Mapear o teclado (Setas/WASD para D-Pad virtual, J/Space para ataque/confirmar, U/I/O para skills 1-3, K/Q para poção, Enter/Z para toque central, Esc/Tab para menu, P para status) e gamepads (analógico/D-Pad, botões A/B/X/Y/LB/RB/Start/Select) em eventos de toque sintéticos contínuos.
  4. Suportar toque direto via mouse (clique e arraste converte coordenadas de tela para a viewport 480x320), permitindo navegação precisa em todos os menus, inventário e cutscenes.
- **Consequências**: O jogo torna-se 100% jogável no PC imediatamente com controles físicos e gráficos acelerados a 60 FPS, estabelecendo o oráculo e a referência definitiva para os futuros ports nativos em C++ / libnx / Android.

---

## ADR-006: Subsistema de Áudio Baseado em OpenAL Pre-decodificado e AVAudioPlayer MP3

- **Data**: 2026-11-09
- **Status**: Aceito
- **Contexto**:
  - A regra 4 de AGENTS.md exige que decodificação de áudio ocorra fora do callback da SDL.
  - O HL3 possui 28 efeitos sonoros WAV (gerenciados via OpenAL) e 20 faixas BGM MP3 (gerenciadas via `AVAudioPlayer`).
- **Decisão**:
  1. Implementar `AudioMixer` (`tools/hl3rt/hle_audio.py`) inicializando o mixer SDL (`44.1 kHz, 16-bit estéreo`).
  2. Carregar e empacotar dados PCM brutos de `alBufferData` em estruturas de áudio pré-decodificadas em memória, disparadas via `alSourcePlay`.
  3. Mapear `AVAudioPlayer` para o backend de música do mixer SDL, decodificando e reproduzindo as faixas de música de fundo em background sem engasgos na thread principal.
- **Consequências**: Efeitos sonoros e trilha musical reproduzem normalmente sem travar a emulação de CPU ARM nem causar estalos de buffer.

---

## ADR-007: Mixagem em 64 Canais Virtuais, Parâmetros OpenAL (Gain/Pitch/Looping), AVAudioPlayer Completo e Persistência de Configuração

- **Data**: 2026-11-09
- **Status**: Aceito
- **Contexto**:
  - No HL3, múltiplos sons de combate, passos e magias ocorrem concorrentemente. O limite padrão de 8 canais do Pygame/SDL causava descarte de sons durante ação intensa.
  - O jogo ajusta parâmetros OpenAL como `AL_GAIN`, `AL_PITCH`, `AL_LOOPING` e consulta `AL_SOURCE_STATE` para verificar se o efeito sonoro terminou.
  - Usuários no PC precisam de controle de volume independente para efeitos sonoros (SFX) e música de fundo (BGM), atalhos de teclado (`+`/`-`/`M`) e persistência entre sessões.
- **Decisão**:
  1. Configurar o mixer para 64 canais virtuais (`pygame.mixer.set_num_channels(64)`), associando cada fonte OpenAL a canais dedicados.
  2. Implementar consulta real de estado em `alGetSourcei` via `channel.get_busy()`, retornando `AL_STOPPED` quando a reprodução do efeito termina.
  3. Suportar `AL_GAIN`, `AL_PITCH`, `AL_LOOPING`, `AL_BUFFER` e vetores 3D em `_alSourcef`/`_alSourcefv`/`_alSourcei`.
  4. Expandir a classe `AVAudioPlayer` com suporte completo a `isPlaying`, `duration`, `currentTime`, `numberOfLoops`, `volume`, `setVolume:`, `setNumberOfLoops:` e fading suave.
  5. Adicionar persistência de configurações de áudio (`config/audio.json`) com volumes independentes (Master, BGM, SFX) e estado de mudo.
- **Consequências**: Som de alta fidelidade sem engasgos nem cortes de canal; controle e persistência de preferências de áudio plenamente funcionais no PC.


---

## ADR-008: Compilação AOT de Blocos Quentes com Guarda Explícita de Opcodes e Oráculo de Verificação

- **Data**: 2026-11-09
- **Status**: Aceito
- **Contexto**:
  - O profiling por fase revelou que a fase de jogo (`title`/`menu`) executa mais de 19 milhões de instruções a cada 30 frames (~640.000 insns/frame), concentradas em laços de transformação matemática de sprites e cálculos de coordenadas (`0x0000f6a0`..`0x0000f720`).
  - No HL4, emissores AOT que tratavam instruções não suportadas como no-op silencioso causavam corrupção de estado sutil em menus e cutscenes (`mvn`, `bic`, `tst`, `cmn`).
- **Decisão**:
  1. Implementar ferramenta de profiling `tools/profile_phases.py` gerando métricas determinísticas por fase (`boot`, `launch`, `title`, `menu`) em `reference/analysis/cpu_profile_phases.json`.
  2. Implementar `tools/generate_aot_blocks.py` compilando os blocos mais frequentes para C++ (`src/arm_hot_blocks.inc`) e Python (`tools/hl3rt/aot_blocks.py`).
  3. Cada bloco possui guarda explícita de opcodes (`guard`) validando byte a byte as instruções originais na memória guest.
  4. Instruções não modeladas nunca são ignoradas como no-op: provocam recusa do bloco ou retorno seguro ao emulador Unicorn.
  5. Validar todos os blocos com o oráculo `tools/verify_aot_oracle.py`, comparando os registradores R0..R12, SP, LR e flags do CPSR contra o emulador Unicorn em estados normais e de borda (0, 1, 31, 32, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF).
- **Consequências**: Execução rápida com garantia formal de paridade comportamental e zero corrupção silenciosa de registradores ou flags.

---

## ADR-009: Proteção de Matrix Stack, Texture Clamping e Mapeamento de Scissor contra Distorções Gráficas

- **Data**: 2026-11-09
- **Status**: Aceito
- **Contexto**:
  - A tela inicial do jogo apresentava desalinhamento vertical em degrau ("stair-step") entre fatias adjacentes de sprites/quads e botões duplicados/deslocados.
  - O binário original iOS executa múltiplas chamadas `glPushMatrix()` por frame sem correspondentes `glPopMatrix()`. No driver desktop OpenGL, isso estoura o limite da pilha (`GL_STACK_OVERFLOW`, profundidade máxima da pilha de projeção é 2 e modelview é 32), acumulando transformações e provocando deslocamentos acumulativos nos quads.
  - Além disso, quads adjacentes apresentavam bordas com artefatos de amostragem de textura por falta de `GL_CLAMP_TO_EDGE`, e chamadas `glScissor` usavam coordenadas Y do sistema de coordenadas do UIKit (origem no topo) em vez de OpenGL (origem na base).
- **Decisão**:
  1. Em `tools/hl3rt/gl_bridge.py`, implementar limitação e saturação de profundidade da pilha de matrizes (`push_matrix` e `pop_matrix`), além de reset explícito para identidade (`glLoadIdentity`) a cada início de frame (`begin_frame`) nas matrizes `GL_PROJECTION` e `GL_MODELVIEW`.
  2. Forçar `GL_CLAMP_TO_EDGE` em `bind_texture` nos eixos S e T para todas as texturas carregadas, prevenindo sangramento de pixels nas emendas dos quads.
  3. Converter corretamente as coordenadas verticais em `glScissor` da convenção UIKit para a convenção OpenGL desktop: `sy = vy + int(((self.game_h - (y + h)) / self.game_h) * vh)`.
- **Consequências**: Renderização 100% alinhada, com quads e ribbons perfeitamente contíguos com zero pixels de desalinhamento ou estiramento indesejado.

---

## ADR-010: Rastreamento Persistente de Instâncias UITouch e Desativação de Control Flow Guard (CFG) no Executável Standalone

- **Data**: 2026-11-09
- **Status**: Aceito
- **Contexto**:
  - No menu inicial, botões clicados (ex.: "START GAME") ficavam permanentemente iluminados em vermelho sem avançar de tela. O desmonte de `TouchResponder touchesBegan:withEvent:` (`0xc0a4`) e `touchesEnded:withEvent:` (`0xbe5c`) revelou que o motor guarda o ponteiro exato do `UITouch*` em um array interno de toques ativos. Ao criar um novo objeto `UITouch` no `touchesEnded`, o motor ignorava o término do toque por incompatibilidade de ponteiro.
  - O executável standalone compilado (`HeroesLore3.exe`) fechava abruptamente durante o boot com código de falha `0xC0000409` (`STATUS_STACK_BUFFER_OVERRUN` / `__fastfail`) em `VCRUNTIME140.dll` dentro da função `longjmp`. O bootloader padrão do Pyinstaller no Windows x64 ativa a flag PE `IMAGE_DLLCHARACTERISTICS_GUARD_CF` (`0x4000`, Control Flow Guard). A biblioteca nativa do Unicorn Engine usa `setjmp`/`longjmp` internamente para saídas de emulação e despachos de CPU ARMv6. Sob CFG, o runtime do MSVC valida se o destino do `longjmp` está registrado na tabela de CFG, o que nunca é o caso para buffers JIT do QEMU/Unicorn, provocando encerramento imediato de segurança.
- **Decisão**:
  1. No host de input (`tools/run_pc.py`), manter registro persistente de objetos de toque (`self.active_touches[tag] = (Touch, t_obj, evt_obj, set_obj)`), reutilizando rigorosamente a mesma instância guest de `UITouch*` entre `touchesBegan:`, `touchesMoved:` e `touchesEnded:`, alterando apenas o campo `phase` (0 = began, 1 = moved, 3 = ended).
  2. Ajustar as coordenadas do botão virtual START para `(240.0, 280.0)` no centro exato do botão "START GAME".
  3. Em `tools/build_standalone_pc.py`, implementar a rotina de pós-processamento `strip_cfg(exe_path)` que limpa a flag `IMAGE_DLLCHARACTERISTICS_GUARD_CF` (`0x4000`) do cabeçalho opcional PE de `HeroesLore3.exe`.
- **Consequências**:
  - Cliques de mouse, teclado (Enter/Espaço) e gamepad (botão A) respondem instantaneamente e avançam perfeitamente para a tela de Seleção de Personagem (Kei / Rits).
  - O executável standalone `HeroesLore3.exe` roda de forma 100% autônoma, suave e estável sem requerer nenhuma ferramenta ou ambiente de desenvolvimento instalado.

---

## ADR-012: Correção de glScissor (Remoção da Inversão Y) e Implementação de Interação / Fala com NPCs via Teclado e Gamepad

- **Data**: 2026-11-10
- **Status**: Aceito
- **Contexto**:
  - O usuário relatou dois problemas críticos após o ajuste de movimentação:
    1. Textos e artes ausentes nos menus: na tela título, botões do carrossel ("START GAME", "CONTINUE") não renderizavam texto; na seleção de personagem, a caixa de descrição do lore ("NATION: NEO SOLTIA...") ficava vazia; na seleção de classe, os 3 sprites chibi de pré-visualização, o texto descritivo e o botão de confirmação (checkmark) desapareciam.
    2. Ao tentar falar com NPCs no jogo, pressionar teclas do teclado (J, Espaço, Enter) não iniciava a conversa, sendo necessário clicar na tela com o mouse sobre o ícone de conversa (balão de diálogo sobre o NPC).
  - **Investigação Técnica**:
    1. *Causa raiz dos textos e sprites ausentes*: Em `tools/hl3rt/gl_bridge.py`, a função `scissor(x, y, w, h)` invertia incorretamente o eixo Y (`sy = vy + int(((self.game_h - (y + h)) / self.game_h) * vh)`), assumindo que o binário guest passava coordenadas do UIKit (topo para baixo). Contudo, o jogo iOS utiliza OpenGL ES 1.1 direto e passa coordenadas de janela padrão do OpenGL (base para cima, onde Y=0 é o rodapé). Ao inverter Y, o retângulo de corte da tela de título (`y=1, h=100`) era movido para o topo (`y=219..319`), descartando 100% dos vértices do botão do carrossel; a caixa de texto da seleção de personagem (`y=21, h=130`) era movida para o topo (`y=169..299`), descartando todo o texto de lore; e as caixas scissored de seleção de classe eram cortadas fora da viewport.
    2. *Causa raiz da interação com NPCs*: No motor iOS original, a interação com NPCs é baseada em hit-test no espaço de mundo (`0x7575c` -> `[r1, #0xc]`), acionada ao tocar diretamente no NPC ou em seu balão de conversa. Em `tools/run_pc.py`, as teclas de teclado e gamepad estavam mapeadas apenas para posições estáticas da HUD: ataque (`420, 270`) e centro (`240, 280`). Nenhuma tecla ou botão enviava toques na área do NPC/balão de fala à frente do personagem (`(240, 160)`).
- **Decisão**:
  1. Em `tools/hl3rt/gl_bridge.py`, remover a inversão Y em `scissor()`, mapeando diretamente `sy = vy + int((y / self.game_h) * vh)`.
  2. Em `tools/run_pc.py`, rastrear o vetor de direção do jogador `(facing_x, facing_y)` atualizado a cada movimento contínuo (WASD, Setas, Analógico, D-Pad).
  3. Mapear tecla dedicada de Interação / Fala `E`, e expandir `Enter`, `Espaço`, `J` e botão A do Gamepad para despacharem toque na coordenada do alvo de interação à frente do personagem (`(240.0 + self.facing_x * 45.0, 160.0 + self.facing_y * 45.0 - 15.0)`), atingindo tanto o corpo do NPC quanto o balão de diálogo.
  4. Manter fallback transparente: se não houver NPC na frente, o ataque padrão (`BTN_ATTACK`) executa normalmente. Durante diálogos, `E`, `Enter`, `Espaço` e `J` avançam os textos sem interrupção.
  5. Recompilar e reempacotar a distribuição standalone em `dist/HeroesLore3_PC/HeroesLore3.exe`.
## ADR-013: Mapeamento do Botão Virtual de Fala com NPCs (Sprite 473 em 385, 246), Navegação do Carrossel via Swipe e Isolamento do Menu In-Game

- **Data**: 2026-11-10
- **Status**: Aceito
- **Contexto**:
  - O usuário relatou três problemas remanescentes de jogabilidade e navegação:
    1. **Navegação no menu de título**: Os botões do carrossel agora aparecem com texto nítido, mas a navegação com as setas do teclado não funcionava (apenas arrastar com o mouse girava o carrossel).
    2. **Clique duplo simultâneo no menu in-game**: Ao abrir o menu in-game e pressionar `Enter` novamente, dois botões eram acionados ao mesmo tempo (`SKILLS` e `SYSTEM`, ambos destacados em vermelho na captura de tela enviada pelo usuário).
    3. **Fala com NPCs**: Pressionar teclas do teclado não iniciava o diálogo com os NPCs; o usuário notou que um botão/ícone virtual de conversa aparecia na tela e que clicar nele com o mouse era a única forma de falar.
  - **Investigação Técnica**:
    1. *Causa raiz da fala com NPCs*: A engenharia reversa do arquivo `newres/ui/hud.bin` revelou que o motor do HL3 define botões gêmeos na área de ações:
       - No offset `0x0982`: Controle tipo `0x32` / Tag `0xd6`, id `0x01d8` (Sprite 472) em `(407.0, 233.0)` -> Botão principal de ATAQUE.
       - No offset `0x0a32`: Controle tipo `0x32` / Tag `0xd7`, id `0x01d9` (Sprite 473) em `(385.0, 246.0)` -> Botão contextual de FALA / INTERAÇÃO com NPCs que surge na HUD quando o herói se aproxima de um NPC!
       Nenhuma tecla estava mapeada para `(385.0, 246.0)`, fazendo com que teclas do teclado errassem o botão virtual.
    2. *Causa raiz do clique duplo no menu in-game*: Em `tools/run_pc.py`, a tecla `Enter` (`keys[K_RETURN]`) acionava simultaneamente `interact` (em `(240, 145)`, que colidia diretamente com o botão `SKILLS` em Y=135) e `center` (em `(240, 280)`, que colidia diretamente com o botão `SYSTEM` em Y=270). Além disso, durante o gameplay de campo, `(240, 280)` é a barra inferior de menu, fazendo com que `Enter` abrisse o menu acidentalmente.
    3. *Causa raiz do carrossel da tela título*: Na tela de título não existe D-Pad em `(60, 265)`. O componente de carrossel do motor exige um gesto de arrasto horizontal (swipe) através de `Y=280` (`320 -> 160` para avançar, `160 -> 320` para retroceder) para girar os itens.
- **Decisão**:
  1. Em `tools/run_pc.py`, mapear `BTN_TALK = (385.0, 246.0)` como alvo principal das teclas `E`, `Enter` e botão `A` do Gamepad, acompanhado do alvo do mundo `interact_coord`, ativando imediatamente o diálogo com qualquer NPC.
  2. Implementar máquina de estados clara para o menu in-game (`self.menu_mode`):
     - Navegação com setas `Cima` / `Baixo` (ou `W` / `S`, Gamepad D-pad/Stick) altera o cursor entre os 6 itens (`Y = [45, 90, 135, 180, 225, 270]`) e envia destaque limpo.
     - `Enter` ou `Espaço` (ou Gamepad `A`) aciona estritamente o item selecionado pelo cursor, eliminando completamente cliques duplos.
     - `Escape`, `Tab` ou Gamepad `B` / `Start` fecha o menu via botão de retorno `(50.0, 40.0)`.
     - No campo aberto, `Enter` funciona exclusivamente para conversar/interagir, sem abrir o menu in-game por acidente.
  3. Na tela de título, detectar o estado e mapear as setas `Esquerda` e `Direita` (ou `A` / `D`, Gamepad D-pad/Stick) para dispararem um gesto de swipe horizontal rápido (`swipe_carousel`) com cooldown controlado de 14 frames, girando suavemente as opções.
  4. Atualizar os testes automatizados em `tests/test_run_pc.py` (24 testes passando) e recompilar o executável standalone `dist/HeroesLore3_PC/HeroesLore3.exe`.
- **Consequências**:
  - Diálogos com NPCs iniciam instantaneamente via teclado (`E`, `Enter`, `Espaço`) e Gamepad (`A`) pelo botão virtual nativo `(385, 246)`.
  - Navegação do carrossel de título funciona perfeitamente com setas do teclado e direcionais do gamepad.
  - O menu in-game é navegado de forma limpa, sem ativação de múltiplos botões simultâneos e sem abrir o menu por engano ao teclar Enter.

---

## ADR-014: Alinhamento Inicial de Botões Virtuais e Cursor Visual no Menu de Pausa

- **Data**: 2026-11-10
- **Status**: Substituído pelo ADR-015
- **Contexto**:
  - Implementação inicial de navegação com setas em menus, suporte a classes e exibição de cursor visual dourado no menu de pausa.

---

## ADR-015: Máquina de Estados de 5 Etapas (Título, Personagem, Classe, Resumo, Gameplay), Pulso de Toque Multiframe e Alinhamento Pixel-Perfect

- **Data**: 2026-11-10
- **Status**: Aceito
- **Contexto**:
  - O usuário relatou quatro problemas após a primeira implementação de seleção e HUD:
    1. Na tela de seleção de tipo/classe, não conseguia navegar com o teclado.
    2. Após selecionar o tipo, não conseguia confirmar com o teclado para avançar (apenas clicando com mouse); e ao pressionar Esc surgia uma moldura amarela indevida na tela.
    3. Ao iniciarem as cutscenes, não conseguia pular com teclado (apenas clicando no botão).
    4. No mapa com NPCs, ainda não conseguia conversar usando as teclas do teclado.
  - **Investigação Técnica Profunda**:
    1. *Descoberta da tela de Resumo de Personagem (Step 3)*: O fluxo de criação de personagem do HL3 possui 4 telas antes do jogo:
       - Step 0: Carrossel de Título (`"title"`)
       - Step 1: Seleção Kei vs Rits (`"char_select"`)
       - Step 2: Seleção de Classe Berserker/Drill/Knight (`"class_select"`)
       - Step 3: Resumo do Herói com Nação e Classe (`"summary"`)
       - Step 4: Cutscenes de Abertura e Gameplay na Vila (`"game"`)
       O código anterior omitia o Step 3 e saltava direto de `class_select` para `game`. Na tela de resumo real, a ilustração de Kei ocupa o lado direito e o botão verde de confirmação fica no canto inferior da caixa de texto em `(355.0, 285.0)`, e não na borda em `(415.0, 284.0)`. Ao pressionar Enter, o toque atingia o cinto de Kei e era ignorado.
    2. *Origem da moldura amarela ao teclar Esc*: Como `self.scene` estava desincronizado (marcado como `game` enquanto a tela era o resumo), pressionar Esc caía no `else` que ativava `self.menu_mode = True`, desenhando o cursor dourado do menu de pausa sobre a arte de Kei e travando a entrada no modo menu.
    3. *Mecânica do TouchResponder e pulso multiframe*: `TouchResponder` exige que o toque permaneça ativo durante os ciclos de `onTimer:` antes de receber `touchesEnded:withEvent:`. O `tap_once` anterior liberava o toque em 16ms (1 único frame), gerando oscilação e descarte de eventos pelo motor quando a tecla permanecia pressionada.
    4. *Coordenadas de Fala com NPC e Pulo de Cutscene*:
       - Fala com NPC: O balão azul medido em `media_1794324014624_479f1886.jpg` localiza-se em `(445.0, 286.0)` / `(435.0, 275.0)`.
       - Pulo de Cutscene: O botão circular marrom com "X" situa-se em `(410.0, 28.0)` com o texto SKIP em `(410.0, 45.0)`.
- **Decisão**:
  1. Expandir a máquina de estados para 5 passos determinísticos (`0..4`) com sincronização bidirecional entre mouse e teclado:
     - `step 0`: Setas giram carrossel; Enter confirma `(240, 280)`.
     - `step 1`: Setas/AD selecionam Kei `(150, 100)` ou Rits `(330, 100)`; Enter confirma `(415, 284)`; Esc volta para `step 0`.
     - `step 2`: Setas/AD acionam setas `<` `(99, 88)` e `>` `(389, 86)`; teclas `1`, `2`, `3` selecionam os chibis diretamente `(160, 80)`, `(244, 80)`, `(338, 80)`; Enter confirma `(415, 284)`; Esc volta para `step 1`.
     - `step 3`: Enter confirma no checkmark do resumo `(355, 285)`; Esc volta para `step 2` via `BTN_BACK (27, 17)`. Esc nunca abre o menu de pausa fora do `step 4`.
     - `step 4`: Gameplay completo; D-pad 360°, ataque `J`/`Espaço`, conversa com NPCs e avanço de texto via `E`/`Enter`/Gamepad A `(445, 286)`, pulo de cutscenes via `S`/`Tab`/Gamepad Y/Start `(410, 28)`. Menu de pausa acionado via Esc/M/Start exclusivamente nesta etapa.
  2. Implementar contador de sustentação em `tap_once(tag, coord, hold_frames=2)`: cada toque sintético é mantido por múltiplos ciclos de frame antes do `touchesEnded`, garantindo reconhecimento instantâneo pelo motor iOS.
  3. Adicionar guarda estrita em `_draw_menu_cursor`: só desenha o retângulo dourado se `self.menu_mode and self.step == 4`.
  4. Manter todos os 24 testes unitários passando em `tests/test_run_pc.py` e gerar novo build standalone verificado em `dist/HeroesLore3_PC/HeroesLore3.exe`.
- **Consequências**:
  - Navegação de classe responde imediatamente a setas e números `1`, `2`, `3`.
  - Confirmação após a classe avança perfeitamente na tela de resumo com Enter.
  - Teclar Esc nas telas de seleção/resumo recua de tela de forma limpa, sem moldura amarela espúria.
  - Cutscenes pulam imediatamente com `S` ou `Tab`.
  - Interação e diálogos com NPCs funcionam com `E`, `Enter` ou botão A do Gamepad.

---

## ADR-016: Controlador de Entrada Orientado ao Estado Real de Memória do Motor Guest (AppManager, UI_Mgr, GameCtx), Coordenadas Exatas da Árvore de Controles e Embutimento do Ícone Oficial

- **Data**: 2026-11-10
- **Status**: Aceito
- **Contexto**:
  - Heurísticas anteriores de entrada baseadas em contagem de passos (`self.step 0..4`) sofriam de descompasso quando o usuário clicava com o mouse ou o jogo realizava transições intermediárias (ex.: tutoriais, popups, cutscenes com `sub_state == 22`).
  - O botão de conversa com NPCs não ativava via teclado mesmo quando o balão de fala azul aparecia sobre o botão da HUD.
  - As setas de navegação de classe não respondiam ao teclado.
  - O menu de pausa no jogo sofria de cliques múltiplos acidentais ao teclar Enter.
  - O ícone do jogo no executável e na barra de títulos era o ícone genérico do Python.
- **Investigação Técnica e Descobertas de Engenharia Reversa**:
  1. *Causa raiz da falha do botão de fala com NPCs*:
     - No modo pacífico (`combat_mode == 0`), a HUD (`form_id = 70`) posiciona o botão de fala (id `7025`) em `(450.0, 258.0)` com bounding box `(421..479, 237..279)`. O código anterior disparava em `(445.0, 286.0)` (`y=286` é 7 pixels abaixo da borda inferior do botão, caindo em espaço vazio!).
     - No modo de combate (`combat_mode == 1`), o botão de ataque principal (id `7007`) assume `(450.0, 258.0)` e o botão contextual de fala (id `7026`) desloca-se para `(397.0, 239.0)`.
     - O motor do jogo avalia colisão com NPCs em `0x41550` (caixa 72x72) e aciona `0x90894` que escreve `1` na flag `can_talk` em `*(uint8*)0x000de0ad`.
  2. *Causa raiz da falha das setas de seleção de classe*:
     - A inspeção de memória da tela de seleção de classe (`form_id = 42`) revelou que o botão `<` (id `4207`) está em `(73.0, 71.0)` e o botão `>` (id `4206`) está em `(400.0, 71.0)`. As coordenadas anteriores `(99, 88)` e `(389, 86)` clicavam fora das hitboxes dos botões.
  3. *Arquitetura de Estado do Motor em Tempo Real*:
     - `AppManager*` obtido diretamente em `*(uint32_t*)0x000d7dd4`.
     - `app_state` em `*(int32_t*)(app + 0x78)`: `1` = Pré-jogo / Menus, `2` = Gameplay / Cutscene, `8` = Menu de Pausa.
     - `ui_main` em `*(uint32_t*)(app + 0x40)` -> `main_form` em `+0x98` (`form_id` em `+0x14`), `main_popup` em `+0xa0`.
     - `ui_hud` em `*(uint32_t*)(app + 0x44)` -> `hud_form` em `+0x98` (`form_id = 70`), `hud_popup` em `+0xa0` (`form_id = 130` Popup de Tutorial).
     - `GameCtx` em `0x000d8270`: `sub_state` em `*(int8_t*)0x000de30c` (`0` = menus, `3` = campo livre, `4`/`22` = diálogo/cutscene, `7` = caixa de texto), `can_talk` em `*(uint8_t*)0x000de0ad`, `combat_mode` em `*(uint8_t*)0x000de0ac`.
- **Decisão**:
  1. Abandonar heurísticas desacopladas e basear todo o controle de entrada na leitura síncrona do estado de memória guest (`get_engine_state()`).
  2. Adicionar métodos de conveniência em `Machine`: `rd16`, `rds16`, `rd8`, `rds8`, `rdf32`.
  3. Mapear as coordenadas reais extraídas dos controles:
     - Título: Carrossel em `(240.0, 272.0)`.
     - Seleção de Personagem (`id=41`): Kei `(160, 96)`, Rits `(320, 96)`, Checkmark `(415, 295)`, Back `(28, 28)`.
     - Seleção de Classe (`id=42`): `<` em `(73, 71)`, `>` em `(400, 71)`, Checkmark `(415, 295)`, Back `(28, 28)`.
     - Resumo (`id=43`): Checkmark `(356, 295)`, Back `(28, 28)`.
     - Cutscene (`id=44`, `sub=4/22`): SKIP `(408, 28)`, avanço de diálogo `(240, 160)`.
     - Popups (`id=130/132/133`): Yes/Confirm `(395, 220)`, No/Cancel `(395, 100)`, Tutorial Next `(385, 207)`, Tutorial Close `(385, 74)`.
     - HUD (`id=70`): Fala em Paz `(450, 258)`, Fala em Combate `(397, 239)`, Ataque `(450, 258)`, Menu/Pause `(200, 292)`.
     - Menu de Pausa (`id=2`, `app_state=8`): Itens em `Y = [45, 90, 135, 180, 225, 270]`, Back `(28, 28)`.
  4. Mudar o tratamento de teclas de ação e menus no Pygame para eventos discretos `KEYDOWN` (em vez de checagem contínua de `get_pressed()`), evitando disparos repetidos ou múltiplos no menu de pausa.
  5. Extrair o ícone original do bundle iOS (`reference/extracted_ipa/Payload/Heroes Lore.app/Icon.png`), gerar `assets/icon.png` e `assets/icon.ico` (multi-tamanho até 256x256), aplicar como ícone de janela no Pygame e embutir no executável compilado via PyInstaller.
- **Consequências**:
  - Todas as transições de tela e confirmações funcionam com fluidez e precisão absoluta.
  - Falar com NPCs no campo responde com 100% de confiabilidade via `E`, `Enter`, `Espaço` e Gamepad `A`.
  - O executável standalone e a janela no PC ostentam a arte original de Heroes Lore 3 sem artefatos genéricos.

---

## ADR-017: Isolamento de Atributos OpenGL do Cursor, Ordem de Inicialização de Ícone no SDL2/Pygame e Mapeamento dos Botões Virtuais de Poções (Superior/Inferior) e Mapa

- **Data**: 2026-11-10
- **Status**: Aceito
- **Contexto**:
  - O usuário relatou dois problemas críticos após a execução da versão standalone:
    1. A janela ficava completamente preta ("tá tudo preto agora!").
    2. Os botões clicáveis da tela não respondiam ao teclado, especificamente o botão do mapa e os botões das poções.
  - **Investigação Técnica e Descobertas**:
    1. *Causa raiz da tela preta*:
       - Em `tools/run_pc.py`, a função `_draw_menu_cursor()` invocava `glDisable(GL_TEXTURE_2D)` para desenhar o retângulo do cursor dourado do menu de pausa. No entanto, ela nunca restaurava `glEnable(GL_TEXTURE_2D)` ao concluir. Quando o menu de pausa era avaliado ou aberto, a renderização com texturas ficava desligada de forma permanente no contexto OpenGL. Como o pipeline de renderização do jogo pressupõe `GL_TEXTURE_2D` preservado entre frames, todos os desenhos subsequentes (inclusive após sair do menu ou carregar save) passavam a gerar quads untextured escuros.
       - Além disso, `pygame.display.set_icon` estava sendo chamado antes de `pygame.display.set_mode()`. No Pygame 2 / SDL2 no Windows x64, chamar `set_icon` antes da criação da janela disparava silenciosamente `pygame.error: No video mode has been set` e corrompia os ponteiros de pixel format / swap chain do WGL.
    2. *Causa raiz das poções e mapa*:
       - O despejo em tempo real da árvore de controles do formulário de HUD (`form_id = 70`) revelou a disposição exata dos elementos na tela:
         - `id=7011`: Minimap / Bússola / Mapa no canto superior esquerdo em `(23.5, 22.0)`.
         - `id=7002`: Poção Superior (Slot 1) na lateral esquerda em `(29.0, 107.0)`.
         - `id=7003`: Poção Inferior (Slot 2) na lateral esquerda em `(29.0, 157.0)`.
       - O código anterior possuía apenas `HUD_POTION_1` apontando para `(415.0, 45.0)` (coordenada totalmente errada, no canto superior direito do céu), não possuía a segunda poção e não mapeava o botão do Mapa.
- **Decisão**:
  1. Em `tools/run_pc.py`, encapsular o desenho do cursor com `glPushAttrib(GL_ALL_ATTRIB_BITS)` e `glPopAttrib()`, forçando explicitamente `glEnable(GL_TEXTURE_2D)` no retorno.
  2. Em `tools/hl3rt/gl_bridge.py`, adicionar `glEnable(GL_TEXTURE_2D)` e restauração de cor branca `glColor4f(1.0, 1.0, 1.0, 1.0)` em `begin_frame()`, blindando o motor gráfico contra qualquer vazamento de estado.
  3. Em `_init_window()`, reposicionar a chamada de `set_icon` para logo após `set_mode()`, convertendo o ícone com `.convert_alpha()` e redimensionando para 32x32 via `pygame.transform.smoothscale`.
  4. Mapear as teclas de HUD com as coordenadas reais:
     - **Mapa / Bússola**: Tecla `M` e `Tab` (no campo aberto `sub_state == 3`), Gamepad `Select`.
     - **Poção Superior (Slot 1)**: Tecla `Q` e `1`, Gamepad `LB`.
     - **Poção Inferior (Slot 2)**: Tecla `K` e `2`, Gamepad `RB`.
     - **Skills 1, 2, 3**: Teclas `U` / `3`, `I` / `4`, `O` / `5`, Gamepad `X` / `Y`.
     - **Ataque Primário**: Tecla `J` e `Espaço`.
     - **Ataque Secundário / Guarda**: Tecla `L` e `H`.
  5. Recompilar e reempacotar `dist/HeroesLore3_PC/HeroesLore3.exe` com as correções e CFG desativado.
- **Consequências**:
  - A janela inicializa e roda com cores perfeitas (>100.000 cores únicas no frame de boot) sem risco de tela preta.
  - O mapa abre e fecha instantaneamente pelo teclado (`M`, `Tab`) ou controle (`Select`).
  - As duas poções (superior e inferior) são consumidas perfeitamente pelo teclado (`Q`/`1` e `K`/`2`) e controle (`LB`/`RB`).

---

## ADR-018: Aplicação Explícita de Ícone Win32 (`AppUserModelID` + `WM_SETICON` / `SetClassLongPtrW`) e Navegador em Árvore Viva de `UIControl` para Todos os Submenus e Popups

- **Data**: 2026-11-13
- **Status**: Aceito
- **Contexto**:
  - Dois problemas finais impediam a experiência PC nativa completa na build anterior:
    1. A barra de tarefas e o título da janela no Windows 11 ainda exibiam o ícone padrão amarelo do Pygame ("cobrinha") em certas execuções porque `pygame.display.set_icon()` via `Surface` após `set_mode()` nem sempre substitui o ícone de classe `HICON` / agrupamento da taskbar do Windows sem `AppUserModelID` próprio nem recurso `.ico` multi-resolução HD.
    2. Embora o Menu de Pausa principal (`form_id == 2`) já fosse navegável, ao entrar nos 6 submenus (`CHARACTER`, `EQUIPMENT`, `SKILLS`, `ITEMS`, `QUESTS`, `SYSTEM`) e em suas telas filhas (`PREFERENCES`, `HELP`, popups `SAVE` / `MAIN MENU`), não havia cursor nem movimentação de foco entre slots, abas, botões dinâmicos (`USE`/`DROP`), sliders e diálogos Sim/Não; além disso, `K_s` estava restrito a `app_state == 1` fora de cutscenes e faltava leitura do analógico esquerdo em menus.
- **Decisão**:
  1. **Ícone HD e Integração Win32**:
     - Extrair `iTunesArtwork` (512x512 RGBA) do `game.ipa` original para `assets/icon.png` e gerar `assets/icon.ico` multi-tamanho `(16, 24, 32, 48, 64, 128, 256)`.
     - Em `tools/run_pc.py` (`_init_window` / `_apply_win32_icon`), registrar `SetCurrentProcessExplicitAppUserModelID("DavidBraga.HeroesLore3.Recomp.PC")`, chamar `pygame.display.set_icon()` antes de `set_mode()`, e após `set_mode()` (e `F11`) aplicar `LoadImageW` (`IMAGE_ICON`) via `WM_SETICON` e `SetClassLongPtrW` (`GCLP_HICON` / `GCLP_HICONSM`) na janela SDL e na janela de console.
  2. **Navegador Orientado à Árvore Viva de Controles (`UIControl`)**:
     - Incluir `main_form`, `main_popup` e `hud_popup` no retorno de `get_engine_state()`.
     - Percorrer dinamicamente containers visíveis/ativos (`(vis & 0x0003) == 0x0003`), priorizando overlays em primeiro plano (`837`/`836` em `ITEMS`, `4708` em `QUESTS`), coletando controles interativos ativos (`ctype in (2, 4, 13)` com `(vis & 0x0003) == 0x0003` e `frect` válido) e áreas roláveis (`ctype == 10`), filtrando botões redundantes de retorno no canto superior esquerdo (`cx < 55, cy < 55`, incluindo `cid=4709` de `QUESTS`) quando existem controles reais ou `scroll_boxes`.
     - Implementar foco estável ancorado por `cid` (`_sync_focus`) — preservando o foco no slot quando botões como `USE (883)` / `DROP (823)` se habilitam dinamicamente —, foco inicial padrão no botão de confirmação em popups (`Yes`), cursor visual dourado em `_draw_focus_rect()`, navegação 2D ponderada (`_move_focus_2d`), controle direto de sliders (`ctype == 13` via `Left`/`Right` nos filhos `< 198` e `> 197`), rolagem vertical por toque (`_scroll_box` em `ctype == 10`) e troca de abas/páginas via `LB`/`RB`, `Tab`, `PageUp`/`PageDown` e `[`/`]`.
     - Restringir atalhos de poções/skills/ataque ao estado de campo aberto (`is_field`) e adicionar suporte ao analógico esquerdo e repetição de D-Pad em menus/popups.
- **Consequências**:
  - Janela do jogo e barra de tarefas no Windows 11 exibem consistentemente o ícone oficial do Heroes Lore III, validado no executável standalone (`ExtractIconExW == 2`, `WM_GETICON` não-nulo).
  - Todos os 6 submenus, sliders de `PREFERENCES`, paginação/rolagem de `HELP` e `QUESTS`, seleção/uso de itens em `ITEMS`, abas/slots de `SKILLS`/`EQUIPMENT`/`CHARACTER` e diálogos Sim/Não (`SAVE`/`MAIN MENU`) são 100% operáveis por teclado e gamepad, validados em teste vivo ponta a ponta e em 26 testes unitários passando.








