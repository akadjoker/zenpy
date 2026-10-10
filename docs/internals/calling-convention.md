# Convenção de chamada

Ficheiros: `vm_dispatch.cpp` (`op_call_shared`, `op_invoke_entry`,
`op_invoke_vt_entry`), `compiler_expressions.cpp` (`call_expr`, `dot_expr`,
`argument_list`), `opcodes.h`.

## Forma geral

Toda a chamada é `R[base] = callee`, `R[base+1..]` = argumentos contíguos.
Para métodos, `R[base]` é o receptor e `R[base+1..]` os argumentos; o callee
vê `self` em `R[0]`. O resultado fica em `R[base]`.

| opcode | forma | palavras |
|---|---|---|
| `CALL A B C` | `R[A] = R[A](B args)`, C resultados | 1 |
| `CALLGLOBAL A B C` + gidx | `R[A] = G[gidx]`, depois CALL | 2 |
| `INVOKE A B C` + (sel<<16 \| nome) | método por nome, qualquer receptor | 2 |
| `INVOKE_VT A B C` + idem | receptor com classe conhecida: slot da vtable primeiro | 2 |
| `INVOKE_R` / `INVOKE_VT_R` | idem, mas o receptor está em `R[C]` e a VM copia-o para `R[A]` | 2 |
| `INVOKE_VT_FAST A B sel` | `Array[T][i].m()`: totalmente estático | 1 |
| `SUPER_INVOKE` | vtable do pai | 3 |
| `CALL_GENERIC` / `INVOKE_GENERIC` | `f<T>(x)`: args de tipo antes dos de valor | 2 / 3 |

## O contador de argumentos tem flags

`B` (nargs) usa 6 bits para a contagem e dois flags:

- `0x80`: o último argumento é uma lista a espalhar (`f(a, *xs)`). A VM
  desempacota no sítio antes de qualquer outra decisão.
- `0x40`: o último argumento é um **mapa de keyword arguments** para um callee
  sem assinatura visível (nativo, ou um valor numa variável). A VM tira-o da
  contagem, guarda-o em `vm->kwargs_` só durante a chamada nativa, e recusa-o
  com erro claro se o callee for uma closure de script.

Kwargs para funções de script com assinatura visível não chegam à VM: o
compilador (`argument_list`) reordena-os para as posições certas e preenche
os que faltam com os defaults declarados.

## Fast paths e fallback

`OP_CALL`: se o callee é closure com `arity == nargs` e sem defaults, *args,
generics ou generator, empurra o frame sem mais testes (um só teste
combinado). Caso contrário o caminho geral trata de defaults, varargs,
generators, classes (`Cls(...)` cria a instância e chama `__init__` pela
vtable) e nativos.

`INVOKE_VT`: lê `klass->vtable[slot]`; se é uma closure com a aridade certa,
frame directo; se é um nativo, chama-o directamente (`ClassBuilder`); tudo o
resto faz `goto op_invoke_entry`, o INVOKE por nome. É isto que faz do tipo
estático **uma dica e nunca uma promessa**: uma anotação errada custa
velocidade, nunca muda o comportamento.

`INVOKE` por nome testa o receptor por tipo: instância primeiro (o caso
comum em jogos), depois array/string/map/set/buffer (as tabelas em
`invoke_*.inl`, que despacham pelo nome do método).

## Onde o compilador poupa MOVEs

- receptor local (`b.m()`, `self.m()`): `INVOKE_R` copia-o para a base, sem
  MOVE separado (`dot_expr`, `receiver_in_c`);
- um local seguido de `.`/`[`/`(` é lido no sítio (`named_variable`,
  `chain_continues()`), para `n = self.x` ser um `GETFIELD_IDX` e não
  MOVE + GETFIELD por nome;
- `a[i] = v` com `a` e `i` locais não os copia antes do RHS (Python avalia
  o RHS primeiro de qualquer forma);
- o resultado de uma chamada cujo destino é um local ainda leva um MOVE:
  `base` tem de ter os argumentos por cima, e um local não tem.
