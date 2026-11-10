# ⚔️ Heroes Lore 4 — projeto de port nativo

**Windows · Linux / Steam Deck · Android · Nintendo Switch**

Port do APK fornecido pelo proprietário, tomando o HL5 como referência.
**Primeira build Windows x64 executável, com cenário, movimento, áudio e menu.**
Abra `dist/HeroesLore4-Windows-English-review/heroes_lore_4.exe`; pacote completo em
`dist/HeroesLore4-Windows-English-review.zip`, com as três DLLs necessárias. Janela horizontal.
Teclado e gamepad virtual SDL validados; controle físico ainda pendente.
Detalhes em [runtime Windows](docs/WINDOWS_RUNTIME.md), [clique/toque](docs/POINTER_INPUT.md)
e [textos/localização](docs/TEXT_LOCALIZATION.md). Catálogo offline com 8.232 registros;
inglês padrão integrado, ainda em revisão.
O projeto está isolado: o HL5 e seus saves não são modificados.

## Começar em uma nova sessão

Abra esta pasta como projeto no Codex ou em outra IA e use o texto de
[PROMPT_NOVA_SESSAO.md](PROMPT_NOVA_SESSAO.md). Leia primeiro [AGENTS.md](AGENTS.md),
[status](docs/STATUS.md) e [handoff](docs/HANDOFF.md).

```powershell
python tools/verify_workspace.py
python tools/verify_bootstrap.py
python tools/inspect_apk.py --check
python tools/project_status.py show
```

Trabalho atual: **S05 — consolidar gameplay**, após loader/boot reais S02–S04.
Movimento, menu/retorno e equivalência de entrada exercitados. Ataque com dano,
habilidades, gamepad físico e save/load completo ainda precisam de validação.
Não copiar endereços/hooks do HL5. [Próxima tarefa](docs/tasks/S05_GAMEPLAY_VALIDATION.md).

## O que já foi confirmado

- APK de entrada identificado por SHA-256; arquivo original preservado.
- ELF32 ARM little-endian em `lib/armeabi/libbinary.so`, com símbolos preservados.
- `res/raw/jarfile.mp3` é um ZIP/JAR de recursos, apesar da extensão `.mp3`.
- O JAR tem 1.935 entradas e nenhuma classe Java; contém `binary.mod` e recursos.
- DEX menciona WIPI/LGT e `com.lgt.arm.ArmInterface`; a biblioteca importa APIs
  `MC_*`/`wipic_*` e não exporta entradas JNI `Java_*`.

O host Windows implementa esse caminho WIPI, com Unicorn executando o ARM original
e HLE próprio para os contratos exercitados. A matriz separa cada validação. Veja [inventário](reference/analysis/INITIAL_FINDINGS.md).

## Organização

| Pasta | Função |
| --- | --- |
| `docs/` | Plano, status, decisões, validação e transferência de contexto |
| `src/`, `tests/` | Runtime HL4 C++/SDL/Unicorn e testes próprios |
| `platforms/` | Integração própria por plataforma, sem IDs herdados do HL5 |
| `tools/` | Inspeção offline, verificação e gerenciamento do status |
| `reference/analysis/` | Evidências reproduzíveis do APK e dos símbolos |
| `reference/extracted_*` | Recursos extraídos localmente, ignorados pelo Git |
| `reference/hl5_snapshot/` | Fonte e documentação do HL5 congeladas, só para consulta |
| `research/` | Diário por sessão, experimentos e resultados negativos |
| `assets/`, `data/` | Recursos futuros do port e dados pessoais isolados |

## Continuidade entre IAs

O status oficial está em `docs/STATUS.json`; a versão Markdown é gerada.
Siga [o protocolo de sessões](docs/SESSION_PROTOCOL.md), documente hipóteses e
decisões e deixe sempre uma próxima ação concreta. Nunca marque uma plataforma
como testada somente porque ela compilou.

Referências principais: [etapas](docs/ROADMAP.md), [arquitetura](docs/ARCHITECTURE.md),
[lições do HL5](docs/HL5_LESSONS.md), [mapa de reaproveitamento](docs/PORTING_MAP.md)
e [critérios de validação](docs/VALIDATION.md).
Veja também [como os ports em JAR se relacionam com o HL4](docs/PREVIOUS_PORTS.md).

APK, recursos extraídos, saves, credenciais e artefatos ficam fora do Git.
O repositório HL4 é privado; assinatura Android é de teste.

Ao clonar o Git em outra máquina, copie o APK original para o caminho indicado em project.json e confira o hash. O APK e os recursos extraídos são locais e não fazem parte do histórico Git.

## Builds no GitHub

Workflow `.github/workflows/build.yml`: Windows, Linux, Android ARM64 e Switch.
Insumos autorizados pelo proprietário ficam em release privado de CI, fora do Git.
As próximas distribuições locais Windows usam sempre `dist/HeroesLore4-Windows-English-review`, preservando saves.
Compilação de Android/Switch não equivale a teste em dispositivo.
