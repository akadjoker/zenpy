#!/bin/bash
echo "=== DETAILED BENCHMARK — SSO Infrastructure ==="
echo "Running each test 3 times for consistency..."
echo ""

declare -A times
tests=(22_stress.py 23_edge_brutal.py 40_gc_torture.py 41_gc_torture2.py 42_gc_torture3.py 43_string_safety.py 44_string_safety2.py 45_string_safety3.py 46_sso_test.py)

for test in "${tests[@]}"; do
  echo -n "$test: "
  total=0
  for run in 1 2 3; do
    # Run test and capture time
    start=$(date +%s%N)
    ./bin/zen "tests/$test" > /dev/null 2>&1
    end=$(date +%s%N)
    elapsed=$(echo "scale=4; ($end - $start) / 1000000000" | bc)
    total=$(echo "scale=4; $total + $elapsed" | bc)
    echo -n "$elapsed "
  done
  avg=$(echo "scale=4; $total / 3" | bc)
  times[$test]=$avg
  echo "| avg: ${avg}s"
done

echo ""
echo "Summary:"
total_time=0
for test in "${tests[@]}"; do
  echo "  ${test}: ${times[$test]}s"
  total_time=$(echo "scale=4; $total_time + ${times[$test]}" | bc)
done

echo ""
echo "Total average time: ${total_time}s"
