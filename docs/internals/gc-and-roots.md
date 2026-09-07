# GC e raízes

Ficheiros: `memory.cpp` (`gc_collect`, `gc_mark_value`, `gc_write_barrier`),
`vm.cpp` (`VM::gc_mark_roots`), `memory.h`.

## O que o colector vê

Mark & sweep incremental por cores (branco/cinza/preto). As raízes
(`VM::gc_mark_roots`) são exactamente duas famílias:

1. as globais (`globals_[]` e os seus nomes);
2. os fibers: o principal e o corrente. Marcar um fiber marca a sua stack
   em `[stack, stack_top)` e os frames (closures), e os upvalues abertos.

Tudo o resto é alcançado a partir daí: constantes das funções, campos de
instâncias, elementos de arrays/maps/sets, `closed` dos upvalues.

Consequências práticas:

- Um `Value` guardado só numa variável C++ **não é raiz**. Se o GC correr,
  desaparece. É por isso que um nativo corre por defeito com o GC pausado
  (`gc_pause`/`gc_resume` à volta de `call_native`).
- Registos acima de `stack_top` não são vistos. Um frame novo faz
  `stack_top = base + num_regs` antes de `clear_new_regs`; nunca deixar
  lixo em registos que o GC vai percorrer.
- O mapa de kwargs (flag 0x40) está num registo do caller: enraizado.

## Alocar

- `zen_alloc(gc, n)` pode disparar uma recolha; `zen_alloc_now` nunca (para
  objectos a meio de construção: `OP_CLOSURE`, `new_instance`).
- Em código da VM que constrói um objecto composto em vários passos
  (`OP_ADD` de listas, `set_binop`, comprehensions em nativos), o padrão é
  `gc_pause(&gc_); ... gc_resume(&gc_);`.
- `array_push` tem write barrier: guardar um objecto branco dentro de um
  preto volta a pô-lo na lista cinzenta. Qualquer estrutura nova que guarde
  `Value`s tem de chamar `gc_write_barrier` na escrita ou marcar-se como
  cinzenta.

## Strings partilhadas

`OBJ_FLAG_SHARED`: uma string que passou a viver noutro sítio (argumento de
chamada, campo, array) não pode ser mutada in-place por `s += x` (o fast
path de `OP_ADD` faz append in-place quando a string é exclusiva). As
chamadas marcam os argumentos string como partilhados antes de entrar.

## Modos de teste

- `./run_tests.sh --stress-gc`: recompila com `ZEN_DEBUG_STRESS_GC`
  (recolha em cada alocação) sobre o build Debug com ASan/UBSan. Qualquer
  raiz em falta aparece aqui como use-after-free.
- `tests/cpp/test_gc_native.cpp`: nativos `GC_SAFE`.

## Regras para código novo

1. Guardaste um `Value` novo num local C++ e vais alocar outra vez antes de
   o usar? Ou o GC está pausado, ou enraíza-o num registo/objecto já vivo.
2. Escreveste um `Value` para dentro de um objecto sem passar por
   `array_push`/`map_set`/`set_add`? Chama a barreira.
3. Alocaste um `ObjFiber`/frame? `stack_top` correcto e registos limpos.
