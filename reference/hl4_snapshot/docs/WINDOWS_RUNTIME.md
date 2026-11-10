# Runtime Windows â€” 2026-10-21

## ImplementaÃ§Ã£o e limite do marco

Existe runtime C++17 Windows x64 executando o ARM/Thumb original via Unicorn
2.1.4, com HLE WIPI prÃ³prio em src/main.cpp. Boot, criaÃ§Ã£o de personagem,
primeiro cenÃ¡rio, movimento, aÃ§Ã£o e menu funcionam no percurso automatizado.
O texto coreano, a resoluÃ§Ã£o e os controles virtuais originais sÃ£o preservados.
NÃ£o hÃ¡ patches em instruÃ§Ãµes do jogo, hooks de endereÃ§os HL5 ou serviÃ§o externo.
O pacote local nÃ£o Ã© uma release final. Ataque com dano/inimigos, habilidades,
transiÃ§Ãµes posteriores e save/load de partida completa precisam de testes.

Janela inicial 800x480. Durante logos, retrato 240x400 com barras; quando o
original ativa nTouchRotate, apresentaÃ§Ã£o 400x240 girada 90 graus e toque inverso
gx=ly, gy=399-lx. A orientaÃ§Ã£o de apresentaÃ§Ã£o permanece horizontal. Android
ainda nÃ£o tem runtime: aplicar landscape no futuro Activity/manifest prÃ³prio;
nÃ£o alterar o manifesto do APK de referÃªncia nem alegar teste em dispositivo.

## Contratos efetivamente exercitados

- ELF ET_DYN, PHDR com VA distinta do offset, BSS zero, RELATIVE/GLOB_DAT/JUMP_SLOT,
  relocaÃ§Ã£o de cÃ³digo, entry/init/_init_module e seis callbacks Clet originais.
- Recursos preservados e verificados por SHA-256; guest decodifica seus sprites,
  fontes e mapas. Arquivo hl4.dat contÃ©m 1.895 entradas (ELF + arquivos do JAR).
- Alocador coalescente, framebuffer RGB565 e primitivas usadas no boot/gameplay.
  Contratos nÃ£o implementados falham com sÃ­mbolo e endereÃ§o de chamada.
- MC_knlSetTimer usa uint64 alinhado em r2:r3; **r1 Ã© padding**, nÃ£o um modo.
  Quinto argumento [sp] Ã© userdata, entregue no r1 do callback(timer,userdata).
  O wrapper _midas_funcCallLibCB 0x225b8 move esse r1 para o evento do cliente.
  TimerCB original controla lÃ³gica/render; produÃ§Ã£o usa SDL_GetTicks64 real.
- Fila entrega no mÃ¡ximo um down/up por tick: o original sÃ³ tem um slot de cada.
  InversÃ£o das quatro direÃ§Ãµes ao girar; traduÃ§Ã£o fica retida atÃ© o keyup.
  Teclado e SDL GameController usam uniÃ£o dos estados e hotplug automÃ¡tico.
- MC_fsOpen modos observados 1=leitura, 4=criar/truncar, 8=existente leitura/escrita.
  Caminhos ficam no saveRoot prÃ³prio, sem '..'. PreferÃªncias kickass sÃ£o criadas
  pelo guest; teste do contrato de arquivo cria/lÃª/reabre/remove dados descartÃ¡veis.
- MC_mdaClipAllocPlayer recebe '-file res/SND/â€¦ogg'. stb_vorbis decodifica Ogg
  real, SDL converte para estÃ©reo float 48 kHz; mixer implementa loop/volume/mute.
  MMMD ainda nÃ£o foi exercitado no percurso e nÃ£o Ã© declarado suportado.
- MC_knlPrintk recebe texto jÃ¡ formatado. Sprintk retorna -1 e preserva prefixo
  para formato invÃ¡lido original '%s%0' (VA 0xd8ed0, caller LR 0xaae3c).
- Telefone/propriedades de rede ausentes retornam erro. Backlight/vibraÃ§Ã£o sÃ£o
  no-ops opcionais explÃ­citos. Desconhecidos essenciais nÃ£o retornam sucesso.
- Rede antiga: MC_netConnect aceita agendamento e entrega callback(-1,userdata)
  no tick seguinte. netConnectCB 0xa82a0 chama netErrorDisconnect, mostrando o
  aviso original. Confirmar volta ao tÃ­tulo e permite novo jogo offline.
  Nenhuma conexÃ£o externa, autenticaÃ§Ã£o fictÃ­cia ou patch de desbloqueio.

Experimentos negativos: -19 em MC_netConnect Ã© interpretado pelo wrapper original
como pronto e chama callback(0,userdata): incorreto para indisponibilidade e
substituÃ­do pela falha assÃ­ncrona. Tratar r1 do timer como modo tambÃ©m era errado.
Pressionar confirm durante roteiro sem concluir a cena impedia avaliar movimento;
fixture agora usa skip e confirma os popups antes de andar. SÃ£o resultados de
investigaÃ§Ã£o, nÃ£o comportamentos corrigidos por alteraÃ§Ã£o do jogo.

## ReproduÃ§Ã£o da build e testes

VS 2022 Community MSVC 14.44.35207, SDK Windows instalado, SDL VC x64 2.30.12 em
scratch/deps/SDL2-2.30.12, wheel Unicorn 2.1.4 em scratch/analysis_deps.
tools/unicorn_importlib.py gera import library do DLL: a .lib no wheel Ã© estÃ¡tica.
Executar tools/build_windows.cmd. Host /MT; Unicorn requer VCRUNTIME140.dll.
SDL2.dll, unicorn.dll e VCRUNTIME140.dll sÃ£o copiados. Python sÃ³ participa do build.

```powershell
cmd /c tools\build_windows.cmd
python tools/package_windows.py
python tools/verify_standalone.py dist/HeroesLore4-Windows --run
$env:SDL_VIDEODRIVER='dummy'
$env:SDL_AUDIODRIVER='dummy'
dist/windows/heroes_lore_4.exe --smoke 80 --deterministic --script tests/boot_navigation.txt --input-mode keyboard --capture-dir scratch/keyboard-final --save-dir scratch/keyboard-final-saves
dist/windows/heroes_lore_4.exe --smoke 80 --deterministic --script tests/boot_navigation.txt --input-mode gamepad --capture-dir scratch/gamepad-final --save-dir scratch/gamepad-final-saves
```

Usar diretÃ³rios NOVOS a cada teste de novo jogo; saves existentes mudam o fluxo.
--deterministic avanÃ§a relÃ³gio de teste em passos de 10 ms, sem acelerar o relÃ³gio
da build normal; nÃ£o serve para medir desempenho. O Ã¡udio continua sendo consumido
pelo dispositivo, portanto Ã¡udio_nonzero e duraÃ§Ã£o real nÃ£o sÃ£o determinÃ­sticos.
Scripts emitem eventos SDL de teclado ou botÃµes de joystick virtual reconhecido
automaticamente como GameController; nÃ£o simulam apenas o resultado no guest.
Sem SDL_* dummy, o teste usa os dispositivos de vÃ­deo/Ã¡udio reais do PC.

## EvidÃªncia e procedÃªncia

reference/analysis/WINDOWS_RUNTIME_TESTS.json registra hashes da build, fixture,
capturas, estados e comparaÃ§Ã£o entre os modos. RelatÃ³rios e imagens completos
ficam em scratch/; pacote em dist/HeroesLore4-Windows e ZIP homÃ´nimo.
--verify-core verifica ARM flags, Thumb/interworking, contexto completo Unicorn
com roundtrip R0â€“R15/CPSR, ABI mÃ³dulo/framebuffer, arquivos e falha de import
desconhecido. NÃ£o equivale a estado salvo completo do jogo nem a testes de saves.

O pacote final executou 80 s com relÃ³gio real e vÃ­deo/Ã¡udio do PC: 1.429 flushes,
1.988.608 amostras nÃ£o nulas, movimento e abertura/retorno do menu.
Percurso de 80 s de relÃ³gio de teste: 2.488 flushes; movimento apÃ³s 63 s, menu e
retorno, aÃ§Ã£o acionada, nenhum import desconhecido. Gamepad fÃ­sico, Ã¡udio ouvido
por humano e qualidade de todas as mÃºsicas/efeitos continuam validaÃ§Ãµes manuais.

Ãšnico cÃ³digo copiado do snapshot: src/audio/stb_vorbis.c, byte a byte, de
reference/hl5_snapshot/src/audio/stb_vorbis.c, licenÃ§a no rodapÃ©. Loader e HLE novos.
Fontes/licenÃ§as e ZIP oficial Unicorn tag 2.1.4 acompanham o pacote. As fontes do
host acompanham para revisÃ£o/rebuild; obter as dependÃªncias nas versÃµes descritas
e o APK do proprietÃ¡rio. Recursos, binÃ¡rios e saves continuam ignorados no Git.

## Atualizacao inputfix

Relato do usuario revelou bug no mouse/toque: a conversao de escala era aplicada
pela SDL e novamente pelo host. Corrigida em src/main.cpp; down/up enfileirados
por tick e SDL_FINGER tratado sem duplicacao. Ver POINTER_INPUT.md e
reference/analysis/POINTER_TESTS.json. A evidencia WINDOWS_RUNTIME_TESTS.json
refere-se a build inicial, preservada; nao atribuir seu hash a nova build.
Distribuicao inputfix preservada: dist/HeroesLore4-Windows-inputfix.zip.
Investigacao de 6.935 registros coreanos em TEXT_LOCALIZATION.md; nenhum texto
foi modificado na imagem do jogo.

## Atualização English-review

Build atual: dist/HeroesLore4-Windows-English-review.zip. Inglês padrão,
hl4.lang e DLLs inclusos, core executado também em PATH System32. Ver
LOCALIZATION_RUNTIME.md e reference/analysis/LOCALIZATION_TESTS.json.
334 registros revisados editorialmente; restante do catálogo é rascunho.
Medição original ARM adaptada aos bytes desenhados; rótulos e quebras revistos.
Comparação de 105 s com cópia de save: 3.358 quadros e 52 amostras de trajetória
e callback idênticos ao coreano; 4.088 desenhos/restaurações, nenhum miss nesse
percurso. Não prova toda campanha ou paginação. Menu/status abertos por pointer,
capturas conferidas. PTBR preenchido, fontes acentuadas pendentes.
Corrigido memcpy de tamanho zero com origem nula no fim da cutscene; cópia
nula de tamanho 1 continua rejeitada. Usuário confirmou correção em jogo.

### Viewport inicial Auto (2026-10-22)

A proporção da janela na abertura agora escolhe canvas com mais mundo, escala
uniforme. --aspect native retorna ao canvas original. Resize durante partida
ainda escala o canvas inicial; detalhes/evidência/limites em DISPLAY.md.
