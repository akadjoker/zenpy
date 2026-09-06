/* ZenPy host for the raylib bunnymark. */
#include "host.h"

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"

#include <cstdio>
#include <cstdlib>

using namespace zen;

static double num(Value v) { return v.type == VAL_FLOAT ? v.as.number : (double)v.as.integer; }

static int n_draw_bunny(VM *, Value *args, int) { host_draw_bunny(num(args[0]), num(args[1])); return 0; }
static int n_rand(VM *, Value *args, int) { args[0] = val_float(host_rand(num(args[0]), num(args[1]))); return 1; }
static int n_screen_width(VM *, Value *args, int) { args[0] = val_int(host_screen_width()); return 1; }
static int n_screen_height(VM *, Value *args, int) { args[0] = val_int(host_screen_height()); return 1; }

static bool zen_add(void *ud, int n)
{
    VM *vm = (VM *)ud;
    Value arg = val_int(n);
    vm->call_global("add_bunnies", &arg, 1);
    return !vm->had_error();
}

static bool zen_update(void *ud, double dt)
{
    VM *vm = (VM *)ud;
    Value arg = val_float(dt);
    vm->call_global("update_all", &arg, 1);
    return !vm->had_error();
}

int main(int argc, char **argv)
{
    char *source = host_read_script(argv[0], "bunny.zen");
    if (!source)
        return 1;

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_native("draw_bunny", n_draw_bunny, 2, ZEN_NATIVE_GC_SAFE);
    vm.def_native("rand", n_rand, 2, ZEN_NATIVE_GC_SAFE);
    vm.def_native("screen_width", n_screen_width, 0, ZEN_NATIVE_GC_SAFE);
    vm.def_native("screen_height", n_screen_height, 0, ZEN_NATIVE_GC_SAFE);

    /* The window must exist before the script asks for its size. */
    struct Runner
    {
        static bool boot(VM &vm, const char *src)
        {
            Compiler compiler;
            ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, src, "bunny.zen");
            if (!fn)
            {
                fprintf(stderr, "zen: compile failed\n");
                return false;
            }
            vm.run(fn);
            return !vm.had_error();
        }
    };

    struct Boot { VM *vm; const char *src; bool booted; } boot = { &vm, source, false };
    BunnyHost host;
    host.ud = &boot;
    host.add_bunnies = [](void *ud, int n) -> bool {
        Boot *b = (Boot *)ud;
        if (!b->booted)
        {
            if (!Runner::boot(*b->vm, b->src))
                return false;
            b->booted = true;
        }
        return zen_add(b->vm, n);
    };
    host.update_all = [](void *ud, double dt) -> bool { return zen_update(((Boot *)ud)->vm, dt); };

    int rc = host_run(argc, argv, "zen", &host);
    free(source);
    return rc;
}
