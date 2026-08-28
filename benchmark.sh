#!/usr/bin/env bash
set -euo pipefail

ZEN=${ZEN:-./bin/zen}
PYTHON=${PYTHON:-python3}
SCRIPTS_DIR=${SCRIPTS_DIR:-scripts/bench}
REPEAT=${REPEAT:-5}
FILTER=${FILTER:-}
ALLOW_DEBUG_BENCH=${ALLOW_DEBUG_BENCH:-0}

BENCHMARKS=(
  "bench_int_arith.py:Int arithmetic"
  "bench_list_ops.py:List append + sum"
  "bench_map_insert.py:Map insert + iterate"
  "bench_map_heavy.py:Map heavy workload"
  "bench_string_concat.py:String concat"
  "bench_string_repeat.py:String repeat"
  "bench_split.py:String split"
  "bench_float_parse.py:Float parse"
  "bench_recursion.py:Recursion"
)

usage() {
  cat <<'EOF'
Usage: ./benchmark.sh [--list] [--repeat N] [--bench FILE]

Options:
  --list         List available benchmarks
  --repeat N     Number of measured runs per runtime (default: 5)
  --bench FILE   Run only one benchmark file from scripts/bench

Environment:
  ZEN=./bin/zen
  PYTHON=python3
  SCRIPTS_DIR=scripts/bench
  ALLOW_DEBUG_BENCH=1   Override the debug-build guard
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --list)
      for entry in "${BENCHMARKS[@]}"; do
        printf '%s\n' "${entry%%:*}"
      done
      exit 0
      ;;
    --repeat)
      REPEAT="$2"
      shift 2
      ;;
    --bench)
      FILTER="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if ! command -v "$PYTHON" >/dev/null 2>&1; then
  echo "Python runtime not found: $PYTHON" >&2
  exit 1
fi

if [[ ! -x "$ZEN" ]]; then
  echo "Zen runtime not found or not executable: $ZEN" >&2
  exit 1
fi

if [[ ! -d "$SCRIPTS_DIR" ]]; then
  echo "Benchmark directory not found: $SCRIPTS_DIR" >&2
  exit 1
fi

detect_cmake_build_type() {
  local runtime=$1
  local runtime_dir
  local repo_root
  local cache_file

  runtime_dir=$(cd "$(dirname "$runtime")" && pwd)
  repo_root=$(cd "$runtime_dir/.." && pwd)
  cache_file="$repo_root/build/CMakeCache.txt"

  if [[ -f "$cache_file" ]]; then
    awk -F= '/^CMAKE_BUILD_TYPE:STRING=/{print $2; exit}' "$cache_file"
  fi
}

enforce_release_runtime() {
  local runtime=$1
  local runtime_name
  local build_type

  runtime_name=$(basename "$runtime")
  build_type=$(detect_cmake_build_type "$runtime" || true)

  if [[ "$ALLOW_DEBUG_BENCH" == "1" ]]; then
    return 0
  fi

  if [[ "$runtime_name" != "zen" ]]; then
    echo "Refusing to benchmark non-standard runtime: $runtime" >&2
    echo "Use ./bin/zen from a Release/RelWithDebInfo build, or set ALLOW_DEBUG_BENCH=1 to override." >&2
    exit 1
  fi

  if [[ -n "$build_type" && "$build_type" != "Release" && "$build_type" != "RelWithDebInfo" && "$build_type" != "MinSizeRel" ]]; then
    echo "Refusing to benchmark non-optimized build type: $build_type" >&2
    echo "Rebuild with: cmake -B build -DCMAKE_BUILD_TYPE=Release && ninja -C build" >&2
    echo "Or set ALLOW_DEBUG_BENCH=1 if you intentionally want debug numbers." >&2
    exit 1
  fi
}

time_cmd() {
  local runtime=$1
  local script=$2
  "$PYTHON" - "$runtime" "$script" <<'EOF'
import subprocess
import sys
import time

runtime = sys.argv[1]
script = sys.argv[2]
start = time.perf_counter()
subprocess.run([runtime, script], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
elapsed = time.perf_counter() - start
print(f"{elapsed:.9f}")
EOF
}

summarize_runs() {
  awk '
    BEGIN { min = -1; sum = 0; n = 0 }
    {
      v = $1 + 0
      if (min < 0 || v < min) min = v
      sum += v
      n++
    }
    END {
      if (n == 0) exit 1
      printf "%.6f %.6f", min, sum / n
    }
  '
}

measure_runtime() {
  local runtime=$1
  local script=$2
  local runs=()
  local i
  local elapsed

  if ! time_cmd "$runtime" "$script" >/dev/null; then
    echo "Benchmark warm-up failed: $runtime $script" >&2
    exit 1
  fi

  for ((i = 0; i < REPEAT; i++)); do
    if ! elapsed=$(time_cmd "$runtime" "$script"); then
      echo "Benchmark run failed: $runtime $script" >&2
      exit 1
    fi
    runs+=("$elapsed")
  done

  printf '%s\n' "${runs[@]}" | summarize_runs
}

enforce_release_runtime "$ZEN"

printf '========================================\n'
printf 'Zen vs Python benchmark suite\n'
printf 'runs per benchmark: %s\n' "$REPEAT"
printf '========================================\n\n'
printf '%-24s %-14s %-14s %-10s\n' 'Benchmark' 'Zen avg(s)' 'Python avg(s)' 'Ratio'
printf '%-24s %-14s %-14s %-10s\n' '------------------------' '------------' '--------------' '----------'

for entry in "${BENCHMARKS[@]}"; do
  file=${entry%%:*}
  name=${entry#*:}
  script="$SCRIPTS_DIR/$file"

  if [[ -n "$FILTER" && "$file" != "$FILTER" ]]; then
    continue
  fi

  if [[ ! -f "$script" ]]; then
    echo "Missing benchmark script: $script" >&2
    exit 1
  fi

  zen_stats=$(measure_runtime "$ZEN" "$script")
  py_stats=$(measure_runtime "$PYTHON" "$script")
  zen_best=${zen_stats%% *}
  zen_avg=${zen_stats#* }
  py_best=${py_stats%% *}
  py_avg=${py_stats#* }
  ratio=$(awk -v py="$py_avg" -v zen="$zen_avg" 'BEGIN { if (zen == 0) print "inf"; else printf "%.2fx", py / zen }')

  printf '%-24s %-14s %-14s %-10s\n' "$name" "$zen_avg" "$py_avg" "$ratio"
  printf '  best run:               %-14s %-14s\n' "$zen_best" "$py_best"
done

printf '\nNotes:\n'
printf '  - Ratios are Python avg / Zen avg for the same script.\n'
printf '  - Use --bench FILE to inspect a single workload.\n'
printf '  - Keep this script for reproducible numbers; use perf separately when profiling internals.\n'
