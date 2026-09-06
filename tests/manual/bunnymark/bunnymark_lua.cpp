/* Bunnymark-style embedding benchmark for Lua 5.4, mirroring
** bunnymark_zen.cpp exactly (same object shape, same frame count, same
** native draw call convention) for a direct comparison. */
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cstdio>
#include <cstdlib>
#include <chrono>

static long g_draw_calls = 0;
static double g_draw_accum = 0.0;

static int native_draw(lua_State *L)
{
    double x = luaL_checknumber(L, 1);
    double y = luaL_checknumber(L, 2);
    g_draw_accum += x + y;
    g_draw_calls++;
    return 0;
}

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 20000;
    int frames = argc > 2 ? atoi(argv[2]) : 300;

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "native_draw", native_draw);

    const char *source = R"LUA(
local Bunny = {}
Bunny.__index = Bunny
function Bunny.new(x, y, vx, vy)
    local self = setmetatable({}, Bunny)
    self.x = x
    self.y = y
    self.vx = vx
    self.vy = vy
    return self
end
function Bunny:update(dt)
    self.x = self.x + self.vx * dt
    self.y = self.y + self.vy * dt
    if self.x > 800 or self.x < 0 then
        self.vx = -self.vx
    end
    if self.y > 600 or self.y < 0 then
        self.vy = -self.vy
    end
    native_draw(self.x, self.y)
end

function make_bunnies(n)
    local bunnies = {}
    for i = 1, n do
        bunnies[i] = Bunny.new(i % 800, i % 600, 1.5, 2.0)
    end
    return bunnies
end

function run_frames(bunnies, frames)
    for f = 1, frames do
        for i = 1, #bunnies do
            bunnies[i]:update(0.016)
        end
    end
end
)LUA";

    if (luaL_dostring(L, source) != LUA_OK)
    {
        fprintf(stderr, "compile failed: %s\n", lua_tostring(L, -1));
        return 1;
    }

    lua_getglobal(L, "make_bunnies");
    lua_pushinteger(L, n);
    lua_call(L, 1, 1); /* bunnies table on stack */

    auto start = std::chrono::high_resolution_clock::now();
    lua_getglobal(L, "run_frames");
    lua_pushvalue(L, -2); /* bunnies */
    lua_pushinteger(L, frames);
    lua_call(L, 2, 0);
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();
    printf("bunnies=%d frames=%d draw_calls=%ld elapsed=%.4f fps=%.1f\n",
           n, frames, g_draw_calls, elapsed, frames / elapsed);

    lua_close(L);
    return 0;
}
