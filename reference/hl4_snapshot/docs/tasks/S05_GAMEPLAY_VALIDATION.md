# S05 — consolidar gameplay e iniciar S06

Ler WINDOWS_RUNTIME.md e reference/analysis/WINDOWS_RUNTIME_TESTS.json antes de
modificar o host. A build local dist/HeroesLore4-Windows é executável e standalone.
S02–S04 concluídas; S05 parcial. Não recomeçar a investigação DEX/loader.

Já exercitado: novo jogo, cenário, movimento segurado em três direções nos
experimentos, ação, abrir/fechar menu, teclado e gamepad virtual automático com
estados e 40 capturas idênticas. Janela paisagem e toque inverso implementados.

Próximas ações:

1. Registrar ataque real com mudança de motion/HP de alvo, habilidades e uma
   transição entre mapas. Usar getters originais e capturas; não alterar flags de
   evento para pular a prova. Ação em 67/77 s do fixture não prova dano.
2. Testar um gamepad físico disponível, incluindo hotplug e soltar ao perder foco.
   Não confundir o dispositivo SDL virtual com hardware conectado.
3. Salvar/carregar/reabrir uma partida em diretório temporário, comparar posição,
   inventário e progresso. Core já prova contrato MC_fs, não save de gameplay.
4. Ouvir BGM/SFX; mapear MMMD se alcançado. Volume/mute/loop são implementados,
   mas sua cobertura completa precisa de teste. Desconhecidos devem diagnosticar.
5. Investigar desempenho com relógio real; --deterministic é só teste. Preservar
   cadência e framebuffer. Android futuro deve forçar landscape na integração
   própria, conforme pedido do proprietário; ainda não há runtime nessa plataforma.

Reprodução: comandos em WINDOWS_RUNTIME.md. Para repetir o cenário inicial use
novos save-dir/capture-dir. Atualizar fixture/evidências quando houver mudança;
não sobrescrever saves do usuário. Reempacotar após mudar fontes e verificar
DLLs com tools/verify_standalone.py. Não publicar em remoto do HL5.

Atualização: clique/toque corrigido, testes em POINTER_INPUT.md. Por pedido do
proprietário, investigar/traduzir inglês em paralelo via S09_TEXT_EXTRACTION.md.
