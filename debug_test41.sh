#!/bin/bash
# Debug test 41 intermittent crash

set -e

ZEN_BIN="./bin/zen"
TEST_FILE="./tests/41_gc_torture2.py"

echo "Debugging intermittent crash in test 41..."
echo "Running test 20 times to capture crash..."
echo ""

CRASHES=0
PASSES=0

for i in {1..20}; do
    printf "[%2d/20] " "$i"
    
    if timeout 15 "$ZEN_BIN" "$TEST_FILE" > /tmp/test41_out.txt 2>&1; then
        PASSES=$((PASSES + 1))
        echo "✓ PASS"
    else
        EXIT_CODE=$?
        CRASHES=$((CRASHES + 1))
        
        # Check for ASAN/LSAN errors
        if grep -q "LeakSanitizer\|AddressSanitizer" /tmp/test41_out.txt 2>/dev/null; then
            echo "✗ CRASH (Sanitizer error)"
            head -20 /tmp/test41_out.txt | grep -E "ERROR|SUMMARY|LEAK" | head -3
        elif grep -q "Segmentation" /tmp/test41_out.txt 2>/dev/null; then
            echo "✗ CRASH (Segfault)"
        else
            echo "✗ TIMEOUT or unknown error (exit code $EXIT_CODE)"
        fi
    fi
    
    # Cool down between tests
    sleep 0.1
done

echo ""
echo "=============================================================="
echo "Results: $PASSES passes, $CRASHES crashes"
echo "Crash rate: $(echo "scale=1; $CRASHES * 100 / 20" | bc)%"
echo "=============================================================="

if [ $CRASHES -gt 0 ]; then
    echo ""
    echo "Last crash output (if available):"
    tail -30 /tmp/test41_out.txt 2>/dev/null | grep -E "ERROR|SUMMARY|LEAK|signal" || echo "(No error details)"
    exit 1
fi
