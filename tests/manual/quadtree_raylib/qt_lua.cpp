/* Lua 5.4 host for the quadtree demo. */
#include "host.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cstdio>
#include <cstdlib>

static int l_draw_point(lua_State *L) { host_draw_point(luaL_checknumber(L, 1), luaL_checknumber(L, 2), (int)luaL_checkinteger(L, 3)); return 0; }
static int l_draw_rect(lua_State *L) { host_draw_rect(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4), (int)luaL_checkinteger(L, 5)); return 0; }
static int l_set_stats(lua_State *L) { host_set_stats((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3)); return 0; }
static int l_mouse_x(lua_State *L) { lua_pushnumber(L, host_mouse_x()); return 1; }
static int l_mouse_y(lua_State *L) { lua_pushnumber(L, host_mouse_y()); return 1; }
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

static bool lua_pcall_report(lua_State *L, int nargs)
{
    if (lua_pcall(L, nargs, 0, 0) != LUA_OK)
    {
        fprintf(stderr, "lua: %s\n", lua_tostring(L, -1));
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    char *source = host_read_script(argv[0], "quadtree.lua");
    if (!source)
        return 1;

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "draw_point", l_draw_point);
    lua_register(L, "draw_rect", l_draw_rect);
    lua_register(L, "set_stats", l_set_stats);
    lua_register(L, "mouse_x", l_mouse_x);
    lua_register(L, "mouse_y", l_mouse_y);
    lua_register(L, "rand", l_rand);
    lua_register(L, "screen_width", l_screen_width);
    lua_register(L, "screen_height", l_screen_height);

    LuaBoot boot = { L, source, false };
    QtHost host;
    host.ud = &boot;
    host.add_points = [](void *ud, int n, double x, double y) -> bool {
        LuaBoot *b = (LuaBoot *)ud;
        if (!b->booted)
        {
            if (!lua_boot(b))
                return false;
            b->booted = true;
        }
        lua_getglobal(b->L, "add_points");
        lua_pushinteger(b->L, n);
        lua_pushnumber(b->L, x);
        lua_pushnumber(b->L, y);
        return lua_pcall_report(b->L, 3);
    };
    host.update_all = [](void *ud, double dt) -> bool {
        LuaBoot *b = (LuaBoot *)ud;
        lua_getglobal(b->L, "update_all");
        lua_pushnumber(b->L, dt);
        return lua_pcall_report(b->L, 1);
    };

    int rc = host_run(argc, argv, "lua", &host);
    lua_close(L);
    free(source);
    return rc;
}
