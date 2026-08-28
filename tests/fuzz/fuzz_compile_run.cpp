/*
** fuzz_compile_run.cpp — libFuzzer harness for Zen compiler + VM.
**
** Feeds arbitrary byte sequences as source code to the compiler.
** If compilation succeeds, runs the resulting bytecode with a short
** instruction limit to avoid infinite loops.
**
** Build:
**   clang++ -g -O1 -fsanitize=fuzzer,address,undefined \
**     -I../../libzen/include -DZEN_DEBUG_STRESS_GC \
**     fuzz_compile_run.cpp ../../build/libzen/libzen.a \
**     -o fuzz_zen -lpthread
**
** Or use the CMake target:
**   cmake .. -G Ninja -DZEN_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
**   ninja fuzz_zen
**
** Run:
**   ./fuzz_zen corpus/ -max_len=4096 -timeout=5
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"

#include <cstring>
#include <cstdlib>

using namespace zen;

/* Suppress noisy output from the VM during fuzzing */
static int silent_print(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)args; (void)nargs;
    return 0;
}

static NativeReg fuzz_overrides[] = {
    {"print", silent_print, -1},
    {"println", silent_print, -1},
};

static NativeLib fuzz_lib = {
    "fuzz_base", fuzz_overrides, 2, nullptr, 0, nullptr
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Cap input size — very large inputs are slow without being useful */
    if (size > 8192) return 0;

    /* Make a null-terminated copy */
    char *source = (char *)malloc(size + 1);
    if (!source) return 0;
    memcpy(source, data, size);
    source[size] = '\0';

    /* --- Phase 1: Compile --- */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        vm.register_lib(&zen_lib_math);
        vm.register_lib(&zen_lib_struct);

        /* Override print to be silent */
        vm.open_lib_globals(&fuzz_lib);

        Compiler compiler;
        ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, "<fuzz>");

        if (!fn)
        {
            /* Compilation error — that's fine, not a bug */
            free(source);
            return 0;
        }

        /* --- Phase 2: Execute --- */
        /* libFuzzer's -timeout=N handles infinite loops.
        ** What we care about: ASAN/UBSAN-detected issues (use-after-free,
        ** buffer overflow, integer overflow, null deref, etc.) */
        vm.run(fn);
    }

    free(source);
    return 0;
}
