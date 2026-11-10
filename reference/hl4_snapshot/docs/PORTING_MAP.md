# Mapa de reaproveitamento do HL5

Snapshot completo selecionado: `reference/hl5_snapshot/`, commit e hashes em
`reference/HL5_SNAPSHOT.json`. Nada ali é código ativo do HL4.

| Referência | Reuso possível | Adaptação obrigatória |
| --- | --- | --- |
| src/core/guest.*, cpu.*, elf_loader.* | Memória, decoder e relocations | Retirar hooks HL5; verificar ISA, ARM/Thumb, ABI, memória e imports do HL4 |
| src/core/hot_blocks.inc | Modelo de AOT e oracle | Não copiar regiões/endereço; gerar novamente só após profiling HL4 |
| src/core/menu_blur.cpp | Modelo de aceleração com equivalência | Não copiar hook ou layout de stack; achar rotina HL4 e provar os limites |
| src/audio/ | Mixer SDL e decoders com suas licenças | Descobrir formatos e contratos MC_mda*; manter callback sem I/O/decodificação |
| src/hle/ | Exemplos de imports/gráficos | JNI/GLES/caches/structs HL5 não valem como ABI WIPI do HL4 |
| src/display_layout.*, input_mapping.* | Política de aspecto e controles | Câmera/culling/framebuffers/toque e ações do HL4 precisam de mapeamento próprio |
| src/localization.*, core/trans.* | Seleção ao desenhar, cache por idioma | Dicionário, pointers, encodings, hooks e limites próprios; não usar corpus HL5 |
| src/native_settings.*, service_ui.* | Menu extra integrado, navegação, fontes | Entradas originais, estado, escala e foco do HL4; fonte em assets próprios |
| src/services.*, service_net.*, service_data.* | Download, backup, rollback e transporte | Repositório, app IDs, OAuth, save paths/archive e retorno ao título próprios |
| CMakeLists/Makefile.switch/android/.github | Padrões de build e pacotes | Não copiar secrets, signing, IDs ou workflow ativo antes de S12 |
| tools/verify_*.py | Metodologia de oracle, capturas e smoke | Mesmos comandos/scripts pressupõem HL5; criar fixtures/cenários HL4 |
| tools/text_tool*, translate_* | Investigação dos formatos de texto | Confirmar formatos antes de parsear/regravar; roundtrip obrigatório |

O snapshot inclui ferramentas específicas e arquivos com nomes/IDs HL5. Estes
não devem ser executados diretamente. A fonte original permite consultar detalhes
sem depender de resumos; a tabela evita confundir referência com implementação.

## Implementação efetiva em 2026-10-21

Host novo em src/main.cpp, ARM/Thumb via Unicorn, SDL2 e HLE WIPI próprio.
A organização acima era a proposta inicial, não os diretórios já implementados.
Único reuso efetivo: src/audio/stb_vorbis.c, byte a byte do snapshot, com licença.
O snapshot segue congelado. Ver WINDOWS_RUNTIME.md para contratos e build.


Atualizacao 2026-10-22: decoder ARM puro do snapshot foi adaptado seletivamente
para src/arm_hot_decoder.inc; dez regioes proprias HL4 geradas e verificadas
contra Unicorn. Nao reutiliza hooks/enderecos HL5. Ver PERFORMANCE.md e
ARM_FAST_TESTS.json. O snapshot permanece congelado e nao foi executado.
