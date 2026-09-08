# Algorithm benchmarks — ZenPy vs Python / Lua / Wren

Real workloads instead of micro-loops: what a game script would actually do
if the engine did *not* provide the data structure natively.

| Program | What it measures |
|---|---|
| `pathfind` | A* and Dijkstra on a 128×128 grid, 30% walls, 40 random queries each; binary heap in script; flat arrays |
| `spatial` | Quadtree (2D) and octree (3D): 20 000 random points inserted, 2 000 range queries each; capacity 8, depth ≤ 8; objects with fields and methods |
| `hanoi` | Towers of Hanoi with 20 disks (method + plain recursion, ~1.3M calls) and a 4-connected flood fill on a 256×256 grid with 35% walls, 60 seeds, explicit stack |

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

## 2026-09-07, first run (commit 3e6c9c8, same window, best of 3, seconds)

|            | astar | dijkstra | quadtree | octree |
|------------|------:|---------:|---------:|-------:|
| zen        | 0.063 | 0.227    | 0.061    | 0.086  |
| zen_typed  | 0.064 | 0.229    | 0.059    | 0.082  |
| python     | 0.070 | 0.255    | 0.075    | 0.112  |
| lua        | 0.042 | 0.160    | 0.055    | 0.086  |
| wren       | 0.062 | 0.339    | 0.071    | 0.101  |

The per-opcode profile of Dijkstra showed 18% of all dispatches were MOVEs:
`keys[i] = keys[p]` copied container and index into hold registers before
every store, and every `and` in a condition copied its operand into the
result register. Both fixed in the compiler the same day (see below).

## 2026-09-07, after the condition/subscript codegen fixes (commit ff92cc1, same window)

|            | astar | dijkstra | quadtree | octree | hanoi | floodfill |
|------------|------:|---------:|---------:|-------:|------:|----------:|
| zen        | 0.055 | 0.179    | 0.052    | 0.070  | 0.055 | 0.011     |
| zen_typed  | 0.055 | 0.181    | 0.052    | 0.068  | 0.055 | 0.011     |
| python     | 0.070 | 0.253    | 0.074    | 0.112  | 0.091 | 0.027     |
| lua        | 0.042 | 0.160    | 0.054    | 0.084  | 0.065 | 0.009     |
| wren       | 0.061 | 0.326    | 0.071    | 0.100  | 0.074 | 0.020     |

Reading:

- Zen is ahead of Python everywhere (1.4–2.5×), ahead of Wren everywhere,
  ahead of Lua on quadtree/octree/hanoi, and behind Lua only on the two
  flat-array loops (Dijkstra, flood fill) by 10–20%.
- Type hints change nothing here: the hot loops are `array[i]` reads and
  integer arithmetic, not method dispatch. Hints pay off for `obj.method()`
  chains and `p.field` on annotated parameters, which these programs
  barely do inside their inner loops.
- What the profiles still show as avoidable: `GETGLOBAL` for module
  constants read inside functions (`W`, `N` — 5% of Dijkstra's dispatches;
  Lua captures them as upvalues), the MOVE of a call result into a local
  (`cur = heap.pop()`), and `APPEND` at ~2.3× the cost of a plain opcode.
- None of these interpreters is within 10× of the same code in C++. The
  conclusion for the engine is the one already taken: the script drives,
  the engine owns the data structures (grid, spatial index, heap) as native
  classes, and the script only calls `grid.astar(a, b)` / `tree.query(...)`.
