# Porque é que o zenpy é mais lento que o zenvm

O VM é o mesmo (o zenpy é um fork do zenvm). A pergunta era se a diferença
vinha do compilador ou do interpretador. Isto é o que ficou medido, e o que
ficou por explicar.

## O que está medido

Mesmo algoritmo, Release -O3, melhor de 3:

| teste | zen | zenpy | |
|---|---:|---:|---|
| ciclo vazio, 20M | 0.054 | 0.061 | +13% |
| soma acumulada, 5M (`s = s + i`) | 0.024 | 0.053 | **+120%** |
| quatro somas, 5M | 0.095 | 0.194 | **+104%** |
| método em receptor tipado, 3M | 0.088 | 0.100 | +14% |

O ciclo vazio e a chamada de método estão perto. O que dispara é a
aritmética: o `ADD` do zenpy custa cerca de 2x o do zenvm.

## O que NÃO é a causa (hipóteses testadas e descartadas)

1. **Não é o `for`/`range()`.** Os dois compilam para `FORPREP`/`FORLOOP`
   com bytecode equivalente. O `FORLOOP` do zenpy escreve três registos
   (contador, restante, variável visível) contra dois do zenvm, mas o ciclo
   vazio mostra que isso vale ~13%, não 120%.

2. **Não é a construção do `Value` no `FORLOOP`.** Trocar
   `R[a+3] = val_int(next)` por escrita campo a campo: 0.0527 → 0.0528.
   Nenhuma diferença.

3. **Não é a cache de instruções, apesar de o `execute` do zenpy ser o
   dobro do tamanho** (114 KB contra 56 KB; o `OP_ADD` sozinho tem 211
   linhas contra ~40, porque toda a semântica de Python está inline).
   Um build com PGO, que reordena o código pelo perfil real, deu
   0.0526 → 0.0533. Nenhuma melhoria. Esta era a minha hipótese principal
   e está errada.

4. **Não é o número de instruções executadas.** O profiler de opcodes
   confirma 10.000.023 dispatches para 5M iterações nos dois: exactamente
   2 por iteração (`ADD` + `FORLOOP`). Mesmo trabalho, tempo diferente.

## O que sobra

O custo é **por dispatch**, não por instrução executada nem por layout.
O profiler dá 20.2 ciclos ao `ADD` e 20.0 ao `FORLOOP` (com o overhead do
`rdtsc` incluído, que sozinho leva o teste de 0.053 para 0.096, por isso os
valores absolutos não servem — só a comparação entre opcodes).

Candidatos por investigar, por ordem do que eu tentaria:

- **Tamanho do `Value`.** Se o zenpy alargou o `Value` (mais um tag, campos
  extra), cada leitura e escrita de registo move mais bytes, e isso bate
  em todo o lado proporcionalmente ao número de acessos a registos. Isto
  explicaria porque a aritmética (3 acessos) sofre mais que o ciclo vazio.
  **Verificar `sizeof(Value)` nos dois.** É o primeiro teste a fazer.
- **Pressão de registos no `execute`.** Com o dobro dos handlers, o
  compilador pode não conseguir manter `ip`/`R`/`K` em registos da máquina
  em todo o corpo, e passar a recarregá-los. Ver o assembly do fast path
  do `ADD` nos dois.
- **`NUM_BINOP` relê `R[ZEN_B(i)]` e `R[ZEN_C(i)]`** depois de o `CASE` já
  os ter lido, com shadowing das variáveis. Provavelmente o compilador
  elimina, mas confirma-se no assembly.

## O que já foi feito ao zenvm com este conhecimento

O caminho inverso (trazer o compilador do zenpy para o zenvm) rendeu, no
branch `perf/compiler-codegen` do zenvm: fusão comparação+salto
(`LTJMPIFNOT`, que existia no VM e nunca era emitido), retarget da
aritmética para o registo destino (mata o `MOVE`), e imediatos
`ADDI`/`SUBI`. Ciclo `while` de 5M: 0.110 → 0.034.

Nada disso se aplica ao zenpy, que já tinha as três.
