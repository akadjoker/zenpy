/* ZenPy host for the texmark: the script owns the texture objects. */
#include "host.h"

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "object.h"

#include <cstdio>
#include <cstdlib>

using namespace zen;

struct TexData { int id; };

static double num(Value v) { return v.type == VAL_FLOAT ? v.as.number : (double)v.as.integer; }

/* Texture(path): native class, instance data = texture id in the host table. */
static void *tex_ctor(VM *vm, int argc, Value *args)
{
    if (argc < 1 || !is_string(args[0]))
    {
        vm->runtime_error("Texture: expected a path string");
        return nullptr;
    }
    int id = host_load_texture(as_cstring(args[0]));
    if (id == 0)
    {
        vm->runtime_error("Texture: cannot load '%s'", as_cstring(args[0]));
        return nullptr;
    }
    TexData *t = (TexData *)malloc(sizeof(TexData));
    t->id = id;
    return t;
}
static void tex_dtor(VM *, void *data)
{
    TexData *t = (TexData *)data;
    host_unload_texture(t->id);
    free(t);
}
/* ClassBuilder convention: args[-1] = self, args[0..] = arguments */
static int tex_draw(VM *, Value *args, int)
{
    TexData *t = zen_instance_data<TexData>(args[-1]);
    host_draw_texture(t->id, num(args[0]), num(args[1]));
    return 0;
}
static int tex_width(VM *, Value *args, int)  { args[0] = val_int(host_texture_width(zen_instance_data<TexData>(args[-1])->id)); return 1; }
static int tex_height(VM *, Value *args, int) { args[0] = val_int(host_texture_height(zen_instance_data<TexData>(args[-1])->id)); return 1; }

/* draw_texture(tex, x, y): the free-function form of the same call. */
static int n_draw_texture(VM *vm, Value *args, int)
{
    if (!is_instance(args[0]) || !as_instance(args[0])->native_data)
    {
        vm->runtime_error("draw_texture: expected a Texture");
        return -1;
    }
    host_draw_texture(zen_instance_data<TexData>(args[0])->id, num(args[1]), num(args[2]));
    return 0;
}
static int n_rand(VM *, Value *args, int) { args[0] = val_float(host_rand(num(args[0]), num(args[1]))); return 1; }
static int n_screen_width(VM *, Value *args, int) { args[0] = val_int(host_screen_width()); return 1; }
static int n_screen_height(VM *, Value *args, int) { args[0] = val_int(host_screen_height()); return 1; }

static bool call_n(VM *vm, const char *fn, Value *args, int n)
{
    vm->call_global(fn, args, n);
    return !vm->had_error();
}

struct Boot { VM *vm; const char *src; bool booted; };

static bool zen_boot(Boot *b)
{
    Compiler compiler;
    ObjFunc *fn = compiler.compile(&b->vm->get_gc(), b->vm, b->src, "sprites.zen");
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
    char *source = host_read_script(argv[0], "sprites.zen");
    if (!source)
        return 1;

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_class("Texture")
        .ctor(tex_ctor)
        .dtor(tex_dtor)
        .method("draw", tex_draw, 2, ZEN_NATIVE_GC_SAFE)
        .method("width", tex_width, 0, ZEN_NATIVE_GC_SAFE)
        .method("height", tex_height, 0, ZEN_NATIVE_GC_SAFE)
        .end();
    vm.def_native("draw_texture", n_draw_texture, 3, ZEN_NATIVE_GC_SAFE);
    vm.def_native("rand", n_rand, 2, ZEN_NATIVE_GC_SAFE);
    vm.def_native("screen_width", n_screen_width, 0, ZEN_NATIVE_GC_SAFE);
    vm.def_native("screen_height", n_screen_height, 0, ZEN_NATIVE_GC_SAFE);

    Boot boot = { &vm, source, false };
    TexHost host;
    host.ud = &boot;
    host.add_sprites = [](void *ud, int n, double x, double y) -> bool {
        Boot *b = (Boot *)ud;
        if (!b->booted)
        {
            if (!zen_boot(b)) /* the window exists by now: the script may load textures */
                return false;
            b->booted = true;
        }
        Value args[3] = { val_int(n), val_float(x), val_float(y) };
        return call_n(b->vm, "add_sprites", args, 3);
    };
    host.update_all = [](void *ud, double dt) -> bool {
        Value arg = val_float(dt);
        return call_n(((Boot *)ud)->vm, "update_all", &arg, 1);
    };

    int rc = host_run(argc, argv, "zen", &host);
    free(source);
    return rc;
}
