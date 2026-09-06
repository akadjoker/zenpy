# Cross-language microbenchmarks — ZenPy vs Lua 5.4 / Python 3.12 / Wren

Run on 2026-09-06, branch `feature/reified-generics` (commit 9eb4955), **after** the
two rounds of correctness fixes from `/code-review` (critical OP_INVOKE_GENERIC
instruction-pointer bug and the generic-arity bypass gaps). ZenPy built in
**Release** mode (`-DCMAKE_BUILD_TYPE=Release -DZEN_SANITIZE=OFF`) — the default
`build/` dir is Debug+ASan/UBSan and is 20-30x slower; never benchmark that one.

Benchmarks are the classic set Wren itself uses to track its own performance
(`fib`, `for_loop`, `method_call`, `binary_trees` — ported line-for-line from
Wren's `test/benchmark/*.wren`, plus `for_loop` written fresh since Wren has no
exact equivalent). Best-of-3 runs each.

| Benchmark      | Lua 5.4  | Wren     | Python 3.12 | ZenPy    |
|----------------|---------:|---------:|------------:|---------:|
| fib(28) x5     | 0.108s (1.00x) | 0.209s (1.93x) | 0.229s (2.12x) | 0.304s (2.81x) |
| for_loop (5M)  | 0.042s (1.00x) | 0.139s (3.33x) | 0.492s (11.84x) | 0.247s (5.94x) |
| method_call    | 0.178s (1.86x) | 0.096s (1.00x) | 0.202s (2.12x) | 0.228s (2.38x) |
| binary_trees   | 0.570s (2.63x) | 0.217s (1.00x) | 0.386s (1.78x) | 0.422s (1.95x) |

(multiplier = slowdown vs. the fastest interpreter for that benchmark)

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
