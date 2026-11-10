# Comparativo com os Ports Anteriores da Franquia

A tabela abaixo resume as diferenças técnicas entre os 5 jogos da franquia trabalhados até aqui, servindo de referência rápida para evitar suposições incorretas:

| Característica | HL1 / HL2 / Zero | Heroes Lore 5 | Heroes Lore 4 | Heroes Lore 3 (Este Projeto) |
| --- | --- | --- | --- | --- |
| **Artefato Original** | `.jar` (J2ME MIDP 2.0) | `.apk` Android nativo | `.apk` Android (wrapper WIPI) | `.apk` touchHLE contendo `assets/game.ipa` |
| **Binário Principal** | Classes Java (`.class`) | ELF ARM32 `libHeroesLore5.so` | ELF ARM32 `libbinary.so` (`binary.mod`) | **Mach-O ARMv6 `Payload/Heroes Lore.app/Heroes Lore`** |
| **ABI / Entrada** | `MIDlet` (`startApp`) | JNI (`Java_co_kr_eamobile_CletEntry_*`) | WIPI C (`GetknlInterface`, `MC_*`, `wipic_*`) | **iOS Objective-C (`UIApplicationMain`, `AppDelegate`, `GLView`)** |
| **Resolução e Orientação Base** | `240x320` (Retrato) | `240x320` (Retrato) | `240x320` (Retrato) | **`480x320` (Paisagem - `LandscapeRight`)** |
| **Renderização** | `Graphics` 2D J2ME | GLES1 / compositor 2D | Framebuffer 16-bit RGB565 (`MC_grpFlushLcd`) | **OpenGL ES 1.1 (`glOrthof`, `glDrawArrays`, `glDrawElements`, `.pvr`)** |
| **Áudio** | MIDI / WAV (`mmapi`) | MP3 / OGG / WAV via JNI/SDL | MIDI / WAV / OGG via WIPI Media | **OpenAL (`.wav`) + `AVAudioPlayer` (`snd/bgm0..16_mf.mp3`)** |
| **Empacotamento de Recursos** | Arquivos soltos no `.jar` | VFS empacotado (`data.vfs.mp3`) | ZIP/JAR interno (`res/raw/jarfile.mp3`) | **Pastas no `.app` (`dat/`, `boss/`, `comm/`, `newres/`, `snd/`)** |
| **Idioma no Pacote Original** | Coreano / Inglês | Coreano / Chinês | Coreano | **Multi-idioma incluso (`en`, `de`, `es`, `fr`, `it`, `ko`, `zh`)** |
| **Persistência / Save** | RMS RecordStore | Arquivos em VFS/appdata | Arquivos WIPI (`saves/kickass`, etc.) | **`Documents/`, `NSUserDefaults` e `trackingdb.sqlite` (SQLite3)** |
