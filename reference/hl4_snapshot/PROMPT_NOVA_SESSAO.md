# Texto para iniciar a próxima sessão

Quero desenvolver o port nativo do Heroes Lore 4 usando o APK que está nesta
pasta, com Windows, Linux, Android e Nintendo Switch como destinos, seguindo as
melhorias e o padrão de qualidade do port de Heroes Lore 5.

Leia AGENTS.md, project.json, docs/STATUS.md, docs/HANDOFF.md, docs/DECISIONS.md,
docs/RESEARCH_LOG.md e docs/tasks/S09_TEXT_EXTRACTION.md. Verifique o workspace
e a integridade do APK com os scripts em tools/. Depois continue a próxima etapa
pendente, realizando a investigação e implementação, sem apenas propor um plano.

Já há um inventário do APK e uma cópia de referência do código/documentação do
HL5 em reference/hl5_snapshot/. Este HL4 usa um módulo ARM/WIPI e um JAR de
recursos; não é um JAR Java para recompilar e não segue o mesmo bootstrap JNI
do HL5. Confira os relatórios, mapeie os contratos e adapte seletivamente o que
for comprovadamente compatível. Não modifique o projeto HL5.

Mantenha etapas/status, decisões, aprendizados, resultados negativos e evidências
atualizados para que qualquer outra IA possa continuar. Use saves temporários,
preserve o APK e não declare funcionalidades ou plataformas validadas sem teste.
O objetivo inicial é um runtime de PC com boot, imagem, entrada e áudio reais;
as demais funções e plataformas seguem os gates do roadmap. Comunique descobertas
e limitações em português. Deixe o handoff pronto ao final de cada sessão.

Estado atualizado: primeira build Windows standalone em dist/HeroesLore4-Windows.
Leia docs/WINDOWS_RUNTIME.md e a evidência WINDOWS_RUNTIME_TESTS.json. S02–S04
concluídas; continuar ataque/habilidades/transições e save/load sem refazer o boot.
O proprietário pediu apresentação horizontal e DLLs inclusas: preservar ambos.

Prioridade mais recente: clique/toque corrigidos na build inputfix, investigar e
preparar inglês. Ler POINTER_INPUT.md e TEXT_LOCALIZATION.md. Catálogo offline
com 6.935 registros; continuar tradução/revisão e integração sem modificar scripts
ou canônico. S05/S06 continuam parciais. Inglês ainda não aparece na build.
