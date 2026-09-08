# Plan: selector-indexed dispatch for builtin methods

`OP_INVOKE` already resolves every method call to a compile-time integer
(`sel_slot`, via `VM::intern_selector`) and uses it for O(1) vtable dispatch on
user classes. Builtin receivers (string/array/map/set/buffer) get the same
`sel_slot` handed to them for free in the instruction word and then throw it
away — `invoke_*.inl` re-resolves the method by walking a linear chain of
`memcmp` comparisons instead. This plan replaces that chain with the same
`sel_slot`-indexed dispatch already proven for classes. Applies to **both**
zenpy and zenvm — same bug, same fix, two codebases.

## Evidence

`libzen/src/vm_dispatch.cpp`, `CASE(OP_INVOKE)`: receiver type is switched
once (cheap tag check), then each branch `#include`s an `.inl` file that
re-resolves the method name via `STR_METHOD("literal")` /
`ARR_METHOD("literal")` / etc. — `length == N && memcmp(...) == 0`, checked in
sequence until one matches. Calling the *last* method in a chain of N pays
N-1 failed `memcmp` calls on every single invocation.

Method counts per `invoke_*.inl` (each entry is one `if` in the chain):

| type   | zenpy | zenvm |
| ------ | ----: | ----: |
| string |    60 |    21 |
| set    |    25 |     7 |
| array  |    18 |    15 |
| map    |    14 |    10 |
| buffer |     7 |     3 |

zenvm's chains are shorter across the board — not because it dispatches
smarter (it runs the *identical* `memcmp`-chain code, confirmed byte-for-byte
against `invoke_string.inl`), just because it has fewer builtin methods.
Plausibly explains part of the zenpy/zenvm speed gap on string/set-heavy
code, not a structural difference between the two VMs.

## What already works (the pattern to extend)

`vm_dispatch.cpp`, `is_instance(receiver)` branch:

```cpp
Value mval = sel_slot < klass->vtable_size ? klass->vtable[sel_slot] : val_nil();
```

`sel_slot` comes from `VM::intern_selector(name, len)` — a global,
deduplicated, compile-time-resolved integer per method name (`vm.cpp`,
`find_selector`/`intern_selector`; called from `compiler.cpp:1975` and
`compiler_expressions.cpp:1882,2797`). It's already sitting in the bytecode's
second instruction word (`(selector_slot << 16) | name_ki`) for *every*
`OP_INVOKE`, regardless of receiver type. Builtins just never read it.

No collision risk: a user class and a builtin type can share the same
`sel_slot` for a name like `"len"` safely, because the receiver-type switch
in `OP_INVOKE` already routes to mutually exclusive branches
(`is_instance` vs `is_string` vs `is_array` ...) before `sel_slot` is used —
the class vtable and the builtin dispatch table are never consulted for the
same call.

## Design

At VM construction, capture the selector slot for every known builtin method
name once, as a `VM::BuiltinSelectors` member (`vm.h`) populated by
`VM::init_builtin_selectors()` (`vm.cpp`) via `intern_selector("len", 3)` etc.
— one call per method, per type.

**Revised from the original plan:** `switch (sel_slot) { case bsel_.str_len:
... }` does not compile — C++ requires `case` labels to be compile-time
constant expressions, and `bsel_.str_len` is a value resolved at VM
construction (a runtime object member), not a `constexpr`. Went with an
`if (sel_slot == bsel_.str_len) { ... } else if (...) { ... }` chain instead
— same `do { if (...) { ...; break; } ... } while (0)` skeleton the old
`STR_METHOD` code used, just testing `sel_slot == bsel_.field` instead of
`STR_METHOD("literal")`. Methods that shared one block via
`STR_METHOD("a") || STR_METHOD("b")` share it the same way via
`sel_slot == bsel_.a || sel_slot == bsel_.b`; internal re-checks used to pick
behavior within a shared block (`title`/`capitalize`/`swapcase`,
`index`/`rfind`/`rindex`, `union`/`intersection`/`difference`/...) became
`sel_slot ==` comparisons the same way, in place.

A true O(1) jump table (computed goto, matching what `OP_INVOKE`'s own opcode
dispatch already uses) is possible but needs a portable fallback for the
existing MSVC "switch dispatch" build mode (`ZEN_DISPATCH_MODE`,
`linux-switch-dispatch` in CI) — parked, see Results below for whether it's
worth the complexity.

`mname`/`mlen` stay available for error messages (`"'%s' has no method
'%s'"`) — only the resolution path changed, not the diagnostics.

## Rollout order (biggest win first)

1. **`invoke_string.inl`** — longest chain in both repos (60 / 21), string
   methods are the hottest of the builtin call sites in practice.
2. **`invoke_set.inl`** — zenpy's second-longest (25); zenvm's is short (7)
   but do it in the same pass since the mechanical change is identical.
3. **`invoke_array.inl`**, **`invoke_map.inl`** — same treatment.
4. **`invoke_buffer.inl`** — smallest chains (7 / 3), lowest priority, but
   trivial once the pattern is established — do it for consistency rather
   than leaving one outlier still doing `memcmp`.

Same order in zenvm after zenpy's is validated — don't parallelize the two
repos on the first (string) step in case the approach needs adjusting.

## Validation

- Microbenchmark per type, before/after, both repos: tight loop calling the
  *first* method in the old chain (best case, should be roughly flat) and the
  *last* method (worst case, should show the real win) — `tests/manual/microbench/`
  already has the harness pattern to extend.
- Full `run_tests.sh` / `tests/run_zen_tests.sh` suite — must stay green,
  this changes dispatch mechanics, not method semantics.
- zenpy: the `linux-fuzz` and `fuzz-embedding` CI jobs (ASan+UBSan,
  `ZEN_DEBUG_STRESS_GC`) already exercise the compiler+VM and the embedding
  API — a real regression net for this change, already wired, no new
  infrastructure needed. zenvm doesn't have an equivalent fuzz harness yet;
  worth porting before or alongside this work rather than after.

## Results (zenpy, 2026-09-08)

All five `invoke_*.inl` done (string, array, map, set, buffer) — see
`libzen/include/zen/vm.h` (`BuiltinSelectors`), `vm.cpp`
(`init_builtin_selectors`). Full test suite green (68 tests; the only 2
failures, `27_io.py`/`28_os.py`, are pre-existing sandbox file-permission
issues, unrelated). 30s ASan fuzz run on `fuzz_zen`: no crashes.

**Measured, not assumed — the win is real but uneven, not uniform:**
`STR_METHOD`'s own `length == N` check already fast-rejects most
non-matching candidates before ever calling `memcmp`, so a chain position
alone doesn't predict the cost — name-*length collisions* with earlier
entries do.

- `rpartition()` (last of 42 names in the old string chain, the "worst case"
  by position): **no measurable difference** — 0.664s vs 0.653s / 3M calls.
  Almost every candidate before it differs in length, so the old chain
  barely touched `memcmp` for this one.
- `islower()` — collides in length (7 bytes) with 9 other names earlier in
  the chain (`replace`, `char_at`, `byte_at`, `reverse`, `isalpha`,
  `isdigit`, `isalnum`, `isspace`, `isupper`): **~18% faster** — 0.221s vs
  0.180s / 5M calls. This is where the old chain actually paid for real
  `memcmp` calls.

Decision (asked, answered 2026-09-08): keep the if/else-chain approach as
committed, apply the same pattern to zenvm — the true jump-table path is not
worth its added complexity (computed-goto + MSVC switch-mode fallback) for a
gain this uneven. Revisit only if profiling on a real workload shows builtin
dispatch is still hot after this.

**zenvm: not started yet** — same plan, same design revision applies
(if/else-chain, not switch). Port `BuiltinSelectors` + the five
`invoke_*.inl` rewrites there next, same order (string → set → array → map →
buffer), then re-run the string microbenchmark to sanity-check the
measured-improvement pattern holds (zenvm's shorter chains mean fewer
absolute length collisions, so the win may be smaller in relative terms
too — worth checking rather than assuming it transfers).

## Out of scope for this pass

- `OP_INVOKE_GENERIC` and `OP_SUPER_INVOKE` — class-only paths, already use
  `sel_slot` correctly, untouched by this plan.
- Growing the global selector table itself (`intern_selector`) — already
  correct, not part of the problem.
