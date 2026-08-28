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
