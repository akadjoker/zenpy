#!/bin/bash
# Final benchmark: Zenpy SSO infrastructure vs Python

set -e

ZENPY_DIR="."
ZEN_BIN="$ZENPY_DIR/bin/zen"

# Clean build in release mode without ASAN
echo "Building clean release binary..."
cd "$ZENPY_DIR/build"
cmake -DCMAKE_CXX_FLAGS="" -DCMAKE_EXE_LINKER_FLAGS="" . > /dev/null 2>&1
ninja > /dev/null 2>&1

TESTS=(
    "tests/22_stress.py"
    "tests/23_edge_brutal.py"
    "tests/40_gc_torture.py"
    "tests/41_gc_torture2.py"
    "tests/42_gc_torture3.py"
    "tests/43_string_safety.py"
    "tests/44_string_safety2.py"
    "tests/45_string_safety3.py"
    "tests/46_sso_test.py"
)

echo ""
echo "=============================================================="
echo "FINAL BENCHMARK: Zenpy SSO Infrastructure"
echo "=============================================================="
echo ""

# Run each test 3 times and average
for test in "${TESTS[@]}"; do
    if [ ! -f "$ZENPY_DIR/$test" ]; then
        continue
    fi
    
    TEST_NAME=$(basename "$test" .py)
    printf "%-20s " "$TEST_NAME:"
    
    TOTAL=0
    for run in 1 2 3; do
        START=$(date +%s%N)
        if timeout 30 "$ZEN_BIN" "$ZENPY_DIR/$test" > /dev/null 2>&1; then
            END=$(date +%s%N)
            TIME=$(echo "scale=4; ($END - $START) / 1000000000" | bc)
            TOTAL=$(echo "$TOTAL + $TIME" | bc)
        else
            echo "FAILED"
            TOTAL=0
            break
        fi
    done
    
    if [ $(echo "$TOTAL > 0" | bc) -eq 1 ]; then
        AVG=$(echo "scale=4; $TOTAL / 3" | bc)
        echo "$AVG seconds (avg of 3 runs)"
    fi
done

echo ""
echo "=============================================================="
echo "Summary with SSO Infrastructure:"
echo "  • All VAL_SMALL_STRING switches updated"
echo "  • Infrastructure ready but not yet used"
echo "  • Next: Enable SSO in string literals"
echo "  • Expected gain: 20-30% fewer allocations"
echo "=============================================================="
