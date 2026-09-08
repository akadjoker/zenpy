/* Lua 5.4 host for the raylib bunnymark. */
#include "host.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cstdio>
#include <cstdlib>

static int l_draw_bunny(lua_State *L) { host_draw_bunny(luaL_checknumber(L, 1), luaL_checknumber(L, 2)); return 0; }
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
    char *source = host_read_script(argv[0], "bunny.lua");
    if (!source)
        return 1;

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "draw_bunny", l_draw_bunny);
    lua_register(L, "rand", l_rand);
    lua_register(L, "screen_width", l_screen_width);
    lua_register(L, "screen_height", l_screen_height);

    LuaBoot boot = { L, source, false };
    BunnyHost host;
    host.ud = &boot;
    host.add_bunnies = [](void *ud, int n) -> bool {
        LuaBoot *b = (LuaBoot *)ud;
        if (!b->booted)
        {
            if (!lua_boot(b))
                return false;
            b->booted = true;
        }
        return call1(b->L, "add_bunnies", true, n);
    };
    host.update_all = [](void *ud, double dt) -> bool { return call1(((LuaBoot *)ud)->L, "update_all", false, dt); };

    int rc = host_run(argc, argv, "lua", &host);
    lua_close(L);
    free(source);
    return rc;
}
