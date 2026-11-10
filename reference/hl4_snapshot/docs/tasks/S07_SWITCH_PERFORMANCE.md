# S07 — Desempenho Switch, revisão de funções completas

Usuário confirmou que otimização anterior melhorou pouco e jogo continua
extremamente lento. S07 continua em andamento. PERFORMANCE.md descreve a nova
implementação: sete regiões de maior cobertura, funções gráficas completas,
páginas nativas e emissão direta. Perfil remanescente em ARM_REMAINING_PROFILE.

CI39774383459 concluída: seis jobs success; Windows/Linux/TCI14checks e
38BMPs/36estados iguais. APKs/NRO validados e atualizados nos nomes fixos.
Próxima ação: reteste físico: mesmo save/cenário, movimento, ataque, menu e transição.
Amostra Windows1.602x não equivale a FPS/ganho no Switch. Não modificar clocks,
física ou relógio para mascarar lentidão.

Usuário explorou/combateu/abriu menus e todo boot.log terminou no boot, sem
[hl4-perf]. Novo performance.log independente grava cabeçalho e amostras5s com
fflush. Coletar esse arquivo, sem pedir novamente o trecho ausente de boot.log.
Também validar nova superfície fullscreen/mode do console, HUD e A/B,X/Y.

Aceite: melhoria reproduzível no mesmo console/cenário, sem regressões visuais,
estado/áudio/saves; demais gates do ROADMAP permanecem. Testes automáticos e
execução em hardware são estados distintos.
