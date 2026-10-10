# GC e raízes

Ficheiros: `memory.cpp` (`gc_collect`, `gc_blacken`, `gc_sweep`,
`gc_mark_value`, `gc_write_barrier`), `vm.cpp` (`VM::gc_mark_roots`),
`vm_dispatch.cpp` (`ZEN_GC_SAFEPOINT`), `memory.h`.

## O que o colector vê

Mark & sweep por cores (branco/cinza/preto), stop-the-world: um ciclo corre de
uma ponta à outra dentro de `gc_collect`. As raízes (`VM::gc_mark_roots`) são:

1. as globais (`globals_[]` e os seus nomes);
2. os nomes dos selectors (`selectors_[]`): strings internadas que só a
   tabela de selectors referencia (os nomes dos métodos builtin, por
   exemplo); sem isto o sweep libertava-as e `find_selector()` lia memória
   libertada;
3. os fibers: o principal e o corrente. Marcar um fiber marca a sua stack,
   os frames (closures), os upvalues abertos, o `caller` e o `error`;
4. o hook opcional `gc->extra_mark(gc, ud)`, chamado a seguir às raízes da
   VM (ver "Para quem embebe", abaixo).

Tudo o resto é alcançado a partir daí: constantes das funções, campos de
instâncias, elementos de arrays/maps/sets, `closed` dos upvalues.

A stack de um fiber é varrida até `max(stack_top, topo da janela mais alta
de qualquer frame)`. Só `stack_top` não chega: um callee cuja janela começa
dentro da do caller e é mais pequena deixa os registos de cima do caller
acima de `stack_top` enquanto corre; esses registos ainda guardam `Value`s
(mortos), e se fossem saltados o objecto era libertado e, quando o callee
voltasse e `stack_top` subisse, o ciclo seguinte marcava um ponteiro
pendurado. O custo é manter vivos uns temporários mortos um pouco mais.

Consequências práticas:

- Um `Value` guardado só numa variável C++ **não é raiz**. Se o GC correr,
  desaparece. É por isso que um nativo corre por defeito com o GC pausado
  (`gc_pause`/`gc_resume` à volta de `call_native`).
- Todo o registo que o varrimento percorre tem de ter um `Value` válido. Um
  frame novo faz `stack_top = base + num_regs` antes de `clear_new_regs`; um
  fiber novo (`new_fiber` com closure) põe os seus registos a nil, porque a
  stack vem da arena por inicializar e um registo ainda por escrever seria
  lido como um `Value` qualquer.
- O mapa de kwargs (flag 0x40) está num registo do caller: enraizado.

## Cores e ciclo de época

`white_val`/`black_val` trocam de valor a cada colecção (`GC_GRAY` é fixo), em
vez de repintar os objectos:

- Um objecto novo nasce com `gc->black_val` (nunca com as constantes
  `GC_WHITE`/`GC_BLACK` à mão: nas épocas ímpares estão ao contrário).
- `gc_collect` embranquece só o **prefixo de recém-nascidos**: a lista
  `gc->objects` é prependida a cada alocação, por isso tudo o que está antes
  de `gc->first_old` nasceu depois do último ciclo e ainda traz o preto. Os
  sobreviventes antigos já estão brancos graças à troca do ciclo anterior.
- Marca a partir das raízes, processa os cinzentos, e o sweep liberta os
  brancos. **O sweep não repinta**: os sobreviventes ficam com `black_val`.
- No fim troca `white_val` com `black_val` (os sobreviventes passam a
  brancos para o ciclo seguinte, em O(1)) e `first_old = gc->objects`.
- `gc_write_barrier` compara com as cores da época actual, pelo mesmo motivo.

## Quando corre uma colecção

Não é só dentro da alocação de buffers (`zen_alloc`/`zen_realloc` com
`bytes_allocated > next_gc`). `alloc_obj` e `zen_alloc_now` nunca recolhem
(um objecto acabado de nascer ainda não está enraizado), por isso um programa
que só cria instâncias e strings nunca chegava a recolher. Há **safe points**
no dispatch (`ZEN_GC_SAFEPOINT`, só actua com `bytes_allocated > next_gc` e
`pause_depth == 0`):

- entrada de função (`ENTER_FRAME`, por onde passa toda a chamada a uma
  closure de script);
- arestas de retorno de ciclos: `OP_JMP` para trás, `OP_FOR_NEXT`,
  `OP_FORLOOP`;
- além disso, depois de `a + b` de listas (o resultado já está num registo).

Entre duas instruções todo o `Value` vivo está num registo, num frame ou
numa global, e é por isso que ali se pode recolher. **Nunca** se recolhe
enquanto um nativo corre: `pause_depth > 0` (e um `execute()` aninhado
lançado de dentro de um nativo herda a pausa). A excepção é o nativo
`ZEN_NATIVE_GC_SAFE`, que corre sem pausa e por isso pode ser interrompido
pela alocação de buffers que ele próprio fizer.

## Afinação (limiar mínimo e factor de crescimento)

Dois parâmetros no `GC` decidem *quando* corre o próximo ciclo (o *como* não
muda):

- `min_threshold` (por omissão **8 MB**, `kGCDefaultMinThreshold`): o heap
  mínimo que dispara uma colecção. A primeira corre quando `bytes_allocated`
  passa este valor.
- `grow_factor` (por omissão **2.0**, `kGCDefaultGrowFactor`, o "pause de 200 %"
  do Lua): depois de cada ciclo,
  `next_gc = max(bytes_allocated * grow_factor, min_threshold)`; e
  `next_gc` fica sempre acima de `bytes_allocated`.

Regra prática: um jogo com um live set de N MB faz uma colecção de
aproximadamente N*(factor-1) MB em N*(factor-1) MB de alocação, mas nunca com o
heap abaixo de `min_threshold`. Hospedeiros móveis ou com pouca RAM podem baixar
o mínimo (o piso é 64 KB, `kGCFloorThreshold`); quem tem muita RAM e quer menos
pausas pode subir o factor.

Medido em `tests/manual/cross_lang_bench/zen/binary_trees.py` (live set ~1 MB;
os valores antigos eram 256 KB / 2.0, o que dava um mark+sweep completo a cada
1-2 MB alocados):

| configuração | tempo | RSS |
|---|---|---|
| antiga (256 KB, 2.0) | 0.229-0.239 s | ~13 MB |
| nova por omissão (8 MB, 2.0) | 0.207-0.210 s | ~13 MB |
| 8 MB, 3.0 | 0.195-0.201 s | ~17 MB |
| GC pausado | 0.208 s | 166 MB |

(Melhor de várias execuções; RSS medido com `/usr/bin/time -v`. O custo do
colector era toda a diferença para a VM irmã neste benchmark.)

API:

- C++: `vm.set_gc_tuning(min_bytes, factor)` e
  `vm.gc_tuning(&min_out, &factor_out)`. Valores fora de gama são cortados, não
  rejeitados: mínimo < 64 KB passa a 64 KB, factor < 1.0 (ou NaN) passa a 1.0.
  Subir o mínimo acima do heap actual sobe `next_gc` logo (o novo valor vale
  já); baixá-lo não mexe num limiar pendente, só vale a partir do próximo
  ciclo. Com o GC pausado (`pause_depth > 0`, o caso de qualquer nativo) o que
  se actualiza é `pause_saved_next_gc`, nunca o `next_gc` vivo (que é o
  sentinela "nunca").
- Scripts: `gc_tune(min_bytes, factor)` e `gc_stats()`, que devolve um map
  `{"bytes", "next_gc", "objects", "collections", "min_threshold",
  "grow_factor"}` (`next_gc` é o limiar pendente, também dentro de um nativo).
  `mem_info()` continua a devolver o tuplo `(objects, bytes, next_gc)`.

`ZEN_DEBUG_STRESS_GC` ignora o limiar (recolhe em cada alocação/safe point).
Testes: `tests/81_gc_tuning.py` e o bloco "GC tuning" de
`tests/cpp/test_embedding.cpp`.

## Para quem embebe: Values fora do heap da VM

Como o GC já corre em qualquer entrada de função ou volta de ciclo, **um
`Value` que viva só em memória C++** (uma struct em `native_data`, um
contentor do host, uma variável estática) desaparece no primeiro
ponto de recolha que o script alcance enquanto o host o guarda: por exemplo
entre dois `vm.call_global(...)`, ou quando o host guarda um callback e o
script corre antes de ele o usar. A forma de primeira escolha é a
**referência do host**:

```cpp
int h = vm.ref(callback);          // raiz do GC gerida pelo C++
// ... qualquer quantidade de script, qualquer número de recolhas ...
Value cb = vm.deref(h);            // o mesmo objecto, ainda vivo
vm.call_fn(cb, args, n);           // aceita closures, nativos e métodos ligados
vm.unref(h);                       // liberta a raiz

zen::Ref cb(vm, v);                // o mesmo, em RAII (move-only, sem STL)
vm.call_fn(cb.get(), args, n);     // solta-se no destrutor / cb.reset()
```

`ref()` devolve um índice numa tabela com lista de livres (os slots libertados
são reutilizados; `ref_count()` conta os vivos, para testes e detecção de
fugas). Todos os slots vivos são marcados em `gc_mark_roots()`. Um `unref()`
de um handle já libertado (ou nunca emitido) é ignorado, não corrompe a lista
de livres, e `deref()` desse handle devolve `nil`. Uma `zen::Ref` tem de morrer
antes da `VM`; a mover-se (`Ref b(std::move(a))`) transfere o único handle e a
origem fica vazia. Um método ligado (`obj.metodo` lido como valor) guardado
assim mantém também a instância viva.

Alternativas, para quando uma referência por valor não serve:

- `vm.def_global(nome, v)` / `vm.set_global(...)`: fica vivo enquanto a
  global o referir. O mais simples para singletons e callbacks.
- Guardá-lo num objecto já alcançável (campo de uma instância, elemento de
  uma lista global): o GC segue-o dali.
- `vm.root(v)` num nativo `ZEN_NATIVE_GC_SAFE`: copia o valor para o
  `stack_top` do fiber corrente e vive até o nativo voltar (o VM repõe
  `stack_top`). É o único sítio onde faz algo útil: num nativo normal o GC já
  está pausado.
- O hook `vm.get_gc().extra_mark` / `extra_mark_ud`: o GC chama-o em cada
  ciclo, e o host marca aí tudo o que guarda com `gc_mark_value(gc, v)` /
  `gc_mark_obj(gc, o)`. É a forma de enraizar um contentor C++ inteiro que
  já existe (um `vector<Value>` do motor) sem o converter para refs.
- Instâncias de classes `persistent(true)` não entram na lista do GC, mas
  também não são raízes: não contes com elas para manter vivos `Value`s do
  script.

Contrato de `ZEN_NATIVE_GC_SAFE`: o nativo promete que tudo o que precisa de
manter vivo entre duas alocações está enraizado (`vm->root(v)`, ou guardado
num objecto já alcançável); os argumentos (`args[0..]`, na stack do fiber) e o
`self` (`args[-1]`) já o estão. Não pode guardar `Value`s em locais C++
através de uma alocação sem os ter enraizado. Marca só nativos que não
alocam objectos Zen (`draw(x, y)`, `width()`): é o caso em que o contrato é
trivial. O caminho `invoke` do C++ mantém sempre a pausa (a janela de
argumentos é um array C que o GC não vê).

## Alocar

- `zen_alloc(gc, n)` pode disparar uma recolha; `zen_alloc_now` nunca (para
  objectos a meio de construção: `OP_CLOSURE`, `new_instance`). `alloc_obj`
  também nunca recolhe: as colecções de programas que só criam objectos
  vêm dos safe points acima.
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
