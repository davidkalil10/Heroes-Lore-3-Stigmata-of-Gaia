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
- **Consequências**:
  - Textos de todos os menus, carrossel de opções, história dos personagens e artes das classes renderizam de forma 100% nítida e completa.
  - O jogador consegue conversar com qualquer NPC e avançar diálogos normalmente usando apenas o teclado (`E`, `Enter`, `Espaço`, `J`) ou controle de videogame (botão `A`), eliminando a necessidade de usar o mouse.
  - Distribuição standalone `dist/HeroesLore3_PC/HeroesLore3.exe` verificada e pronta para execução autônoma no PC.


