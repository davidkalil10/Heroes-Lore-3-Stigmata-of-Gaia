# Matriz de Paridade e Recursos - Heroes Lore 3 Recomp

Comparação entre o pacote original iOS (`v1.0.1` via wrapper touchHLE), o alvo **Windows x64 (`v1.0.3`, disponível)** e os alvos multiplataforma planejados (**Linux x86_64**, **Android ARM64**, **Nintendo Switch**).

| Recurso | Original iOS (`v1.0.1`) | Windows x64 (`v1.0.3`) | Linux / Android / Switch (`S12`–`S14`) | Notas |
| --- | --- | --- | --- | --- |
| **Execução ARMv6 Mach-O** | ARMv6 iOS nativo (emulado via wrapper) | ✅ Loader Mach-O + Unicorn + `28` blocos AOT (`tools/hl3rt/`) | ⏳ Planejado (`S12`/`S13`, core C++ + `src/arm_hot_blocks.inc`) | Oráculo Unicorn (`48/48` comparações `PASS`). |
| **Objective-C 2.0 & UIKit/Foundation** | Nativos iOS 3.0 | ✅ HLE própria (`objc.py`, `hle_foundation.py`, `hle_uikit.py`) | ⏳ Planejado (`S12`) | `30` classes e `679` seletores mapeados. |
| **Gráficos OpenGL ES 1.1 & `.pvr`** | GLES 1.1 iOS (`480×320`) | ✅ `hle_gl.py` + `gl_bridge.py` (`GL3`/OpenGL desktop) | ⏳ Planejado (`S12`/`S13`) | Texturas `.pvr` (PVRTC/RGBA/RGB565) e mipmaps estáveis. |
| **True Widescreen & FPS (`30`/`60`)** | 3:2 fixo (`480×320`) | ✅ `Auto` / `16:9` / `3:2` (`F9`), `30`/`60` FPS (`F8`), tela cheia (`F11`) | ⏳ Planejado (`S12`/`S13`) | Projeção ortográfica e HUD centralizada/ancorada sem esticar sprites. |
| **Áudio (`20` MP3s + `28` WAVs)** | OpenAL + AVAudioPlayer | ✅ `hle_audio.py` (`pygame.mixer`, `64` canais virtuais, fora do callback SDL) | ⏳ Planejado (`S12`/`S13`, `dr_mp3` + SDL2) | Volume (`+`/`-`) e mudo (`N`/`M`) persistidos em `config/audio.json`. |
| **Controles (Teclado / Gamepad / Toque)** | Somente toque virtual iPhone | ✅ Teclado (`WASD`/Setas/Atalhos) + Gamepad + Mouse/Toque 1:1 | ⏳ Planejado (`S12`/`S13`) | Navegação de menus, combate, habilidades e submenus. |
| **Localização `EN` & `PT-BR` ao vivo** | EN + DE/ES/FR/IT/KO/ZH | ✅ EN padrão + PT-BR completo (`6.733/6.733` slots) via `F7` ou menu | ⏳ Planejado (`S12`) | Atlas `.rff`/`.pvr` estendidos (`Ã`, `Ô`, `Õ`, `ã`, `õ`), reload live em `TextUtils`. |
| **Menu Integrado do Port & Sobre** | Menu clássico apenas | ✅ Dupla entrada (`Help & About` + `System`) + crédito `David Kalil Braga (2026) - v1.0.3` | ⏳ Planejado (`S12`/`S13`) | Layout responsivo em paisagem e retrato. |
| **Cloud Save (Google Drive)** | Não existe | ✅ OAuth Device Code + `Heroes Lore III - Stigmata of Gaia/heroes_lore_3_save.json` | ⏳ Planejado (`S12`/`S13`) | Backup automático em `data/save-backups/` e reload limpo ao Título. |
| **OTA Updater (GitHub Releases)** | Não existe | ✅ Checagem startup configurável + download HTTPS/SHA-256 + updater staging | ⏳ Planejado (`S12`/`S13`) | Preserva `config/` e `data/` com rollback em caso de erro. |
| **Empacotamento Standalone** | `.apk` touchHLE legado | ✅ `dist/HeroesLore3_PC/` (`assets/game.ipa`, sem APK) + `dist/heroes_lore_3_windows_x64.zip` | ⏳ Planejado (`S12`/`S13`) | `applicationId` `com.davidbraga.heroeslore3`, Title ID `0100F3A300030000`. |
