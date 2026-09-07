/* Wren 0.4 host for the quadtree demo. */
#include "host.h"

extern "C" {
#include "wren.h"
}

#include <cstdio>
#include <cstdlib>
#include <cstring>

static void w_write(WrenVM *, const char *text) { fputs(text, stdout); }
static void w_error(WrenVM *, WrenErrorType type, const char *module, int line, const char *msg)
{
    if (type == WREN_ERROR_COMPILE) fprintf(stderr, "wren: %s:%d: %s\n", module, line, msg);
    else if (type == WREN_ERROR_RUNTIME) fprintf(stderr, "wren: runtime error: %s\n", msg);
    else fprintf(stderr, "wren:   %s:%d in %s\n", module, line, msg);
}

static void f_draw_point(WrenVM *vm) { host_draw_point(wrenGetSlotDouble(vm, 1), wrenGetSlotDouble(vm, 2), (int)wrenGetSlotDouble(vm, 3)); }
static void f_draw_rect(WrenVM *vm) { host_draw_rect(wrenGetSlotDouble(vm, 1), wrenGetSlotDouble(vm, 2), wrenGetSlotDouble(vm, 3), wrenGetSlotDouble(vm, 4), (int)wrenGetSlotDouble(vm, 5)); }
static void f_set_stats(WrenVM *vm) { host_set_stats((int)wrenGetSlotDouble(vm, 1), (int)wrenGetSlotDouble(vm, 2), (int)wrenGetSlotDouble(vm, 3)); }
static void f_mouse_x(WrenVM *vm) { wrenSetSlotDouble(vm, 0, host_mouse_x()); }
static void f_mouse_y(WrenVM *vm) { wrenSetSlotDouble(vm, 0, host_mouse_y()); }
static void f_rand(WrenVM *vm) { wrenSetSlotDouble(vm, 0, host_rand(wrenGetSlotDouble(vm, 1), wrenGetSlotDouble(vm, 2))); }
static void f_screen_width(WrenVM *vm) { wrenSetSlotDouble(vm, 0, host_screen_width()); }
static void f_screen_height(WrenVM *vm) { wrenSetSlotDouble(vm, 0, host_screen_height()); }

static WrenForeignMethodFn bind_foreign(WrenVM *, const char *, const char *cls, bool is_static, const char *sig)
{
    if (strcmp(cls, "Native") != 0 || !is_static) return nullptr;
    if (!strcmp(sig, "drawPoint(_,_,_)")) return f_draw_point;
    if (!strcmp(sig, "drawRect(_,_,_,_,_)")) return f_draw_rect;
    if (!strcmp(sig, "setStats(_,_,_)")) return f_set_stats;
    if (!strcmp(sig, "mouseX")) return f_mouse_x;
    if (!strcmp(sig, "mouseY")) return f_mouse_y;
    if (!strcmp(sig, "rand(_,_)")) return f_rand;
    if (!strcmp(sig, "screenWidth")) return f_screen_width;
    if (!strcmp(sig, "screenHeight")) return f_screen_height;
    return nullptr;
}

struct WrenBoot { WrenVM *vm; const char *src; bool booted; WrenHandle *game; WrenHandle *add; WrenHandle *update; };

static bool wren_boot(WrenBoot *b)
{
    if (wrenInterpret(b->vm, "main", b->src) != WREN_RESULT_SUCCESS)
        return false;
    wrenEnsureSlots(b->vm, 1);
    wrenGetVariable(b->vm, "main", "Game", 0);
    b->game = wrenGetSlotHandle(b->vm, 0);
    b->add = wrenMakeCallHandle(b->vm, "addPoints(_,_,_)");
    b->update = wrenMakeCallHandle(b->vm, "updateAll(_)");
    return true;
}

int main(int argc, char **argv)
{
    char *source = host_read_script(argv[0], "quadtree.wren");
    if (!source)
        return 1;

    WrenConfiguration config;
    wrenInitConfiguration(&config);
    config.writeFn = w_write;
    config.errorFn = w_error;
    config.bindForeignMethodFn = bind_foreign;
    WrenVM *vm = wrenNewVM(&config);

    WrenBoot boot = { vm, source, false, nullptr, nullptr, nullptr };
    QtHost host;
    host.ud = &boot;
    host.add_points = [](void *ud, int n, double x, double y) -> bool {
        WrenBoot *b = (WrenBoot *)ud;
        if (!b->booted)
        {
            if (!wren_boot(b))
                return false;
            b->booted = true;
        }
        wrenEnsureSlots(b->vm, 4);
        wrenSetSlotHandle(b->vm, 0, b->game);
        wrenSetSlotDouble(b->vm, 1, n);
        wrenSetSlotDouble(b->vm, 2, x);
        wrenSetSlotDouble(b->vm, 3, y);
        return wrenCall(b->vm, b->add) == WREN_RESULT_SUCCESS;
    };
    host.update_all = [](void *ud, double dt) -> bool {
        WrenBoot *b = (WrenBoot *)ud;
        wrenEnsureSlots(b->vm, 2);
        wrenSetSlotHandle(b->vm, 0, b->game);
        wrenSetSlotDouble(b->vm, 1, dt);
        return wrenCall(b->vm, b->update) == WREN_RESULT_SUCCESS;
    };

    int rc = host_run(argc, argv, "wren", &host);
    if (boot.add) wrenReleaseHandle(vm, boot.add);
    if (boot.update) wrenReleaseHandle(vm, boot.update);
    if (boot.game) wrenReleaseHandle(vm, boot.game);
    wrenFreeVM(vm);
    free(source);
    return rc;
}
