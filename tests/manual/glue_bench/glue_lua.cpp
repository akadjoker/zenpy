/* Lua 5.4: script-side quadtree vs the C++ QuadTree as a userdata. */
#include "qtree.h"
#include "bench.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
#include <chrono>

static const char *kMeta = "QuadTree";

static int l_new(lua_State *L)
{
    QuadTree **ud = (QuadTree **)lua_newuserdatauv(L, sizeof(QuadTree *), 0);
    *ud = new QuadTree(luaL_checknumber(L, 1), luaL_checknumber(L, 2));
    luaL_setmetatable(L, kMeta);
    return 1;
}
static int l_gc(lua_State *L) { QuadTree **ud = (QuadTree **)luaL_checkudata(L, 1, kMeta); delete *ud; *ud = nullptr; return 0; }
static int l_clear(lua_State *L) { (*(QuadTree **)lua_touserdata(L, 1))->clear(); return 0; }
static int l_insert(lua_State *L) { (*(QuadTree **)lua_touserdata(L, 1))->insert(lua_tonumber(L, 2), lua_tonumber(L, 3)); return 0; }
static int l_count(lua_State *L)
{
    lua_pushinteger(L, (*(QuadTree **)lua_touserdata(L, 1))->count(lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5)));
    return 1;
}

static bool run_mode(lua_State *L, const char *fn, const BenchArgs &a, long &checksum, double &secs)
{
    lua_getglobal(L, fn);
    lua_pushinteger(L, a.n);
    lua_pushinteger(L, a.frames);
    auto t0 = std::chrono::steady_clock::now();
    if (lua_pcall(L, 2, 1, 0) != LUA_OK) { fprintf(stderr, "lua: %s\n", lua_tostring(L, -1)); return false; }
    secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    checksum = (long)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return true;
}

int main(int argc, char **argv)
{
    BenchArgs a = bench_args(argc, argv);
    char *src = read_script(argv[0], "glue.lua");
    if (!src) return 1;
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    luaL_newmetatable(L, kMeta);
    lua_pushvalue(L, -1); lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_gc); lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, l_clear); lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, l_insert); lua_setfield(L, -2, "insert");
    lua_pushcfunction(L, l_count); lua_setfield(L, -2, "count");
    lua_pop(L, 1);
    lua_register(L, "QuadTree", l_new);
    if (luaL_dostring(L, src) != LUA_OK) { fprintf(stderr, "lua: %s\n", lua_tostring(L, -1)); return 1; }
    long checksum; double secs;
    if (!run_mode(L, "run_script", a, checksum, secs)) return 1;
    bench_result("lua", "script", a, secs, checksum);
    if (!run_mode(L, "run_native", a, checksum, secs)) return 1;
    bench_result("lua", "native", a, secs, checksum);
    lua_close(L);
    free(src);
    return 0;
}
