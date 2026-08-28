#!/usr/bin/env bash
# run_tests.sh — Run all Zen VM tests with optional stress GC mode.
#
# Usage:
#   ./run_tests.sh              # normal run
#   ./run_tests.sh --stress-gc  # rebuild with stress GC, then run
#   ./run_tests.sh --filter 11  # run only tests matching "11"
#   ./run_tests.sh --verbose    # show output of failing tests
#
# Environment overrides (used by CI to test non-native builds):
#   ZEN=path/to/zen.exe         # interpreter binary to exercise
#   EMBED=path/to/test_embedding
#   ZEN_RUNNER=node             # launcher prefix, e.g. node for the wasm build

set -uo pipefail
shopt -s nullglob 2>/dev/null || true

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"
ZEN="${ZEN:-$ROOT/bin/zen}"
EMBED="${EMBED:-$ROOT/bin/test_embedding}"
ZEN_RUNNER="${ZEN_RUNNER:-}"

# With a runner the artifacts are scripts, not executables (node zen.js ...).
if [[ -n "$ZEN_RUNNER" ]]; then
    ZEN_CMD=("$ZEN_RUNNER" "$ZEN")
    EMBED_CMD=("$ZEN_RUNNER" "$EMBED")
    have_zen() { [[ -f "$ZEN" ]]; }
    have_embed() { [[ -f "$EMBED" ]]; }
else
    ZEN_CMD=("$ZEN")
    EMBED_CMD=("$EMBED")
    have_zen() { [[ -x "$ZEN" ]]; }
    have_embed() { [[ -x "$EMBED" ]]; }
fi

STRESS_GC=0
FILTER=""
VERBOSE=0

for arg in "$@"; do
    case "$arg" in
        --stress-gc) STRESS_GC=1 ;;
        --verbose|-v) VERBOSE=1 ;;
        --filter=*) FILTER="${arg#--filter=}" ;;
        --filter) shift_next=1 ;;
        *)
            if [[ "${shift_next:-0}" == "1" ]]; then
                FILTER="$arg"
                shift_next=0
            fi
            ;;
    esac
done

# --- Rebuild if needed ---
rebuild() {
    local extra_defs=""
    if [[ "$STRESS_GC" == "1" ]]; then
        extra_defs="-DCMAKE_CXX_FLAGS=-DZEN_DEBUG_STRESS_GC -DCMAKE_C_FLAGS=-DZEN_DEBUG_STRESS_GC"
    fi

    echo "=== Rebuilding (stress_gc=$STRESS_GC) ==="
    rm -rf "$BUILD"
    mkdir -p "$BUILD"
    cd "$BUILD"
    cmake .. -G Ninja $extra_defs -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1
    ninja 2>&1 | grep -E "error:|warning:|^\[" | tail -20
    cd "$ROOT"
    echo ""
}

if [[ "$STRESS_GC" == "1" ]]; then
    rebuild
elif ! have_zen; then
    echo "zen binary not found, building..."
    rebuild
fi

# --- Colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

# --- Run Python tests ---
passed=0
failed=0
skipped=0
failures=()

echo -e "${BOLD}=== Zen VM Test Suite ===${NC}"
if [[ "$STRESS_GC" == "1" ]]; then
    echo -e "${YELLOW}  (stress GC mode — GC at every allocation)${NC}"
fi
echo ""

for test_file in "$ROOT"/tests/[0-9]*.py; do
    name=$(basename "$test_file")

    # Filter
    if [[ -n "$FILTER" && "$name" != *"$FILTER"* ]]; then
        continue
    fi

    printf "  %-40s" "$name"

    output=$("${ZEN_CMD[@]}" "$test_file" 2>&1) && ret=0 || ret=$?

    if [[ $ret -eq 0 ]]; then
        echo -e "${GREEN}OK${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (exit $ret)"
        failed=$((failed + 1))
        failures+=("$name")
        if [[ "$VERBOSE" == "1" ]]; then
            echo "    --- output ---"
            echo "$output" | head -20 | sed 's/^/    /'
            echo "    ---"
        fi
    fi
done

echo ""

# --- Run C++ embedding tests ---
if have_embed; then
    printf "  %-40s" "test_embedding (C++)"
    output=$("${EMBED_CMD[@]}" 2>&1) && ret=0 || ret=$?
    if [[ $ret -eq 0 ]]; then
        echo -e "${GREEN}OK${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (exit $ret)"
        failed=$((failed + 1))
        failures+=("test_embedding")
        if [[ "$VERBOSE" == "1" ]]; then
            echo "$output" | tail -20 | sed 's/^/    /'
        fi
    fi
fi

# --- Host output hook test (only built with -DZEN_HOST_OUTPUT=ON) ---
HOSTOUT="${HOSTOUT:-$ROOT/bin/test_host_output}"
if [[ -n "$ZEN_RUNNER" ]]; then have_hostout() { [[ -f "$HOSTOUT" ]]; }
else have_hostout() { [[ -x "$HOSTOUT" ]]; }; fi
if have_hostout; then
    printf "  %-40s" "test_host_output (C++)"
    if [[ -n "$ZEN_RUNNER" ]]; then output=$("$ZEN_RUNNER" "$HOSTOUT" 2>&1) && ret=0 || ret=$?
    else output=$("$HOSTOUT" 2>&1) && ret=0 || ret=$?; fi
    if [[ $ret -eq 0 ]]; then
        echo -e "${GREEN}OK${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (exit $ret)"
        failed=$((failed + 1))
        failures+=("test_host_output")
        if [[ "$VERBOSE" == "1" ]]; then
            echo "$output" | tail -20 | sed 's/^/    /'
        fi
    fi
fi

# --- Summary ---
total=$((passed + failed))
echo ""
echo -e "${BOLD}=== Results ===${NC}"
echo -e "  Passed: ${GREEN}${passed}${NC}"
echo -e "  Failed: ${RED}${failed}${NC}"
echo -e "  Total:  ${total}"

if [[ ${#failures[@]} -gt 0 ]]; then
    echo ""
    echo -e "${RED}Failed tests:${NC}"
    for f in "${failures[@]}"; do
        echo "  - $f"
    done
    exit 1
fi

exit 0
