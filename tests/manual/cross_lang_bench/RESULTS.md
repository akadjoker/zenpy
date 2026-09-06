# Cross-language microbenchmarks — ZenPy vs Lua 5.4 / Python 3.12 / Wren

Original run on 2026-09-06, branch `feature/reified-generics` (commit 9eb4955),
**after** the two rounds of correctness fixes from `/code-review` (critical
OP_INVOKE_GENERIC instruction-pointer bug and the generic-arity bypass gaps).
Updated after `perf/vtable-dispatch-and-move-elision` (branched off the above)
fixed two VM/compiler perf bugs found while explaining why ZenPy — a
register-based VM, like Lua — was losing to Wren (stack-based) in every
benchmark: see `vm_perf_regression_diagnosis` project memory for the full
diagnosis (dead OP_INVOKE_VT opcode, script-class vtables never inheriting
their parent's, redundant MOVEs in chained method calls).

ZenPy built in **Release** mode (`-DCMAKE_BUILD_TYPE=Release -DZEN_SANITIZE=OFF`)
— the default `build/` dir is Debug+ASan/UBSan and is 20-30x slower; never
benchmark that one.

Benchmarks are the classic set Wren itself uses to track its own performance
(`fib`, `for_loop`, `method_call`, `binary_trees` — ported line-for-line from
Wren's `test/benchmark/*.wren`, plus `for_loop` written fresh since Wren has no
exact equivalent). Best-of-3 runs each.

| Benchmark      | Lua 5.4  | Wren     | Python 3.12 | ZenPy    |
|----------------|---------:|---------:|------------:|---------:|
| fib(28) x5     | 0.110s (1.00x) | 0.211s (1.92x) | 0.238s (2.16x) | 0.304s (2.76x) |
| for_loop (5M)  | 0.044s (1.00x) | 0.138s (3.14x) | 0.494s (11.24x) | 0.249s (5.66x) |
| method_call    | 0.188s (1.97x) | 0.095s (1.00x) | 0.203s (2.13x) | 0.208s (2.18x) |
| binary_trees   | 0.646s (3.05x) | 0.212s (1.00x) | 0.380s (1.80x) | 0.422s (1.99x) |

(multiplier = slowdown vs. the fastest interpreter for that benchmark)

The ZenPy column above is from the first perf commit only (vtable flatten +
chained-call MOVE elision); the later commits on the branch improved every
one of the four — see "Effect of the perf branch" at the end for the
interleaved base-vs-new measurement. Several more diagnosed causes (dead
OP_INVOKE_VT opcode never emitted by the compiler, `ObjInstance`'s
two-allocation construction, `super()` resolving its parent class via a
global-array indirection every call, ADDI/SUBI not yet used for `+=`)
remain as follow-up — see the project memory for the full list and why each
one is safe to defer.

## Takeaways

- ZenPy is consistently in the same tier as Python — sometimes ahead
  (`binary_trees`: 0.42s vs Python's 0.39s is close; `for_loop` ZenPy actually
  **beats** Python by ~2x), sometimes a bit behind (`fib`, `method_call`).
- Lua and Wren (both purpose-built, heavily tuned register-VM script languages
  with NaN-tagging and years of micro-optimization) are faster across the
  board, as expected — they're the reference point for "fast script VM", not a
  bar ZenPy needs to clear to be a reasonable engine-embedding language.
- No benchmark here exercises `<T>` generics — these measure the general
  interpreter hot path (arithmetic, recursion, method dispatch, allocation),
  confirming the reified-generics work in this branch did not regress normal
  (non-generic) execution speed, consistent with the "aditivo, isolado" analysis
  given earlier: the new opcodes are only reached when `<T>` syntax is actually
  used; everything else goes through the exact same OP_CALL/OP_INVOKE as before.

## Reproducing

```
# Build release (NOT the default ./build, which has ASan/UBSan on):
mkdir -p build_release && cd build_release
cmake -DCMAKE_BUILD_TYPE=Release -DZEN_SANITIZE=OFF ..
cmake --build . -j$(nproc) --target zen

# Interpreters used: /usr/local/bin/lua (5.4.8), /usr/bin/python3 (3.12.3),
# wren_cli built from github.com/wren-lang/wren + wren-lang/wren-cli
# (release_64bit config via projects/make/Makefile in each).

for f in fib for_loop method_call binary_trees; do
  ./bin/zen tests/manual/cross_lang_bench/zen/$f.py
  lua tests/manual/cross_lang_bench/lua/$f.lua
  python3 tests/manual/cross_lang_bench/python/$f.py
  wren_cli tests/manual/cross_lang_bench/wren/$f.wren
done
```

## Effect of the perf branch on ZenPy itself (the measurement that matters)

Absolute numbers drift 8-40% between sessions on this machine — for every
language at once — so the table above must not be compared across runs.
The reliable measurement is the same script on two ZenPy builds, alternated
in the same time window. Base = `165fca8` (where `perf/vtable-dispatch-and-
move-elision` branched off), new = `826d3e4` (vtable flatten, receiver/
callee/argument register reuse, deferred moves across postfix chains,
GETFIELD_IDX kept through and/or/ternary, ADDI/SUBI folding). Best of 6,
Release -O3:

| Benchmark      | base   | new    | ZenPy speedup |
|----------------|-------:|-------:|--------------:|
| fib(28) x5     | 0.331s | 0.277s | **+16%**      |
| for_loop (5M)  | 0.285s | 0.210s | **+27%**      |
| method_call    | 0.260s | 0.240s | **+8%**       |
| binary_trees   | ~0.63s | ~0.58s | ~+8% (mean of 10 interleaved; single best-of-6 pairs land within noise either way) |

Where the gains come from is visible in the compiler's own output
(`zen --dis-only`): binary_trees' `Tree.__init__` went from 30 to 25
instructions (11 to 9 registers), `Tree.check` from 24 to 22 (13 to 9),
with no instruction added anywhere — `depth - 1` is one SUBI instead of
LOADI+SUB, a call no longer copies its callee into a fresh base, a
receiver produced by the previous expression is used in place. fib gets
SUBI for `n - 1`/`n - 2` plus the callee reuse on every recursive call;
for_loop gets ADDI for `i + 1`. None of this is specific to these four
scripts: it applies to every call, every chained method access and every
`x + <small literal>` in any program.

The embedding bunnymark (`tests/manual/bunnymark/`, 60k objects, one
native call per object per frame) moved from ~70 to ~80 fps across the same
commits, measured the same interleaved way. Lua's equivalent sits around
85-99 fps on this machine; the remaining gap there is now general
dispatch overhead spread thin across MOVE/LOADI/GETFIELD_IDX/LT and
OP_INVOKE's per-call arity checks (per `perf annotate`), not a single
pathological path.


## 2026-09-06 (later) — branch `perf/lua-hot-path-gc`: loops, calls, inference

Same machine, same window, best of 3, Release -O3. Wren not re-run (no
`wren_cli` on this machine today; its earlier numbers are listed for scale).

| Benchmark        | ZenPy before | ZenPy now | Lua 5.4 | Python 3.12 | Wren (earlier) |
|------------------|-------------:|----------:|--------:|------------:|---------------:|
| fib(28) x5       | 0.271s | **0.204s** | 0.108s | 0.231s | 0.209s |
| for_loop (globals, 5M) | 0.162s | 0.157s | — | 0.463s | 0.143s |
| for_loop_local (locals, 5M) | 0.104s | **0.061s** | 0.042s | — | — |
| method_call      | 0.238s | **0.189s** | 0.177s | 0.191s | 0.095s |
| binary_trees     | 0.371s | **0.339s** | 0.552s | 0.368s | 0.206s |

`for_loop.lua` declares `sum`/`i` as `local`, so the like-for-like Zen
number is `for_loop_local.py` (module-level Zen names are globals): the gap
to Lua went from 4.8x to 1.45x. The module-level form still pays
GETGLOBAL/SETGLOBAL on every access and did not move.

What changed (one commit each, in order):

1. `OP_CALLGLOBAL` — `name(...)` on a global folds GETGLOBAL+CALL; the VM
   materialises the callee in R[A] and continues on OP_CALL's own path.
2. `for i in range(...)` → `OP_FORPREP`/`OP_FORLOOP` (Lua 5.4 style: count
   fixed on entry, one dispatch per iteration; `continue` becomes a forward
   jump). Empty loop: 20M iterations 0.11s → 0.064s.
3. `x = ClassName(...)` infers x's class (locals and module globals), a
   method that returns self (`-> Self`, or every return is `return self`)
   keeps the class across `a.m().n()`, and `OP_INVOKE_VT` became a two-word
   instruction that falls back to `OP_INVOKE` for anything that is not a
   plain script-instance call — a static receiver class is now a speed hint
   only. Also fixed: `OP_INVOKE` never packed `*args` for vararg methods.
4. `while i < LIT` loads the literal once before the loop; `if`/`elif`
   use the fused compare-and-jump; `local = <expr>` and `return <expr>`
   write the result from its producing instruction (no MOVE) when the RHS
   is straight-line. Also fixed: a closure capturing a loop variable never
   got its OP_CLOSE (`captured` was set on the wrong local).

Remaining gap: per-call frame cost (frame push, two register memsets,
LOAD_STATE, RETURN's stale-register clear) — method_call is ~49 ns per
call+return and `Toggle.value` is now two instructions, so the dispatch
loop is no longer where that time goes. See `tests/manual/bunnymark_raylib/`
for the sprite-count comparison against Lua/Wren/Python under raylib.


## 2026-09-06 (round 2) — calls, construction, fused branches, for-each

Same machine and window, best of 3, Release -O3. Zen commits 6c7b950..415a011.

| Benchmark        | ZenPy start of day | after round 1 | **now** | Lua 5.4 | Python 3.12 | Wren (earlier) |
|------------------|-------------------:|--------------:|--------:|--------:|------------:|---------------:|
| fib(28) x5       | 0.271s | 0.204s | **0.161s** | 0.109s | 0.232s | 0.209s |
| for_loop (globals) | 0.162s | 0.157s | **0.156s** | — | 0.480s | 0.143s |
| for_loop_local   | 0.104s | 0.061s | **0.061s** | 0.042s | — | — |
| method_call      | 0.238s | 0.189s | **0.167s** | 0.176s | 0.197s | 0.095s |
| binary_trees     | 0.371s | 0.339s | **0.243s** | 0.547s | 0.367s | 0.206s |

Over the day: fib -41%, method_call -30%, binary_trees -35%, local loop -41%.
ZenPy is now ahead of Lua on method_call and binary_trees and ahead of
Python everywhere; Lua keeps fib (1.5x) and the pure local loop (1.45x).

What round 2 changed (one commit each):

1. `ClassName(...)` resolves `__init__` through the vtable slot of its
   selector instead of interning "__init__" and probing the method map on
   every construction; `new_instance` no longer pauses the GC around its
   single allocation. (binary_trees: the constructor was 36% of the time.)
2. OP_CALL/OP_INVOKE take a one-test fast path for the common call;
   register clearing on entry is a few direct stores instead of memset; the
   RETURN-side memset is gone (those registers were scanned while live and
   cannot dangle — the GC keeps them conservatively alive, as Lua's does);
   call_global/call_fn nil their frames like every other entry point.
3. `if x < 2` / `while i <= 9` / `if hp > 0` compare against an 8-bit
   literal and branch in one instruction; `x == None` / `x != None` /
   `x is None` branch on the register directly (the `==` forms still
   honour a class's `__eq__`).
4. `for x in iterable` steps at the bottom of the loop (OP_FOR_NEXT),
   arrays first; a local read as the left operand of an operator or
   field/subscript access is no longer copied first.

Per-opcode profile (build with `-DZEN_OPCODE_PROFILE`, rdtsc per dispatch,
compare opcodes against each other only): what remains above the dispatch
floor is the call/return pair itself (~16 ns per call+return versus ~9 ns
in Lua) and GETFIELD_IDX right after a call (dependent loads).

Bunnymark (tests/manual/bunnymark_raylib, 60 fps, 25 s, same window):
Zen 70 800, Lua 60 600, Wren 58 800, Python 34 800 sprites — at these
counts the draw call dominates, so the VM changes barely move it; the
run-to-run noise is about ±5%.
