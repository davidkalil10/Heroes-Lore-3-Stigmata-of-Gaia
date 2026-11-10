# Relação entre os ports anteriores e este projeto

O proprietário confirmou que **Heroes Lore 1, 2 e Zero partiram de jogos .jar**.
A documentação desses três projetos foi copiada para `reference/jar_ports_docs/`;
procedência, estado do checkout e hashes estão em `reference/JAR_PORTS_DOCS.json`.
São referências congeladas de outros jogos; instruções e status históricos não
descrevem o HL4 e não autorizam alterar os projetos de origem.

| Projeto | Input conhecido | Utilidade para HL4 |
| --- | --- | --- |
| HL1 / HL2 / Zero | Jogos .jar, conforme informado pelo proprietário | Interface, escala móvel, controles, save/cloud, updater e organização; confirmar compatibilidade antes de copiar código |
| HL5 | APK com núcleo ARM e bootstrap JNI | Referência principal para CPU/loader/HLE/mixer, desempenho e publicação multiplataforma |
| HL4 atual | APK LGT/WIPI, ELF ARM e JAR de recursos sem .class | Necessita contrato próprio WIPI/ARM, loader e bootstrap; pesquisa ainda pendente |

Ter um arquivo `.jar` dentro do APK não prova que o jogo seja Java. O inventário
do HL4 verificou ZIP válido com 1.935 entradas, nenhuma `.class`, recursos e um
`binary.mod` ELF. Não encaminhar este projeto automaticamente ao pipeline de
recompilação Java dos três primeiros. Os aprendizados de recursos do host continuam
valiosos e podem orientar etapas posteriores sem determinar a CPU/runtime.
