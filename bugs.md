# VM Bugs

## 1. Fiber errors are swallowed by `resume`

- Severity: high
- Location: `libzen/src/vm_dispatch.cpp`, `OP_RESUME`
- Problem: after `execute(target)`, the caller always resumes and consumes `target->transfer_value` without checking `had_error_` or `target->state == FIBER_ERROR`.
- Impact: a failing fiber can leave the caller running with stale or invalid state.
- Status: not reproduced yet with `await error("...")`; needs a narrower repro before changing code.

## 2. Fiber errors are swallowed by fiber iteration

- Severity: high
- Location: `libzen/src/vm_dispatch.cpp`, `OP_FOR_ITER`
- Problem: the fiber iteration path resumes the target fiber and continues execution without checking whether the target transitioned to `FIBER_ERROR`.
- Impact: generator or fiber failures can be silently ignored by the consumer loop.
- Status: fixed in branch `bugs/catch`.

## 3. `fiber->error` is never populated

- Severity: medium
- Location: `libzen/src/vm.cpp`, `runtime_error()`
- Problem: fibers carry an `error` field and GC marks it, but `runtime_error()` only flipped `FIBER_ERROR` and never stored an error object or message there.
- Impact: consumers such as `await` degraded to reporting `unknown` instead of the actual failure cause.
- Status: fixed in branch `bugs/catch`.

## 4. `had_error_` is sticky across VM entry points

- Severity: medium
- Location: `libzen/src/vm.cpp`
- Problem: public execution entry points like `run`, `call_global`, `call_fn`, `invoke`, and `resume_fiber` did not reset `had_error_` before starting a fresh execution.
- Impact: a previous runtime error could leak into later embedding calls and cause misleading control flow.
- Status: fixed in branch `bugs/catch`.

## 5. `runtime_error()` bypasses VM callbacks

- Severity: medium
- Location: `libzen/src/vm.cpp`, `runtime_error()`
- Problem: errors were printed directly to `stderr` instead of using the configured `print_err` callback from `ZenCallbacks`.
- Impact: embedded hosts could not reliably redirect or capture runtime diagnostics.
- Status: fixed in branch `bugs/catch`.

## 6. Division and modulo by zero did not raise runtime errors

- Severity: high
- Location: `libzen/src/vm_dispatch.cpp`, `OP_DIV`, `OP_DIV_OBJ`, `OP_MOD`, `OP_MOD_OBJ`
- Problem: `/` returned IEEE float results and `%` returned `0` or `NaN` instead of raising a runtime error on zero divisors.
- Impact: scripts could continue after invalid arithmetic instead of stopping with a deterministic runtime error.
- Status: fixed in branch `bugs/catch`.

## 7. Array out-of-bounds read returned `None` silently

- Severity: high
- Location: `libzen/src/vm_dispatch.cpp`, `OP_GETINDEX`
- Problem: `array_get()` returns `val_nil()` when the index is out of range, and `OP_GETINDEX` did not check bounds — both positive and negative OOB indices silently produced `None`.
- Impact: scripts could silently use a `None` from a bad index and continue running, hiding data access bugs.
- Note: `OP_SETINDEX` and `OP_ITER_ELEM` already had bounds checks; this was an inconsistency.
- Status: fixed in branch `bugs/catch`.

## 8. Bitshift by out-of-range amount was silently wrong

- Severity: low
- Location: `libzen/src/vm_dispatch.cpp`, `OP_SHL`, `OP_SHR`
- Problem: shift amount was masked with `& 63` before use, so `1 << 64` returned `1` and `1 << -1` returned `INT64_MIN` with no error.
- Impact: silently wrong results on bad shift amounts.
- Status: fixed in branch `bugs/catch` — now raises `shift amount out of range`.

## 9. Tuple unpack mismatch was silent

- Severity: medium
- Location: `libzen/src/compiler_statements.cpp`, multi-assign parsing
- Problem: `a, b = 1, 2, 3` silently discarded the extra value; `a, b, c = 1, 2` silently left `c` as `None`. Python raises `ValueError` in both cases.
- Impact: data bugs masked at compile time.
- Status: fixed in branch `bugs/catch` — now raises a compile error with expected/got counts.

## Open item

- Revisit `OP_RESUME` error propagation only after a minimal failing repro exists.em bugs.md 


# Zen VM Bug Hunting Guide

## Project Structure
- `libzen/src/vm_dispatch.cpp` — Main VM dispatch loop, ALL opcodes (~4100 lines)
- `libzen/src/vm.cpp` — VM setup, fiber management, invoke_operator, close_upvalues
- `libzen/src/memory.cpp` — GC, allocators, string ops, object constructors
- `libzen/include/zen/object.h` — Obj base struct, ObjString, all object types
- `libzen/include/zen/value.h` — Value union (VAL_INT, VAL_FLOAT, VAL_BOOL, VAL_OBJ, VAL_NIL)
- `libzen/include/zen/vm.h` — VM class, kMaxFrames=2000
- `libzen/src/bytecode.cpp` — Compiler output, closure creation
- `libzen/src/compiler.cpp` — Compiler (not audited yet)

## Build Commands
- **Debug (ASAN+UBSan):** `cd build && ninja zen` → `bin/zen`
- **Release (-O3):** `cd build_release && ninja zen` → `bin/zen`
- Both write to same `bin/zen` — last build wins!
- **Run all tests:** `for f in tests/*.py; do result=$(timeout 30 bin/zen "$f" 2>&1 | tail -1); echo "$f: $result"; done`

## VM Architecture (key concepts for finding bugs)

### Register-based VM with overlapping frames
- Each function has `fn->num_regs` registers allocated in a contiguous stack
- Call frames OVERLAP: `new_frame->base = &R[a + 1]` (normal call) or `&R[a]` (__init__)
- This means callee's register 0 = caller's register `a+1`
- Stack is flat: `fiber->stack[0..stack_capacity]`, frames are windows into it
- `fiber->stack_top` points to end of topmost frame's registers

### GC: Mark-sweep, arena allocator
- `zen_alloc()` triggers GC if threshold exceeded
- `zen_alloc_now()` does NOT trigger GC (used for small internal allocs)
- GC roots: fiber stack (all Values from stack to stack_top), globals, open upvalues
- **GC danger pattern:** Getting a raw `Obj*` pointer, then calling something that allocates (triggers GC), then using the pointer → use-after-free if object wasn't rooted

### Fibers (coroutines/generators)
- Main fiber: stack_capacity = kMaxFrames*16 = 32000 Values
- Generator fibers: stack_capacity = max(256, fn->num_regs), max_frames = kMaxFrames
- Fiber states: READY → RUNNING → SUSPENDED (yield) → RUNNING (resume) → DONE
- `fiber->transfer_value` passes values between yield/resume
- `fiber->yield_dest` = register index in suspended frame where resume value goes
- `fiber->caller` = fiber that resumed this one

### String system (already fully audited)
- ObjString: flexible array member `chars[]`, has `length`, `capacity`, `interned` flag
- `OBJ_FLAG_SHARED` on `obj.flags` — set when string escapes to another location
- `string_append_inplace` — O(1) amortized, only used for `s += x` (A==B in OP_ADD)
- `new_string_concat` — always allocates new string
- Interned strings: hash-consed, used for identifiers/constants, never modified

## What Was Already Audited & Fixed

### String safety (COMPLETE — 20 escape points, 60 tests, debug validator)
All paths where a string reference can escape to another location now mark OBJ_FLAG_SHARED.
See `/memories/repo/string-safety.md` for full list.

### Stack overflow protection (DONE)
- `CHECK_STACK_SPACE(fiber, base, num_regs)` macro added at ALL 7 call sites
- `invoke_operator` in vm.cpp also checks frame_count + data bounds
- Generator fibers now get 256 Values minimum (was fn->num_regs = ~16)

### Integer overflows (DONE for known paths)
- OP_MUL string repetition: uses int64 multiply + overflow check
- new_string_concat: int64 length check
- string_append_inplace: overflow check on new_len
- OP_FOR_ITER range: removed int32 truncation casts

## What Has NOT Been Audited Yet (next areas to check)

### 1. Compiler (HIGH PRIORITY)
- `libzen/src/compiler.cpp` — NOT audited at all
- Could emit invalid bytecode (wrong register indices, bad upvalue descriptors)
- Could have buffer overflows in constant tables, local variable tables
- Could have off-by-one in scope management

### 2. GC safety during allocation sequences
- Pattern to grep for: any code that does `as_string(X)` or `as_instance(X)` to get a raw pointer, then calls a function that may allocate (new_string, new_array, zen_alloc, etc.), then uses that raw pointer
- Especially dangerous in: OP_ADD (string coercion path), OP_FSTRING, OP_STRADD, native functions
- The `invoke_operator` path is somewhat risky: it writes to `fiber->stack_top` (which IS a GC root) before allocating, so it should be safe, but worth double-checking
- Search: `grep -n "as_string\|as_instance\|as_array\|as_map" vm_dispatch.cpp | head -80`

### 3. Native function safety
- Native functions receive `(VM *vm, Value *args, int nargs)` but many don't validate nargs or types
- Location: `modules/` directory — each module registers native functions
- Pattern: look for `args[N]` without checking `nargs > N` or type checks
- Also check: do native functions that store Values into containers mark them as shared?

### 4. Map/Set operations
- `libzen/src/memory.cpp` — map_set, map_get, set_add, set_has
- Hash collision handling: linear probing, could have subtle bugs
- Map growth: `realloc()` not checked for NULL return
- Map key deletion: tombstone handling — could leave stale entries
- Iterator invalidation: modifying map while iterating over it

### 5. Array operations
- `array_push`, `array_pop`, `array_insert`, `array_remove`
- Capacity growth: `grow_capacity()` could overflow int32
- Negative index handling in OP_GETINDEX, OP_SETINDEX
- Slice operations: mostly audited, looks OK after clamping

### 6. Error handling / error recovery
- When `runtime_error()` is called, does it clean up properly?
- Are open upvalues closed? Are fibers marked as error?
- Can a script catch errors and continue with corrupted state?
- Pattern: look for `had_error_` checks — are they always checked after calls that can error?

### 7. Import system
- `vm.cpp` — import_script_module, import_native_module
- Module fiber created with stack_capacity=256 — same issue as generators (now fixed)
- Module globals: are they properly shared-marked? (OP_SETGLOBAL marks them, but direct C API `def_global` might not)
- Circular import detection: is it robust?

### 8. Eval (OP_EVAL)
- Compiles and runs arbitrary code in a new frame
- The compiled function shares the same fiber/stack — could interact badly with caller state
- Closure captures from eval'd code: do upvalue descriptors point to valid locations?

### 9. Class system edge cases
- Multiple inheritance: NOT supported (single parent only), but what if user tries?
- Operator overloading: slots array bounds — `klass->operator_slots[slot]` with slot from 0..SLOT_OPERATOR_COUNT
- `__init__` with wrong return type: what if __init__ returns a value?
- Field access on nil/non-instance: type checks in OP_GETFIELD, OP_SETFIELD

### 10. Upvalue edge cases
- `close_upvalues()` — assumes `upval->location` is valid (could dangle if stack corrupted)
- Upvalue chains: `fiber->open_upvalues` is a sorted linked list, if sorting invariant breaks → infinite loop
- Cross-fiber upvalues: a closure captured in one fiber used in another — are upvalue locations still valid?

## Debugging Techniques

### ASAN (Address Sanitizer)
- Debug build has `-fsanitize=address,undefined` 
- Catches heap-buffer-overflow, use-after-free, stack-buffer-overflow, UB
- If ASAN fires, the stack trace shows EXACTLY where the bad access happened

### Debug validator (string safety only)
- `#ifndef NDEBUG` block in OP_ADD: exhaustive scan of ALL frames/globals/upvalues/fibers before in-place string mutation
- If it finds an alias, fires `assert(false)` with description
- Only active in debug build (not release)

### Reproduce-then-fix workflow
1. Write a .py test that triggers the bug
2. Run with debug build (ASAN catches memory issues)
3. Fix the C++ code
4. Run full test suite: `for f in tests/*.py; do ...`
5. Build release and verify benchmarks don't regress

### Useful grep patterns
```bash
# Find all places where raw Obj* pointers are obtained
grep -n "as_string\|as_instance\|as_array\|as_map\|as_closure\|as_fiber" vm_dispatch.cpp

# Find all allocation calls (potential GC triggers)
grep -n "new_string\|new_array\|new_map\|new_set\|zen_alloc\b\|alloc_obj" vm_dispatch.cpp

# Find all frame pushes (need stack bounds check)
grep -n "frame_count++" vm_dispatch.cpp vm.cpp

# Find all register writes without bounds check
grep -n "R\[.*\] =" vm_dispatch.cpp | head -50

# Find all native function registrations
grep -rn "def_native\|register_fn\|add_method" modules/
```

## Test Files (48 total, all pass)
- tests/01-19: Language features (print, functions, classes, generators, etc.)
- tests/20-23: Edge cases, scope, stress tests
- tests/24-30: Modules (buffers, structs, numpy, io, os, path, json)
- tests/31-37: Async/await, signals, net, http, match, records, enums
- tests/38-39: Operators, VM edge cases
- tests/40-42: GC torture (3 levels)
- tests/43-45: String safety (3 levels, 60 tests total)
- tests/basic.py, bench_*.py, fib.py: Benchmarks

## Performance Baselines (release -O3)
- bench_math: ~0.047s
- fib(20): ~0.072s  
- bench_full (list 500K): ~0.033s
- string 100K build: ~0.002-0.004s



Medições atuais, no mesmo core:

| Demo | Zen | Lua | Wren | Python |
|---|---:|---:|---:|---:|
| fib | 0,246s | 0,109s | 0,209s | 0,245s |
| for_loop | 0,199s | 0,042s | 0,143s | 0,489s |
| method_call | 0,207s | 0,185s | 0,095s | 0,191s |
| binary_trees | 0,360s | 0,515s | 0,206s | 0,367s |

`for_loop` é o caso mais claro. Como corre no topo do módulo, `sum` e `i` são globais. Cada volta faz:

```text
GETGLOBAL i, LOADK limite, LT, JMPIFNOT,
GETGLOBAL sum, GETGLOBAL i, ADD, SETGLOBAL sum,
GETGLOBAL i, ADDI, SETGLOBAL i, JMP
```

São 12 instruções por iteração. A mesma lógica dentro de `def loop()` caiu de 0,199s para 0,150s só porque passa a usar registos locais. Isto é um ganho geral, não um opcode de bunnymark.

O próximo trabalho deve ser:

1. Fechar o compilador de loops:
   - `i += 1` hoje gera `LOADI 1 + ADD`; devia emitir `ADDI`, como já faz `i = i + 1`.
   - `while i < limite` ainda gera `LT` e `JMPIFNOT` separados, apesar de já existir `OP_LTJMPIFNOT`.
   - constantes como `5000000` são carregadas em cada volta; num loop local podem ficar num registo antes do loop.
   - terminar a emissão do `for` numérico geral; `OP_FORPREP/OP_FORLOOP` já existe, mas o compilador ainda não o usa.

2. Fechar chamadas normais:
   - `fib()` ainda compila para `GETGLOBAL fib` + `CALL`.
   - `OP_CALLGLOBAL` já existe na VM, mas o compilador nunca o emite.
   - usar isso em toda chamada direta a função global beneficia fib, helpers, callbacks e jogos.

3. Inferência de tipo local para chamadas:
   - `toggle = Toggle(...)` já prova o tipo de `toggle`, mas o compilador esquece isso.
   - `activate()` devolve `self`, mas o compilador não propaga `Self` para `activate().value()`.
   - se propagar esses factos, usa os `INVOKE_VT_FAST` que já criámos, sem inventar outro caminho para o demo.

4. `binary_trees` já está bem melhor:
   - Zen está ao nível de Python e bate Lua neste caso.
   - o que falta é tipo de campos, por exemplo `left: Tree?`, para `self.left.check()` deixar de ser dispatch dinâmico.



### Estado (2026-09-06, branch perf/lua-hot-path-gc)

Feito, cada ponto num commit, suite 60/60 (3 testes novos: 56, 57, 58):

1. Loops: `i += 1` → ADDI e `while a < b` → LTJMPIFNOT já estavam; agora
   também `if`/`elif` fundem a comparação, a constante de `while i < 5000000`
   é carregada uma vez antes do loop, e `for i in range(...)` compila para
   FORPREP/FORLOOP (contagem fixa à entrada, 1 dispatch por volta).
2. Chamadas: `fib()` compila para CALLGLOBAL (a VM lê a global para R[A] e
   segue o caminho do OP_CALL, logo classes/bound methods/erros iguais).
3. Inferência: `toggle = Toggle(...)` regista a classe (local ou global de
   módulo); `activate()` que devolve self (`-> Self` ou todos os `return`
   são `return self`) propaga a classe em `activate().value()`; ambos os
   elos saem INVOKE_VT. INVOKE_VT passou a 2 palavras com fallback para
   OP_INVOKE, portanto nunca muda comportamento.
4. `local = expr` / `return expr` escrevem o resultado direto no registo
   destino (sem MOVE) quando o RHS é linear.

Medições (mesma janela, best of 3): fib 0,271→0,204s; for_loop (globais)
0,162→0,157s; for_loop com locais 0,104→0,061s (Lua 0,042s); method_call
0,238→0,189s; binary_trees 0,371→0,339s. Tabela completa em
tests/manual/cross_lang_bench/RESULTS.md.

Bugs pré-existentes corrigidos pelo caminho: OP_INVOKE não empacotava
`*args` em métodos variádicos com receptor dinâmico; `resolve_upvalue`
marcava `captured` no local com índice==registo, por isso uma variável de
loop capturada por closure nunca recebia OP_CLOSE (as lambdas viam lixo).

Bunnymark raylib (tests/manual/bunnymark_raylib, 4 linguagens, mesmo host,
mesma classe Bunny, 60 fps): Zen 70 400, Lua 63 000, Wren 54 600, Python
30 800 sprites.

Próximo: o que resta no method_call é custo por chamada (push de frame,
dois memsets de registos, LOAD_STATE, limpeza no RETURN) — ~49 ns por
call+return; `Toggle.value` já são só 2 instruções. Ponto 4 (tipo de campos
`left: Tree?`) continua por fazer.


### Ronda 2 (2026-09-06, commits 6c7b950..415a011)

Modelo de custo com microbenchmarks (função vazia, 3 args, 33 registos,
método tipado/dinâmico) + profiler por opcode (`-DZEN_OPCODE_PROFILE`, rdtsc
em cada DISPATCH; comparar opcodes entre si, não com o relógio). Conclusões:
INVOKE vs INVOKE_VT < 1 ns; o custo estava no call/return (~20 ns → ~16 ns
agora; Lua ~9 ns) e, em binary_trees, na construção `Tree(...)` (36% do
tempo: `intern_string("__init__")` + `map_get` em cada construção).

Feito:
- `__init__` resolvido pela vtable (slot do selector; `init_selector_` fica
  definido quando o nome é internado, também via bytecode carregado);
  `new_instance` sem pause/resume do GC.
- Fast path de chamada em OP_CALL/OP_INVOKE (um teste: aridade exacta, sem
  defaults/*args, não genérico, não generator); `clear_new_regs` em stores
  directos para frames pequenos; memset de saída do RETURN removido
  (registos foram vistos vivos pelo GC; ficam conservadoramente vivos até
  serem reutilizados, como em Lua); `call_global`/`call_fn` limpam o frame.
- Saltos fundidos: `x < 2`/`<=`/`>`/`>=` com literal int8
  (LTI/LEI/GTI/GEI JMPIFNOT, 2 palavras) e `x == None`/`!= None`/`is None`
  (JMPIFNIL/JMPIFNOTNIL identidade; JMPIFEQNIL/JMPIFNEQNIL respeitam
  `__eq__`). Bytecode 2.5.
- `for x in iterável` com OP_FOR_NEXT no fim do corpo (1 dispatch/iteração,
  arrays primeiro); local lido como operando esquerdo/objecto de acesso já
  não é copiado para `dest`; LOAD_STATE sem o ramo de closure nulo.

Medições finais do dia (best of 3, mesma janela): fib 0,271→0,161s (Lua
0,109); for_loop locais 0,104→0,061s (Lua 0,042); method_call 0,238→0,167s
(Lua 0,176); binary_trees 0,371→0,243s (Lua 0,547, Python 0,367).

Pré-existente, encontrado agora e NÃO corrigido: `v > 2` / `2 < v` com `v`
instância que define `__gt__` devolve False — o compilador troca operandos
(`LT 2, v`) e a VM despacha pelo slot `__lt__` sem reflectir para `__gt__`.
Igual antes e depois desta ronda (tests/59 não o fixa).

Próximos candidatos: GETFIELD verificado (2 palavras: idx + nome) para
receptores de classe estática que não a actual (parâmetros anotados
`e: Enemy`, locais inferidos de outra classe, elementos `Array[T]`) — hoje
só `self`/classe actual usam GETFIELD_IDX; o resto faz varrimento linear
por nome. E o custo restante por chamada (frame push + LOAD_STATE).

- Acesso a campos verificado (commit 15a3d81): `OP_GETFIELD_IDXC`/`OP_SETFIELD_IDXC`
  (palavra 1 = índice directo aceite só se `klass->field_names[idx]` for o
  nome pedido; palavra 2 = o GETFIELD/SETFIELD por nome original, executado
  caso contrário). Emitidos para qualquer receptor de classe estática que
  não `self`: parâmetros anotados, locais inferidos, elementos `Array[T]`,
  cadeias que devolvem self, globais de módulo. As anotações de parâmetros
  (`p: P`, `xs: Array[T]`) passam a ser registadas como tipo do local, logo
  `p.x` usa o índice verificado e `p.m()` a vtable. Só `self` mantém o
  GETFIELD_IDX sem verificação. Microbench `sum5(p: P)` com 5 campos:
  0,092→0,075s (−19%). Suite 63/63.


### Ronda 3 (2026-09-06, commit d042203)

- FIX `a > b` / `a >= b` em instâncias: o compilador troca para `LT b, a` e a
  VM usava o mesmo slot como reflexo, logo `__gt__`/`__ge__` nunca eram
  chamados (corria `__lt__`/`__le__` com operandos trocados). Slots novos
  `SLOT_GT`/`SLOT_GE` como reflexo de LT/LE (kOperatorSlotCount 16→18).
- FIX `obj.campo += x` em receptor que não `self` era ERRO DE COMPILAÇÃO
  ("Expected expression"). Agora compila (índice verificado se a classe é
  conhecida; ADDI/SUBI para literais pequenos; `self` ganha `//=` e `**=`).
- `if x == 0` / `while n != 0`: OP_EQIJMPIFNOT/OP_NEIJMPIFNOT (literal int8).
- Classe inferida por campo: `self.left = Tree(...)` (só construtores de uma
  classe, ou None) → `self.left.check()` é INVOKE_VT e `self.left.item` é
  GETFIELD_IDXC; formas verificadas, um palpite errado só custa velocidade.
- `ObjFiber::stack_end` para o CHECK_STACK_SPACE; OP_RETURN com fast path
  de um teste (1 valor, caller script, sem stop depth). call_f0 0,130→0,126s.
- Microbenchmarks guardados em tests/manual/microbench (README com números).

Estado final do dia (best of 3): fib 0,160s (Lua 0,109) · for_loop locais
0,061s (Lua 0,042) · method_call 0,166s (Lua 0,176) · binary_trees 0,243s
(Lua 0,547, Wren 0,206) · call+return ~15 ns (Lua ~9) · suite 64/64 + stress-GC.

### Plano para amanhã (por ordem)

1. **Validar o dia**: `cd build_release && ninja`, `ZEN=build_release/bin/zen
   ./run_tests.sh`, `./run_tests.sh --stress-gc`; correr os 4 benchmarks
   (`tests/manual/cross_lang_bench/zen/*.py`) e `tests/manual/microbench`
   para ter a baseline da máquina nesse dia (varia 8-40% entre sessões:
   comparar sempre intercalado, nunca com números antigos).

2. **Custo por chamada (~15 ns vs Lua ~9)** — o que sobra em fib/method_call:
   - guardar `constants` e `upvalues` no `CallFrame` ao empurrar o frame
     (`new_frame->constants = fn->constants; new_frame->upvalues =
     cl->upvalues`) e fazer `LOAD_STATE` ler só do frame (hoje: 2 loads
     dependentes `frame->func->constants` e `frame->closure->upvalues`).
     Sítios: todos os `new_frame->closure = cl` em vm_dispatch.cpp
     (OP_CALL fast+slow, class __init__, OP_INVOKE fast+slow, INVOKE_VT,
     INVOKE_VT_FAST, SUPER_INVOKE, CALL_GENERIC, INVOKE_GENERIC), vm.cpp
     (call_global, call_fn, run, fibers). Medir com call_f0/call_m_typed.
   - no fast path de OP_CALL/INVOKE, saltar o loop de "marcar strings
     partilhadas" quando `nargs == 0`.
   - juntar `frame_count >= kMaxFrames` + `CHECK_STACK_SPACE` num só teste
     se `frames` e `stack` forem dimensionados juntos.
   Objectivo realista: 15 → 12 ns.

3. **`INVOKE_VT` com receptor noutro registo** (`b.m()` com `b` local gera
   `MOVE base = b` + INVOKE): opcode `OP_INVOKE_VT_R` A=base, B=receptor,
   C=nargs, word2 = sel/nome — a VM copia o receptor para `base` ela
   própria: um dispatch a menos por chamada de método em locais (foreach,
   `self.left.check()` não, esse já é temp). Ver `dot_expr` ramo
   `obj_is_local` em compiler_expressions.cpp.

4. **Anotações de campos na classe** (`left: Tree = None`, `left: Tree?`):
   alimentar `class_field_class_` explicitamente (hoje só por inferência de
   construtores). Ver `class_field_literal`/CLASSFIELDDEF em
   compiler_statements.cpp (corpo da classe) — decidir sintaxe do `?`.

5. **Comprehensions** ainda usam FOR_ITER+JMP: passar para OP_FOR_NEXT
   (mesma transformação que for_statement; grep `emit_for_iter`).

6. **Globais de módulo em loops** (for_loop globais 0,156 vs Wren 0,143):
   só se sobrar tempo — GETGLOBAL/SETGLOBAL por acesso; hipótese barata:
   `while` no topo do módulo copiar globais lidas-e-escritas para registos
   não é seguro sem análise de chamadas; provavelmente deixar.

7. **Verificar `x is not None`**: confirmar como é analisado (pode estar a
   ler `x is (not None)`); se for bug, corrigir em comparison()/Pratt e
   ligar ao OP_JMPIFNIL.

8. **Docs**: docs/ (build_docs.py) — listar os opcodes novos (2.5): CALLGLOBAL,
   FORPREP/FORLOOP, INVOKE_VT 2 palavras, LTI/LEI/GTI/GEI/EQI/NEI JMPIFNOT,
   JMPIF*NIL, FOR_NEXT, GETFIELD_IDXC/SETFIELD_IDXC; e a semântica de
   "tipo estático é dica, nunca promessa".

9. **Wren**: recompilar `wren_cli` (fontes em
   /media/projectos/projects/languages/wren-0.4.0) para voltar a ter a
   coluna Wren nos microbenchmarks.

10. **Integração engine (Kinetix2D/Radion)**: o host do bunnymark
    (tests/manual/bunnymark_raylib/bunny_zen.cpp) é o modelo mínimo de
    embedding: `def_native` + `call_global`; padrão de script recomendado:
    `typed: Array[Bunny] = lista` + loop por índice, parâmetros anotados
    (`p: P`) para campos/métodos O(1).
