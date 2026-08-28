# Zen Programming Language

A fast, embeddable **Python-syntax scripting language** written in C++11.

Zen compiles `.py` files to register-based bytecode and runs them on a custom VM with computed-goto dispatch. **No AST, no transpilation** — a single-pass compiler goes straight from tokens to bytecode.

```python
def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

print(fib(30))  # 832040
```

> 📚 **[Read the Complete API Docs](docs/README.md)** — 11 core modules, patterns and examples
>
> ⬇️ **[Download Pre-built Binaries](https://github.com/akadjoker/zenpy/releases)** — Linux, Windows, macOS, wasm, Android
>
> 🎮 The graphics/audio/engine modules, the runners and the editor live in a
> separate repository — this one is the VM, the CLI and the tests.

## Key Features

| Category | What's supported |
|----------|-----------------|
| **Control flow** | `if`/`elif`/`else`, `while`, `for`/`in`, `break`, `continue` |
| **Functions** | `def`, generic calls (`f<Type>()`), `async def`, `await`, default arguments, `*args`, closures, decorators, generators (`yield`) |
| **Classes** | `class`, `__init__`, inheritance, `super()`, method override |
| **Data types** | `int`, `float`, `bool`, `None`, `str`, `list`, `dict`, `set` |
| **Expressions** | f-strings, list/dict/set comprehensions, slices, ternary, chained comparisons |
| **Assignment** | `a, b = b, a`, `a += 1`, negative indexing, `del` |
| **Modules** | `import math`, `import time`, `import net`, `import http`, dotted paths, C++ plugins (`.so`/`.dll`) |
| **Builtins** | `print`, `len`, `range`, `type`, `isinstance`, `assert`, `input`, `eval`, `zip`, `map`, `filter`, `enumerate` |
| **Events** | `Signal` class with `connect`, `disconnect`, `emit`, `count`, `clear` |
| **Bytecode** | Dump to `.zenbc`, load and execute — full round-trip serialization |

## Standard Library Modules

**11 built-in modules**, all compiled into the `zen` binary:

| Module | Purpose | Highlight |
|--------|---------|-----------|
| **[base](docs/zenpy_base.md)** | Core types, arrays, maps | 41 functions, Array/Map classes |
| **[math](docs/zenpy_math.md)** | Trigonometry, random, calc | sin, cos, sqrt, random, PI, E |
| **[json](docs/zenpy_json.md)** | JSON parsing/stringify | parse(), stringify() |
| **[time](docs/zenpy_time.md)** | Timing, delays | time(), sleep(), perf_counter() |
| **[io](docs/zenpy_io.md)** | File & stream I/O | open(), read_all(), File class |
| **[path](docs/zenpy_path.md)** | Path manipulation | join(), dirname(), basename(), etc |
| **[os](docs/zenpy_os.md)** | OS interface | getcwd(), listdir(), mkdir(), etc |
| **[numpy](docs/zenpy_numpy.md)** | Numerical computing | zeros, ones, arange, linspace, 40+ funcs |
| **[http](docs/zenpy_http.md)** | HTTP client | get(), post(), download() |
| **[net](docs/zenpy_net.md)** | TCP/UDP sockets | tcp_connect, udp_create, send, recv |
| **[struct](docs/zenpy_struct.md)** | Binary packing/unpacking | pack(), unpack(), calcsize() |

Graphics, audio and engine modules (`canvas`, `audio`, `gl`, `raylib`, `sdl2`,
`glm`, `sqlite`, `dnn`, …) are provided by the runners in a separate repository
and are not part of this build.

**→ [View complete module reference](docs/README.md)**

## Getting Started

## Installation

### 📦 Option 1: Pre-built Binaries (Recommended)

Download from [Releases](https://github.com/akadjoker/zen/releases):

**Linux (x86_64)**
```bash
tar -xzf zen-linux-x86_64.tar.gz
./zen script.py
```

**Windows (x86_64)**
```cmd
unzip zen-windows-x86_64.zip
zen.exe script.py
```

**macOS (ARM64)**
```bash
tar -xzf zen-macos-arm64.tar.gz
./zen script.py
```

**Web (wasm)**
```bash
unzip zen-web-wasm.zip
node zen.js script.py
```

### 🔨 Option 2: Build from Source

Requirements:
- C++11 compiler (GCC, Clang, MSVC)
- CMake 3.12+
- Ninja (optional, but recommended)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./bin/zen tests/01_print.py
```

### Getting Started

Try the interactive REPL:

```bash
zen
>>> print("Hello from Zen!")
>>> var x = 10
>>> print(x * 2)
>>> exit()
```

### Quick Start

```python
print("Hello, World!")
```

### File I/O

```python
import io

# Write
var f = io.open("data.txt", "w")
f.write("Hello, Zen!")
f.close()

# Read
var data = io.read_all("data.txt")
print(data)
```

### JSON

```python
import json

data = {name: "Alice", score: 100}
json_str = json.stringify(data)
print(json_str)  # {"name":"Alice","score":100}
```

### HTTP

```python
import http

response = http.get("http://api.example.com/data")
if response is not None:
    print(response["status"])
```

### Numerical arrays

```python
import numpy as np

var a = np.arange(0, 10)
var b = np.linspace(0, 1, 10)
print(np.dot(a, b))
```

## Building

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Produces:
- `bin/zen` — CLI runtime
- `bin/test_embedding` — C++ embedding checks
- `build/libzen/libzen.a` — the VM as a static library

Requirements: C++17 compiler (GCC, Clang, MSVC). No external dependencies.

## Usage

```bash
zen script.py                      # run a file
zen -e "print(2 ** 10)"            # inline code
zen                                # REPL
zen --dump out.zenbc script.py     # compile to bytecode
zen out.zenbc                      # run bytecode
zen --dis script.py                # disassemble + run
zen --dis-only script.py           # disassemble only
zen -I ./libs script.py            # add module search path
```

## Examples

```python
# Generators
def squares(n):
    for i in range(n):
        yield i * i

for val in squares(5):
    print(val)

# Classes with inheritance
class Animal:
    def __init__(self, name):
        self.name = name
    def speak(self):
        return f"{self.name} says hello"

class Dog(Animal):
    def speak(self):
        return f"{self.name} barks"

print(Dog("Rex").speak())  # Rex barks

# Runtime generics — type arguments are values available to the function
def identity<T>(value):
    return value

print(identity<Dog>(Dog("Rex")).speak())

# Default args, closures, decorators
def log(fn):
    def wrapper(*args):
        print(f"calling {fn.__name__}")
        return fn(*args)
    return wrapper

@log
def add(a, b=0):
    return a + b

print(add(3, 4))  # calling add → 7
```

## Examples & Patterns

See **[docs/README.md](docs/README.md)** for:
- 🎮 Game development patterns
- 📊 Data processing examples
- 🌐 Network programming
- 🤖 Machine learning
- 📁 File & database handling
- 🎨 Graphics & animation

## Performance

Zen is **fast and lightweight**:

- **Bytecode VM**: Single-pass compiler, register-based opcodes
- **Computed Goto**: Direct dispatch loop (faster than switch)
- **Inline Caching**: Method lookups optimized
- **Native Modules**: C++ functions at VM level (no overhead)

Performance depends on workload. Some interpreter-heavy paths are already competitive; others still need VM/compiler work.

Run benchmarks on an optimized build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
ninja -C build
./benchmark.sh --repeat 5
```

`benchmark.sh` refuses non-optimized build types and expects the standard `bin/zen` runtime.

Sample results from this Linux x86-64 workspace with `./benchmark.sh --repeat 5`:

| Benchmark | Zen avg | Python avg | Ratio |
|----------|---------|------------|-------|
| Int arithmetic | 0.050s | 0.356s | 7.07x |
| List append + sum | 0.017s | 0.072s | 4.31x |
| Map insert + iterate | 0.009s | 0.034s | 3.65x |
| Map heavy workload | 0.015s | 0.059s | 3.99x |
| String concat | 0.005s | 0.140s | 28.96x |
| String repeat | 0.005s | 0.013s | 2.80x |
| String split | 0.113s | 0.141s | 1.25x |
| Float parse | 0.077s | 0.121s | 1.57x |
| Recursion | 0.219s | 0.228s | 1.04x |

These are workload-specific measurements, not a blanket speed claim.

## Platform Support

Every target below is built on each push by
[`.github/workflows/ci.yml`](.github/workflows/ci.yml), which uploads the
resulting binary as an artifact and fails if any of them goes missing.

| Platform | Toolchain | Test suite in CI | Artifact |
|----------|-----------|------------------|----------|
| Linux x86-64 | gcc + clang | ✅ (Debug runs under ASan/UBSan) | `zen` |
| Windows x86-64 | MSVC | ✅ | `zen.exe` |
| Windows x86-64 | mingw-w64 (cross from Linux) | — | `zen.exe` |
| macOS arm64 | AppleClang | ✅ | `zen` |
| Web | emscripten | ✅ under node | `zen.js` + `zen.wasm` |
| Android | NDK — arm64-v8a, armeabi-v7a, x86_64 | — (no device in CI) | `zen`, `libzen.a` |

### Building for the cross targets yourself

```bash
# Windows, cross-compiled from Linux
cmake -B build-mingw -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
      -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
      -DZEN_BIN_DIR=$PWD/out
cmake --build build-mingw

# Web — produces out/zen.js + out/zen.wasm, run with `node out/zen.js script.py`
emcmake cmake -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release -DZEN_BIN_DIR=$PWD/out
cmake --build build-web

# Android
cmake -B build-android -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
      -DZEN_BIN_DIR=$PWD/out
cmake --build build-android
```

`ZEN_BIN_DIR` keeps each target's binaries apart so several platform builds can
share one source tree.

## C++ Embedding

Zen is designed to be embedded. Link against `libzen.a` and use the C++ API:

```cpp
#include <zen/vm.h>
#include <zen/compiler.h>
#include <zen/module.h>

int main() {
    zen::VM vm;
    vm.open_lib_globals(&zen::zen_lib_base);
    vm.register_lib(&zen::zen_lib_math);

    // Run script
    zen::ObjFunc *fn = zen::compile_source(&vm, R"(
        print(f"2 + 3 = {2 + 3}")
    )", "example.py");
    vm.run(fn);

    // Register native functions
    vm.def_native("greet", 1, [](zen::VM *vm, zen::Value *args, int) -> int {
        printf("Hello, %s!\n", zen::as_cstring(args[0]));
        return 0;
    });
}
```

### ClassBuilder — expose C++ classes to scripts

```cpp
zen::ClassBuilder(vm, "Vec2")
    .field("x").field("y")
    .method("__init__", 2, vec2_init)
    .method("length", 0, vec2_length)
    .build();
```

Script classes can inherit from C++ classes — the VM handles the vtable dispatch.

### Trimming modules

A host that does not ship a module can leave it out of the build instead of
maintaining its own source list:

```bash
cmake -B build -DZEN_MODULE_NUMPY=OFF -DZEN_MODULE_NET=OFF -DZEN_MODULE_HTTP=OFF
```

Options exist for `math`, `time`, `struct`, `numpy`, `io`, `os`, `path`, `json`,
`net` and `http`, all `ON` by default (`base` is opened as globals, not
imported, so it is always in). Imports resolve through a runtime registry, so a
module that was not compiled is simply one nothing can register — `import numpy`
then fails with `module 'numpy' not found` rather than failing to link.

### Host output

An editor or game host usually wants `print()` and runtime errors in its own
console rather than on stdout. Build with `-DZEN_HOST_OUTPUT=ON` and register a
writer:

```cpp
#include <zen/zen_host_output.h>

zen_host_set_writer([](const char *text, size_t len, int is_error, void *user) {
    ((Console *)user)->append(text, len, is_error);
}, &my_console);
```

The option is off by default, so the CLI keeps writing straight to stdout with
no indirection. With it on and no writer registered, output still falls back to
stdout/stderr.

### Plugins

Shared libraries exporting `zen_open_<name>()` are loaded automatically on `import`:

```cpp
// myplugin.cpp
#include <zen/zen_plugin.h>
ZEN_EXPORT const zen::NativeLib* zen_open_myplugin() {
    static zen::NativeReg fns[] = { {"hello", my_hello, 0} };
    static zen::NativeLib lib = { "myplugin", fns, 1 };
    return &lib;
}
```

```bash
g++ -shared -fPIC -o myplugin.so myplugin.cpp -Ilibzen/include/zen
```

```python
import myplugin
myplugin.hello()
```

## Using zen as a submodule

This repository is the single source of truth for the VM. Projects consume it
whole and build the library out of it:

```bash
git submodule add https://github.com/akadjoker/zenpy.git external/zen
```

```cmake
set(ZEN_HOST_OUTPUT ON CACHE BOOL "" FORCE)   # optional, see above
set(ZEN_MODULE_NUMPY OFF CACHE BOOL "" FORCE) # optional, see above
add_subdirectory(external/zen/libzen)
target_link_libraries(my_target PRIVATE zen_static)
```

`libzen/CMakeLists.txt` builds only the library — the CLI, tests and CI live in
the root `CMakeLists.txt` and are not pulled in. Fixes belong here first; a
project should never patch its checkout of the submodule.

## Architecture

```
source.py → Lexer → Compiler → Emitter → Bytecode → VM
              │         │          │          │         │
           indent    single-    register    32-bit   computed
           aware     pass +     alloc +     packed   goto
                     Pratt      patching    instrs   dispatch
```

### Source layout

```
libzen/
├── include/zen/       # public headers
│   ├── vm.h           # VM, GC, value representation
│   ├── compiler.h     # compiler API
│   ├── object.h       # object types (string, func, class, ...)
│   ├── opcodes.h      # 89 opcodes
│   ├── bytecode.h     # serialization API
│   ├── module.h       # NativeLib / NativeReg
│   └── zen_plugin.h   # plugin macros
├── src/
│   ├── vm.cpp              # VM core, globals, call dispatch
│   ├── vm_dispatch.cpp     # main dispatch loop (computed goto)
│   ├── compiler.cpp        # compiler driver
│   ├── compiler_expressions.cpp
│   ├── compiler_statements.cpp
│   ├── emitter.cpp         # bytecode emission
│   ├── lexer.cpp           # tokenizer with indent tracking
│   ├── memory.cpp          # GC (mark-and-sweep)
│   ├── bytecode.cpp        # .zenbc dump/load
│   ├── debug.cpp           # disassembler
│   ├── builtin_base.cpp    # 30 core builtins
│   ├── builtin_math.cpp    # math module (25 functions + 5 constants)
│   ├── builtin_time.cpp    # time module
│   └── invoke_*.inl        # method dispatch (string, array, map, set, buffer)
cli/
└── main.cpp           # CLI entry point, REPL
tests/
├── *.py               # script test suite
├── cpp/               # C++ embedding checks
└── fuzz/              # libFuzzer harness (-DZEN_BUILD_FUZZ=ON, clang only)
```

### Instruction format

All instructions are 32 bits:

```
Standard: [opcode:8 | A:8 | B:8 | C:8]    e.g. ADD R[A] = R[B] + R[C]
Wide:     [opcode:8 | A:8 | Bx:16]         e.g. LOADK R[A] = K[Bx]
Signed:   [opcode:8 | A:8 | sBx:16]        e.g. JMP pc += sBx
```

Multi-word instructions for complex ops: `INVOKE` (2 words), `SUPER_INVOKE` (3 words), `CALL_GLOBAL` (2 words).

### Bytecode format (`.zenbc`, version 2.1)

```
Header:  ZENBC(5) | major(u16) | minor(u16) | flags(u32)
Globals: count(u32) | [name + optional value] × count
Selectors: count(u32) | [string] × count
Functions: recursive tree of function bodies
```

## Builtin Modules

### Base (always available)
`str`, `int`, `float`, `char`, `ord`, `type`, `isinstance`, `len`, `range`, `print`, `input`, `assert`, `error`, `format`, `enumerate`, `zip`, `map`, `filter`, `collect`, `mem_used`, `mem_info`, `clock`

### `math`
`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sqrt`, `cbrt`, `exp`, `log`, `log2`, `log10`, `ceil`, `floor`, `round`, `pow`, `abs`, `min`, `max`, `clamp`, `random`, `hypot`, `isnan`, `isinf` — plus constants `pi`, `e`, `tau`, `inf`, `nan`

### `time`
`time`, `monotonic`, `perf_counter`, `sleep`

### `net`
`resolve`, `tcp_connect`, `tcp_listen`, `tcp_accept`, `udp_create`, `send`, `recv`, `sendto`, `recvfrom`, `set_blocking`, `set_nodelay`, `poll`, `close`

### `http`
`get`, `post`, `download`, `ping`, `get_local_ip`

### Methods on built-in types
- **string**: `len`, `upper`, `lower`, `strip`, `split`, `join`, `find`, `replace`, `startswith`, `endswith`, `contains`, `substr`, ...
- **array**: `push`, `pop`, `insert`, `remove`, `sort`, `reverse`, `map`, `filter`, `find`, `contains`, `len`, ...
- **map**: `keys`, `values`, `items`, `get`, `set`, `remove`, `contains`, `len`, ...
- **set**: `add`, `remove`, `contains`, `union`, `intersection`, `difference`, `len`, ...

## Script API Docs

Generate `JSON + HTML + RST` from a folder of Zen scripts:

```bash
python3 tools/generate_api_docs.py <folder>
```

Outputs `docs/api/api.json`, `docs/api/api.html` and `docs/api/api.rst`. To edit
the API in JSON by hand and regenerate only the rendered docs:

```bash
python3 tools/generate_api_docs.py --from-json docs/api/api.json
```

`build_docs.py` bundles `docs/*.md` into a single browsable HTML page:

```bash
python3 build_docs.py ./docs site/docs.html
```

## Tests

```bash
./run_tests.sh                  # 50 script tests + the C++ embedding checks
./run_tests.sh --stress-gc      # rebuild with GC at every allocation, then run
./run_tests.sh --filter 41 -v   # one test, with output
```

`run_tests.sh` takes `ZEN`, `EMBED` and `ZEN_RUNNER` from the environment, which
is how CI exercises the non-native builds:

```bash
ZEN=./bin/zen.exe EMBED=./bin/test_embedding.exe ./run_tests.sh
ZEN=./out/zen.js EMBED=./out/test_embedding.js ZEN_RUNNER=node ./run_tests.sh
```

Bytecode round-trip:

```bash
for f in tests/[0-9][0-9]_*.py; do
    bin/zen --dump /tmp/t.zenbc "$f" && bin/zen /tmp/t.zenbc
done
```

Covers: print, functions, recursion, control flow, locals, loops, classes, inheritance, super, range, imports, decorators, f-strings, eval, generators, scoping, edge cases, stress tests, async/await, signals/events, networking (`net`/`http`), I/O, OS/path, buffers/struct, JSON.

## Learn More

### 📖 Documentation
- **[Complete Module Reference](docs/README.md)** — 11 core modules, patterns
- **[Benchmarks](scripts/bench/)** — the workloads behind the table above
- **[CI workflow](.github/workflows/ci.yml)** — how every platform is built and checked

### 🚀 Quick Links
- **[Core Language Features](#key-features)** — if, for, def, class, etc.
- **[Module Reference by Use Case](docs/README.md#quick-navigation)** — data, networks, system scripts
- **[Performance Benchmarks](#performance)** — vs Python & others

### 📝 Architecture
- Single-pass compiler → register-based bytecode
- Computed-goto VM dispatch
- Inline method caching
- Mark-and-sweep GC

---

## License

MIT
