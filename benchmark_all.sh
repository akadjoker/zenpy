#!/bin/bash
echo "=== FULL BENCHMARK — SSO Infrastructure (no usage yet) ==="
echo ""

TOTAL_TIME=0
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

for test in "${TESTS[@]}"; do
  name=$(basename "$test")
  result=$(/usr/bin/time -f "%e seconds" ./bin/zen "$test" 2>&1 | tail -1)
  echo "$name: $result"
  # Extract time in seconds
  time_sec=$(echo "$result" | grep seconds | awk '{print $1}')
  if [ ! -z "$time_sec" ]; then
    TOTAL_TIME=$(echo "$TOTAL_TIME + $time_sec" | bc)
  fi
done

echo ""
echo "Total time: $TOTAL_TIME seconds"
echo ""
echo "=== BENCHMARK COMPLETE ==="
