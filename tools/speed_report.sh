#!/usr/bin/env bash
# Full speed report: clean build times, binary sizes, suite time, benchmarks.
#   tools/speed_report.sh <label>      -> tests/manual/baseline/<label>.md
# Run it before and after a change and diff the two files.
set -u
cd "$(dirname "$0")/.."
ROOT=$PWD
LABEL=${1:-$(git rev-parse --short HEAD)}
OUT=tests/manual/baseline/$LABEL.md
mkdir -p tests/manual/baseline
ZEN=$ROOT/build_release/bin/zen
TIMEFORMAT=%3R
wall() { { time "$@" >/dev/null 2>&1; } 2>&1 | tail -1; }
best3() { local b=999; for i in 1 2 3; do t=$(wall "$@"); b=$(echo "$t $b" | awk '{print ($1<$2)?$1:$2}'); done; echo $b; }

{
echo "# Speed report: $LABEL ($(date +%F), commit $(git rev-parse --short HEAD), branch $(git branch --show-current))"
echo
echo "## Clean build time (ninja, wall seconds, $(nproc) cores)"
for d in build_release build; do
  [ -d $d ] || continue
  (cd $d && ninja -t clean >/dev/null 2>&1)
  t=$(wall sh -c "cd $d && ninja >/dev/null 2>&1")
  cfg=$(grep -E 'CMAKE_BUILD_TYPE:' $d/CMakeCache.txt | cut -d= -f2)
  echo "- $d ($cfg): $t s"
done
echo
echo "## Sizes (bytes)"
for f in build_release/bin/zen build_release/libzen/libzen.a build/libzen/libzen.a; do
  [ -f $f ] && echo "- $f: $(stat -c %s $f)"
done
echo
echo "## Test suite (release, wall seconds)"
echo "- run_tests.sh: $(wall sh -c "ZEN=$ZEN ./run_tests.sh >/dev/null 2>&1")"
echo
echo "## Cross-language benchmarks (zen, best of 3, seconds)"
for f in fib for_loop for_loop_local method_call binary_trees; do
  printf -- "- %-16s %s\n" $f $(best3 $ZEN tests/manual/cross_lang_bench/zen/$f.py)
done
echo
echo "## Microbenchmarks (best of 3, seconds)"
for f in call_base call_f0 call_f3 call_fbig call_m_typed call_m_dyn call_m_void typedparam selffield foreach forrange forrange_empty; do
  printf -- "- %-16s %s\n" $f $(best3 sh -c "cd tests/manual/microbench && $ZEN $f.py")
done
echo
echo "## algo_bench (best of 3, seconds per phase)"
LC_NUMERIC=C tests/manual/algo_bench/run.sh 2>/dev/null | grep -E "^ |^zen "
echo
echo "## Script compile time (100 x zen --check, best of 3, seconds; includes process start)"
for f in tests/22_stress.py tests/23_edge_brutal.py tests/manual/algo_bench/py/spatial.py; do printf -- "- %-40s %s (100 runs)\n" $f $(best3 sh -c "for i in \$(seq 100); do $ZEN --check $f; done"); done
} > $OUT
echo "saved $OUT"
