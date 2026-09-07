# Cross-language raylib texmark — script-owned textures

The bunnymark's follow-up. There the host owned the sprite and the script
called `draw_bunny(x, y)`. Here the **script loads the texture**
(`Texture("wabbit_alpha.png")`), keeps it in each of its own objects and
draws through the native object (`self.tex.draw(x, y)`). That is the shape of
a real game script driving engine-owned resources: script objects, a native
object per resource, one native method call per sprite per frame.

Five binaries, one host (`host.cpp`: window, texture table, frame loop,
rolling-30-frame fps, auto-add policy). Each language binary embeds its VM
and exposes the same natives:

| | Texture class | draw | free function |
|---|---|---|---|
| Zen | `ClassBuilder` with native ctor/dtor (`native_data` = texture id) | `tex.draw(x, y)` | `draw_texture(tex, x, y)` |
| Lua 5.4 | userdata + metatable (`__index`, `__gc`) | `tex:draw(x, y)` via `luaL_checkudata` | `draw_texture` |
| Lua fast | same, `lua_touserdata` without the registry check | | |
| Wren 0.4 | `foreign class` (allocate/finalize) | `tex.draw(x, y)` | `Native.drawTexture` |
| CPython 3.12 | `PyTypeObject` in the `native` module | `tex.draw(x, y)` | `native.draw_texture` |

The scripts (`sprites.zen/.lua/.wren/.py`) have the same shape as the
bunnymark ones plus the `tex` field. `RESULT` lines carry `draws=` (native
draw calls in the last frame) as the check that every sprite went through the
native object.

## Build / run

```
make            # same prerequisites as ../bunnymark_raylib (raylib, liblua.a, wren 0.4, python3-dev)
./tex_zen --fixed 20000 --seconds 4
./run_all.sh    # all five, auto-add, 25 s each
```

## Results (2026-09-07, same machine, same window, 1280x720, vsync off)

Fixed 20 000 sprites, 4 s:

| Language | fps | bunnymark fps (host-owned texture) |
|---|---:|---:|
| ZenPy | 209 | 190 |
| Lua 5.4 (`luaL_checkudata`) | 134 | 165 |
| Lua 5.4 (unchecked) | 131 | |
| Wren 0.4 | 137 | 160 |
| CPython 3.12 | 71 | |

Auto-add, target 60 fps, 25 s (`./run_all.sh`), max sprites at >= 60 fps:

| Language | sprites | bunnymark |
|---|---:|---:|
| ZenPy | 68 600 | 70 800 |
| Lua 5.4 (`luaL_checkudata`) | 42 400 | 63 000 |
| Lua 5.4 (unchecked) | 52 400 | |
| Wren 0.4 | 45 000 | 58 800 |
| CPython 3.12 | 31 000 | 34 800 |

Reading:

- For Zen the native object costs nothing measurable: the script-owned
  `Texture` gives the same count as the host-owned sprite. `self.tex.draw()`
  is an INVOKE on a native-class instance: vtable slot to a NativeFn, the
  texture id read from `native_data`.
- Lua loses a third of its bunnymark count once the draw goes through a
  userdata method: the `__index` metatable hop plus `luaL_checkudata`'s
  registry lookup per call. Trusting the receiver recovers part of it.
- Wren's foreign class costs it ~20%; CPython is where it was.
- At these counts the raylib draw call dominates every binary; the gap
  between languages is the per-sprite scripting overhead on top of it.
