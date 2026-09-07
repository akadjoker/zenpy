# Glue benchmark — quadtree in script vs C++ quadtree driven from script vs pure C++

The question behind the whole "script as glue over C++" plan, measured
headless. One workload: N points move and bounce; every frame the tree is
rebuilt and each point asks how many points lie within 6 px of it. Three
ways to run it:

- **script**: the quadtree written in the script language (capacity 8,
  depth ≤ 8, fresh nodes every frame), the same code as in
  `../algo_bench/py/spatial.py` and `../quadtree_raylib`;
- **native**: the quadtree in C++ (`qtree.h`, node pool, `clear()` is O(1))
  exposed as a class — `ClassBuilder` in Zen, userdata + metatable in Lua,
  `foreign class` in Wren — and the script drives it: moves its own point
  objects and calls `tree.clear()`, `tree.insert(x, y)`, `tree.count(x0, y0,
  x1, y1)`;
- **cpp**: the same loop with no script at all (`glue_cpp.cpp`).

The count of points in a rectangle does not depend on the tree's shape, so
every variant must produce the same checksum — that is the correctness check.
Points and velocities come from the same integer LCG in every language.

```
make            # needs libzen.a (Release build), liblua.a, libwren.a
./run_all.sh                      # n=10000, 30 frames
./run_all.sh --n 30000 --frames 10
```

## Results (2026-09-07, same machine, best of 3)

### 10 000 points

| | ms/frame | vs C++ |
|---|---:|---:|
| C++ (no script) | 3.36 | 1.0× |
| Zen, tree in script | 56.94 | 16.9× |
| Zen, C++ tree | 6.55 | 1.9× |
| Lua 5.4, tree in script | 70.26 | 20.9× |
| Lua 5.4, C++ tree | 6.67 | 2.0× |
| Wren 0.4, tree in script | 68.33 | 20.3× |
| Wren 0.4, C++ tree | 7.86 | 2.3× |

checksum 235684 in every row (n=10000, 30 frames, best of 3).

### 30 000 points

| | ms/frame | vs C++ |
|---|---:|---:|
| C++ (no script) | 16.18 | 1.0× |
| Zen, tree in script | 281.10 | 17.4× |
| Zen, C++ tree | 26.00 | 1.6× |
| Lua 5.4, tree in script | 324.46 | 20.1× |
| Lua 5.4, C++ tree | 29.32 | 1.8× |
| Wren 0.4, tree in script | 326.69 | 20.2× |
| Wren 0.4, C++ tree | 30.62 | 1.9× |

checksum 297034 in every row (n=30000, 10 frames, best of 3).

Reading:

- With the data structure in C++ and the script only driving it, Zen runs
  the frame within 2× of pure C++; Lua and Wren within ~2.3×. What remains
  is the script's own work: moving 10 000 point objects and making 20 000
  native calls per frame.
- The same tree written in the script language is 9–10× slower than the
  native one in every language. That is the cost of allocating and walking
  the nodes in the interpreter, and it is the same order of magnitude for
  all three: no interpreter here changes the conclusion.
- The Zen host (`glue_zen.cpp`) binds the C++ `QuadTree` with
  `zen_bind.hpp`: `.ctor<double, double>().dtor().method<&QuadTree::insert>
  ("insert", ZEN_NATIVE_GC_SAFE)` and so on, with no hand-written thunks.
  Same numbers as the hand-written version it replaced — the thunk the
  header generates is the one we used to write by hand.
- So the engine owns the spatial index, the grid, the heap; the script owns
  the game objects and the decisions. Zen's ClassBuilder path (native
  ctor/dtor, `zen_instance_data`, vtable slot to a NativeFn) is what makes
  the native mode cost nothing beyond the call itself.
