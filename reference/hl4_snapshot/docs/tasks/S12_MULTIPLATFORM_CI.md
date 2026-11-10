# S12 — builds próprios do HL4

Pedido explícito do proprietário em 2026-10-21: publicar código na main do
repositório HL4 e ativar Actions Windows/Linux/Android/Switch, consultando HL5.
Também autorizou APK e catálogo em release privado para insumos do CI. Nenhuma
chave local autorizada/enviada. Android usa certificado de teste gerado no CI.

Workflow build.yml dispara em main/dispatch. CMake compila fontes pinadas de
Unicorn 2.1.4 e SDL2 2.30.12; Android ARM64 via SDLActivity/NDK, orientação
sensorLandscape, applicationId próprio. Saves ficam em storage privado, Linux
em SDL_GetPrefPath e Switch em sdmc:/switch/heroes_lore_4/saves. RomFS próprio.
Windows preserva saves ao lado do EXE. SHA-256 de recursos agora portátil,
verificado localmente contra as 1.895 entradas do pack e core nativo.

Switch tenta fork interpretado Unicorn-TCI fixado em e3f075a, sem memória
executável, com adaptações Horizon guardadas. Compilação, semântica TCI e teste
em console são validações separadas. Não publicar NRO de placeholder ou stub.
Desconhecidos devem falhar no log. Acompanhar e corrigir o job real no GitHub.

Fonte consultada somente leitura: ../heroes_lore_5_recomp/.github/workflows/build.yml,
CMakeLists.txt, Makefile.switch e android/app/*. Nenhum script HL5 foi executado,
nenhuma chave, endereço guest ou identificador reaproveitado. Snapshot congelado
permanece intacto. Configuração ativa é nova, específica deste runtime WIPI.

Pacotes de teste locais devem reutilizar dist/HeroesLore4-Windows-English-review;
package_windows.py agora usa esse nome por padrão e exclui saves do ZIP.
Não marcar S12 concluído por um workflow escrito: registrar resultados reais.

Atualização do pedido: pacotes jogáveis sem fontes internas; fontes em um único
artifact separado. Android deve gerar heroes_lore_4_android_arm64.apk e
heroes_lore_4_android_global.apk (ARM64, ARMv7 e x86_64), como a matriz HL5.

Proprietário confirmou Android físico funcionando; Switch fecha antes do boot.
07ac588 reduz cache TCI64 de 1 GiB padrão para 32 MiB e registra boot.log na SD
antes do SDL. Hipótese ainda requer reteste no console. Ícone original extraído
do APK para cada plataforma; sem insumos binários no Git. Run 39691502454.

Resultado 07ac588/run39691502454: seis jobs success. APKs e NRO baixados e
verificados; TCI32 core + smoke72s com estados/capturas iguais ao nativo.
Proximo: reteste fisico do Switch, boot.log se falhar. Nao marcar S12 done.


Novo log fisico: host pronto, guest parou em desRound VA0x24d54 sob watchdog5s;
code_gen_buffer e erro secundario de munmap na limpeza. 9263dc57/39693888307
corrige budgets TCI e libera cache real, com teste do adapter Linux e core
watchdog/recuperacao. Ver SWITCH_BOOT_WATCHDOG.json. Aguardar CI e reteste NRO.

NRO9263dc57 gerado e verificado (icone/RomFS/ZIP apenas NRO), substitui
dist/heroes_lore_4.nro. TCI passou contrato de memoria, timeout/recuperacao
e smoke72s com36estados/38BMPs iguais ao nativo. Reteste fisico pendente.
