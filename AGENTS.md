# AGENTS.md - Heroes Lore 3 Recomp

Este repositório contém o projeto de recompilação/runtime nativo multiplataforma de **Heroes Lore 3: Stigmata of Gaia** (`heroes_lore_3_recomp`).

Qualquer agente de IA que iniciar trabalho nesta pasta deve ler e seguir este arquivo antes de editar código, rodar builds ou propor mudanças arquiteturais.

## Ordem obrigatória de leitura no início de toda sessão

1. `docs/STATUS.json` (ou `python tools/project_status.py show`)
2. `docs/HANDOFF.md`
3. `docs/ROADMAP.md`
4. `docs/HL4_AND_HL5_LESSONS.md`
5. `docs/DECISIONS.md`
6. `reference/analysis/INITIAL_FINDINGS.md`
7. O arquivo da etapa ativa em `docs/tasks/` (quando existir para `current_step`, ex.: `docs/tasks/S02_RUNTIME_CONTRACT.md`)

Depois da leitura inicial, rode:

```powershell
python tools/verify_workspace.py
```

## Escopo e isolamento de diretórios

- Trabalhe **exclusivamente** dentro de `C:\Users\david\Desktop\Projetos_Flutter\heroes_lore_3_recomp`.
- **Nunca** modifique `heroes_lore_5_recomp`, `heroes_lore_4_recomp`, `heroes_lore_recomp`, `heroes_lore_2_recomp` ou `heroes_lore_zero_recomp`.
- Para consultar soluções anteriores de HL4, HL5 ou dos ports J2ME, use os snapshots locais congelados em:
  - `reference/hl4_snapshot/` (manifesto `reference/HL4_SNAPSHOT.json`)
  - `reference/hl5_snapshot/` (manifesto `reference/HL5_SNAPSHOT.json`)
  - `reference/jar_ports_docs/` (manifesto `reference/JAR_PORTS_DOCS.json`)
- Preserve o arquivo original `Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk` intacto na raiz.

## Regras fundamentais de engenharia (descobertas específicas do HL3 + lições de HL4/HL5)

1. **O artefato do HL3 é um bundle iOS (`assets/game.ipa`) com executável Mach-O ARMv6 dentro de um wrapper touchHLE**:
   - Diferente do HL5 (ELF Android via JNI `CletEntry`) e do HL4 (ELF Android via wrapper WIPI `MC_*`/`wipic_*`), o APK do HL3 embute `assets/game.ipa` (`43.686.130` bytes), cujo executável real é `Payload/Heroes Lore.app/Heroes Lore` (`915.824` bytes, **Mach-O 32-bit ARMv6 `MH_EXECUTE`**, bundle `com.ea.heroesloresg.bv` v1.0.1).
   - O binário usa **Objective-C 2.0 (`libobjc.A.dylib`)**, **UIKit / Foundation / CoreFoundation / QuartzCore** (`UIApplicationMain`, `AppDelegate`, `GLView`, `TouchResponder`, `EAGLContext`, `NSTimer`), **OpenGL ES 1.1** (`glOrthof`, `glBindTexture`, `glDrawArrays`, `glDrawElements`, `glCompressedTexImage2D`, `glBindFramebufferOES`), **OpenAL + AVAudioPlayer/AudioToolbox**, **SQLite3** e **zlib**.
   - **Proibido** assumir JNI do HL5 ou WIPI do HL4; o loader e a HLE do HL3 devem mapear segmentos Mach-O (`__TEXT`, `__DATA`, `__LINKEDIT`), resolver `__nl_symbol_ptr`/`__la_symbol_ptr`, inicializar seletores/classes Objective-C necessários e prover HLE para GLES1, OpenAL/Audio, Foundation/UIKit e libc/libstdc++.
2. **Não reutilizar offsets, ponteiros ou IDs de outros jogos**:
   - Endereços de memória guest, tabelas de câmera, buffers de texto, save paths, `applicationId` Android (`com.davidbraga.heroeslore3`), `Title ID` do Switch (`0100F3A300030000`) e repositório GitHub devem ser exclusivos do HL3.
3. **Performance de CPU vem antes de empacotamento para Switch/Android**:
   - Qualquer camada de execução ARMv6/Thumb deve nascer com contadores por fase (`boot`, `title`, `field`, `combat`, `menu`), blocos quentes compilados (AOT C++) validados contra oráculo completo de memória/registradores (Unicorn) e guarda explícita de opcodes nas bordas de blocos (lição crítica do HL4 onde `mvn`/`bic`/`tst`/`cmn` omitidos causaram corrupção silenciosa antes do guarda de decodificação).
   - Jamais crie ou destrua `std::thread` por chamada de emulação (use thread persistente ou contador determinístico de blocos/instruções).
4. **Áudio fora do callback da SDL**:
   - Decodificação de `.mp3` (`snd/bgm0_mf.mp3`..`bgm16_mf.mp3` via `AVAudioPlayer`/`dr_mp3`) e `.wav` (28 efeitos sonoros via OpenAL) nunca deve ocorrer dentro do callback de áudio da SDL; pré-decodifique e faça mixagem leve em buffer intermediário.
5. **True Widescreen sem esticar HUD**:
   - O jogo original opera em paisagem (`UIInterfaceOrientationLandscapeRight`, 480x320 no iPhone OS 3.0) via projeção ortográfica OpenGL ES 1.1 (`glOrthof`) e texturas `.pvr`.
   - A expansão de viewport (16:9, 16:10, 20:9, 4:3, janela livre) deve expandir o campo visível do mundo ou aplicar composição ciente de fase sem distorcer proporções da HUD/menus.
6. **Idiomas (EN / PT-BR) em tempo real**:
   - O bundle iOS já inclui localização oficial em inglês (`en.lproj/Localizable.strings`, `help_text.txt`, `dmg_strings.txt`, `store_strings.txt`, `dat/InGame_txt`), além de `de`, `es`, `fr`, `it`, `ko`, `zh` e fontes `.rff`/`.pvr`.
   - Mantenha **Inglês (EN)** revisado como idioma padrão de boot e implemente **Português (PT-BR)** selecionável em tempo real sem reiniciar o app e sem corromper saves.

## Protocolo obrigatório ao encerrar qualquer sessão

Antes de concluir uma resposta final de sessão:

1. Atualize `docs/STATUS.json` e regenere `docs/STATUS.md` com:
   ```powershell
   python tools/project_status.py render
   python tools/project_status.py check
   ```
2. Atualize `docs/HANDOFF.md` com:
   - O que foi feito e validado na sessão
   - O que ficou pendente
   - O próximo passo concreto e comando exato de retomada
3. Se uma decisão arquitetural foi tomada, registre um novo ADR em `docs/DECISIONS.md`.
4. Se houve descoberta técnica sobre o binário Mach-O, classes ObjC ou formatos de dados (`dat/`, `newres/`, `boss/`, `comm/`), registre em `docs/RESEARCH_LOG.md` e em `research/sessions/`.
5. Rode:
   ```powershell
   python tools/verify_workspace.py
   ```
   e confirme que todos os checks passam sem erro.
