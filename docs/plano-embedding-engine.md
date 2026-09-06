# Plan: zenpy as an engine VM

What Kinetix2D (the 2D engine + editor that embeds zen) needs from the VM, ordered by
what it costs the engine today. Every item cites where the cost shows up in Kinetix2D
(`scripting/src/ZenScriptComponent.cpp` unless stated), sketches the API, and says what
the engine deletes once it lands. Numbers are measured, not estimated.

Scale of the embedding, for sizing: one 7527-line binding file, 512 natives, 51 classes
(`def_class`), 96 globals (`def_native`), 71 `valueToCString` calls, 500-600 line scripts,
scenes of ~400 objects with 20-30 live script instances per frame.

## State of the tree (2026-09-05)

Kinetix2D vendors `c61c198`, which is the head of `main`. Nothing to bump. What the
engine needs and the VM already has is a matter of **adoption**, not implementation:

| Already in `main` | Used by Kinetix2D? | Used by Radion? |
| --- | --- | --- |
| `zen_script_info.h` — `find_script_class`, `script_class_properties`, `check_script_contract` (`c61c198`) | no — own 318-line source scanner | no — own scanner (`ScriptProperty.cpp`) |
| `NativeStructBuilder` — zero-copy typed C++ structs (`f32/byte/ptr`, `read_only`) | no | not checked |
| `ClassBuilder::ctor/dtor` — VM-owned native lifetime | no (right call: engine owns objects) | — |
| `zen_host_output.h` | yes | — |
| `ZEN_NATIVE_GC_SAFE` | no (all natives run with GC paused; fine) | — |

Pending on branches, not in `main`: `vm-fiber-error-and-len` (compile-time keyword
arguments `ceabb79`, fiber error propagation, `len()` hook), `gc-native-roots`,
`perf-strings`. In the vendored VM `f(name=value)` still silently mis-binds; the fix is
a merge, not a bump.

## P0 — adopt what exists

### P0.1 Replace the property scanner with `zen_script_info`

**Evidence.** The Inspector shows exported script properties through
`ScanZenScriptProperties` (`ZenScriptProperties.cpp`, 318 lines of text parsing:
class-body `name = literal`, `self.x = literal` inside `__init__`, module constants).
`script_class_properties()` reads the same facts off `ObjClass::field_defaults` with
the Zen type preserved (int stays int, which picks the widget). Every script in the
tokenkay project is class-body style with literal defaults (0 `__init__`, 0 non-literal
class defaults), so coverage is complete for the shipping content. Radion's scanner
reads `__init__` self-assignments, which `script_class_properties` does not cover — see
P2.2 for the VM side.

**Plus what the scanner never gave:** `check_script_contract` with the engine's hook
table reports at load time `on_updte` (SUSPECT_NAME), `on_update(self)` without `dt`
(WRONG_ARITY — today it errors on the first frame, every frame), and a hook the host
never calls (UNCALLED_HOOK). `find_script_class` replaces the engine's own "find the
class that inherits ScriptComponent" walk and reports zero/multiple classes.

**Engine deletes.** The scanner, minus an optional `__init__` fallback until P2.2.

### P0.2 Merge `vm-fiber-error-and-len`

Keyword arguments cost the Kinetix2D user ~2 hours once (`set_pose(..., moving=True)`
read `False` in the callee, looked like an input bug). Fiber error propagation overlaps
P1.3. Whatever is blocking the merge is cheaper than the next silent mis-bind.

### P0.3 `NativeStructBuilder` for value types crossing the boundary

Kinetix2D passes `Vec2` as two returns (`x, y = node.get_position()`), which is fine and
allocation-free. Where it stops scaling is records: a collision hands the script
`(other, began)` and drops point, normal and sensor; a ray hit is `hit, x, y`. A native
struct (`CollisionInfo { f32 px, py, nx, ny; byte began, sensor; }`) is the zero-copy
answer the VM already ships. Measure before switching `Vec2`; adopt for records.

## P1 — correctness

### P1.1 Host-side handle invalidation

**Evidence.** `ObjInstance::native_data` is a raw `void*`. When the engine deletes a
`GameObject`, any handle a script still holds keeps pointing at freed memory. ASan caught
it in Play: allocated in `Scene::createObject`, freed in `Scene::flushDisposed`, read in
`natNodeIsActiveInHierarchy` one frame later. The engine's workaround is a
`ct::HashMap<void*, CachedInstance>` + `forgetInstance(ptr)` from the object destructor,
and the actual fix was `native_data = nullptr` inside `forgetInstance` — a patch, because
the VM has no notion of "the owner is gone". The older zenengine host (zenpy_projects)
had the same hole and never patched it.

**API.** Smallest change that removes the class of bug:

```cpp
// object.h
void zen_instance_detach(Value v);   // native_data = nullptr, fields untouched, GC state untouched
```

`destroy_instance` is not a substitute: it frees `fields`, so a script still holding the
handle would read a half-destroyed instance. Detach leaves the handle valid-but-empty and
every `zen_instance_data<T>()` call returns `nullptr`, which the 512 natives already check.

Optional stronger form, if a later design wants handles without a host-side map:

```cpp
struct ObjInstance { ...; void* native_data; uint32_t native_generation; };
template <class T> T* zen_instance_data(Value v, uint32_t expected_generation);
```

One integer compare; the host bumps the generation in its destructor. This is the same
model the Light VM plan adopts (`PersistentId {index, generation}`).

**Engine deletes.** `forgetInstance`'s pointer-clearing branch; eventually the whole
handle cache if generations land.

**Acceptance.** A script that stores `self.other = node.find("X")`, has `X` destroyed,
then calls `self.other.get_name()` gets `""`/nil and no ASan report, with
`HashMap` lookups removed from the engine.

### P1.2 `try_invoke` / cheap method presence

**Evidence.** Kinetix's `node.call("take_damage", v)` fans out to every script component
on the node. `VM::invoke` on a method the class does not define is a runtime error
(`vtable slot %d is nil`), so every script needs empty stubs for its neighbours' messages
(`rojas.py` grew 5, `thought_bubble.py` grew 2). The engine already has the check
(`ZenScriptComponent::hasFunction` via `find_selector` + vtable) but `callFunction` cannot
use it cheaply on the hot path. The old zenengine host cached a bitmask of present hooks
per instance (`HAS_UPDATE`...) and skipped the invoke — the right pattern, hand-rolled.

**API.**

```cpp
bool VM::has_method(Value instance, int selector) const;      // vtable slot non-nil
bool VM::try_invoke(Value instance, int selector, Value* args, int nargs, Value* out);  // false if absent, no error
```

**Engine deletes.** All message stubs; `callFunction` becomes `try_invoke`.

### P1.3 Errors scoped to the call, VM stays usable

**Evidence.** `had_error` is VM-wide and sticky (`bugs.md` #4). The editor reacts by
pausing the entire Play session (`EditorApplication.cpp`, `mScriptErrorPaused`, toast
"Script error - Play paused"). One buggy enemy stops the game.

**API.** Every entry point (`invoke`, `call_global`, `run`) returns a status and, on
failure, an `Error` object with message + stack; `had_error` is cleared on entry. This is
the direction of `vm-fiber-error-and-len`; the engine-facing requirement is that after an
error in instance A, `invoke` on instance B works without a `reset`.

**Engine deletes.** Global pause; replaces it with per-component quarantine.

### P1.4 GC pauses are unbounded and land anywhere in the frame

**Evidence.** `VM::invoke(slot)` does not pause the GC; collection triggers inside the
allocator whenever `bytes_allocated > next_gc && pause_depth == 0`, i.e. in the middle of
whichever `on_update` happens to allocate the byte that crosses the threshold. `gc_collect`
is a full stop-the-world mark and sweep. The engine cannot bound the pause or choose the
point in the frame.

**API, two layers.** Engine-side today: `gc_pause()` at frame start, `gc_resume()` plus an
explicit `gc_collect()` at frame end (`pause_saved_next_gc` already exists for this), so
the pause is at least deterministic and measurable. VM-side later: `gc_step(budget_bytes)`
incremental (the Light plan's model). Add a `gc.last_pause_us` counter either way.

## P2 — binding ergonomics

### P2.1 Host slot on `ObjClass` for O(1) class -> binding

**Evidence.** `get_component<T>()` lowers to `get_component(T)` with the class as the
argument. The binding reads `requested->name->chars` and `strcmp`s through 48 entries x 3
aliases. Measured (Release, 2M calls): **250.8 ns** worst case. Scripts survive by
caching handles in `on_start`.

**API.**

```cpp
struct ObjClass { ...; void* host_data; uint32_t host_id; };
ClassBuilder& ClassBuilder::host(void* data, uint32_t id);
```

**Engine deletes.** `findComponentBinding` string walk; the alias table becomes an array
indexed by `host_id`.

### P2.2 Extend `script_class_properties` to `__init__` literals

**Evidence.** `zen_script_info` reads class-body defaults only. Radion's scripts (and older
Kinetix ones) set properties as `self.speed = 160.0` inside `__init__`, which is why both
engines kept a scanner. The compiler sees those assignments; recording `self.<name> =
<literal>` in `__init__` into `field_defaults` (first assignment wins, `_` prefix skipped)
lets both engines drop their scanners entirely.

**API.** No new API — `script_class_properties()` returns them. One flag on
`ScriptPropertyInfo` saying where the default came from, for the Inspector's tooltip.

### P2.3 Typed binder header

**Evidence.** Exposing six joint classes took 610 lines, 118 natives, each repeating the
same eight lines: cast self, null-check, `nargs` check, `(float)zen::to_number(args[i])`,
write results to `args[0..n]`, `return n`. 512 natives total in the engine.

**API.** A header-only `zen/bind.hpp` on top of the existing `def_class().method()`:

```cpp
zen::bind::method(cls, "set_length", &DistanceJoint2D::setLength);              // (float) -> void
zen::bind::method(cls, "get_anchor_a", &Joint2D::anchorA);                      // () -> Vec2 => 2 returns
zen::bind::method(cls, "set_motor", &RevoluteJoint2D::setMotor);                // (bool,float,float)
```

Arg unpacking by parameter type, `self` via `zen_instance_data<T>` with the null path
returning nil results, multi-return for `Vec2`-like types through a user trait. No
`std::function`; plain function templates over member pointers. Optional sugar, but a
canonical version in zen keeps every embedder consistent.

**Engine deletes.** ~70% of the 7527 lines, estimated from the joint commit.

### P2.4 String views at the boundary

**Evidence.** `valueToCString(vm, v, small, sizeof small)` is called 71 times with 16, 32
or 128-byte local buffers; small strings are copied and **silently truncated** to the
buffer. An asset path over 128 bytes in `spawn()` or `set_texture()` fails with no message.

**API.**

```cpp
struct StringView { const char* bytes; int length; };
StringView zen_string_view(Value v);    // works for small strings and ObjString, no copy
```

### P2.5 Modules for engine natives

**Evidence.** 96 flat globals (`draw_rect`, `audio_play_at`, `nav_point_free`,
`set_number` ...). `register_lib` exists and the stdlib uses it; the engine chose globals.
The old zenengine did the same (97+ globals per its API reference).

**Plan.** Document the pattern and provide `NativeLib` builders so an engine registers
`draw`, `audio`, `nav`, `blackboard` as importable modules. Enables P4.2 and removes
name collisions with game code.

### P2.6 Fix the README

`def_native("greet", 1, lambda)` and `ClassBuilder(vm, "Vec2")....build()` in the README
do not match `vm.h` (`def_native(name, fn, arity)`, `method(name, fn, arity)`, `.end()`).
An embedder copying the README gets a compile error on line one.

## P3 — performance

### P3.1 Do not mark persistent instances

**Evidence.** `markRuntimeRoots` (engine, `ZenRuntime.cpp`) walks the whole handle map
each collection calling `gc_mark_value` on instances that are `persistent` and therefore
**not in the GC object list**. Pure overhead per handle per collection.

**Plan.** `gc_mark_value` early-outs on `persistent` instances (still marks their
`fields`), and the docs say so.

### P3.2 Byte access without allocation

**Evidence.** `text[i]` on a string does `new_string(&s->chars[idx], 1)`: one allocation
per character. Word-wrapping a 30-character line allocates 30 strings.

**API.** `s.byte(i) -> int`, `for b in s.bytes()` yielding ints, `ord()`/`chr()` if not
present. Keep `s[i]` returning a string for Python compatibility.

### P3.3 `random.choice` / `random.int`

**Evidence.** Picking a random list element in script is
`lines[int(math.random(0.0, len(lines) - 0.001))]` because `math.random` is float and
array indices must be int. Correct (semi-open upper bound, verified over 200k draws) but
every game script rediscovers the dance.

**API.** `random.choice(list)`, `random.int(lo, hi)` inclusive, `random.range(n)`.

### P3.4 Selector / symbol table limits

**Evidence.** The engine's bytecode bundle test fails with `too many selectors in
bytecode`. Real engine scale: 51 classes, ~600 methods, 96 globals.

**Plan.** Size the defaults to that with headroom, make them configurable, and make the
error name the limit that was hit and the current value.

## P4 — tooling

### P4.1 `zen --check`

**Evidence.** To validate script syntax without launching the editor I wrote a C++ harness
against `libzen.a`. The CLI has `--dis`, `--dis-only`, `--dump`; nothing compile-only
with stable diagnostics.

**Plan.** `zen check file.py [...]`: compile only, exit code, `file:line:col: message`
per diagnostic. Runs the base lib + `register_lib` for `math` etc. so engine scripts that
import them parse. Optionally takes a hook table and runs `check_script_contract`.

### P4.2 Stub generation

**Evidence.** The IDE shows ~40 "X is not defined" warnings per engine script
(`ScriptComponent`, `set_flag`, `raycast`, `NavigationAgent`...) because it cannot see
what the host registered.

**Plan.** After registration, `vm.dump_stubs(writer)` emits a `.pyi`-style file from the
class/native tables: classes with methods and arities, globals, module functions.

### P4.3 Profiling hooks

**Evidence.** The engine's `vm.update` / `vm.render` / `vm.other` tick counters (wrapped
around `invoke`) found a 137 ms/frame editor bug in one profiler capture.

**Plan.** Optional per-native and per-`invoke` counters in the VM (`ZEN_PROFILE`), exposed
as a table the host can read and reset each frame. Opcode counters in the same switch.
`gc.last_pause_us` from P1.4 belongs here too.

## What the old zenengine host got right, and Kinetix2D dropped

Read from `zenpy_projects/modules/zenengine` (`builtin_zenengine.cpp`, `ZenScriptHost.*`,
`Signal.hpp`) — the design is worth carrying, the code is STL and cannot cross into
Kinetix2D:

- **Signals** with typed args (`emit_signal("hit", {int, float, ptr})`, `connect`,
  `_on_signal(name, args)`) instead of `node.call(name, one_double)` + a string-keyed
  blackboard. This is the inter-script messaging Kinetix2D is missing.
- **Hook presence cached once** as a bitmask of vtable slots; absent hooks are never
  invoked (P1.2 without VM support).
- **The script instance is the node handle** (`class Player(Sprite2D)`, `native_data =
  node`): no `self.node` hop. Kinetix2D's component model (several scripts per node,
  `self.node`) is a deliberate trade, not a regression — but it is why `node.call` exists.

And what Kinetix2D fixed that the old host had wrong: script fields were not GC-marked
unless the script opted in (`set_gc_fields_enabled`, flagged **IMPORTANT** in its own API
docs); Kinetix2D roots every live instance. The old host also created persistent
wrappers for script-less nodes on demand and never freed them; Kinetix2D's handle cache
gives one handle per pointer.

## Order

1. P0.1 adopt `zen_script_info` in Kinetix2D — engine-only change, deletes 318 lines,
   adds three load-time diagnostics. Do it this week.
2. P0.2 merge `vm-fiber-error-and-len`; P1.1 detach; P1.2 `try_invoke`; P2.4 string
   views — all small, all unblock deleting engine code.
3. P1.4 GC policy engine-side (pause/resume per frame) and measure the pause.
4. P2.2 `__init__` defaults, P2.1 host slot — remove the last scanner and the strcmp table.
5. P4.1 `check`, P4.2 stubs, P2.6 README — developer experience, no engine risk.
6. P2.3 binder, P2.5 modules, P0.3 native structs — big engine cleanup, after the API
   above is stable.
7. P3.x — measured, none is a bottleneck today.

Each item should come with a regression test under `tests/cpp/` that embeds the VM the way
Kinetix2D does (persistent classes, `native_data`, `invoke` on instances, several script
instances per host object) — `test_embedding.cpp` and `test_script_info.cpp` are the
templates, and every bug above lived at that layer, not in script-level tests.
