/* ZenPy host for the quadtree demo. */
#include "host.h"

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"

#include <cstdio>
#include <cstdlib>

using namespace zen;

static double num(Value v) { return v.type == VAL_FLOAT ? v.as.number : (double)v.as.integer; }
static int ival(Value v) { return v.type == VAL_INT ? (int)v.as.integer : (int)v.as.number; }

static int n_draw_point(VM *, Value *args, int) { host_draw_point(num(args[0]), num(args[1]), ival(args[2])); return 0; }
static int n_draw_rect(VM *, Value *args, int) { host_draw_rect(num(args[0]), num(args[1]), num(args[2]), num(args[3]), ival(args[4])); return 0; }
static int n_set_stats(VM *, Value *args, int) { host_set_stats(ival(args[0]), ival(args[1]), ival(args[2])); return 0; }
static int n_mouse_x(VM *, Value *args, int) { args[0] = val_float(host_mouse_x()); return 1; }
static int n_mouse_y(VM *, Value *args, int) { args[0] = val_float(host_mouse_y()); return 1; }
static int n_rand(VM *, Value *args, int) { args[0] = val_float(host_rand(num(args[0]), num(args[1]))); return 1; }
static int n_screen_width(VM *, Value *args, int) { args[0] = val_int(host_screen_width()); return 1; }
static int n_screen_height(VM *, Value *args, int) { args[0] = val_int(host_screen_height()); return 1; }

struct Boot { VM *vm; const char *src; bool booted; };

static bool zen_boot(Boot *b)
{
    Compiler compiler;
    ObjFunc *fn = compiler.compile(&b->vm->get_gc(), b->vm, b->src, "quadtree.zen");
    if (!fn)
    {
        fprintf(stderr, "zen: compile failed\n");
        return false;
    }
    b->vm->run(fn);
    return !b->vm->had_error();
}

int main(int argc, char **argv)
{
    char *source = host_read_script(argv[0], "quadtree.zen");
    if (!source)
        return 1;

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_native("draw_point", n_draw_point, 3, ZEN_NATIVE_GC_SAFE);
    vm.def_native("draw_rect", n_draw_rect, 5, ZEN_NATIVE_GC_SAFE);
    vm.def_native("set_stats", n_set_stats, 3, ZEN_NATIVE_GC_SAFE);
    vm.def_native("mouse_x", n_mouse_x, 0, ZEN_NATIVE_GC_SAFE);
    vm.def_native("mouse_y", n_mouse_y, 0, ZEN_NATIVE_GC_SAFE);
    vm.def_native("rand", n_rand, 2, ZEN_NATIVE_GC_SAFE);
    vm.def_native("screen_width", n_screen_width, 0, ZEN_NATIVE_GC_SAFE);
    vm.def_native("screen_height", n_screen_height, 0, ZEN_NATIVE_GC_SAFE);

    Boot boot = { &vm, source, false };
    QtHost host;
    host.ud = &boot;
    host.add_points = [](void *ud, int n, double x, double y) -> bool {
        Boot *b = (Boot *)ud;
        if (!b->booted)
        {
            if (!zen_boot(b))
                return false;
            b->booted = true;
        }
        Value args[3] = { val_int(n), val_float(x), val_float(y) };
        b->vm->call_global("add_points", args, 3);
        return !b->vm->had_error();
    };
    host.update_all = [](void *ud, double dt) -> bool {
        Boot *b = (Boot *)ud;
        Value arg = val_float(dt);
        b->vm->call_global("update_all", &arg, 1);
        return !b->vm->had_error();
    };

    int rc = host_run(argc, argv, "zen", &host);
    free(source);
    return rc;
}
