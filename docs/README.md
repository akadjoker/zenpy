# Zen Programming Language — Standard Library Reference

API documentation for the modules built into the Zen VM.

The graphics, audio and engine modules (`canvas`, `audio`, `gl`, `raylib`,
`sdl2`, `glm`, `sqlite`, the ZenEngine API, …) are built by the runners in
**zenpy_projects** and documented there — this repository ships the VM, the CLI
and the tests.

---

## Core Modules (11)

Always available in the `zen` binary.

| Module | Description | Functions |
|--------|-------------|-----------|
| [**base**](zenpy_base.md) | Core functions, arrays, maps | 41 functions + 2 classes |
| [**math**](zenpy_math.md) | Mathematics, trigonometry, random | 31 functions + 5 constants |
| [**struct**](zenpy_struct.md) | Binary packing/unpacking | 5 functions |
| [**json**](zenpy_json.md) | JSON parsing and serialization | 2 functions + patterns |
| [**time**](zenpy_time.md) | Timing, delays, performance | 4 functions + patterns |
| [**io**](zenpy_io.md) | File and stream I/O | 8 functions + 1 class |
| [**path**](zenpy_path.md) | Path manipulation | 7 functions + patterns |
| [**os**](zenpy_os.md) | Operating system interface | 10 functions + patterns |
| [**numpy**](zenpy_numpy.md) | Numerical computing | 40 functions + patterns |
| [**http**](zenpy_http.md) | HTTP client operations | 5 functions + patterns |
| [**net**](zenpy_net.md) | Network sockets & UDP | 13 functions + patterns |

---

## Quick Navigation

**Data processing:** [numpy](zenpy_numpy.md) · [json](zenpy_json.md) · [struct](zenpy_struct.md)

**Network applications:** [http](zenpy_http.md) · [net](zenpy_net.md)

**System scripts:** [os](zenpy_os.md) · [io](zenpy_io.md) · [path](zenpy_path.md) · [time](zenpy_time.md)

---

## Import Syntax

```python
import base     // Core (always available)
import math
import struct
import json
import time
import io
import path
import os
import numpy
import http
import net
```

---

## Common Patterns

### Hello World
```python
print("Hello, World!")
```

### File I/O
```python
import io

var f = io.open("data.txt", "w")
f.write("Hello")
f.close()

var data = io.read_all("data.txt")
```

### JSON
```python
import json

var data = {name: "Alice", score: 100}
var json_str = json.stringify(data)
var parsed = json.parse(json_str)
```

### HTTP Request
```python
import http

var response = http.get("https://api.example.com/data")
print(response)
```

---

## Error Handling

Most functions return `nil` or throw exceptions on error. Check documentation for specific error behavior.

```python
import io

var f = io.open("missing.txt", "r")
if f == nil:
    print("File not found")
```

---

## Memory Management

- Automatic garbage collection
- No manual memory management needed
- Arrays and maps resize dynamically

---

## Thread Safety

**Note:** Zen is single-threaded. Use explicit serialization for multi-threaded code.

---

## Platform Support

Every row below is built and checked by CI on each push
(see [`.github/workflows/ci.yml`](../.github/workflows/ci.yml)).

| Platform | Build | Test suite | Notes |
|----------|-------|------------|-------|
| Linux x86-64 | ✅ gcc + clang | ✅ | Debug builds run under ASan/UBSan |
| Windows x86-64 | ✅ MSVC + mingw | ✅ (MSVC) | mingw job cross-compiles from Linux |
| macOS arm64 | ✅ | ✅ | |
| Web (wasm) | ✅ emscripten | ✅ under node | `zen.js` + `zen.wasm`, NODERAWFS |
| Android | ✅ NDK, arm64-v8a / armeabi-v7a / x86_64 | — | artifact only, no device in CI |

---

**Total Coverage:** 11 core modules
