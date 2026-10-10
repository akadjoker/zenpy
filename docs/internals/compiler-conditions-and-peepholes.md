# O compilador: condições e peepholes

Ficheiros: `compiler.cpp` (`condition`, `cond_false_jump`, `emit_cond_jump`,
`retarget_last_producer`), `compiler_expressions.cpp` (`comparison`,
`binary`, `dot_expr`, `argument_list`, `comprehension_into`),
`compiler_statements.cpp` (`if/while/for_statement`, `print_statement`).

O compilador é de uma passagem (parser de Pratt que emite à medida que lê).
As optimizações são todas locais: olham para a última instrução emitida e
substituem-na. Nenhuma é semântica: cada forma fundida tem o comportamento
exacto da sequência que substitui (incluindo `__lt__`/`__eq__` em
instâncias e strings).

## Condições de `if`/`elif`/`while`: `condition()`

Devolve o registo de uma condição simples, ou compila uma cadeia `and`/`or`
de topo como saltos:

- `a and b`: um salto-se-falso por operando, ambos para o `else`;
- `a or b`: os saltos-se-falso de `a` aterram em `b`; se `a` for verdadeiro,
  `JMP` para o corpo. Nunca se constrói o booleano nem se faz MOVE para
  juntar operandos.

`cond_false_jump(reg)` é o branch de uma condição simples:

1. se a última coisa foi uma comparação com literal pequeno ou `None`
   (`last_cmp_`), funde: `LTIJMPIFNOT` etc., `JMPIFNIL`, `JMPIFEQNIL`
   (este respeita `__eq__`); `x is not None` → `JMPIFNIL`;
2. se foi `NOT reg`: apaga o NOT e salta com o sentido invertido (`JMPIF`);
   se debaixo do NOT está um `EQ` no mesmo registo (é assim que `a != b`
   se emite), funde em `NEJMPIFNOT`;
3. se foi `LT`/`LE`/`EQ` entre registos: `LTJMPIFNOT`/`LEJMPIFNOT`/`EQJMPIFNOT`;
4. senão `JMPIFNOT reg`.

Guarda: `cmp_chain_end_`. Numa comparação encadeada (`a < b < c`) o salto de
curto-circuito aterra no fim do último compare esperando o booleano no
registo; nesse ponto a fusão é recusada. (Bug real encontrado pelo harness
CPython: `a != b != c` dava o resultado errado.)

`not` liga mais fraco do que comparações: `not a == b` é `not (a == b)`
(`unary()` analisa o operando a `PREC_COMPARISON`).

## Outros peepholes

- `x + 1`, `x -= 2` → `ADDI`/`SUBI` com imediato int8 (`binary()`,
  `named_variable()`), contrato completo do ADD (strings, `__add__`).
- `while i < N` com `N` literal: o LOADK sai do loop (`while_statement`).
- `retarget_last_producer()`: `x = <expr linear>` faz o último produtor
  escrever directamente em `x`, em vez de temp + MOVE. Só quando a
  instrução escreve A uma vez e não há saltos no RHS.
- `for i in range(...)` → `FORPREP`/`FORLOOP`; `for x in xs` → `FOR_NEXT` no
  fim do corpo (entrada por JMP); comprehensions usam o mesmo
  (`comprehension_into`, vários `for`, vários `if`, alvos em tuplo).
- `f(x for x in xs)`: o genexpr compila como a lista equivalente no sítio.
- Tipos estáticos (`p: P`, `x = P()`, `Array[T]`, campos anotados na classe
  ou inferidos dos construtores) só escolhem opcodes verificados:
  `GETFIELD_IDXC` (índice se o nome bater, senão por nome), `INVOKE_VT`.
- `self.x` é `GETFIELD_IDX` sem verificação: as classes são seladas após a
  declaração (`CLASSSEAL`), o índice é fixo.
- `print(a, b, sep=, end=)`: `OP_PRINT` com C=2 imprime só o valor.

## Onde não mexer sem ler antes

- `next_reg`: a pilha de temporários. Quem aloca um registo e depois usa
  `state_->next_reg = X` para descartar temporários tem de garantir que
  `X` não deixa um local vivo acima. O bug do `for x in [literal]` (o índice
  do FOR_NEXT tem de ser `iter_reg + 1`) veio daqui.
- `last_cmp_` e `cmp_chain_end_` são invalidados por qualquer emissão
  intermédia; se acrescentares uma instrução entre a comparação e o salto,
  a fusão desaparece silenciosamente (o código continua correcto).
- `argument_list()` decide entre kwargs posicionais (assinatura visível) e
  mapa 0x40; uma nova forma de chamada tem de respeitar `INVOKE_VT_FAST`
  (`!(nargs & 0xC0)`) e `patch_c_at` do multi-assign (que reescreve C).
