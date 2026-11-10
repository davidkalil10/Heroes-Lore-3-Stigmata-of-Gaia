# Lições Consolidadas de Heroes Lore 4 e Heroes Lore 5

Este documento reúne os aprendizados práticos de engenharia obtidos nos ports de **Heroes Lore 5: Covenant of Darkness** (`reference/hl5_snapshot/`) e **Heroes Lore 4: Knight of Soltia** (`reference/hl4_snapshot/`), adaptados para orientar o desenvolvimento de **Heroes Lore 3: Stigmata of Gaia**.

---

## 1. Performance de CPU e Compilação AOT de Blocos Quentes

1. **Meça por fase antes de otimizar às cegas**:
   - Instrumente desde o início (`S03`/`S04`) contadores de instruções, chamadas HLE e tempo de frame separados por fase (`boot`, `title`, `menu`, `field`, `combat`).
   - No HL5 e no HL4, o gargalo de fluidez em todas as plataformas não estava na GPU, mas no custo de fronteira de execução ARM (milhares de entradas/saídas do Unicorn ou invalidações de TB por frame).
2. **Compilação AOT de blocos básicos / superblocos com guarda de opcode**:
   - Compilar os blocos ARM/Thumb mais quentes para C++ (`arm_hot_blocks.inc` / `hot_blocks.inc`) reduz drasticamente o overhead do interpretador.
   - **Lição crítica do HL4**: Nunca permita que o emissor AOT traduza instruções não suportadas como `no-op` silencioso! No HL4, instruções `mvn`, `bic`, `tst` e `cmn` não tratadas causavam corrupção de estado em menus/cutscenes até que um **guarda explícito de decodificação de opcodes** (`verify_arm_emit.py`) e um **oráculo completo de memória + registradores** (`verify_fast_runtime.py`) fossem implementados.
   - Todo bloco AOT gerado para o HL3 deve:
     - Falhar de forma explícita na geração caso encontre qualquer instrução não modelada.
     - Ser verificado contra o oráculo Unicorn comparando `r0..r15`, `CPSR` e **todas as páginas de memória guest sujas**.
3. **Proibido criar/destruir `std::thread` por chamada de emulação**:
   - No HL4, criar uma `std::thread` de watchdog a cada `uc_emu_start` degradava severamente o desempenho no Windows, Android e Switch.
   - Use um único worker persistente (`src/emulation_watchdog.inc` do HL4) ou limite determinístico de instruções/blocos no `uc_emu_start`.

---

## 2. Portabilidade e Estabilidade no Nintendo Switch (`libnx`)

1. **Orçamento de memória do TCI (`tb_gen_buffer`)**:
   - No Switch (Modo Applet e Aplicação), reservar buffers gigantes de JIT/TCI no Unicorn causa falha imediata de alocação ou tela preta.
   - Aplicar o patch determinístico (`tools/patch_unicorn_switch.py` e `platforms/switch/unicorn_os.c` do `reference/hl4_snapshot/`) limitando `DEFAULT_CODE_GEN_BUFFER_SIZE_1` a **32 MiB** (`32 * 1024 * 1024`).
2. **Logging precoce em arquivo SD antes da SDL**:
   - Se o app falhar antes de abrir a janela SDL no Switch, `stdio` padrão não mostra nada. Use escrita direta com `fsFsOpenFile` / `fsFileWrite` + `fsFileFlush` logo na primeira linha do `main()` para diagnosticar qualquer falha de boot em segundos.
3. **Orientação e aproveitamento de tela**:
   - Diferente de HL4/HL5 (que nasceram retrato 240x320), **o HL3 de iOS já é nativamente Paisagem (`480x320`, `UIInterfaceOrientationLandscapeRight`)**, o que casa perfeitamente com a tela horizontal do Nintendo Switch, PC e Android em modo landscape.

---

## 3. Renderização OpenGL ES 1.1 e True Widescreen

1. **Tradução limpa de GLES 1.1 e texturas `.pvr`**:
   - O HL3 renderiza via OpenGL ES 1.1 (`glOrthof`, `glMatrixMode`, `glVertexPointer`, `glTexCoordPointer`, `glDrawArrays`, `glDrawElements`, `glCompressedTexImage2D`, `glTexImage2D`).
   - As texturas em `newres/sprites/*.pvr` e `newres/fonts/*.pvr` usam contêiner PowerVR (`.pvr`), que pode conter PVRTC 2bpp/4bpp ou RGBA4444/RGBA8888/RGB565. Decodifique `.pvr` para RGBA8888 em cache ao carregar a textura para que funcione identicamente em qualquer GPU Desktop, Android ou Switch.
2. **True Widescreen sem esticar HUD**:
   - O viewport original do HL3 no iPhone 3G/3GS é `480x320` (proporção `3:2` = `1.50`). Em telas `16:9` (`1.777`), `16:10` (`1.60`) ou `20:9` (`2.22`), jamais estique o framebuffer `480x320` horizontalmente.
   - Mantenha a escala vertical íntegra: expanda a projeção ortográfica/culling do mapa em campo quando aplicável e ancore os elementos de HUD/menus sem distorção de aspecto, validando com testes automatizados de aspecto (`verify_aspect`).

---

## 4. Subsistema de Áudio (OpenAL + MP3 + WAV)

1. **Jamais decodifique MP3/WAV dentro do callback de áudio da SDL**:
   - O HL3 possui 20 faixas de música em `snd/bgm0_mf.mp3`..`bgm16_mf.mp3` (controladas via `AVAudioPlayer`) e 28 efeitos sonoros `.wav` (controlados via OpenAL `alGenBuffers`/`alBufferData`/`alSourcePlay`).
   - Decodifique os arquivos `.mp3` usando `dr_mp3.h` (já disponível em `reference/hl5_snapshot/src/audio/dr_mp3.h`) em cache de PCM em memória ou thread dedicada, e faça apenas mixagem inteira leve no callback da SDL.

---

## 5. Localização (EN / PT-BR) e Troca de Idioma em Tempo Real

1. **Vantagem enorme do HL3: Inglês oficial já incluso no pacote iOS**:
   - Enquanto HL4 e HL5 exigiram traduzir do coreano/chinês desde o zero para obter o inglês, o bundle iOS do HL3 (`Payload/Heroes Lore.app/`) já traz localização oficial em inglês (`en.lproj/Localizable.strings`, `help_text.txt`, `dmg_strings.txt`, `store_strings.txt`, além de `dat/InGame_txt`).
   - Ainda assim, devemos auditar todas as strings em `S09` para garantir que nenhum texto residual em outro idioma apareça em EN, e usar essa base oficial em inglês para construir a tradução completa em **Português (PT-BR)** em `S10`.
2. **Interceptação em tempo de leitura/desenho com buffers estáveis**:
   - A troca de idioma (EN <-> PT-BR) deve ocorrer em tempo real pelo menu nativo sem exigir reinicialização do app e sem gravar strings traduzidas dentro do arquivo de save.
   - Sempre verifique o suporte a caracteres acentuados do português (`á, é, í, ó, ú, ã, õ, ç`) nas fontes `.rff`/`.pvr` (`font_hud.rff`, `font_mainmenu.rff`, `font_menu_medium.rff`, `font_menu_small.rff`).

---

## 6. Menu Nativo, Cloud Save e OTA Updater

1. **Acesso explícito e universal ao Menu Nativo**:
   - Ofereça sempre botão visível na tela (toque/mouse), tecla `F10` no PC e botão `R3` (clique do analógico direito ou combinação clara) no gamepad/Switch.
2. **Cloud Save com staging atômico e retorno seguro ao título**:
   - Ao baixar um save do Google Drive, grave primeiro em arquivo temporário, valide integridade, substitua os arquivos locais de save (`Documents/` / `NSUserDefaults` / `trackingdb.sqlite`) e **reinicie o estado da sessão para a tela título** para que o jogo recarregue a lista de saves limpa da memória.
