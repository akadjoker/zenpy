# Cross-language raylib bunnymark

Four binaries, one host. `host.cpp` owns the raylib window, the sprite, the
frame loop, a rolling-30-frame FPS and the auto-add policy; each
`bunny_<lang>.cpp` only embeds its VM, exposes the same four natives
(`draw_bunny(x, y)`, `rand(lo, hi)`, `screen_width()`, `screen_height()`) and
hands the host `add_bunnies(n)` / `update_all(dt)`. The scripts
(`bunny.zen`, `bunny.lua`, `bunny.wren`, `bunny.py`) have the same shape: a
`Bunny` class with `x/y/vx/vy`, an `update(dt)` method that moves, bounces
and calls the native draw, a module-level list, and the two entry points.
So the number measured is "script object update + one native call per
sprite", which is what a scripted game loop costs.

## Build

```
make            # needs raylib (/usr/local), liblua.a (/usr/local), wren 0.4 sources, python3-dev
make ZEN_BUILD=../../../build_release WREN_DIR=/path/to/wren-0.4.0
```

The Zen binary links `libzen.a` from `ZEN_BUILD` (a **Release** build — the
default `build/` is Debug+ASan and 20-30x slower).

## Run

```
./bunny_zen                        # auto-add: +200 bunnies/frame while rolling fps >= 60, until ESC
./bunny_zen --seconds 25           # same, quits after 25 s and prints a RESULT line
./bunny_lua --fixed 50000 --seconds 10
./run_all.sh                       # all four, auto-add, 25 s each, RESULT lines only
./run_all.sh --fixed 20000 --seconds 5
```

Options: `--start N`, `--step N`, `--target FPS`, `--fixed N`, `--seconds S`,
`--size WxH`, `--vsync` (off by default so fps can exceed the refresh rate).
The RNG is seeded identically in every language, so the spawn sequence is
the same.

## Results (2026-09-06, same machine, same window, 1280x720, vsync off)

Auto-add, target 60 fps, 25 s each (`./run_all.sh`):

| Language        | max bunnies at >= 60 fps (run 1 / run 2) |
|-----------------|-----------------------------------------:|
| ZenPy           | 70 400 / 70 800 |
| Lua 5.4.8       | 63 000 / 60 600 |
| Wren 0.4.0      | 54 600 / 58 800 |
| CPython 3.12    | 30 800 / 34 800 |

(Run 2 is after the round-2 VM work; the spread between runs is the
measurement noise — at these counts the raylib draw call per sprite
dominates, not the script.)

Fixed 20 000 bunnies, 4 s (`./run_all.sh --fixed 20000 --seconds 4`):

| Language     | fps |
|--------------|----:|
| ZenPy        | 190 |
| Lua 5.4.8    | 165 |
| Wren 0.4.0   | 160 |
| CPython 3.12 | 91  |

ZenPy branch `perf/lua-hot-path-gc` at the commit that added this directory.
The Zen script uses the recommended hot-loop shape (`typed: Array[Bunny] =
bunnies` + index loop, so `typed[i].update(dt)` is a static vtable call and
`self.x = self.x + self.vx * dt` is the fused field multiply-add). The other
scripts use their idiomatic fast form (Lua metatable class + numeric for,
Wren fields + `for (b in list)`, Python `__slots__` + `for b in list`).
