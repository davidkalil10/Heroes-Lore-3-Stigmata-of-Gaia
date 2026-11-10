# Etapa S02: Contrato de Runtime Mach-O / iOS HLE e Formatos de Recursos

## Objetivo

Produzir o documento completo `docs/RUNTIME_CONTRACT.md` e um script determinístico de inspeção profunda (`tools/probe_macho_contract.py`) que mapeie todas as estruturas binárias necessárias para carregar e executar `Payload/Heroes Lore.app/Heroes Lore` em `S03`.

## Contexto Já Confirmado em `S01`

- O executável `Payload/Heroes Lore.app/Heroes Lore` (`915.824` bytes, SHA-256 `39c6ed1db25cbf6020c9edd2cfa145410ff391f3361a9150d9646b9a4fada849`) é um binário **Mach-O 32-bit ARMv6 (`MH_EXECUTE`)**.
- Segmentos principais:
  - `__PAGEZERO`: `0x00000000..0x00001000`
  - `__TEXT`: `0x00001000..0x000cc000` (`__text` em `0x00002b60..0x000bdc2c`, `__symbol_stub4` em `0x000bdc2c..0x000bf1fc`)
  - `__DATA`: `0x000cc000..0x000e2000` (`__mod_init_func`, `__nl_symbol_ptr`, `__la_symbol_ptr`, `__cfstring`, `__objc_classlist`, `__objc_const`, `__objc_selrefs`, `__objc_classrefs`, `__objc_data`, `__data`, `__common`, `__bss`)
  - `__LINKEDIT`: `0x000e2000..0x000ea000`
- Ponto de entrada `LC_UNIXTHREAD` (ARM thread state): deve ser extraído e documentado junto com os `27` ponteiros de `__mod_init_func`.

## Entregáveis Obrigatórios da Etapa `S02`

1. **`tools/probe_macho_contract.py`**:
   - Extrair o `LC_UNIXTHREAD` (`r0..r15`, `cpsr`, `pc` inicial).
   - Mapear os `27` endereços de `__mod_init_func` (`0x000cc000`).
   - Resolver a tabela de símbolos indiretos (`LC_DYSYMTAB` + `__nl_symbol_ptr` + `__la_symbol_ptr` + `__symbol_stub4`), gerando `reference/analysis/macho_import_bindings.json` com o endereço exato em memória guest de cada um dos 389 símbolos importados.
   - Decodificar as estruturas Objective-C 2.0 (`class_t` e `class_ro_t` em `__objc_classlist` / `__objc_data` / `__objc_const`), listando para cada uma das 30 classes (`AppDelegate`, `GLView`, `TouchResponder`, etc.) seus métodos de instância/classe, seletores, tipos e endereços de implementação ARM/Thumb (`IMP`), salvando em `reference/analysis/objc_classes_detailed.json`.
   - Inspecionar os cabeçalhos binários de:
     - `newres/sprites/*.pvr` (versão do header PVR, flags, formato de pixel PVRTC/RGBA, dimensões, mipmaps).
     - `newres/fonts/*.rff` (estrutura da tabela de glifos/métricas).
     - `dat/InGame_txt` e demais arquivos `dat/*`, `boss/*`, `comm/*`.
2. **`docs/RUNTIME_CONTRACT.md`**:
   - Documentar o mapa de memória guest proposto (incluindo stack, heap, área de traps de símbolos importados e área de objetos HLE Objective-C).
   - Documentar a sequência exata de boot: `__mod_init_func` -> `start` -> `UIApplicationMain` -> `AppDelegate` -> `GLView` -> `NSTimer`/frame tick.
   - Agrupar os 389 imports por subsistema e definir a política de implementação para cada grupo (`libc/libstdc++`, `Objective-C/Foundation/CoreFoundation`, `UIKit/QuartzCore/EAGL`, `OpenGLES 1.1`, `OpenAL/AudioToolbox/AVFoundation`, `SQLite3`, `zlib`, e neutralização offline de `EAMTX/DMG/SystemConfiguration`).
3. **Atualização de Status**:
   - Marcar `S02` como `done` em `docs/STATUS.json`, avançar `current_step` para `S03`, renderizar `docs/STATUS.md`, atualizar `docs/HANDOFF.md` e validar com `python tools/verify_workspace.py`.
