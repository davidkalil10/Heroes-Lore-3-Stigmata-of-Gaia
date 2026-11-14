<div align="center">

<img src="assets/icon.png" alt="Heroes Lore 3: Stigmata of Gaia" width="144" />

# ⚔️ Heroes Lore 3: Stigmata of Gaia

### Recompilação / Runtime Nativo · Mach-O ARMv6 + iOS HLE + OpenGL ES 1.1

**Windows x64 · Linux x86_64 (`AppImage`) · Android (`Global` & `ARMv8`) · Nintendo Switch (`.nro`) · Runtime Nativo C++17 (`v1.0.5`)**

[![Port Version](https://img.shields.io/badge/Port-v1.0.5-d49b35)](#-downloads-e-plataformas)
[![Original Game](https://img.shields.io/badge/Original_iOS-v1.0.1-6b7280)](#-arquitetura-técnica)
[![Idiomas](https://img.shields.io/badge/Idiomas-English%20%7C%20PT--BR-2ea043)](#-idioma-em-tempo-real-en--pt-br)
[![Multiplatform CI](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/actions/workflows/build.yml/badge.svg)](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/actions/workflows/build.yml)
[![Releases](https://img.shields.io/github/v/release/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia)](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/releases)

</div>

Port nativo em **C++17** e runtime multiplataforma de **Heroes Lore 3: Stigmata of Gaia** (*EA Mobile / Hands-On Mobile*), rodando o executável original iOS **Mach-O 32-bit ARMv6** sobre uma camada HLE própria (`UIKit`, `Foundation`, `EAGL`, `OpenGL ES 1.1`, `OpenAL` / `MP3`+`WAV`, `SQLite3`) com áudio moderno, **True Widescreen**, **30/60 FPS**, **tradução integral PT-BR alternável em tempo real**, **Cloud Save no Google Drive** e **atualizador OTA pelo GitHub Releases**.

---

## 📥 Downloads e plataformas

Os pacotes compilados ficam publicados em **[Releases](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/releases)** e nos artefatos do **[GitHub Actions](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/actions/workflows/build.yml)**.

| Plataforma | Status | Pacote(s) | Observações |
| --- | --- | --- | --- |
| **Windows x64** | ✅ C++17 Estático | `heroes_lore_3_windows_x64.zip` / `HeroesLore3.exe` | Executável standalone (`HeroesLore3.exe` + `assets/game.ipa`), sem Python/DLLs/APK wrapper |
| **Linux x86_64 / Steam Deck** | ✅ CI (`AppImage`) | `heroes_lore_3_linux_x86_64.AppImage` | AppImage portável com `libcurl`, mixer OpenAL e saves em `$XDG_DATA_HOME` |
| **Android** | ✅ C++17 / NDK | `heroes_lore_3_android_arm64.apk` (**ARMv8**)<br>`heroes_lore_3_android_universal.apk` (**Global**) | `com.davidbraga.heroeslore3` (`arm64-v8a` dedicado e universal `arm64-v8a + armeabi-v7a + x86_64`), SDL2 + GLES2 + Cloud/OTA JNI |
| **Nintendo Switch** | ✅ CI (`devkitA64`) | `heroes_lore_3.nro` | Homebrew `.nro` direto sem `.zip` (Title ID `0100F3A300030000`), Unicorn-TCI + RomFS + Joy-Con/Touch |

> [!NOTE]
> A builds de **Windows x64** e **Android (Global + ARMv8)** também podem ser compiladas localmente; o workflow [`.github/workflows/build.yml`](.github/workflows/build.yml) gera todos os **4 alvos** automaticamente na nuvem (`Windows`, `Linux AppImage`, `Android Global & ARMv8` e `Nintendo Switch .nro` sem `.zip`).

---

## ✨ Recursos da versão `v1.0.5`

- 🖥️ **True Widescreen (`Auto` / `16:9` / `Original 3:2`)**: expansão real da projeção ortográfica OpenGL ES 1.1 (`480×320` nativo até `1020 px` de largura lógica) preservando a proporção de sprites, menus e botões do HUD sem esticar a imagem.
- ⚡ **Seleção de FPS (`30` / `60`) em tempo real**: alternância pelo menu do port ou tecla `F8`, com aviso estilizado na tela (OSD).
- 🌎 **Inglês (padrão) e Português do Brasil (PT-BR) em tempo real**: todos os **`6.733` slots** de texto de menus, missões, itens, habilidades e diálogos + extensões nativas nos atlas `.rff`/`.pvr` (`Ã`, `Ô`, `Õ`, `ã`, `õ`), alternáveis instantaneamente via **`F7`** ou no menu de opções sem reiniciar o jogo e sem alterar saves.
- ⚙️ **Menu Integrado do Port (`Port Options & Cloud`)**:
  - No Menu de Título: **Help & About → Port Options & Cloud** (ou **Ajuda e Sobre → Opções do Port e Nuvem** em PT-BR)
  - Durante a partida: **Menu → System → Port Options & Cloud** (ou **Sistema → Opções do Port e Nuvem** em PT-BR)
  - Navegável por **mouse/toque** ou **teclado/gamepad** como item nativo integrado às telas do jogo, com layout responsivo adaptável à proporção da tela.
- ☁️ **Cloud Save no Google Drive**: autenticação OAuth Device Code no próprio menu, envio e restauração da pasta isolada **`Heroes Lore III - Stigmata of Gaia`** (`heroes_lore_3_save.json`), backup automático em `data/save-backups/` antes de restaurar e recarga limpa para a tela de título.
- 🔄 **Atualizador OTA pelo GitHub Releases**: verificação opcional no boot (configurável em `config/services.json` e no menu) + consulta manual, visualização de *Release Notes*, validação de HTTPS e **SHA-256**, proteção contra ZIP Slip e script de substituição que preserva `config/` e `data/` com rollback em caso de falha.
- 🎵 **Subsistema de Áudio fora do callback SDL**: pré-decodificação e mixagem separada para os **20 temas `.mp3`** (`bgm0_mf.mp3`..`bgm16_mf.mp3`) e **28 efeitos `.wav`** via OpenAL HLE, com controle de volume e mudo persistidos em `config/audio.json`.
- 🏷️ **Créditos e Versão no Menu Sobre**: logo abaixo da versão original do jogo (`1.0.1`), o menu **About** exibe em tempo real o crédito **`Port by David Kalil Braga (2026) - v1.0.5`** (EN) / **`Port por David Kalil Braga (2026) - v1.0.5`** (PT-BR).

---

## 🚀 Como jogar (Windows)

1. Baixe `heroes_lore_3_windows_x64.zip` na página de [Releases](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/releases).
2. Extraia o ZIP para uma pasta de sua preferência (evite diretórios protegidos como `Arquivos de Programas`, para permitir salvar e atualizar sem permissão de administrador).
3. Execute **`HeroesLore3.exe`**.

Os dados criados em tempo de execução ficam contidos ao lado do executável:
- **Saves e SQLite**: `data/internal/`
- **Backups de restauração do Cloud Save**: `data/save-backups/`
- **Preferências locais** (áudio, tela, idioma, checagem de atualização e token OAuth): `config/`

Argumentos úteis de linha de comando:
```powershell
HeroesLore3.exe --aspect 16:9 --fps 60
HeroesLore3.exe --fullscreen
HeroesLore3.exe --scale 3
```

---

## 🌎 Idioma em tempo real (`EN` ↔ `PT-BR`)

- O jogo inicia por padrão em **Inglês (`en`)**, utilizando o texto original revisado.
- Pressione **`F7`** a qualquer momento — ou acesse **Port Options & Cloud → Language** — para alternar para **Português (Brasil)** (`pt-BR`) sem fechar o jogo.
- Ao alternar ao vivo:
  - As tabelas de `TextUtils` em memória do executável ARM são atualizadas instantaneamente (`t_pt-br` / `hdr_pt-br` / `InGame_txt_2_pt-br`).
  - Formulários ativos de menu e banners de diálogo em andamento recalculam cores (`[Personagem]#`), tags `#|{}^` e quebras de linha no ato.
  - Nenhum arquivo de save é sobrescrito ou convertido — os mesmos slots continuam compatíveis nos dois idiomas.

---

## ☁️ Cloud Save (Google Drive)

O sistema de backup na nuvem utiliza exatamente o mesmo fluxo seguro dos ports anteriores, isolado para o HL3:

1. Salve seu progresso normalmente dentro do jogo antes de abrir o painel na nuvem.
2. Acesse **Port Options & Cloud → Cloud Save (Google Drive)**.
3. Selecione **Connect Google Account**, acesse a URL exibida no navegador e digite o código mostrado na tela.
4. Use **Upload Backup** para enviar os arquivos locais (`data/internal/Documents/`) para `Heroes Lore III - Stigmata of Gaia / heroes_lore_3_save.json` no seu Google Drive.
5. Para restaurar em outra máquina ou reinstalação, escolha **Restore Cloud Backup**: o sistema valida o payload JSON, salva uma cópia de segurança em `data/save-backups/`, aplica os arquivos e retorna o jogo de forma limpa à tela de título sem risco de sobrescrever o estado restaurado.

> [!IMPORTANT]
> Credenciais pessoais (`config/cloud_auth.json`) e saves pessoais nunca são incluídos nos pacotes de release nem enviados para o repositório Git.

---

## 🔄 Atualizações pelo GitHub Releases

- Por padrão, ao iniciar em modo interativo, o `UpdateManager` consulta de forma assíncrona o endpoint oficial de releases públicas de **`davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia`** (pode ser ligado/desligado em **Port Options & Cloud → Startup Update Check**).
- Em **Port Options & Cloud → System Updates (GitHub)** você pode:
  1. Consultar manualmente se há versão SemVer mais nova que a instalada (`v1.0.5`).
  2. Baixar o pacote `.zip` com barra de progresso em tempo real e validação obrigatória de URL HTTPS confiável + checksum SHA-256 (quando publicado pela release).
  3. Clicar em **Install Update**: no Windows, o atualizador staging cria backup de rollback em `data/updates/`, preserva `config/` e `data/`, substitui os binários após o encerramento seguro do processo atual e reabre `HeroesLore3.exe` automaticamente.

---

## 🖥️ Tela e proporções (`Auto` / `16:9` / `Original 3:2`)

| Modo | Comportamento |
| --- | --- |
| **`Auto` (padrão)** | Adapta dinamicamente a área renderizada à janela atual ou tela cheia, expandindo a largura lógica mantendo a altura base de `320 px` sem barras pretas em monitores widescreen (`16:9`, `16:10`, `21:9` até o teto seguro de instruções ARM de `1020 px`). |
| **`16:9`** | Força área ampla `568×320` lógicos ideal para monitores e telas 16:9. |
| **`Original (3:2)`** | Mantém a proporção clássica original do iPhone (`480×320`). |

---

## 🎮 Controles

| Ação | Teclado | Gamepad (Xbox / Padrão) | Mouse / Toque |
| --- | --- | --- | --- |
| **Mover / Navegar** | `WASD` ou `Setas` | `D-pad` ou `Analógico Esquerdo` | Toque/arraste no direcional virtual ou clique nos itens |
| **Confirmar / Falar / Interagir** | `Enter`, `Espaço`, `E`, `Z` | `A` | Clique / toque |
| **Cancelar / Voltar / Fechar** | `Esc`, `Backspace` (`P`/`C` em submenus) | `B` (ou `Start` em submenus) | Botão voltar na tela |
| **Atacar (Primário / Secundário)** | `J` / `L`, `H` | `X` (em combate) / `Y` | Botões de ataque na tela |
| **Habilidades 1 / 2 / 3** | `U` / `3`, `I` / `4`, `O` / `5` | `X` (fora de combate) | Ícones de habilidade na HUD |
| **Poções 1 / 2** | `Q` / `1`, `K` / `2` | `LB` / `RB` | Ícones de frasco na HUD |
| **Pular Cutscene** | `S` (ou `M` / `Tab`) | `Y` ou `Start` | Toque em Skip |
| **Menu do Personagem / Pausa** | `P`, `C` | `Start` | Retrato no canto inferior |
| **Minimapa** | `M` | `Select` | Ícone da bússola na HUD |
| **Alternar Abas (Submenus)** | `Tab`, `PageUp`/`PageDown`, `[` / `]` | `LB` / `RB` | Clique nas abas |
| **Volume / Mudo** | `+` / `-` e `N` (ou `M` fora de mapa) | — | — |
| **Trocar Idioma (`EN` ↔ `PT-BR`)** | **`F7`** | — | **Port Options & Cloud** |
| **Alternar FPS (`30` ↔ `60`)** | **`F8`** | — | **Port Options & Cloud** |
| **Alternar Proporção (`Auto`/`16:9`/`3:2`)** | **`F9`** | — | **Port Options & Cloud** |
| **Abrir Opções do Port & Cloud** | — | — | `Help & About` / `System → Port Options & Cloud` |
| **Tela Cheia / Janela** | **`F11`** | — | Botão maximizar da janela |
| **Capturar Screenshot** | **`F12`** | — | — |

---

## 🧠 Arquitetura Técnica

Diferente de *Heroes Lore 5* (ELF Android JNI), *Heroes Lore 4* (ELF WIPI) e da trilogia original *HL1/HL2/Zero* (J2ME `.jar`), a análise binária estática de *Heroes Lore 3* (`Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk`) revelou que o wrapper embarca um bundle iOS completo (`assets/game.ipa`, `43.686.130` bytes):

- **Executável original**: `Payload/Heroes Lore.app/Heroes Lore` — **Mach-O 32-bit ARMv6 (`MH_EXECUTE`)** com **`915.824` bytes** (bundle `com.ea.heroesloresg.bv`, versão `1.0.1`).
- **Camada de Runtime & Execução (`tools/hl3rt/`)**:
  - **Loader Mach-O + Runtime Objective-C (`objc.py`, `machine.py`, `runtime.py`)**: mapeamento dos segmentos `__PAGEZERO`, `__TEXT`, `__DATA`, `__LINKEDIT`, resolução de ponteiros `__nl_symbol_ptr` / `__la_symbol_ptr`, inicialização de seletores/classes ObjC (`AppDelegate`, `GLView`, `TouchResponder`, `EAMTX_*`, `DMG*`, `SBJson*`) e execução ARMv6 via Unicorn Engine + blocos quentes acelerados com guarda estrita de opcodes (`aot_blocks.py`, validados por oráculo e com geração C++ em `src/arm_hot_blocks.inc` para `S12`).
  - **iOS HLE (`hle_foundation.py`, `hle_uikit.py`, `hle_libc.py`, `hle_audio.py`, `hle_gl.py`)**: emulação de nível de API para `UIKit`, `Foundation`, `CoreFoundation`, `QuartzCore`, `EAGLContext`, `SQLite3`, `zlib`, `OpenAL` + `AVAudioPlayer` (`pygame.mixer` com 64 canais virtuais fora do callback SDL) e tradução **OpenGL ES 1.1 → OpenGL Desktop (`gl_bridge.py`)** com suporte a texturas `.pvr`.
  - **Qualidade de Vida (`display.py`, `widescreen.py`, `i18n.py`, `options_menu.py`, `cloud_save.py`, `updater.py`, `version.py`)**: patches determinísticos de largura/altura e centralização UI/HUD, modal responsivo adaptável ao DPI/resolução da janela, sincronização Google Drive e atualizador GitHub Releases.

Mais detalhes em [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), [`docs/DECISIONS.md`](docs/DECISIONS.md) e [`docs/RESEARCH_LOG.md`](docs/RESEARCH_LOG.md).

---

## 🗺️ Progresso do Roadmap (`S00`–`S14`)

| Etapa | Descrição | Status |
| --- | --- | --- |
| `S00`–`S02` | Estrutura, inventário estático APK/IPA/Mach-O e contrato iOS HLE | ✅ Concluído |
| `S03`–`S05` | Loader Mach-O ARMv6, host Windows/GLES1, gameplay completo & saves locais | ✅ Concluído |
| `S06`–`S07` | Subsistema de áudio MP3+WAV fora do callback SDL e otimização CPU/AOT + Oráculo | ✅ Concluído |
| `S08` | True Widescreen (`Auto`/`16:9`/`3:2`), projeção ortográfica dinâmica e `30`/`60` FPS | ✅ Concluído |
| `S09`–`S10` | Auditoria EN (`6.733` slots), extensão de atlas `.rff`/`.pvr` e tradução ao vivo PT-BR | ✅ Concluído |
| `S11` | Menu nativo integrado, Cloud Save (Google Drive), créditos About e OTA GitHub Releases | ✅ Concluído |
| `S12` | Core nativo C++17, Linux AppImage, Android (`arm64-v8a` + `universal`) & CI | 🔄 Em validação no CI |
| `S13` | Port Nintendo Switch (`.nro`, Title ID `0100F3A300030000`, Unicorn-TCI) | 🔄 Em validação no CI |
| `S14` | Polimento multiplataforma e paridade final entre todas as plataformas | ⏳ Próximo |

---

## 🛠️ Desenvolvimento, Build Multiplataforma e Validação

### Comandos essenciais (Windows C++17 / Python / Android)

```powershell
# 1. Verificar integridade completa do workspace, snapshots, SHA-256 e docs
python tools/verify_workspace.py

# 2. Verificar ou subir a versão sincronizada em todas as plataformas (veja docs/VERSIONING.md)
python tools/bump_version.py --check
# python tools/bump_version.py patch

# 3. Compilar runtime C++17 nativo no Windows (MSYS2 UCRT64) e gerar ZIP em dist/
python tools/build_pc_cpp.py --build

# 4. Preparar assets/keystore do Android e compilar APKs ARMv8 (arm64-v8a) + Global (universal)
python tools/prepare_platform_assets.py android
gradle -p android assembleRelease --no-daemon

# 5. Rodar testes CTest e Python
ctest --test-dir build_cpp --output-on-failure
python -m unittest discover -s tests -p "test_*.py" -v
```

---

## 📂 Estrutura do Repositório

- [`src/`](src/) & [`CMakeLists.txt`](CMakeLists.txt): runtime nativo C++17 completo (loader Mach-O, máquina Unicorn ARMv6/TCI, ObjC 2.0, GLES1 → OpenGL/GLES2, OpenAL + `dr_mp3`, widescreen, Cloud Save, OTA updater e backends Windows/Linux/Android/Switch).
- [`android/`](android/) & [`platforms/switch/`](platforms/switch/): projeto Gradle/NDK Android (`com.davidbraga.heroeslore3`) e toolchain/adaptador de memória Horizon/Switch (`0100F3A300030000`).
- [`.github/workflows/build.yml`](.github/workflows/build.yml): pipeline GitHub Actions para Windows, Linux AppImage, Android (Global + ARMv8) e Nintendo Switch (`.nro` direto).
- [`tools/`](tools/): empacotadores (`build_pc_cpp.py`, `prepare_platform_assets.py`, `fetch_build_deps.py`), verificadores (`verify_android_package.py`, `verify_switch_package.py`, `bump_version.py`, `verify_workspace.py`) e protótipo de referência Python (`run_pc.py`, `hl3rt/`).
- [`localization/`](localization/) e [`assets/`](assets/): chunks de tradução EN/PT-BR validados, fontes estendidas `.rff`/`.pvr`, ícones e branding.
- [`tests/`](tests/): suítes C++ (`tests/cpp/`) e Python (`unittest` cobrindo runtime, widescreen, options menu, updater, localização e contratos CI).
- [`docs/`](docs/) e [`research/`](research/): especificações, ADRs, logs técnicos de engenharia reversa, protocolo de sessão e roadmap.

---

## 🙏 Créditos e Agradecimentos

- **Port / Recompilação / Tradução PT-BR / Cloud Save / OTA Updater**: **David Kalil Braga (2026)**
- **Jogo Original**: *Heroes Lore 3: Stigmata of Gaia* (iOS v1.0.1 — `com.ea.heroesloresg.bv`) © 2010 Electronic Arts Inc.
- Preservação e pesquisa independente dos capítulos anteriores (*HL1*, *HL2*, *HL Zero*, *HL4*, *HL5*).
