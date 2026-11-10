# Registro de decisões

## ADR-001 — Isolar HL4 e congelar referências (2026-10-21, aceito)

Pedido: novo projeto/sessão, sem interferir no HL5. Criar estrutura própria,
documentação e snapshot selecionado do HL5 com hashes/commit. Referência não é
implementação HL4. Evita depender de caminhos vivos e de memória de conversa.

## ADR-002 — Investigar WIPI/ARM antes do bootstrap (2026-10-21, aceito)

APK contém ELF ARM e DEX LGT/WIPI, JAR sem classes, imports MC_*/wipic_* e sem
exports JNI Java_*. Não copiar o bootstrap JNI do HL5 nem iniciar recompilação
Java dos ports HL1/HL2/Zero. A estratégia candidata é loader/CPU ARM com HLE WIPI;
S02/S03 devem comprovar contrato e revisar esta escolha se necessário.

## ADR-003 — Status e evidências portáveis (2026-10-21, aceito)

STATUS.json é fonte única; Markdown gerado. Handoff, ADRs e diário registram
decisões, testes e próximos passos. DONE exige evidência e critérios cumpridos.
Separar compilação, execução no host e validação real por dispositivo.

## ADR-004 — Oráculo ARM limitado para S02 (2026-10-21, aceito)

Problema: nomes dos símbolos não comprovam ABI e não há player WIPI no APK.
Usar Capstone/pyelftools/Androguard para inspeção local e Unicorn como oráculo
para funções originais isoladas, com memória temporária, bases de teste próprias,
traps de imports e fixtures declaradas. Não adaptar o bootstrap JNI do HL5 nem
tratar esses probes como runtime nativo. Ferramentas novas em tools/, dependências
em scratch, versões fixadas. Nenhum conteúdo do jogo é enviado na instalação.

Evidência: RUNTIME_CONTRACT.md e reference/analysis/ABI_PROBES.json. Os 21 casos
verificam tabelas/globais, chamadas de init, eventos, nomes/ownership de recursos,
descritor de framebuffer e passagem de timer. Desconhecidos falham com nome e
argumentos. Limite: serviços gráficos/áudio/event loop não estão implementados.
S02 continua aberta enquanto esses contratos impedirem boot. Reconsiderar o
uso do oráculo se surgir incompatibilidade de ISA/ABI; comparar o núcleo S03
com estados completos antes de qualquer aceleração.


## ADR-005 — host Windows original ARM/WIPI e pacote local (2026-10-21, aceito)

Implementar src/main.cpp com Unicorn 2.1.4, loader/HLE próprios e SDL2 2.30.12.
O oráculo limitado ADR-004 permanece para regressões; agora o runtime executa
até gameplay, sem JNI, hooks HL5 ou patches no guest. Reuso seletivo apenas de
stb_vorbis com licença. Contratos desconhecidos diagnosticam. Rede indisponível
entrega callback de erro e aviso original, sem pedidos externos. ABI detalhada
em WINDOWS_RUNTIME.md; r1 do timer é padding, não modo.

Pedido do proprietário: Windows standalone e orientação horizontal. Empacotar
recursos, SDL2.dll, unicorn.dll, VCRUNTIME140.dll, licenças e fontes. Janela
paisagem apresenta a rotação original com toque/direções inversos; logos mantêm
proporção. Android futuro deverá forçar landscape em sua própria integração.

Aceite: core/contexto, teclado/gamepad virtual com 40 quadros idênticos, movimento,
menu e 80 s da distribuição no PC com vídeo/áudio reais. DLLs verificadas com
PATH restrito e cwd separado. Controle físico, saves de partida e combate completo
seguem pendentes. Não concluir S05/S06/S12 inteiras.

## Modelo para decisões futuras

ID/data/estado, problema, evidências, alternativas, decisão, consequências,
limites, teste de aceite e condição para reconsiderar. Não apagar decisões
superadas: marcar como substituídas e apontar a nova ADR.

## ADR-006 — coordenadas uma vez e localização offline (2026-10-21, aceito)

SDL_RenderSetLogicalSize é a única conversão janela→lógico; o host aplica apenas
rotação inversa. Fila de pointer por tick preserva cliques rápidos sem alterar
cadência/guest. SDL_FINGER sem mouse sintetizado duplicado. Testes exercitam
renderer/filtro SDL e código original, não só uma fórmula de coordenadas.

Pedido do proprietário abre S09 em paralelo. Extrair localmente com DES ARM
original e preservar scripts, tags e padding, com roundtrip antes de tradução.
Corpus fica em scratch/localization; Git guarda metadados. Inglês começa como
rascunho revisável e futuro buffer separado, mantendo canônico. Não trocar arquivo
SC inteiro por texto plano nem concluir S10 enquanto não houver desenho validado.

## ADR-007 — idioma separado e revisão explícita (2026-10-21, aceito)

Pedido do proprietário autoriza tradução EN/PTBR e inglês padrão. Inferência
automática inteiramente local; não enviar corpus. Catálogo preenchido não é
tradução validada: separar rascunhos de correções editoriais por registro.
Buffers guest estáveis, canônico e scripts imutáveis; hooks HL4 com guards de
opcodes no desenho e medição. Restauração da pilha obrigatória, original ARM
calcula/desenha fontes. Não confundir quinto argumento de desenho (fonte) com
altura. Não instalar hook de retorno dentro de TB já em execução.
PTBR só selecionável quando fonte acentuada funcionar; original via --language ko.
S10 permanece em andamento: cobertura de recursos, revisão e paginação pendentes.
Teste de aceite e limites em LOCALIZATION_RUNTIME.md / LOCALIZATION_TESTS.json.

## ADR-008 — CI privado HL4 e caminhos por plataforma (2026-10-21)

Proprietário pediu push na main e Actions das quatro plataformas, usando HL5
como referência de estrutura. Repositório HL4 existente confirmado privado.
Autorização adicional explícita: APK e catálogo no release de insumos privado,
fora do histórico Git; nenhuma chave local enviada. Release de insumos precisa
estar publicado dentro do repositório privado: rascunho não permite leitura pelo
GITHUB_TOKEN limitado a contents:read. Checksums fixados em ci-inputs.json.

CMake comum, SDL 2.30.12 e Unicorn 2.1.4; Android NDK/SDLActivity, identificador
próprio, sensorLandscape. Arquivos em assets via SDL_RWFromFile, saves privados;
Linux usa SDL_GetPrefPath, Switch RomFS + pasta sdmc própria, Windows mantém
portable saves. SHA-256 portátil substitui BCrypt e prova os mesmos recursos.
Operações fopen/seek/tell têm adaptação explícita por host, preservando contrato.

Unicorn JIT desktop/Android não é transplantável diretamente para Horizon.
Switch usa fork TCI fixado, com backend de memória somente dados e recusando
PROT_EXEC. Testar referência TCI em Linux e ARM/Thumb/contexto/localização antes
de oferecer build experimental Switch. Compilação não prova hardware/FPS.
Não usar NRO stub para satisfazer workflow verde. Fontes correspondentes acompanham.

Distribuições locais futuras reutilizam dist/HeroesLore4-Windows-English-review,
conforme pedido: preservar saves e atualizar somente arquivos do pacote.


## ADR-009 — ícone original e orçamento TCI no Switch (2026-10-21)

Extrair res/drawable/icon.png somente do APK com checksum conferido, gerar
formatos no build e manter binários fora do Git. APKs ARM64/global separados;
fontes correspondentes em artifact único, fora dos pacotes jogáveis.

TCI 64-bit herda reserva de cache 1 GiB do QEMU. Fixar 32 MiB antes do primeiro
mapa que dispara lazy init; desktop/Android JIT mantêm comportamento anterior.
Mesmo limite no job referência TCI Linux, com core e smoke de jogo. Redução de
memória é observável em código, mas ainda não prova a causa do fechamento no
console. Log persistente antes do SDL distingue falha de memória, montagem,
vídeo e guest; loop respeita appletMainLoop. Não chamar NRO compilado de validado.


## ADR-010 - watchdog TCI e contrato de memoria (2026-10-21)

Log Switch parou dentro de desRound com limite host de 5s; erro posterior de
munmap nao e causa de alocacao. Usar budget120s no boot/30s em chamadas TCI,
sem modificar tempo do guest ou motor JIT. Manter watchdog finito e consultar
UC_QUERY_TIMEOUT para diferenciar parada de expiracao. Registrar entrada/PC VA,
tempo e budget, sem instrumentar cada instrucao.

Cache TCI permanece heap RW de32MiB. Adapter registra propriedade/tamanho,
libera com free e rejeita mprotect/exec/fixed/shared nao implementados. Guard
TCG opcional omitido explicitamente apenas Switch+TCI; limites internos da
regiao continuam. Nao devolver sucesso ficticio de permissao nem falhar a
liberacao por tentar restaurar protecao que nunca foi aplicada. Testar adapter
real em Linux separadamente de compilacao/execucao em console.


## ADR-011 - especializacao ARM do HL4 apos perfil (2026-10-22)

Aplicar metodo HL5, nunca seus hooks: decoder puro adaptado do snapshot e
blocos gerados de10regioes graficos/DES do APK HL4. Validar opcodes, GP/CPSR,
memoria inteira e percurso com Unicorn independente. Fallback original para
opcodes divergentes; no-fast para oracle. Ativo por padrao apenas Switch/TCI;
JIT PC/Android preservado. Sem alterar tempo/fisica/viewport/clocks.

Usar PC write individual na ponte Unicorn (batch nao seta skip_sync_pc_on_exit
na versao pinada); deixar epilogo LDM-PC interpretado e rejeita-lo no gerador.
Falhas encontradas por testes antes de publicacao, documentadas em PERFORMANCE.
Custo pesado do oracle so opt-in PC/CI; metrica fisica Switch agregada10s.

## ADR-012 - Android imersivo e recuo de notificacoes (2026-10-22)

Solicitar fullscreen na janela SDL Android, que controla as barras além do
tema Manifest. Manter proporção uniforme. Não promover framebuffer maior
a true widescreen sem evidência de expansão da câmera/culling. Prova240x534
expande painel e foi rejeitada. Em notificações, remover recuo coreano feito
com pontos invisíveis no pacote traduzido e padronizar apenas os dois estados
coreanos exatos. Preservar originais e renderizador ARM, com teste de linhas.

## ADR-013 - Auto inicial com viewport HL4 (2026-10-22)

Criar janela antes do guest e ampliar uma dimensão, pares/cap1280. Guardar
contrato de touch_setUiChange/m_TouchDirectKeySize do próprio ELF HL4 para
manter painel80 e liberar mundo adicional. Zero continua oculto. Native CLI
sem hook como referência. AOT mantém opcodes/guards e passou oracle ampliado.
Integrar Auto inicial após prova de pixels/cenário/estados/toque. Não alegar
resize vivo, cobertura integral de mapas/UI ou hardware; manter S08 parcial.

## ADR-014 — Funções gráficas completas e marca própria (2026-10-22)

Substituir regiões parciais por CFG das duas funções HL4 completas, em páginas
nativas limitadas, opcodes guardados e emissão direta fiel ao decoder. Manter
oracle independente, registradores/memória completos e epílogos originais.
Agrupar contadores diagnósticos sem alterar instruções/tempo do jogo. Ganho de
host não encerra S07: proprietário ainda relata lentidão severa no Switch.

Switch pede modo de tela do console/fullscreen e troca A/B,X/Y conforme pedido.
Métricas vão também para arquivo independente com flush periódico, pois usuário
jogou e fechou sem receber linhas no boot.log; causa de stdout ainda incerta.
Arte2048 fornecida pelo proprietário vira insumo privado pinado, fora do Git.
Todos os formatos são derivados offline; Android usa recurso adaptativo opaco.


## ADR-015 — combate mobile, timeout e nomes dinâmicos (2026-10-22)

Perfilar combate com fast ativo, não generalizar cidade. Três regiões próprias
extras, guardas/oracle e kernels sintéticos para CI sem upload de save pessoal.
Ativar fast noAndroid; manter relógio e budgets do jogo. Watchdog persistente
serializado substitui polling/thread por chamada, com teste da janela arm/entry
e rollback diagnóstico --legacy-watchdog. Resultado de host não fecha S07.

Extrair nomes de tabelas DES verificadas e compor só spans conhecidos/ASCII;
preservar originais e usar medição/desenho ARM. Cache limitado recicla somente
fora de draw ativo, evitando reverter a coreano após512combinações. Glossário
substitui frases automáticas inadequadas; PTBR/fantasia ainda provisórios.

LogSwitch abre FS/cartão diretamente antes do SDL e força flush por amostra.
IDNACP próprio0100F4A400040000 deve coincidir com novo forwarder; não mexe em
registro instalado nem em save path. Explicar essa distinção ao proprietário.
