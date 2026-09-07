/*
** fuzz_embedding.cpp — libFuzzer harness for the public C++ embedding API.
**
** Unlike fuzz_compile_run, this harness keeps a small, valid Zen program
** alive and fuzzes the boundary used by a host: call_global(), call_fn(),
** invoke() and invoke_operator(). It deliberately mixes invalid calls with
** valid calls, large argument windows, native methods and native->Zen
** callbacks. ASan/UBSan and ZEN_DEBUG_STRESS_GC turn boundary mistakes into
** reproducible corpus inputs.
**
** Build:
**   cmake -B build-fuzz -G Ninja -DZEN_BUILD_FUZZ=ON \
**     -DCMAKE_CXX_COMPILER=clang++
**   cmake --build build-fuzz --target fuzz_embedding
**
** Run:
**   ./bin/fuzz_embedding tests/fuzz/corpus_embedding/ -timeout=5 -max_len=4096
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"

#include <cstddef>
#include <cstdint>

using namespace zen;

static void silent_output(const char *, int, void *) {}

static int native_sum(VM *vm, Value *args, int nargs)
{
    (void)vm;
    /* Native ClassBuilder methods receive their receiver at args[-1]. */
    if (!is_instance(args[-1]))
        return -1;
    int64_t sum = 0;
    for (int i = 0; i < nargs; i++)
        if (is_int(args[i]))
            sum += args[i].as.integer;
    args[0] = val_int(sum);
    return 1;
}

static int native_add(VM *vm, Value *args, int nargs)
{
    (void)vm;
    if (!is_instance(args[-1]) || nargs != 1 || !is_int(args[0]))
        return -1;
    args[0] = val_int(args[0].as.integer + 1);
    return 1;
}

/* Calls back into a fuzz-selected Zen callable while the native boundary is
** active. This is where stale stack/frame restoration bugs tend to surface. */
static int native_apply(VM *vm, Value *args, int nargs)
{
    if (nargs != 2)
        return -1;
    Value call_args[1] = { args[1] };
    Value result = vm->call_fn(args[0], call_args, 1);
    if (vm->had_error())
        return -1;
    args[0] = result;
    return 1;
}

static bool run_setup(VM &vm)
{
    static const char source[] = R"(
def identity(x):
    return x

def count(first, *rest):
    return first + len(rest)

class ScriptBox:
    def __init__(self, value=0):
        self.value = value

    def add(self, value):
        self.value = self.value + value
        return self.value

box = ScriptBox()
)";

    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, "<embedding-fuzz>");
    if (!fn)
        return false;
    vm.run(fn);
    return !vm.had_error();
}

static Value fuzz_value(uint8_t tag, uint8_t payload)
{
    switch (tag & 3)
    {
    case 0: return val_int((int8_t)payload);
    case 1: return val_bool((payload & 1) != 0);
    case 2: return val_nil();
    default: return val_float((double)(int8_t)payload / 3.0);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 4096)
        return 0;

    VM vm;
    ZenCallbacks callbacks = zen_default_callbacks();
    callbacks.print = silent_output;
    callbacks.print_err = silent_output;
    vm.set_callbacks(callbacks);
    vm.open_lib_globals(&zen_lib_base);
    vm.def_native("apply", native_apply, 2);

    ObjClass *native_box_class = vm.def_class("NativeBox")
        .method("sum", native_sum, -1)
        .method("__add__", native_add, 2)
        .end();
    Value native_box = vm.make_instance(native_box_class);

    if (!run_setup(vm))
        return 0;
    Value script_box = vm.get_global("box");
    Value identity = vm.get_global("identity");

    for (size_t pos = 0; pos + 2 < size; pos += 3)
    {
        const uint8_t op = data[pos] % 8;
        const int nargs = data[pos + 1] % 33; /* Covers the old 16-arg edge. */
        Value args[32];
        for (int i = 0; i < nargs; i++)
            args[i] = fuzz_value(data[pos + 2] + (uint8_t)i, data[(pos + 2 + (size_t)i) % size]);

        switch (op)
        {
        case 0:
            vm.call_global("identity", args, nargs);
            break;
        case 1:
            vm.call_global("count", args, nargs);
            break;
        case 2:
            vm.call_fn(identity, args, nargs);
            break;
        case 3:
            vm.invoke(script_box, "add", args, nargs);
            break;
        case 4:
            vm.invoke(native_box, "sum", args, nargs);
            break;
        case 5:
            vm.invoke_operator(native_box, VM::SLOT_ADD, args, nargs);
            break;
        case 6:
        {
            Value callback_args[2] = { identity, fuzz_value(data[pos + 2], data[pos + 1]) };
            vm.call_global("apply", callback_args, 2);
            break;
        }
        case 7:
            /* Invalid public inputs must report an error, never dereference. */
            vm.invoke(val_int(1), "sum", nullptr, 0);
            vm.call_global(vm.num_globals() + 1, nullptr, 0);
            break;
        }
    }
    return 0;
}
