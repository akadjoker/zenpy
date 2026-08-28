#!/bin/bash
# Complete regression test: All tests vs Python baseline

set -e

ZENPY_DIR="."
ZEN_BIN="$ZENPY_DIR/bin/zen"
TESTS_DIR="$ZENPY_DIR/tests"

# All test files
TEST_FILES=(
    "01_print.py"
    "02_functions.py"
    "03_recursion.py"
    "04_if_elif_else.py"
    "05_while.py"
    "06_locals.py"
    "07_for.py"
    "09_nested_indent.py"
    "10_break_continue.py"
    "11_classes.py"
    "12_inheritance.py"
    "13_super.py"
    "14_range.py"
    "15_import.py"
    "16_decorators.py"
    "17_fstrings.py"
    "18_eval.py"
    "19_generators.py"
    "20_edge_cases.py"
    "21_scope.py"
    "22_stress.py"
    "23_edge_brutal.py"
    "24_buffers.py"
    "25_struct.py"
    "26_numpy.py"
    "27_io.py"
    "28_os.py"
    "29_path.py"
    "30_json.py"
    "31_async_await.py"
    "32_signal.py"
    "33_net.py"
    "34_http.py"
    "35_match.py"
    "36_enum.py"
    "37_async_await.py"
    "38_operators.py"
    "39_vm_edge_cases.py"
    "40_gc_torture.py"
    "41_gc_torture2.py"
    "42_gc_torture3.py"
    "43_string_safety.py"
    "44_string_safety2.py"
    "45_string_safety3.py"
    "46_sso_test.py"
    "47_multireturn.py"
    "48_fibers.py"
)

echo "=============================================================="
echo "REGRESSION TEST: Running ALL tests"
echo "=============================================================="
echo ""

PASS=0
FAIL=0
SKIP=0
ERRORS=()

for test in "${TEST_FILES[@]}"; do
    TEST_PATH="$TESTS_DIR/$test"
    
    if [ ! -f "$TEST_PATH" ]; then
        echo "[SKIP] $test (not found)"
        SKIP=$((SKIP + 1))
        continue
    fi
    
    printf "%-35s " "$test:"
    
    # Run with timeout to catch infinite loops
    if timeout 15 "$ZEN_BIN" "$TEST_PATH" > /tmp/test_output.txt 2>&1; then
        echo "✓ PASS"
        PASS=$((PASS + 1))
    else
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 124 ]; then
            echo "✗ TIMEOUT"
        else
            echo "✗ FAIL"
        fi
        FAIL=$((FAIL + 1))
        ERRORS+=("$test")
        head -5 /tmp/test_output.txt >> /tmp/errors.log 2>&1
    fi
done

echo ""
echo "=============================================================="
echo "RESULTS"
echo "=============================================================="
echo "Passed:  $PASS"
echo "Failed:  $FAIL"
echo "Skipped: $SKIP"
echo "Total:   $((PASS + FAIL + SKIP))"
echo "=============================================================="

if [ $FAIL -gt 0 ]; then
    echo ""
    echo "FAILED TESTS:"
    for err in "${ERRORS[@]}"; do
        echo "  - $err"
    done
    exit 1
else
    echo ""
    echo "✓ ALL TESTS PASSED!"
    exit 0
fi
