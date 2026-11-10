# Nintendo Switch experimental

CMake/toolchain.cmake e Unicorn-TCI pinado, adaptado para libnx/newlib.
Sem JIT: buffer TCI é somente dados. Proteções executáveis recusadas.
Ferramenta tools/patch_unicorn_switch.py registra adaptações guardadas; fonte
correspondente acompanha artefato. RomFS com hl4.dat/hl4.lang; saves em
sdmc:/switch/heroes_lore_4/saves. Libnx/SDL portlibs próprios do devkitPro.
Não usar hooks, title ID, chaves ou saves do HL5. Compilar não valida console/FPS.

Cache TCI limitado a 32 MiB antes da inicialização; padrão do fork em host64
é 1 GiB e não cabe no orçamento normal de homebrew. Log de inicialização:
sdmc:/switch/heroes_lore_4/boot.log, fallback sdmc:/hl4-boot.log. Inclui falhas
SDL/ARM e último estágio alcançado. Usar title override/full RAM no teste;
modo applet é registrado no log, mas não foi validado. Ícone original e RomFS
embutidos. Proprietário relatou fechamento da versão anterior: novo NRO ainda
precisa de reteste físico, desempenho e gamepad no console continuam pendentes.

Log posterior confirmou interrupcao em desRound pelo watchdog antigo de5s.
TCI agora usa120s no boot/30s nas demais chamadas, com diagnostico entry/PC/
budget/elapsed. Nao altera tempo do guest. Memoria cache heap RW: adapter
registra e libera mappings reais; nao oferece mprotect. Guard opcional TCG
omitido explicitamente no patch Switch TCI; limites internos preservados.
Teste do adapter executa em Linux CI; isso nao prova funcionamento no console.

Atualizacao2026-10-22: proprietario confirmou boot/jogo apos9263dc57, mas lento.
35280b7 ativa10regioes ARM proprias compiladas (graficos/DES), O3 e menosI/O.
NovoNRO emdist/heroes_lore_4.nro; testes independentes e limites em PERFORMANCE.md.
boot.log recebe [hl4-perf] cada10s, separando entrada/guest/presentacao e FPS
do laco/flushes. Retestar movimento/ataques/menu e enviar essas linhas se preciso.
