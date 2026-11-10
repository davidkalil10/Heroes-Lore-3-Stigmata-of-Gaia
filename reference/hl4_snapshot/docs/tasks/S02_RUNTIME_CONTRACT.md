# S02 — descobrir o contrato real WIPI/ARM

Estado inicial: inventário estático disponível; nenhum runtime HL4.

Atualização 2026-10-21: **em andamento**. Contrato parcial e pendências em
docs/RUNTIME_CONTRACT.md; 21 probes ARM isolados e catálogo de 108 imports em
reference/analysis. Ainda não há contrato suficiente para declarar boot.
Próxima ação: trace limitado startClet → onStartApp, properties e contextos
gráficos, explicitando cada fixture e parando em contrato desconhecido.

## Entradas

project.json, relatórios JSON/CSV em reference/analysis, classes.dex e os dois
módulos originais extraídos com `python tools/inspect_apk.py --extract`.
Usar símbolos do próprio HL4; os valores ELF não incluem LOAD_BASE do HL5.

## Trabalho

1. Disassemblar DEX e ARM com ferramenta local adequada (jadx/baksmali,
   llvm-objdump/binutils ARM/Ghidra conforme disponibilidade). Registrar versão,
   comando e hash do input. Não usar dump do HL5 como disassembly deste binário.
2. Confirmar manifeste/activity e fluxo de launch/bind/executeArm do DEX. Mapear
   quais parâmetros/resource paths o wrapper transmite e o que o serviço externo
   precisaria fornecer. Documentar fatos separados de inferências.
3. Ler ELF headers/PHDR/dynamic/relocations e atributos ARM: entry 0x220c4,
   init/_init_module, init_array se existir, ARM/Thumb/interworking, BSS e ranges.
   Comparar binary.mod/so e documentar os 80 bytes adicionais sem os descartar.
4. Mapear assinatura/ordem de init/startClet/onStartApp/onHandleEvent/paintClet,
   callbacks, estrutura de eventos e timers. Registrar callers/callees/bytes.
5. Classificar todos os imports: libc/dlib, aeabi, MC_knl/resources/timers,
   MC_grp/framebuffer/image, MC_mda/audio, MC_fs/persistência e MC_net/legado.
   Priorizar os indispensáveis ao primeiro boot; não criar stubs silenciosos.
6. Descrever recursos JAR: nomes/IDs/case, handles, tamanho, compressão e memória.
   Inspecionar headers reais de imagens/áudio/textos; extensão não é formato.
7. Comparar decoder/loader genéricos do snapshot, listar hooks HL5 a remover e
   lacunas de ISA/HLE. Propor o menor harness PC para S03/S04, com testes de ABI.

## Entregas e aceite

Criar docs/RUNTIME_CONTRACT.md com mapa de memória, relocations, ABI/entradas,
eventos, resource ownership e fronteiras HLE. Criar catálogo de imports com
estado (confirmado/hipótese/não mapeado), prioridades e evidências. Registrar
ADRs, comandos/resultados e problemas abertos. Status DONE somente quando há
contrato mínimo comprovado suficiente para implementar loader/boot, não apenas
nomes de símbolos. Pendências que impedem boot mantêm etapa em andamento.

## Próxima etapa

S03: adaptar seletivamente memória/CPU/loader, sem hooks do HL5; teste mínimo
reproduzível de carregamento e chamadas. Não adiantar tradução/cloud/OTA antes
de ter imagem, entrada e persistência corretas.

## Resultado em 2026-10-21

Gate atendido pelo runtime integrado. Ver WINDOWS_RUNTIME.md: boot e primeiro
cenário executados no PC. S02–S04 concluídas; próxima tarefa S05_GAMEPLAY_VALIDATION.md.
Catálogo estático é histórico; contratos corrigidos e cobertura ativa estão no
registro do host. Imports fora do percurso permanecem não mapeados e diagnosticam.
