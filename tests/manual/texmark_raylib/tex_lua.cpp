/* Lua 5.4 host for the texmark: Texture is a userdata with a metatable. */
#include "host.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cstdio>
#include <cstdlib>

static const char *kTexMeta = "Texture";

static int l_texture_new(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int id = host_load_texture(path);
    if (id == 0)
        return luaL_error(L, "Texture: cannot load '%s'", path);
    int *ud = (int *)lua_newuserdatauv(L, sizeof(int), 0);
    *ud = id;
    luaL_setmetatable(L, kTexMeta);
    return 1;
}
static int l_texture_gc(lua_State *L) { int *ud = (int *)luaL_checkudata(L, 1, kTexMeta); if (*ud) { host_unload_texture(*ud); *ud = 0; } return 0; }
static int l_texture_draw(lua_State *L)
{
#ifdef LUA_UNCHECKED
    /* The fast idiom: trust the receiver (no registry lookup per call). */
    int *ud = (int *)lua_touserdata(L, 1);
    host_draw_texture(*ud, lua_tonumber(L, 2), lua_tonumber(L, 3));
#else
    int *ud = (int *)luaL_checkudata(L, 1, kTexMeta);
    host_draw_texture(*ud, luaL_checknumber(L, 2), luaL_checknumber(L, 3));
#endif
    return 0;
}
static int l_texture_width(lua_State *L)  { lua_pushinteger(L, host_texture_width(*(int *)luaL_checkudata(L, 1, kTexMeta))); return 1; }
static int l_texture_height(lua_State *L) { lua_pushinteger(L, host_texture_height(*(int *)luaL_checkudata(L, 1, kTexMeta))); return 1; }
static int l_draw_texture(lua_State *L)
{
    int *ud = (int *)luaL_checkudata(L, 1, kTexMeta);
    host_draw_texture(*ud, luaL_checknumber(L, 2), luaL_checknumber(L, 3));
    return 0;
}
static int l_rand(lua_State *L) { lua_pushnumber(L, host_rand(luaL_checknumber(L, 1), luaL_checknumber(L, 2))); return 1; }
static int l_screen_width(lua_State *L) { lua_pushinteger(L, host_screen_width()); return 1; }
static int l_screen_height(lua_State *L) { lua_pushinteger(L, host_screen_height()); return 1; }

struct LuaBoot { lua_State *L; const char *src; bool booted; };

static bool lua_boot(LuaBoot *b)
{
    if (luaL_dostring(b->L, b->src) != LUA_OK)
    {
        fprintf(stderr, "lua: %s\n", lua_tostring(b->L, -1));
        return false;
    }
    return true;
}

static bool call_add(lua_State *L, int n, double x, double y)
{
    lua_getglobal(L, "add_sprites");
    lua_pushinteger(L, n);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    if (lua_pcall(L, 3, 0, 0) != LUA_OK)
    {
        fprintf(stderr, "lua: %s\n", lua_tostring(L, -1));
        return false;
    }
    return true;
}

static bool call1(lua_State *L, const char *fn, bool is_int, double v)
{
    lua_getglobal(L, fn);
    if (is_int) lua_pushinteger(L, (lua_Integer)v); else lua_pushnumber(L, v);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
    {
        fprintf(stderr, "lua: %s\n", lua_tostring(L, -1));
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    char *source = host_read_script(argv[0], "sprites.lua");
    if (!source)
        return 1;

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_newmetatable(L, kTexMeta);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_texture_gc);     lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, l_texture_draw);   lua_setfield(L, -2, "draw");
    lua_pushcfunction(L, l_texture_width);  lua_setfield(L, -2, "width");
    lua_pushcfunction(L, l_texture_height); lua_setfield(L, -2, "height");
    lua_pop(L, 1);
    lua_register(L, "Texture", l_texture_new);
    lua_register(L, "draw_texture", l_draw_texture);
    lua_register(L, "rand", l_rand);
    lua_register(L, "screen_width", l_screen_width);
    lua_register(L, "screen_height", l_screen_height);

    LuaBoot boot = { L, source, false };
    TexHost host;
    host.ud = &boot;
    host.add_sprites = [](void *ud, int n, double x, double y) -> bool {
        LuaBoot *b = (LuaBoot *)ud;
        if (!b->booted)
        {
            if (!lua_boot(b))
                return false;
            b->booted = true;
        }
        return call_add(b->L, n, x, y);
    };
    host.update_all = [](void *ud, double dt) -> bool { return call1(((LuaBoot *)ud)->L, "update_all", false, dt); };

#ifdef LUA_UNCHECKED
    int rc = host_run(argc, argv, "lua_fast", &host);
#else
    int rc = host_run(argc, argv, "lua", &host);
#endif
    lua_close(L);
    free(source);
    return rc;
}
