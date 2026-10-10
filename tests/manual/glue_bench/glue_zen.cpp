/* ZenPy: script-side quadtree (mode=script) vs the C++ QuadTree as a native
** class driven from the script (mode=native). Bound with zen_bind.hpp. */
#include "qtree.h"
#include "bench.h"

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "zen_bind.hpp"

#include <chrono>

using namespace zen;

static double num(Value v) { return v.type == VAL_FLOAT ? v.as.number : (double)v.as.integer; }

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
    bind::def_class<QuadTree>(vm, "QuadTree")
        .ctor<double, double>()
        .dtor()
        .method<&QuadTree::clear>("clear", ZEN_NATIVE_GC_SAFE)
        .method<&QuadTree::insert>("insert", ZEN_NATIVE_GC_SAFE)
        .method<&QuadTree::count>("count", ZEN_NATIVE_GC_SAFE)
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
