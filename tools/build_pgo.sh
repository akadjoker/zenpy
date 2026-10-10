#!/usr/bin/env bash
# build_pgo.sh — profile-guided build of the zen binary.
#
# The interpreter's execute() is one very large function, and the compiler
# has to guess which of its branches are hot. PGO replaces the guess with
# counts from a real run: same source, better code layout. Measured at -10%
# on integer arithmetic and -3% on the game benchmark.
#
#   tools/build_pgo.sh              # build with the default workload
#   tools/build_pgo.sh my.py ...    # profile on your own scripts instead
#
# Result: bin/zen. Intermediate trees are build_pgo_gen/ and build_pgo_use/.
#
# Re-run it after any substantial change to the interpreter — a stale profile
# is not wrong, just less useful, and gcc warns about source it cannot match.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

GEN_DIR=build_pgo_gen
USE_DIR=build_pgo_use
PROFILE_DIR="$ROOT/$GEN_DIR/profile"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

# The workload decides what gets optimised. The default covers the shapes the
# VM actually runs: recursion and calls, spatial queries, float-heavy physics,
# component arrays, and a tight integer loop.
BENCH_DIR=tests/manual/algo_bench/zen_typed
if [[ $# -gt 0 ]]; then
    WORKLOAD=("$@")
else
    WORKLOAD=(
        "$BENCH_DIR/hanoi.py"
        "$BENCH_DIR/pathfind.py"
        "$BENCH_DIR/spatial.py"
        "$BENCH_DIR/gamebench.py"
    )
fi

for w in "${WORKLOAD[@]}"; do
    [[ -f "$w" ]] || { echo "workload not found: $w" >&2; exit 2; }
done

echo "==> 1/3  instrumented build ($GEN_DIR)"
rm -rf "$GEN_DIR" "$PROFILE_DIR"
mkdir -p "$PROFILE_DIR"
cmake -S . -B "$GEN_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -fprofile-generate=$PROFILE_DIR" \
    -DCMAKE_EXE_LINKER_FLAGS="-fprofile-generate=$PROFILE_DIR" >/dev/null
cmake --build "$GEN_DIR" -j"$JOBS" >/dev/null

GEN_BIN="$ROOT/bin/zen"
[[ -x "$GEN_BIN" ]] || { echo "instrumented binary not found at $GEN_BIN" >&2; exit 1; }

echo "==> 2/3  profiling run"
for w in "${WORKLOAD[@]}"; do
    printf '     %s\n' "$w"
    "$GEN_BIN" "$w" >/dev/null 2>&1 || echo "     (workload exited non-zero, profile still counts)"
done

count=$(find "$PROFILE_DIR" -name '*.gcda' 2>/dev/null | wc -l)
[[ "$count" -gt 0 ]] || { echo "no profile data written — nothing to use" >&2; exit 1; }
echo "     $count profile files"

echo "==> 3/3  optimised build ($USE_DIR)"
rm -rf "$USE_DIR"
# -fprofile-correction: the profile is single-threaded here, but keep it
# tolerant of counter races. -Wno-missing-profile: files the workload never
# reached (the fuzzers, unused modules) have no data and that is expected.
cmake -S . -B "$USE_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -fprofile-use=$PROFILE_DIR -fprofile-correction -Wno-missing-profile" \
    -DCMAKE_EXE_LINKER_FLAGS="-fprofile-use=$PROFILE_DIR" >/dev/null
cmake --build "$USE_DIR" -j"$JOBS" >/dev/null

echo
echo "bin/zen is now the PGO build."
echo "Verify it before trusting it:  ./run_tests.sh"
