# Microbenchmarks (cost model)

Small loops that isolate one VM cost each. Run with the **Release** build:

```
Z=../../../build_release/bin/zen
for f in call_base call_f0 call_f3 call_fbig call_m_typed call_m_dyn typedparam selffield foreach forrange forrange_empty; do
  printf "%-16s " $f; $Z $f.py | tail -1
done
lua call_base.lua; lua call_f0.lua; lua call_m.lua     # Lua reference for the call cost
```

| File | Isolates |
|---|---|
| call_base | the bare `while i < n: i = i + 1` loop (subtract from the others) |
| call_f0 / call_f3 | call + return of an empty function, 0 and 3 arguments |
| call_fbig | same with a 33-register callee (register clearing cost) |
| call_m_typed / call_m_dyn | method call on a typed (INVOKE_VT) / dynamic (INVOKE) receiver |
| call_m_void | typed method call with no return value (INVOKE_VT_R + RETURNNIL) |
| typedparam | five field reads through an annotated parameter (GETFIELD_IDXC) |
| selffield | `x = self.a` into a local (GETFIELD_IDX straight into the local's register) |
| foreach | `for b in xs: b.m()` over 1000 objects (FOR_NEXT + INVOKE) |
| forrange / forrange_empty | numeric `for i in range(n)` with and without a body (FORPREP/FORLOOP) |

Per-opcode cycle profile: build with `-DZEN_OPCODE_PROFILE` (see
`build_prof/`) and run any script; the table is printed to stderr at exit.
rdtsc adds ~20 cycles to every dispatch, so compare opcodes with each other,
not with wall-clock.

Numbers on 2026-09-06 (best of 3): call_base 0.048s, call_f0 0.126s,
call_m_typed 0.153s, typedparam 0.075s, foreach 0.074s, forrange 0.086s,
forrange_empty (20M) 0.064s; Lua: call_base 0.030s, call_f0 0.080s,
call_m 0.166s.

Numbers on 2026-09-07 (best of 3, commit 7cf79a8): call_base 0.051s,
call_f0 0.114s, call_f3 0.128s, call_fbig 0.155s, call_m_typed 0.134s,
call_m_dyn 0.135s, typedparam 0.074s, selffield 0.126s (was 0.191s),
foreach 0.066s, forrange 0.058s, forrange_empty 0.065s; Lua: call_base
0.032s, call_f0 0.081s, call_m 0.170s. call+return ≈ 12.6 ns (Lua ≈ 9.8).
Later that day (ff92cc1): call_m_void 0.169 → 0.158s with RETURNNIL.
