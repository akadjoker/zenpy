/*
** test_gc_native.cpp — natives running with the GC live (ZEN_NATIVE_GC_SAFE).
**
** The default convention pauses the GC for the whole native call. A GC_SAFE
** native runs with the collector on and roots what it keeps via vm->root().
** This test proves the mechanism the only way that counts:
**
**   1. a GC_SAFE native builds 200k unique strings — collections MUST have
**      happened while it ran (the counter says so), and nothing it built
**      may have been swept (the script checks every element).
**   2. the default convention still collects zero times mid-native.
**   3. vm->root() overflow raises a clean runtime error, not a corruption.
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"

#include <cstdio>
#include <cstring>

using namespace zen;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) printf("  %-52s", name)
#define PASS() do { printf("OK\n"); g_tests_passed++; } while (0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_tests_failed++; } while (0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while (0)

static bool run_source(VM &vm, const char *source)
{
    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, "<gc>");
    if (!fn)
        return false;
    vm.run(fn);
    return !vm.had_error();
}

/* Collections observed inside the most recent burst call. */
static size_t g_cycles_in_native = 0;
/* Whether the burst under test runs GC_SAFE (collect mid-call) or paused. */
static bool nat_burst_gc_safe = true;

/* burst(n) → array of n unique strings. The GC_SAFE discipline in one place:
** the array is rooted through the args window, the string in flight through
** one reusable vm->root() slot — array_push can allocate before storing. */
static int nat_burst(VM *vm, Value *args, int nargs)
{
    if (nargs != 1 || !is_int(args[0]))
    { vm->runtime_error("burst: expected int"); return -1; }
    int64_t n = args[0].as.integer;

    GC *gc = &vm->get_gc();
    size_t before = gc->collections;

    ObjArray *arr = new_array(gc);
    args[0] = val_obj((Obj *)arr);
    Value *tmp = vm->root(val_nil());
    if (!tmp)
        return -1;

    char buf[32];
    for (int64_t i = 0; i < n; i++)
    {
        int len = snprintf(buf, sizeof(buf), "item-%lld", (long long)i);
        *tmp = val_obj((Obj *)vm->make_string(buf, len));
        array_push(gc, arr, *tmp);
        /* The VM's natural trigger points are sparse by design (objects are
        ** born BLACK from the arena; growth reallocs pause around
        ** themselves), so force real cycles to exercise the rooting: every
        ** live object this native holds must survive each one. */
        if ((i & 4095) == 0 && (nat_burst_gc_safe))
            vm->collect();
    }

    g_cycles_in_native = gc->collections - before;
    return 1;
}

/* Same construction under the default paused convention: no explicit
** collects — the point is that allocations alone never trigger one. */
static int nat_burst_paused(VM *vm, Value *args, int nargs)
{
    nat_burst_gc_safe = false;
    int r = nat_burst(vm, args, nargs);
    nat_burst_gc_safe = true;
    return r;
}

/* root_flood() → forces the fiber stack to run out of root slots. */
static int nat_root_flood(VM *vm, Value *args, int nargs)
{
    (void)args; (void)nargs;
    for (int i = 0; i < 1000000; i++)
        if (!vm->root(val_int(i)))
            return -1; /* runtime error already raised */
    return 0;
}

int main()
{
    printf("=== GC-safe natives ===\n");

    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        vm.def_native("burst", nat_burst, 1, ZEN_NATIVE_GC_SAFE);

        TEST("GC_SAFE native survives live collections");
        bool ok = run_source(vm,
            "a = burst(200000)\n"
            "assert len(a) == 200000\n"
            "assert a[0] == \"item-0\"\n"
            "assert a[123456] == \"item-123456\"\n"
            "assert a[199999] == \"item-199999\"\n");
        CHECK(ok, "script failed — something was swept or corrupted");

        TEST("collections really happened mid-call");
        CHECK(g_cycles_in_native >= 40, "fewer cycles than forced — trigger broken");
        printf("      (%zu collection(s) inside one call)\n", g_cycles_in_native);
    }

    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        vm.def_native("burst", nat_burst_paused, 1); /* default: paused */

        TEST("default convention still pauses the GC");
        bool ok = run_source(vm,
            "a = burst(200000)\n"
            "assert len(a) == 200000\n"
            "assert a[199999] == \"item-199999\"\n");
        CHECK(ok && g_cycles_in_native == 0, "expected zero cycles mid-native");
    }

    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        vm.def_native("flood", nat_root_flood, 0, ZEN_NATIVE_GC_SAFE);

        TEST("root overflow is a clean runtime error");
        bool ok = run_source(vm, "flood()\n");
        CHECK(!ok, "flood was expected to raise");
    }

    {
        /* The stack_top restore after a native must leave the interpreter
        ** healthy: keep computing afterwards. */
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        vm.def_native("burst", nat_burst, 1, ZEN_NATIVE_GC_SAFE);

        TEST("interpreter healthy after GC_SAFE calls");
        bool ok = run_source(vm,
            "total = 0\n"
            "for i in range(10):\n"
            "    total += len(burst(1000))\n"
            "assert total == 10000\n");
        CHECK(ok, "post-native execution broke");
    }

    printf("\n  passed: %d  failed: %d\n", g_tests_passed, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
