/* Wren 0.4: script-side quadtree vs the C++ QuadTree as a foreign class. */
#include "qtree.h"
#include "bench.h"

extern "C" {
#include "wren.h"
}
#include <chrono>

static void w_write(WrenVM *, const char *text) { fputs(text, stdout); }
static void w_error(WrenVM *, WrenErrorType type, const char *module, int line, const char *msg)
{
    if (type == WREN_ERROR_COMPILE) fprintf(stderr, "wren: %s:%d: %s\n", module, line, msg);
    else if (type == WREN_ERROR_RUNTIME) fprintf(stderr, "wren: runtime error: %s\n", msg);
    else fprintf(stderr, "wren:   %s:%d in %s\n", module, line, msg);
}

static void qt_allocate(WrenVM *vm)
{
    QuadTree **ud = (QuadTree **)wrenSetSlotNewForeign(vm, 0, 0, sizeof(QuadTree *));
    *ud = new QuadTree(wrenGetSlotDouble(vm, 1), wrenGetSlotDouble(vm, 2));
}
static void qt_finalize(void *data) { delete *(QuadTree **)data; }
static void qt_clear(WrenVM *vm) { (*(QuadTree **)wrenGetSlotForeign(vm, 0))->clear(); }
static void qt_insert(WrenVM *vm) { (*(QuadTree **)wrenGetSlotForeign(vm, 0))->insert(wrenGetSlotDouble(vm, 1), wrenGetSlotDouble(vm, 2)); }
static void qt_count(WrenVM *vm)
{
    QuadTree *t = *(QuadTree **)wrenGetSlotForeign(vm, 0);
    wrenSetSlotDouble(vm, 0, t->count(wrenGetSlotDouble(vm, 1), wrenGetSlotDouble(vm, 2), wrenGetSlotDouble(vm, 3), wrenGetSlotDouble(vm, 4)));
}

static WrenForeignMethodFn bind_foreign(WrenVM *, const char *, const char *cls, bool is_static, const char *sig)
{
    if (strcmp(cls, "QuadTree") != 0 || is_static) return nullptr;
    if (!strcmp(sig, "clear()")) return qt_clear;
    if (!strcmp(sig, "insert(_,_)")) return qt_insert;
    if (!strcmp(sig, "count(_,_,_,_)")) return qt_count;
    return nullptr;
}
static WrenForeignClassMethods bind_class(WrenVM *, const char *, const char *cls)
{
    WrenForeignClassMethods m = { nullptr, nullptr };
    if (!strcmp(cls, "QuadTree")) { m.allocate = qt_allocate; m.finalize = qt_finalize; }
    return m;
}

static bool run_mode(WrenVM *vm, WrenHandle *game, const char *sig, const BenchArgs &a, long &checksum, double &secs)
{
    WrenHandle *h = wrenMakeCallHandle(vm, sig);
    wrenEnsureSlots(vm, 3);
    wrenSetSlotHandle(vm, 0, game);
    wrenSetSlotDouble(vm, 1, a.n);
    wrenSetSlotDouble(vm, 2, a.frames);
    auto t0 = std::chrono::steady_clock::now();
    bool ok = wrenCall(vm, h) == WREN_RESULT_SUCCESS;
    secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (ok) checksum = (long)wrenGetSlotDouble(vm, 0);
    wrenReleaseHandle(vm, h);
    return ok;
}

int main(int argc, char **argv)
{
    BenchArgs a = bench_args(argc, argv);
    char *src = read_script(argv[0], "glue.wren");
    if (!src) return 1;
    WrenConfiguration config;
    wrenInitConfiguration(&config);
    config.writeFn = w_write;
    config.errorFn = w_error;
    config.bindForeignMethodFn = bind_foreign;
    config.bindForeignClassFn = bind_class;
    WrenVM *vm = wrenNewVM(&config);
    if (wrenInterpret(vm, "main", src) != WREN_RESULT_SUCCESS) return 1;
    wrenEnsureSlots(vm, 1);
    wrenGetVariable(vm, "main", "Game", 0);
    WrenHandle *game = wrenGetSlotHandle(vm, 0);
    long checksum; double secs;
    if (bench_wants(a, "script"))
    {
        if (!run_mode(vm, game, "runScript(_,_)", a, checksum, secs)) return 1;
        bench_result("wren", "script", a, secs, checksum);
    }
    if (bench_wants(a, "native"))
    {
        if (!run_mode(vm, game, "runNative(_,_)", a, checksum, secs)) return 1;
        bench_result("wren", "native", a, secs, checksum);
    }
    wrenReleaseHandle(vm, game);
    wrenFreeVM(vm);
    free(src);
    return 0;
}
