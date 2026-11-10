# Etapas e critérios de aceite

Status oficial: `STATUS.json`, visualização: `STATUS.md`. Não saltar um gate de
runtime só porque algum módulo do HL5 já oferece a funcionalidade desejada.

| Etapa | Entrega | Critério para DONE |
| --- | --- | --- |
| S00 | Preparação/continuidade | Estrutura, instruções, snapshot rastreável e verificador funcionando |
| S01 | Inventário estático | Hash, ZIP/JAR, manifesto, DEX, ELF/imports/exports e extração reproduzíveis |
| S02 | Contrato WIPI/ARM | Documentar loader/ABI/entry/eventos/recursos e priorizar imports com evidência |
| S03 | CPU e loader no PC | Carregar ELF/relocalizar/inicializar BSS; testes ARM/Thumb/ABI e imports conhecidos |
| S04 | Boot e primeiro quadro | Inicialização real até logo/título, framebuffer não vazio e loop responsivo |
| S05 | Entrada e gameplay | Novo jogo, movimento, ataque, menus e retorno; teclado e controle mapeados |
| S06 | Áudio e persistência | BGM/SFX audíveis; salvar/carregar/fechar/reabrir com dados descartáveis |
| S07 | Desempenho de base | Perfil por fase, baseline reprodutível e acelerações equivalentes ao original |
| S08 | Viewport expandido | Auto ocupa a área útil nas quatro plataformas com mais cenário real, câmera/culling/HUD/toque corretos, sem esticar/cortar; critérios em tasks/S08_VIEWPORT_EXPANSION.md |
| S09 | Texto e extração | Inventariar codecs/encodings/format tokens; roundtrip sem mudar dados de gameplay |
| S10 | EN/PT-BR em tempo real | Tradução completa, PT-BR padrão, troca imediata, acentos, layout sem sobreposição |
| S11 | Opções do port | Acesso explícito nas opções originais, F10/R3, toque legível e navegação sem travas |
| S12 | Builds multiplataforma | PC/Linux/Android/Switch compilam, pacote independente e IDs próprios definidos |
| S13 | Atualizador e cloud save | GitHub/Google configurados para HL4; progresso, rollback e restore para título |
| S14 | Validação e primeira release | Matriz de dispositivos, assinatura/versionamento, documentação e pacotes revistos |

S07 começa com medições desde S04, mas só é concluída com gameplay relevante.
S12 pode receber infraestrutura antes de S11; não concluir sem executar ao menos
o smoke test viável e registrar separadamente as pendências em hardware.
S09 pode ocorrer em paralelo ao gameplay. Tradução não deve atrasar a confirmação
de que o runtime executa o binário corretamente.

Para cada etapa: objetivo, entradas, mudanças, testes, evidências, riscos,
próxima ação. Se faltar ferramenta/APK/credencial/hardware, registrar precisamente
a dependência; continuar trabalho independente. Mudança de estratégia exige ADR.
