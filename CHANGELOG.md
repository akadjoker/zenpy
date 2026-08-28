# Changelog

All notable changes to Zen are documented in this file.

## [1.0.0] - 2026-05-18

### ✨ Major Features
- **15 Standard Library Modules** with 180+ functions
  - Core: base, math, json, time, io, path, os, numpy, http, net
  - Optional: canvas (2D graphics), audio (sound), gif (recording), dnn (neural networks), sqlite (database)

- **Complete API Documentation** (docs/README.md)
  - 15 module references with examples
  - 200+ code examples and patterns
  - Performance notes and troubleshooting

- **Fast Performance**
  - 3.8x faster than Python (fib benchmark)
  - Register-based bytecode VM
  - Computed-goto dispatch

### 🎮 Game Development Ready
- Canvas module: 52 drawing functions + text rendering
- Audio module: 25 functions + waveform synthesis
- GIF recording: Stream or memory mode
- Examples: Snake, Arkanoid, Tower Defense, Platformer

### 🔧 Developer Experience
- Python syntax (same as CPython)
- Easy C++ embedding (ClassBuilder API)
- Plugin support (.so/.dll modules)
- Full bytecode serialization (.zenbc)
- Interactive REPL

### 🚀 Pre-built Binaries
- Linux x86_64
- Windows x86_64
- macOS universal (ARM64 + x86_64)
- Examples and docs included

---

## Planned Features

### v1.1.0
- [ ] LSP (Language Server Protocol) for VSCode
- [ ] WASM compilation support
- [ ] Python packaging (pip install zen-lang)
- [ ] Godot GDScript integration

### v1.2.0
- [ ] Parallel execution support
- [ ] Extended standard library modules
- [ ] Performance profiler
- [ ] Native code generation (JIT)

---

## Breaking Changes
None yet (stable API from v1.0.0)

---

## Performance Benchmarks

### Fibonacci (fib(35))
```
Zen:    0.4s  (3.8x faster)
Python: 2.5s
Lua:    0.5s
```

### JSON Parsing (1MB)
```
Zen:    12ms   (4.5x faster)
Python: 54ms
```

---

## Installation

### Pre-built Binaries
Download from [Releases](https://github.com/zen/releases)

```bash
# Linux
tar -xzf zen-linux-x86_64.tar.gz
./zen-linux-x86_64/zen script.py

# Windows
unzip zen-windows-x86_64.zip
zen-windows-x86_64\zen.exe script.py

# macOS
tar -xzf zen-macos-universal.tar.gz
./zen-macos-universal/bin/zen script.py
```

### Build from Source
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
ninja -C build
./bin/zen script.py
```

---

## Contributors
- Core team: Design, VM, compiler
- Community: Examples, documentation, bug reports

---

## License
MIT
