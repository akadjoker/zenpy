# Speed report: port-ct-7ca94f1 (2026-09-07, commit 7ca94f1, branch port/containers)

## Clean build time (ninja, wall seconds, 12 cores)
- build_release (Release): 9,754 s
- build (Debug): 8,860 s

## Sizes (bytes)
- build_release/bin/zen: 628792
- build_release/libzen/libzen.a: 958336
- build/libzen/libzen.a: 17728416

## Test suite (release, wall seconds)
- run_tests.sh: 2,140

## Cross-language benchmarks (zen, best of 3, seconds)
- fib              0,131
- for_loop         0,158
- for_loop_local   0,066
- method_call      0,132
- binary_trees     0,277

## Microbenchmarks (best of 3, seconds)
- call_base        0,051
- call_f0          0,146
- call_f3          0,134
- call_fbig        0,170
- call_m_typed     0,138
- call_m_dyn       0,155
- call_m_void      0,169
- typedparam       0,075
- selffield        0,122
- foreach          0,076
- forrange         0,059
- forrange_empty   0,066

## algo_bench (best of 3, seconds per phase)
               astar  dijkstra  quadtree    octree     hanoi floodfill
zen            0.057     0.188     0.053     0.071     0.052     0.011

## Script compile time (100 x zen --check, best of 3, seconds; includes process start)
- tests/22_stress.py                       0,370 (100 runs)
- tests/23_edge_brutal.py                  0,405 (100 runs)
- tests/manual/algo_bench/py/spatial.py    0,331 (100 runs)
