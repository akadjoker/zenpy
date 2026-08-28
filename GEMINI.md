# Zen Programming Language

## Project Overview

Zen is a fast, embeddable Python-syntax scripting language written in C++. It features a custom virtual machine with computed-goto dispatch and a single-pass compiler that compiles `.py` files directly to register-based bytecode (no AST generation).

The project is designed to be lightweight, embeddable within C++ applications, and high-performance. It includes a comprehensive standard library implemented via C++ modules (e.g., `math`, `time`, `net`, `http`, `canvas`, `audio`, `sqlite`, etc.) and supports C++ embedding and plugins.

## Main Technologies
- **Language:** C++17 (Core VM, Compiler, Builtins), Python (Syntax, Scripts, Testing)
- **Build System:** CMake, Ninja
- **Core Features:** Mark-and-sweep GC, computed-goto VM loop, custom bytecode (`.zenbc`), inline method caching.

## Architecture & Directory Structure
- **`libzen/`**: Core engine code. Contains the VM (`vm.cpp`), compiler (`compiler.cpp`), lexer, emitter, GC (`memory.cpp`), and core builtins.
- **`cli/`**: Entry point for the `zen` executable and REPL (`main.cpp`).
- **`modules/`**: Standard library modules implemented as C++ native extensions (e.g., `audio`, `canvas`, `opengl`, `sdl2`, etc.).
- **`editor/`**: Source code for an included IDE/editor.
- **`tests/`**: Test suite consisting of multiple Python `.py` scripts covering language features, control flow, builtins, and stress testing. Also contains C++ embedding tests in `tests/cpp`.
- **`docs/`**: Markdown API documentation and references.

## Building and Running

### Build Instructions
The project uses CMake and requires a C++ compiler supporting C++11 (though C++17 is set in CMakeLists.txt) and Ninja (optional but recommended).

```bash
# Debug build (with Address/Undefined Behavior Sanitizers)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
ninja -C build

# Release build (Optimized)
cmake -B build -DCMAKE_BUILD_TYPE=Release
ninja -C build
```
Binaries are output to the `bin/` directory (e.g., `bin/zen`).

### Running Scripts
```bash
# Run a specific script
./bin/zen path/to/script.py

# Launch REPL
./bin/zen

# Disassemble a script
./bin/zen --dis-only path/to/script.py
```

### Testing
Tests are managed via shell scripts that run the Python files in the `tests/` directory against the compiled `zen` binary.

```bash
# Run all tests
./run_tests.sh

# Run tests with GC stress mode (GC at every allocation)
./run_tests.sh --stress-gc

# Run complete regression suite with timeout checks
./test_all_regression.sh
```

### Benchmarking
To run performance benchmarks against Python:
```bash
./benchmark.sh --repeat 5
```
*Note: Benchmarking requires a Release build.*

## Development Conventions
- **Code Style:** C++ codebase prefers explicit structural handling. Avoid hiding logic behind complex prototype manipulation; favor explicit composition.
- **Testing:** New language features or bug fixes must be accompanied by a new `.py` test case in the `tests/` directory or an update to an existing one. Ensure tests pass under both normal and `--stress-gc` modes.
- **Native Modules:** When adding standard library functions, expose them via native C++ modules in the `modules/` directory or as core builtins in `libzen/src/builtin_*.cpp`. Ensure proper registration with the VM using `zen::NativeReg`.
- **Validation:** Always empirically reproduce bugs by creating a test case or script before attempting a fix. Validate fixes thoroughly using `./run_tests.sh --stress-gc`.
