# Speed report: baseline-1aaa13b (2026-09-07, commit 1aaa13b, branch feat/embedding-fuzz-tests)

## Clean build time (ninja, wall seconds, 12 cores)
- build_release (Release): 9,529 s
- build (Debug): 8,748 s

## Sizes (bytes)
- build_release/bin/zen: 653128
- build_release/libzen/libzen.a: 978294
- build/libzen/libzen.a: 18891248

## Test suite (release, wall seconds)
- run_tests.sh: 2,105

## Cross-language benchmarks (zen, best of 3, seconds)
- fib              0,131
- for_loop         0,158
- for_loop_local   0,066
- method_call      0,133
- binary_trees     0,273

## Microbenchmarks (best of 3, seconds)
- call_base        0,052
- call_f0          0,142
- call_f3          0,133
- call_fbig        0,171
- call_m_typed     0,136
- call_m_dyn       0,145
- call_m_void      0,170
- typedparam       0,077
- selffield        0,124
- foreach          0,073
- forrange         0,059
- forrange_empty   0,067

## algo_bench (best of 3, seconds per phase)
               astar  dijkstra  quadtree    octree     hanoi floodfill
zen            0.057     0.187     0.053     0.069     0.052     0.011

## Script compile time (100 x zen --check, best of 3, seconds; includes process start)
- tests/22_stress.py                       0,377 (100 runs)
- tests/23_edge_brutal.py                  0,414 (100 runs)
- tests/manual/algo_bench/py/spatial.py    0,328 (100 runs)
