# Handoff - Heroes Lore 3 Recomp

## Estado Atual

- **Última entrega concluída**: **Correção da música de fundo (BGM `.mp3`), script unificado `tools/bump_version.py` + `docs/VERSIONING.md` e publicação da Release `v1.0.4` (`Android versionCode 5`)** com chave persistente (`61ca6931c36cc8f1139b9922a07fd5e1e5479c935ed7c12728296b5f1dac4905`).
- **Próxima etapa recomendada**: Testar o som (músicas + efeitos) e a atualização automática OTA in-app para a `v1.0.4` (a partir de qualquer build assinada com a chave persistente), além de validar Linux `.AppImage` / Switch `.nro` em hardware real.
- **Versão atual do Port**: `v1.0.4` (`versionCode 5` no Android, sincronizada entre `tools/hl3rt/version.py`, `src/core/common.h`, `project.json`, `android/app/build.gradle`, `.github/workflows/build.yml` e `README.md`)
- **Idioma padrão no boot**: Inglês (`en`), alternável em tempo real para Português (`pt-BR`) via `F7` ou `Port Options & Cloud` sem reiniciar nem alterar saves

## O que foi feito e validado na Sessão 26

1. **Diagnóstico e Correção do BGM (`.mp3`) (`src/hle/hle_foundation.cpp`, `tools/hl3rt/hle_foundation.py`, `src/audio/audio_mixer.*`, `src/audio/hle_audio.cpp`)**:
   - Identificado via trace Objective-C que `+[NSString stringWithCString:length:]` era chamado (`lr=0xa7b8c`) para construir `"/snd/bgm0_mf.mp3"` (`len=16`), mas retornava `nil` por não estar registrado — fazendo o jogo abrir `"/var/mobile/Applications/HL3/Heroes Lore.app/"` sem nome de arquivo enquanto o OpenAL (`.wav`) tocava normalmente.
   - Implementados `+[NSString stringWithCString:length:]`, `+[NSString stringWithCString:encoding:]` e `-[NSString stringByAppendingFormat:]` em C++ e Python.
   - Adicionado `AudioMixer::player_bgm_volume_` separado de `bgm_volume_` para que `[AVAudioPlayer setVolume:]` (fades internos `0.0 -> 0.61`) não sobrescreva a preferência do usuário em `config/audio.json`.
   - Validado no `ctest` (`headless_runtime`: confirma `bgm0_mf.mp3` tocando + amostras PCM não nulas após 60 frames) e no `unittest` (`test_audio_subsystem.py`).
2. **Script Multiplataforma de Versionamento (`tools/bump_version.py`, `docs/VERSIONING.md`, `tests/test_updater.py`)**:
   - Expandido `tools/bump_version.py` (com `--check`, `--current`, `--dry-run`, `--android-code`, `--root` e `patch|minor|major|X.Y.Z`) cobrindo `tools/hl3rt/version.py`, `src/core/common.h`, `project.json`, `android/app/build.gradle` (`versionName` + `versionCode`), `.github/workflows/build.yml` e `README.md`.
   - Executado `python tools/bump_version.py patch` (`v1.0.3 -> v1.0.4`; `Android versionCode 4 -> 5`) e atualizado `docs/RELEASE_NOTES.md`.

## O que ficou pendente

- Validar ao vivo a atualização OTA in-app para a `v1.0.4` no Android/Windows e conferir músicas + SFX em aparelho físico e Switch/Linux.

## Próximo passo concreto ao retomar

Para futuras releases, use o fluxo documentado em `docs/VERSIONING.md`:

```powershell
python tools/bump_version.py patch --dry-run
python tools/bump_version.py patch
python tools/bump_version.py --check
python tools/verify_workspace.py
```
