cmake_minimum_required(VERSION 3.16)
project(zen LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Build types ---
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Where the executables land. Overridable so several platform builds can share
# one source tree without overwriting each other's binaries.
set(ZEN_BIN_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "Output directory for zen executables")

# Common warnings
if(MSVC)
    add_compile_options(/W3 /wd4244 /wd4267 /wd4996)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Sanitizer flags for Debug builds. Host toolchains only — the cross targets
# (mingw, emscripten, Android NDK) and MSVC do not take these flags.
option(ZEN_SANITIZE "Enable ASan/UBSan on Debug host builds" ON)
if(ZEN_SANITIZE AND CMAKE_BUILD_TYPE STREQUAL "Debug"
        AND NOT MSVC AND NOT CMAKE_CROSSCOMPILING)
    add_compile_options(-g -O0 -fno-omit-frame-pointer)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# --- libzen (the VM) ---
add_subdirectory(libzen)

# --- CLI (zen executable — VM only, no modules) ---
# The runners, editor and engine modules that used to be built here live in
# zenpy_projects; this repository ships the VM, the CLI and the tests.
add_subdirectory(cli)

# --- Tests (C++ embedding tests) ---
add_subdirectory(tests/cpp)

# --- Fuzzer (requires clang + -DZEN_BUILD_FUZZ=ON) ---
option(ZEN_BUILD_FUZZ "Build libFuzzer harness" OFF)
add_subdirectory(tests/fuzz)
