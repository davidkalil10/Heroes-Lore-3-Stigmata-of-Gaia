Heroes Lore 4 - primeira build Windows x64

Extraia a pasta inteira e abra heroes_lore_4.exe. Mantenha hl4.dat e as DLLs
ao lado do executavel. Nao precisa instalar Python, SDL ou Unicorn.
Destino: Windows 10/11 x64. Testado neste PC Windows; outros PCs ainda pendentes.

A janela abre na horizontal. As telas iniciais em retrato mantem suas proporcoes;
o jogo passa automaticamente a paisagem, com mouse e direcoes ajustados.
Texto e botoes virtuais originais coreanos foram preservados.

Teclado                         Gamepad SDL reconhecido automaticamente
Setas ou WASD: mover            Direcional ou analogico esquerdo: mover
Enter/Espaco/Z: confirmar/acao   A: confirmar/acao
Esc/X/Tab: voltar/menu          B ou Start: voltar/menu
Backspace/F3: pular cena        Back/Select: pular cena
1 e 3                          X e Y
Shift e 0                      LB e RB
Q e E                          Teclas auxiliares originais

O clique nos controles virtuais tambem funciona. Soltar teclas ao trocar de
janela evita movimentos presos. Gamepad pode ser conectado com o jogo aberto.

No primeiro uso pode aparecer uma pergunta do jogo sobre servicos SMS antigos:
use 2 ou B/Esc para recusar. Nenhum pedido externo e enviado. A tentativa original
de conexao retorna falha e mostra o aviso do proprio jogo; confirme para continuar.
Novo jogo e criacao de personagem seguem as telas originais.

Saves ficam em saves/ nesta pasta, separados dos outros ports. Nao mova apenas
o EXE e nao execute dentro do ZIP. Use uma pasta onde voce tenha permissao de escrita.

Esta e uma build inicial de validacao, com ingles parcial e sem servicos online.
Teclado e gamepad virtual passam pelo mesmo caminho SDL de entrada; o controle
fisico ainda precisa de teste manual. Audio Ogg real implementado; formatos e
contratos nao encontrados no percurso inicial podem causar erro diagnostico.
Salvar/carregar uma partida completa ainda precisa de validacao dedicada.

Licencas acompanham o pacote em licenses/. Fontes sao disponibilizadas separadas.
Os recursos do jogo pertencem aos respectivos titulares e vieram do APK local.
Este pacote foi gerado localmente para o proprietario; nao foi publicado.

Atualizacao inputfix - 21/10/2026
Mouse/toque corrigidos para escala, barras e rotacao; Skip de clique rapido
preservado ate o jogo processar. Testes incluem janela redimensionada.
Feche a build anterior antes de abrir esta. Para continuar seu progresso, copie
a pasta saves da build anterior para esta pasta. O pacote nao inclui seus saves.
Atualizacao English-review - 21/10/2026
Ingles e o idioma padrao. Mantenha hl4.lang junto do EXE e das DLLs.
Para o coreano original, execute heroes_lore_4.exe --language ko.
Menus principais, abertura e primeiros tutoriais foram revisados; o restante
do catalogo ainda contem rascunhos automaticos que precisam de revisao.
Textos em imagens e recursos ainda nao catalogados podem continuar em coreano.
PT-BR esta no catalogo de traducao, mas ainda nao e selecionavel nesta build.
Corrigida a falha de copia vazia com origem nula no fim de uma cutscene.
