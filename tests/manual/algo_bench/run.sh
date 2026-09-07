#!/usr/bin/env bash
# Runs every algorithm benchmark in every language, best of 3 (the time each
# script prints for each phase), and prints one table.
#   ./run.sh [zen-binary]      default: ../../../build_release/bin/zen
cd "$(dirname "$0")"
ZEN="${1:-../../../build_release/bin/zen}"
WREN="${WREN:-/media/projectos/projects/languages/wren-0.4.0/bin/wren_test}"
PHASES="astar dijkstra quadtree octree"

declare -A best
run3() { # label cmd... -> fills best[label/phase]
  local label=$1; shift
  for i in 1 2 3; do
    "$@" 2>/dev/null | while read -r name val; do
      case " $PHASES " in *" $name "*) echo "$name $val";; esac
    done > /tmp/algo_bench_$$.txt
    while read -r name val; do
      local key="$label/$name"
      if [[ -z "${best[$key]}" ]] || awk "BEGIN{exit !($val < ${best[$key]})}"; then best[$key]=$val; fi
    done < /tmp/algo_bench_$$.txt
  done
}
rm -f /tmp/algo_bench_$$.txt

for f in pathfind spatial; do
  run3 zen        "$ZEN" py/$f.py
  run3 zen_typed  "$ZEN" zen_typed/$f.py
  run3 python     python3 py/$f.py
  run3 lua        lua lua/$f.lua
  [[ -x "$WREN" ]] && run3 wren "$WREN" wren/$f.wren
done
rm -f /tmp/algo_bench_$$.txt

printf "%-10s" ""; for p in $PHASES; do printf "%10s" $p; done; echo
for l in zen zen_typed python lua wren; do
  printf "%-10s" $l
  for p in $PHASES; do v=${best[$l/$p]}; printf "%10s" "${v:+$(printf %.3f $v)}"; done
  echo
done
