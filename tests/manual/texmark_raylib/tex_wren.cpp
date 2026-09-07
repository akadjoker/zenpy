/* Wren 0.4 host for the texmark: Texture is a foreign class. */
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

static void tex_allocate(WrenVM *vm)
{
    int *id = (int *)wrenSetSlotNewForeign(vm, 0, 0, sizeof(int));
    *id = host_load_texture(wrenGetSlotString(vm, 1));
}
static void tex_finalize(void *data) { int *id = (int *)data; if (*id) host_unload_texture(*id); }
static void tex_draw(WrenVM *vm)
{
    int *id = (int *)wrenGetSlotForeign(vm, 0);
    host_draw_texture(*id, wrenGetSlotDouble(vm, 1), wrenGetSlotDouble(vm, 2));
}
static void tex_width(WrenVM *vm)  { wrenSetSlotDouble(vm, 0, host_texture_width(*(int *)wrenGetSlotForeign(vm, 0))); }
static void tex_height(WrenVM *vm) { wrenSetSlotDouble(vm, 0, host_texture_height(*(int *)wrenGetSlotForeign(vm, 0))); }
static void f_draw_texture(WrenVM *vm)
{
    int *id = (int *)wrenGetSlotForeign(vm, 1);
    host_draw_texture(*id, wrenGetSlotDouble(vm, 2), wrenGetSlotDouble(vm, 3));
}
static void f_rand(WrenVM *vm) { wrenSetSlotDouble(vm, 0, host_rand(wrenGetSlotDouble(vm, 1), wrenGetSlotDouble(vm, 2))); }
static void f_screen_width(WrenVM *vm) { wrenSetSlotDouble(vm, 0, host_screen_width()); }
static void f_screen_height(WrenVM *vm) { wrenSetSlotDouble(vm, 0, host_screen_height()); }

static WrenForeignMethodFn bind_foreign(WrenVM *, const char *, const char *cls, bool is_static, const char *sig)
{
    if (!strcmp(cls, "Texture") && !is_static)
    {
        if (!strcmp(sig, "draw(_,_)")) return tex_draw;
        if (!strcmp(sig, "width")) return tex_width;
        if (!strcmp(sig, "height")) return tex_height;
        return nullptr;
    }
    if (strcmp(cls, "Native") != 0 || !is_static) return nullptr;
    if (!strcmp(sig, "drawTexture(_,_,_)")) return f_draw_texture;
    if (!strcmp(sig, "rand(_,_)")) return f_rand;
    if (!strcmp(sig, "screenWidth")) return f_screen_width;
    if (!strcmp(sig, "screenHeight")) return f_screen_height;
    return nullptr;
}

static WrenForeignClassMethods bind_class(WrenVM *, const char *, const char *cls)
{
    WrenForeignClassMethods m = { nullptr, nullptr };
    if (!strcmp(cls, "Texture"))
    {
        m.allocate = tex_allocate;
        m.finalize = tex_finalize;
    }
    return m;
}

struct WrenBoot
{
    WrenVM *vm;
    const char *src;
    bool booted;
    WrenHandle *game;
    WrenHandle *add;
    WrenHandle *update;
};

static bool wren_boot(WrenBoot *b)
{
    if (wrenInterpret(b->vm, "main", b->src) != WREN_RESULT_SUCCESS)
        return false;
    wrenEnsureSlots(b->vm, 1);
    wrenGetVariable(b->vm, "main", "Game", 0);
    b->game = wrenGetSlotHandle(b->vm, 0);
    b->add = wrenMakeCallHandle(b->vm, "addSprites(_)");
    b->update = wrenMakeCallHandle(b->vm, "updateAll(_)");
    return true;
}

static bool wren_call(WrenBoot *b, WrenHandle *method, double arg)
{
    wrenEnsureSlots(b->vm, 2);
    wrenSetSlotHandle(b->vm, 0, b->game);
    wrenSetSlotDouble(b->vm, 1, arg);
    return wrenCall(b->vm, method) == WREN_RESULT_SUCCESS;
}

int main(int argc, char **argv)
{
    char *source = host_read_script(argv[0], "sprites.wren");
    if (!source)
        return 1;

    WrenConfiguration config;
    wrenInitConfiguration(&config);
    config.writeFn = w_write;
    config.errorFn = w_error;
    config.bindForeignMethodFn = bind_foreign;
    config.bindForeignClassFn = bind_class;
    WrenVM *vm = wrenNewVM(&config);

    WrenBoot boot = { vm, source, false, nullptr, nullptr, nullptr };
    TexHost host;
    host.ud = &boot;
    host.add_sprites = [](void *ud, int n) -> bool {
        WrenBoot *b = (WrenBoot *)ud;
        if (!b->booted)
        {
            if (!wren_boot(b))
                return false;
            b->booted = true;
        }
        return wren_call(b, b->add, n);
    };
    host.update_all = [](void *ud, double dt) -> bool { return wren_call((WrenBoot *)ud, ((WrenBoot *)ud)->update, dt); };

    int rc = host_run(argc, argv, "wren", &host);
    if (boot.add) wrenReleaseHandle(vm, boot.add);
    if (boot.update) wrenReleaseHandle(vm, boot.update);
    if (boot.game) wrenReleaseHandle(vm, boot.game);
    wrenFreeVM(vm);
    free(source);
    return rc;
}
