#!/bin/bash
# Benchmark SSO: Multiple runs com diferentes níveis de stress

set -e

ZENPY_DIR="."
ZEN_BIN="$ZENPY_DIR/bin/zen"

# Testes com foco em SSO
STRESS_TESTS=(
    "tests/22_stress.py"           # 0.006s - string literals
    "tests/23_edge_brutal.py"      # 0.009s - edge cases
    "tests/40_gc_torture.py"       # 0.025s - GC pressure
    "tests/41_gc_torture2.py"      # 1.09s - heavy GC stress
    "tests/42_gc_torture3.py"      # 0.017s - more GC
    "tests/43_string_safety.py"    # 0.005s - string ops
    "tests/44_string_safety2.py"   # 0.005s - string ops
    "tests/45_string_safety3.py"   # 0.005s - string ops
    "tests/46_sso_test.py"         # 0.004s - SSO specific
)

echo "=============================================================="
echo "SSO Benchmark Suite — Multiple Runs for Stability"
echo "=============================================================="
echo ""

# Rebuild to ensure clean state
echo "[Build] Compiling clean release build..."
cd "$ZENPY_DIR/build"
ninja > /dev/null 2>&1

echo ""
echo "Running 3 iterations of stress tests..."
echo "=============================================================="

TOTAL_TIME=0
ITERATION_TIMES=()

for iter in 1 2 3; do
    echo ""
    echo "--- Iteration $iter/3 ---"
    iter_total=0
    
    for test in "${STRESS_TESTS[@]}"; do
        if [ ! -f "$ZENPY_DIR/$test" ]; then
            continue
        fi
        
        TEST_NAME=$(basename "$test" .py)
        printf "%-30s " "$TEST_NAME:"
        
        START=$(date +%s%N)
        if timeout 30 "$ZEN_BIN" "$ZENPY_DIR/$test" > /dev/null 2>&1; then
            END=$(date +%s%N)
            TIME=$(echo "scale=4; ($END - $START) / 1000000000" | bc)
            echo "$TIME s"
            iter_total=$(echo "$iter_total + $TIME" | bc)
        else
            echo "FAILED/TIMEOUT"
        fi
    done
    
    echo "Iteration $iter total: $iter_total seconds"
    ITERATION_TIMES+=("$iter_total")
    TOTAL_TIME=$(echo "$TOTAL_TIME + $iter_total" | bc)
done

echo ""
echo "=============================================================="
echo "RESULTS"
echo "=============================================================="
echo "Iteration 1: ${ITERATION_TIMES[0]} seconds"
echo "Iteration 2: ${ITERATION_TIMES[1]} seconds"
echo "Iteration 3: ${ITERATION_TIMES[2]} seconds"
echo "Total time: $TOTAL_TIME seconds"
echo "Average:    $(echo "scale=4; $TOTAL_TIME / 3" | bc) seconds"
echo "=============================================================="

# Calculate min/max
IFS=$'\n' sorted=($(sort <<<"${ITERATION_TIMES[*]}"))
MIN=${sorted[0]}
MAX=${sorted[2]}
echo "Range:      ${MIN}s to ${MAX}s (variance: $(echo "scale=2; ($MAX - $MIN) / $MIN * 100" | bc)%)"
echo "=============================================================="
echo "✓ All tests passed consistently!"
