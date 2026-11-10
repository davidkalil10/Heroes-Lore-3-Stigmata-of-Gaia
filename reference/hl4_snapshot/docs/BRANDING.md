# Arte de aplicativo fornecida pelo proprietário

Fonte exata: `scratch/branding/app-icon.jpg`, JPEG 2048×2048. SHA-256 e
procedência em `assets/branding/icon.json`. Arte autorizada nesta conversa;
nenhuma modificação artística. Arquivo binário fora do Git e disponível no
release privado de insumos referenciado por `ci-inputs.json`.

`python tools/prepare_icons.py` valida o hash e gera PNG1024 Linux, BMP256
para janela SDL, ICO multirresolução16–256 para executável Windows e JPEG256
para NRO Switch. A build Windows local agora também incorpora o recurso ICO.
Android recebe fallback512 e recurso adaptativo: camada opaca1296 com arte864
no centro e margem de animação escura. Manifest usa mipmap adaptativo tanto
para icon quanto roundIcon. Não acrescenta placa branca de ícone legado.

A geometria segue o canvas108dp/viewport72dp documentado pelo Android:
https://developer.android.com/develop/ui/compose/system/icon_design_adaptive

A verificação de APK compara pixels decodificados das imagens empacotadas,
exige adaptive-icon no XML compilado, verifica ABIs/SDL_main e dependências.
NRO exige JPEG/data/lang exatos. Visualização circular local em
`scratch/branding/android-circle-preview.png`; launcher físico ainda precisa
reteste, pois máscara e cache dependem do dispositivo. O original permanece
inalterado. Não colocar ícones ou outros binários no histórico Git.
