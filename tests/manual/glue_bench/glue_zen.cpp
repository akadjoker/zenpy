/* ZenPy: script-side quadtree (mode=script) vs the C++ QuadTree as a native
** class driven from the script (mode=native). */
#include "qtree.h"
#include "bench.h"

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "object.h"

#include <chrono>

using namespace zen;

static double num(Value v) { return v.type == VAL_FLOAT ? v.as.number : (double)v.as.integer; }

static void *qt_ctor(VM *, int, Value *args) { return new QuadTree(num(args[0]), num(args[1])); }
static void qt_dtor(VM *, void *data) { delete (QuadTree *)data; }
static int qt_clear(VM *, Value *args, int) { zen_instance_data<QuadTree>(args[-1])->clear(); return 0; }
static int qt_insert(VM *, Value *args, int) { zen_instance_data<QuadTree>(args[-1])->insert(num(args[0]), num(args[1])); return 0; }
static int qt_count(VM *, Value *args, int)
{
    args[0] = val_int(zen_instance_data<QuadTree>(args[-1])->count(num(args[0]), num(args[1]), num(args[2]), num(args[3])));
    return 1;
}

static bool run_mode(VM &vm, const char *fn, const BenchArgs &a, long &checksum, double &secs)
{
    Value args[2] = { val_int(a.n), val_int(a.frames) };
    auto t0 = std::chrono::steady_clock::now();
    Value r = vm.call_global(fn, args, 2);
    secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (vm.had_error())
        return false;
    checksum = (long)num(r);
    return true;
}

int main(int argc, char **argv)
{
    BenchArgs a = bench_args(argc, argv);
    char *src = read_script(argv[0], "glue.zen");
    if (!src) return 1;

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_class("QuadTree")
        .ctor(qt_ctor)
        .dtor(qt_dtor)
        .method("clear", qt_clear, 0, ZEN_NATIVE_GC_SAFE)
        .method("insert", qt_insert, 2, ZEN_NATIVE_GC_SAFE)
        .method("count", qt_count, 4, ZEN_NATIVE_GC_SAFE)
        .end();

    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, src, "glue.zen");
    if (!fn) { fprintf(stderr, "zen: compile failed\n"); return 1; }
    vm.run(fn);
    if (vm.had_error()) return 1;

    long checksum; double secs;
    if (bench_wants(a, "script"))
    {
        if (!run_mode(vm, "run_script", a, checksum, secs)) return 1;
        bench_result("zen", "script", a, secs, checksum);
    }
    if (bench_wants(a, "native"))
    {
        if (!run_mode(vm, "run_native", a, checksum, secs)) return 1;
        bench_result("zen", "native", a, secs, checksum);
    }
    free(src);
    return 0;
}
