# Validação e matriz de execução

## Primeiro marco definido pelo proprietário — PC

Em 2026-10-21, o proprietário definiu a primeira validação como executar o
HL4 neste PC com teclado e mapeamento equivalente/automático para gamepad,
como nos ports anteriores. Esse marco atravessa S03/S04/S05; concluir a
pesquisa S02 ou executar probes ARM não o atende.

Aceite do marco: abrir o jogo, chegar ao título, iniciar partida, movimentar,
atacar, navegar/confirmar/cancelar menus e retornar, usando separadamente
teclado e gamepad detectado automaticamente. Ambos deverão acionar as mesmas
ações internas HL4, com pressionar/segurar/soltar e sem teclas presas ao perder
foco ou desconectar o controle. Registrar modelo do controle, mapeamento,
capturas, cenário e resultado real neste PC. A tabela de códigos/ações do HL4
ainda precisa ser descoberta; não copiar a ABI de entrada de outro jogo.

Os ports anteriores orientam a experiência desejada, sem preencher a matriz
de execução do HL4. Próximas plataformas seguem os gates do roadmap.

## Preparação atual

`python tools/verify_workspace.py`: integridade do APK, referências/status e
manifesto de hashes do snapshot. `python tools/inspect_apk.py --check`: evidências
atuais correspondem aos bytes de entrada. `--extract` extrai de forma segura e
repetível; não executa nada do APK. Tudo funciona com Python padrão, offline.

## Depois do primeiro runtime

Durante S02, `tools/inspect_runtime.py --check`, `tools/probe_abi.py --check`
e `python -m unittest discover -s tests -v` validam relatórios, ABI isolada e
rejeição de entradas alteradas. Exigem tools/requirements-analysis.txt; ver
RUNTIME_CONTRACT.md. Não são testes de boot ou de controle conectado.

1. CPU/loader: ARM e Thumb se usados, relocations, BSS, stack, overflow, flags,
   convenções AAPCS/soft-float efetivamente identificadas e retornos 64-bit.
2. HLE: eventos e timers, mapeamento de recursos, imagem/alpha/clipping/paleta,
   formato/volume/loop de áudio e arquivos com retorno/erro correto.
3. Boot: inicialização, título, novo jogo e primeiro mapa com capturas e logs.
4. Gameplay: movimento, ataques, habilidades, menu, transições e input segurado.
5. Saves: gravar/ler/reabrir em diretório temporário; erro e rollback sem perda.
6. Performance: cenário repetível, mesma build de referência/configuração,
   warmup/mediana/p95, input/CPU/apresentação separados, sem verifier/profiler no
   timing final. Verificar visual/estado antes de aceitar aceleração.
7. Aspecto/idioma: redimensionar e trocar idioma ao vivo, orientação, acentos,
   fontes no Switch, widgets PT-BR limitados, sem resíduos do idioma anterior.
8. Serviços: transporte mock/local, arquivos de teste, download parcial/erro,
   assinatura/versionCode crescente, restore e instalação com falhas controladas.

Evidências devem indicar commit, comando, plataforma real, flags, cenário,
arquivos usados e resultados. Guardar capturas/áudio grandes localmente; registrar
hashes e resumos portáveis em Git. Não usar saves pessoais como fixture.

## Matriz inicial

Verificação pendente de aproveitamento da tela (relato Android em 2026-10-21):
seguir [S08 — viewport expandido](tasks/S08_VIEWPORT_EXPANSION.md). Exigir cenário
adicional real em Auto, com HUD/toque e fullscreen/áreas seguras corretos nas
quatro plataformas. Comparar proporções, capturas, estado, áudio e custo de
memória/quadro. Ainda não implementado nem validado no HL4.

| Plataforma | Compila HL4 | Boot | Gameplay/áudio | Serviços | Hardware |
| --- | --- | --- | --- | --- | --- |
| Windows x64 | Sim; standalone local | Sim, original ARM | Movimento/menu e Ogg; combate/save de partida parciais | Rede antiga falha explicitamente | PC real; teclado SDL e gamepad virtual; pad físico pendente |
| Linux | Não implementado | Não testado | Não testado | Não implementado | Não testado |
| Android | Não implementado | Não testado | Não testado | Não implementado | Não testado |
| Switch | Não implementado | Não testado | Não testado | Não implementado | Não testado |

Atualizar conforme evidência. Teste do HL5 não preenche esta matriz.

## Execução Windows 2026-10-21

Ver WINDOWS_RUNTIME.md e reference/analysis/WINDOWS_RUNTIME_TESTS.json: 80 s no
pacote final com relógio real, vídeo/áudio do PC, movimento/menu; 1.429 flushes,
1.988.608 amostras não nulas. Percurso determinístico separado por teclado e
controle SDL virtual: 2.488 flushes, mesmos estados e 40 capturas iguais.
Core prova ARM flags/Thumb, contexto Unicorn e R0–R15/CPSR, ABI módulo/framebuffer,
roundtrip de arquivos temporários e rejeição de import desconhecido. Verificador
standalone confere DLLs e executa core com PATH System32/cwd distinto.
Não houve teste manual de pad físico, audição humana, combate com dano, save/load
de partida completa ou outra plataforma. S05/S06/S12 continuam parciais por isso.

## Correção após relato do usuário

Mouse/toque da primeira build tinham conversão dupla de escala. Nova evidência:
POINTER_TESTS.json, com Skip 1 ms, ação, movimento/menu/retorno, mouse em resolução
normal/letterbox e eventos finger. Estados iguais em 80 s de relógio de teste;
sem touchscreen físico. Build nova inputfix com core/DLLs standalone verificados.
TEXT_INVENTORY.json: 345 scenes/6.935 registros catalogados com roundtrips exatos;
sete regressões de análise/formato passam. Nenhum inglês desenhado ainda.


## 2026-10-22 - boot Switch confirmado, desempenho em validacao

Proprietario confirmou que NRO9263dc57 abriu e executou, mas ficou lento.
Android ja tinha instalacao/jogo confirmados. S07 agora mede e acelera
regioes graficas/DES proprias HL4. Ver PERFORMANCE.md e ARM_FAST_TESTS.json:
20comparacoes regionais de GP/CPSR/memoria inteira,36estados/38BMPs iguais no PC.
Novo NRO35280b7 compilado/empacotado; fluidez fisica ainda aguarda reteste.
