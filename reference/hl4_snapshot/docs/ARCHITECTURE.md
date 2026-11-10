# Arquitetura candidata e limites conhecidos

## Evidências iniciais

O APK tem DEX pequeno de integração LGT/WIPI, uma biblioteca ELF32 ARM e um
arquivo `res/raw/jarfile.mp3` que é ZIP/JAR. Há imports `MC_grp*`, `MC_mda*`,
`MC_knl*`, `MC_fs*`, `MC_net*`, `wipic_*` e `__aeabi_*`. Não há exports `Java_*`.
O JAR contém recursos e `binary.mod`, sem `.class`. Não adotar pipeline Java/J2ME.

## Fluxo a comprovar

APK/JAR preservados → loader ELF ARM → inicialização do módulo WIPI →
callbacks/eventos/timers → lógica original → HLE de framebuffer/imagem,
áudio, arquivos e relógio → host SDL → integrações por plataforma.

A biblioteca do APK e o módulo no JAR devem ser comparados: o inventário registra
igualdade do prefixo e os 80 bytes finais adicionais no módulo. Não descartá-los
nem atribuir significado sem análise. Escolher explicitamente o binário carregado.

## Fronteiras a implementar

- `src/core/`: memória convidada, loader, relocations, ARM/Thumb e traps.
- `src/hle/`: contratos WIPI verificados, recursos, gráficos, arquivos e eventos.
- `src/audio/`: mixagem/decoders reutilizáveis, ponte de chamadas mapeada do HL4.
- `src/platform/` e `platforms/`: SDL, input, paths, lifecycle e empacotamento.
- `assets/locales/`: futura localização própria, sem recursos textuais do HL5.
- `tests/`: provas de ABI, fixtures mínimas e cenários com estado descartável.

O HL5 oferece decoder/loader/mixer e exemplos valiosos, mas seu cpu.cpp combina
semântica ARM com hooks do jogo. Separar os hooks antes de reutilizar o núcleo.
Seu HLE mistura libc/JNI/GLES e detalhes do jogo: reaproveitar por contrato,
nunca por compilação cega. O uso de SDL/GPU não elimina o custo da CPU convidada.

Não há arquitetura final aprovada até S02/S03. Uma alternativa justificada por
evidências deve ser registrada em DECISIONS.md, sem ocultar trabalho descartado.

## Implementação efetiva em 2026-10-21

Host novo em src/main.cpp, ARM/Thumb via Unicorn, SDL2 e HLE WIPI próprio.
A organização acima era a proposta inicial, não os diretórios já implementados.
Único reuso efetivo: src/audio/stb_vorbis.c, byte a byte do snapshot, com licença.
O snapshot segue congelado. Ver WINDOWS_RUNTIME.md para contratos e build.


Atualizacao 2026-10-22: decoder ARM puro do snapshot foi adaptado seletivamente
para src/arm_hot_decoder.inc; dez regioes proprias HL4 geradas e verificadas
contra Unicorn. Nao reutiliza hooks/enderecos HL5. Ver PERFORMANCE.md e
ARM_FAST_TESTS.json. O snapshot permanece congelado e nao foi executado.
