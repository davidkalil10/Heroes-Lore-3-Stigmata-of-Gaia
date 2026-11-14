<div align="center">

<img src="assets/icon.png" alt="Heroes Lore 3: Stigmata of Gaia" width="144" />

# ⚔️ Heroes Lore 3: Stigmata of Gaia

### Recompilação / Runtime Nativo · Mach-O ARMv6 + iOS HLE + OpenGL ES 1.1

**Windows x64 (Disponível · v1.0.1) · Linux x86_64 · Android ARM64 · Nintendo Switch (Roadmap)**

[![Port Version](https://img.shields.io/badge/Port-v1.0.1-d49b35)](#-downloads-e-plataformas)
[![Original Game](https://img.shields.io/badge/Original_iOS-v1.0.1-6b7280)](#-arquitetura-técnica)
[![Idiomas](https://img.shields.io/badge/Idiomas-English%20%7C%20PT--BR-2ea043)](#-idioma-em-tempo-real-en--pt-br)
[![Releases](https://img.shields.io/github/v/release/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia)](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/releases)

</div>

Port nativo e runtime multiplataforma de **Heroes Lore 3: Stigmata of Gaia** (*EA Mobile / Hands-On Mobile*), rodando o executável original iOS **Mach-O 32-bit ARMv6** sobre uma camada HLE própria (`UIKit`, `Foundation`, `EAGL`, `OpenGL ES 1.1`, `OpenAL` / `MP3`+`WAV`, `SQLite3`) com áudio moderno, **True Widescreen**, **30/60 FPS**, **tradução integral PT-BR alternável em tempo real**, **Cloud Save no Google Drive** e **atualizador OTA pelo GitHub Releases**.

---

## 📥 Downloads e plataformas

Os pacotes compilados ficam publicados em **[Releases](https://github.com/davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia/releases)**.

| Plataforma | Status | Pacote | Observações |
| --- | --- | --- | --- |
| **Windows x64** | ✅ Disponível (`v1.0.1`) | `heroes_lore_3_windows_x64.zip` | Executável standalone (`HeroesLore3.exe`), sem dependência de Python ou SDK |
| **Linux x86_64 / Steam Deck** | ⏳ Planejado (`S12`) | — | Base C++/SDL2 nativa após estabilização da v1.0.1 no Windows |
| **Android ARM64** | ⏳ Planejado (`S12`) | `com.davidbraga.heroeslore3` | App nativo próprio sem wrapper touchHLE original |
| **Nintendo Switch** | ⏳ Planejado (`S13`) | Homebrew `.nro` (`0100F3A300030000`) | Layout adaptado a controles Joy-Con / Pro Controller e cache limitado |

> [!NOTE]
> Na v1.0.1, **somente o pacote de Windows x64** é gerado e publicado. Linux, Android ARM64 e Nintendo Switch compõem as próximas etapas do [Roadmap](docs/ROADMAP.md) (`S12`–`S14`).

---

## ✨ Recursos da versão `v1.0.1`

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
- 🏷️ **Créditos e Versão no Menu Sobre**: logo abaixo da versão original do jogo (`1.0.1`), o menu **About** exibe em tempo real o crédito **`Port by David Kalil Braga (2026) - v1.0.1`** (EN) / **`Port por David Kalil Braga (2026) - v1.0.1`** (PT-BR).

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
  1. Consultar manualmente se há versão SemVer mais nova que a instalada (`v1.0.1`).
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
| `S12` | Core nativo C++ / Linux x86_64 / Android ARM64 (`com.davidbraga.heroeslore3`) + CI | ⏳ Planejado |
| `S13` | Port Nintendo Switch (`.nro`, Title ID `0100F3A300030000`) | ⏳ Planejado |
| `S14` | Polimento multiplataforma e paridade final entre todas as plataformas | ⏳ Planejado |

---

## 🛠️ Desenvolvimento, Build Standalone e Validação

### Pré-requisitos (ambiente Windows)
- **Python 3.11+** (testado em Python 3.13) com dependências (`pygame`, `PyOpenGL`, `unicorn`, `Pillow`, `pyinstaller`, etc.)
- O arquivo base original intacto na raiz (`Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk`) para extração determinística de assets caso `reference/extracted_ipa/` ainda não tenha sido extraído.

### Comandos essenciais

```powershell
# 1. Verificar integridade completa do workspace, snapshots, SHA-256 e docs
python tools/verify_workspace.py

# 2. Verificar sincronização da versão do port (version.py <-> project.json)
python tools/bump_version.py --check

# 3. Extrair APK/IPA e revalidar inventário estático (necessário no primeiro setup)
python tools/inspect_apk.py --extract --check

# 4. Rodar a suíte completa de testes automatizados
python -m unittest discover -s tests -v

# 5. Executar o jogo diretamente pelo código-fonte
python tools/run_pc.py

# 6. Gerar o pacote standalone Windows x64 + ZIP de release em dist/
python tools/build_standalone_pc.py
```

---

## 📂 Estrutura do Repositório

- [`tools/run_pc.py`](tools/run_pc.py): ponto de entrada do runtime PC, loop principal, atalhos de teclado/gamepad, OSD e integração com o guest ARMv6.
- [`tools/hl3rt/`](tools/hl3rt/): núcleo do emulador Mach-O ARMv6, runtime ObjC, módulos HLE (GL, Audio, UIKit, Foundation, libc), widescreen, localização, opções responsivas, Cloud Save e OTA Updater.
- [`tools/build_standalone_pc.py`](tools/build_standalone_pc.py): gerador do bundle standalone (`dist/HeroesLore3_PC/` e `dist/heroes_lore_3_windows_x64.zip`).
- [`tools/bump_version.py`](tools/bump_version.py): gerenciador e verificador de versão SemVer do port (`tools/hl3rt/version.py` ↔ `project.json`).
- [`localization/`](localization/) e [`assets/`](assets/): chunks de tradução EN/PT-BR validados, fontes estendidas `.rff`/`.pvr`, ícones e branding.
- [`tests/`](tests/): suíte de testes unitários e de regressão (`runtime`, `widescreen`, `options_menu`, `updater`, `localization`).
- [`docs/`](docs/) e [`research/`](research/): especificações, ADRs, logs técnicos de engenharia reversa, protocolo de sessão e roadmap.

---

## 🙏 Créditos e Agradecimentos

- **Port / Recompilação / Tradução PT-BR / Cloud Save / OTA Updater**: **David Kalil Braga (2026)**
- **Jogo Original**: *Heroes Lore 3: Stigmata of Gaia* (iOS v1.0.1 — `com.ea.heroesloresg.bv`) © 2010 Electronic Arts Inc.
- Preservação e pesquisa independente dos capítulos anteriores (*HL1*, *HL2*, *HL Zero*, *HL4*, *HL5*).
