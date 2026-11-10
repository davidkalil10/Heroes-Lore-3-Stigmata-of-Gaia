# Tarefa S06: Subsistema de Áudio (OpenAL + AVAudioPlayer / MP3 + WAV)

## Status e Metadados

- **ID da Etapa**: `S06`
- **Nome**: Subsistema de Áudio (OpenAL + AVAudioPlayer/MP3 + WAV)
- **Fase**: `pc_playable_complete`
- **Data de Início**: 2026-11-09
- **Dependências Concluídas**: `S00`, `S01`, `S02`, `S03`, `S04`, `S05`
- **Objetivo**: Implementar e refinar o subsistema de áudio HLE do Heroes Lore 3 (OpenAL para os 28 efeitos sonoros WAV e AVAudioPlayer para as 20 trilhas BGM em MP3) operando de forma determinística fora do callback de áudio da SDL, com controle de volume independente, mixagem em 64 canais, detecção real de término de som e persistência em arquivo de configuração.

---

## 1. Escopo Técnico e Aprendizados Anteriores

1. **Ativos Sonoros do HL3 (`snd/`)**:
   - **28 Efeitos Sonoros WAV**: `sd000_mf.wav` a `sd027_mf.wav` (PCM 16-bit 44.1 kHz, mono e estéreo), totalizando ~4.2 MB.
   - **20 Músicas MP3**: `battle_cry.mp3` e `bgm0_mf.mp3` a `bgm18_mf.mp3` (estéreo 44.1 kHz), totalizando ~26 MB.
2. **Lição Fundamental (Regra 4 do HL4/HL5)**:
   - Toda decodificação deve ocorrer fora de qualquer callback de áudio da SDL.
   - Efeitos WAV são pré-empacotados em memória via `alBufferData` e disparados via canais dedicados do mixer SDL.
   - Músicas MP3 rodam no canal de música de fundo em streaming assíncrono.
3. **Capacidades Exigidas**:
   - Canais de som suficientes: expandir para 64 canais virtuais para eliminar corte de efeitos sonoros quando múltiplos monstros atacam simultaneamente.
   - Detecção real de estado: `alGetSourcei(sid, AL_SOURCE_STATE)` deve consultar o canal ativo e retornar `AL_STOPPED` quando o efeito terminar.
   - Parâmetros OpenAL: suporte a `AL_GAIN`, `AL_PITCH`, `AL_LOOPING`, `AL_BUFFER` e vetores 3D.
   - `AVAudioPlayer` completo: `isPlaying`, `duration`, `currentTime`, `numberOfLoops`, `volume`, fading.
   - Persistência: salvar e carregar preferências (`config/audio.json`).

---

## 2. Critérios de Aceitação e Entregáveis

- [x] Todos os 28 arquivos `.wav` e 20 arquivos `.mp3` validados e íntegros.
- [x] `AudioMixer` com 64 canais, controle independente de volumes (BGM, SFX, Master) e mute.
- [x] Suporte a fade out / crossfade de música.
- [x] Persistência em `config/audio.json`.
- [x] Controles de volume integrados no host PC (`run_pc.py` via `+`, `-`, `M`).
- [x] Testes unitários automatizados em `tests/test_audio_subsystem.py`.
- [x] Verificação do workspace com zero regressão (`python tools/verify_workspace.py`).

