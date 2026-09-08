# Cross-language raylib quadtree demo (Zen / Lua / Wren)

Interactive: the whole quadtree lives in the script. Every frame the script
moves its points, rebuilds the tree with fresh nodes, runs one neighbour
query per point (red = has a neighbour within 6 px), one rectangle query
around the mouse (yellow), and draws points and node bounds through natives.
Left click adds `--click` points at the cursor (default 500), N toggles the
node bounds, ESC quits. `--fixed N --seconds S`, `--auto`, `--burst X,Y` as in
the texmark. The HUD shows the script's ms per frame next to the fps.

```
make && ./qt_zen
./run_all.sh --fixed 10000 --seconds 5
```

Smoke numbers (5 000 points, 2 s, 1280x720): Zen 39.6 fps / 25.1 ms script,
Lua 34.0 fps / 32.2 ms, Wren 33.8 fps / 28.6 ms. The headless script-vs-C++
comparison is in ../glue_bench.
