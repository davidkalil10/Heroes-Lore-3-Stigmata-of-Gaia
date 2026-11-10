# Diretório `src/` - Heroes Lore 3 Recomp

Código-fonte do runtime nativo multiplataforma do **Heroes Lore 3**:

- `src/core/`: Loader Mach-O 32-bit ARMv6, memória guest e execução ARMv6/Thumb + blocos quentes AOT.
- `src/hle/`: Mini-runtime Objective-C 2.0, HLE iOS (`UIKit`, `Foundation`, `EAGL`), OpenGL ES 1.1 + `.pvr`, SQLite3, zlib e libc.
- `src/audio/`: Mixer SDL2, OpenAL (`.wav`) e `AVAudioPlayer` (`dr_mp3` para `.mp3`).
- `src/platform/`: Caminhos de plataforma, Menu Nativo de Opções, Localização em tempo real (`EN` / `PT-BR`), Cloud Save (`Google Drive`) e OTA Updater.
