# Algorithm benchmarks — ZenPy vs Python / Lua / Wren

Real workloads instead of micro-loops: what a game script would actually do
if the engine did *not* provide the data structure natively.

| Program | What it measures |
|---|---|
| `pathfind` | A* and Dijkstra on a 128×128 grid, 30% walls, 40 random queries each; binary heap in script; flat arrays |
| `spatial` | Quadtree (2D) and octree (3D): 20 000 random points inserted, 2 000 range queries each; capacity 8, depth ≤ 8; objects with fields and methods |

`py/*.py` runs unchanged under **CPython and Zen** (the subset both accept:
no `[0] * n`, no `abs/min/max` builtins, no tuple swap on subscripts, no
multi-line conditions in parentheses). `lua/` and `wren/` are line-for-line
ports. `zen_typed/` adds Zen type hints (`node: Quad`, `c0: Quad?`,
`kids: Array[Oct]`, `heap: Heap`) to the same code. Every version prints the
same checksums — that is the correctness check.

Wren runs through `wren-0.4.0/bin/wren_test file.wren` (no `wren_cli` on this
machine); Lua is 5.4; Python 3.12; Zen is the Release build.

```
./run.sh              # best of 3 per phase, one table
```

## 2026-09-07 (commit after 36e09c3, same window, best of 3, seconds)

|            | astar | dijkstra | quadtree | octree |
|------------|------:|---------:|---------:|-------:|
| zen        | 0.063 | 0.227    | 0.061    | 0.086  |
| zen_typed  | 0.064 | 0.229    | 0.059    | 0.082  |
| python     | 0.070 | 0.255    | 0.075    | 0.112  |
| lua        | 0.042 | 0.160    | 0.055    | 0.086  |
| wren       | 0.062 | 0.339    | 0.071    | 0.101  |

Reading:

- Zen is in Python's tier or a bit ahead on every workload, ties Lua on the
  octree, beats Wren on three of four. Lua keeps a clear lead only on the
  heap-heavy Dijkstra (flat-array indexing and arithmetic in a tight loop).
- Type hints change nothing here: the hot loops are `array[i]` reads and
  integer arithmetic, not method dispatch. Hints pay off for `obj.method()`
  chains and `p.field` on annotated parameters, which these programs
  barely do inside their inner loops.
- None of these interpreters is within 10× of the same code in C++. The
  conclusion for the engine is the one already taken: the script drives,
  the engine owns the data structures (grid, spatial index, heap) as native
  classes, and the script only calls `grid.astar(a, b)` / `tree.query(...)`.
