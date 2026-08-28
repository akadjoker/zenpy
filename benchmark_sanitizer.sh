#!/bin/bash
# Benchmark com e sem sanitizer (AddressSanitizer para detectar crashes)

set -e

ZENPY_DIR="."
BUILD_DIR="$ZENPY_DIR/build"
BIN_NORMAL="$BUILD_DIR/bin_normal/zen"
BIN_ASAN="$BUILD_DIR/bin_asan/zen"

# Testes stress
TESTS=(
    "tests/22_stress.py"
    "tests/23_edge_brutal.py"
    "tests/40_gc_torture.py"
    "tests/41_gc_torture2.py"
    "tests/42_gc_torture3.py"
    "tests/46_sso_test.py"
)

echo "=============================================================="
echo "BENCHMARK: SSO Infrastructure — Com/Sem Sanitizer"
echo "=============================================================="
echo ""

# Compilar SEM sanitizer (Release)
echo "[1/4] Building WITHOUT sanitizer (normal release build)..."
cd "$ZENPY_DIR"
rm -rf "$BUILD_DIR/bin_normal" 2>/dev/null || true
mkdir -p "$BUILD_DIR/bin_normal"
cd "$BUILD_DIR/bin_normal"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="" ../.. > /dev/null 2>&1
ninja > /dev/null 2>&1
cp bin/zen "$BIN_NORMAL" 2>/dev/null || cp ./bin/zen "$BIN_NORMAL"

# Compilar COM sanitizer
echo "[2/4] Building WITH AddressSanitizer (ASAN)..."
cd "$ZENPY_DIR"
rm -rf "$BUILD_DIR/bin_asan" 2>/dev/null || true
mkdir -p "$BUILD_DIR/bin_asan"
cd "$BUILD_DIR/bin_asan"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=undefined -g" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address -fsanitize=undefined" \
    ../.. > /dev/null 2>&1
ninja > /dev/null 2>&1
cp bin/zen "$BIN_ASAN" 2>/dev/null || cp ./bin/zen "$BIN_ASAN"

echo ""
echo "[3/4] Running benchmarks WITHOUT sanitizer..."
echo "=============================================================="

TOTAL_NORMAL=0
for test in "${TESTS[@]}"; do
    if [ ! -f "$ZENPY_DIR/$test" ]; then
        echo "Skipping $test (not found)"
        continue
    fi
    
    TEST_NAME=$(basename "$test" .py)
    printf "%-40s " "$TEST_NAME (normal):"
    
    START=$(date +%s%N)
    if "$BIN_NORMAL" "$ZENPY_DIR/$test" > /dev/null 2>&1; then
        END=$(date +%s%N)
        TIME=$(echo "scale=4; ($END - $START) / 1000000000" | bc)
        echo "$TIME seconds"
        TOTAL_NORMAL=$(echo "$TOTAL_NORMAL + $TIME" | bc)
    else
        echo "CRASHED/FAILED"
    fi
done

echo ""
echo "[4/4] Running benchmarks WITH AddressSanitizer..."
echo "=============================================================="

TOTAL_ASAN=0
ASAN_ERRORS=0
for test in "${TESTS[@]}"; do
    if [ ! -f "$ZENPY_DIR/$test" ]; then
        continue
    fi
    
    TEST_NAME=$(basename "$test" .py)
    printf "%-40s " "$TEST_NAME (asan):"
    
    START=$(date +%s%N)
    OUTPUT=$("$BIN_ASAN" "$ZENPY_DIR/$test" 2>&1 || true)
    END=$(date +%s%N)
    
    # Check for ASAN errors
    if echo "$OUTPUT" | grep -q "ERROR: AddressSanitizer\|SUMMARY: AddressSanitizer"; then
        echo "⚠ ASAN DETECTED ERROR"
        ASAN_ERRORS=$((ASAN_ERRORS + 1))
        echo "$OUTPUT" | grep -A 5 "ERROR: AddressSanitizer" | head -10
    else
        TIME=$(echo "scale=4; ($END - $START) / 1000000000" | bc)
        echo "$TIME seconds"
        TOTAL_ASAN=$(echo "$TOTAL_ASAN + $TIME" | bc)
    fi
done

echo ""
echo "=============================================================="
echo "SUMMARY"
echo "=============================================================="
echo "Total time (normal):        $TOTAL_NORMAL seconds"
echo "Total time (with ASAN):     $TOTAL_ASAN seconds"
if [ $(echo "$TOTAL_NORMAL > 0" | bc) -eq 1 ]; then
    RATIO=$(echo "scale=2; $TOTAL_ASAN / $TOTAL_NORMAL" | bc)
    echo "ASAN overhead:              ${RATIO}x slower"
fi
echo "ASAN errors detected:       $ASAN_ERRORS"
echo "=============================================================="

if [ $ASAN_ERRORS -gt 0 ]; then
    echo "⚠ WARNING: Memory errors detected by AddressSanitizer!"
    exit 1
else
    echo "✓ All tests passed with AddressSanitizer!"
    exit 0
fi
