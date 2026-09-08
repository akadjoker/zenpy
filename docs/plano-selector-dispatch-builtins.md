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

At VM init, capture the selector slot for every known builtin method name
once, as a named constant:

```cpp
static const int SEL_STR_LEN = vm.intern_selector("len", 3);
static const int SEL_STR_SUB = vm.intern_selector("sub", 3);
// ... one per method, per type
```

Replace each `invoke_*.inl`'s `if (STR_METHOD("...")) { ... } if (...) { ... }`
chain with `switch (sel_slot) { case SEL_STR_LEN: ...; case SEL_STR_SUB: ...;
default: goto not_found; }`. A switch this dense compiles to a jump table on
every compiler we target — no hand-rolled array, no bookkeeping for the
global selector table's growth (that's `intern_selector`'s problem already,
solved, untouched by this change).

`mname`/`mlen` stay available for error messages (`"'%s' has no method
'%s'"`) — only the resolution path changes, not the diagnostics.

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

## Out of scope for this pass

- `OP_INVOKE_GENERIC` and `OP_SUPER_INVOKE` — class-only paths, already use
  `sel_slot` correctly, untouched by this plan.
- Growing the global selector table itself (`intern_selector`) — already
  correct, not part of the problem.
