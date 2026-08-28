# Minimized fuzzer findings: malformed / deeply-nested source must fail the
# COMPILER cleanly (caught by run_tests.sh expecting exit 0 on the good cases
# below). The crashing inputs — 1000 parens, 200 braces — are covered by the
# C++/shell harness because they must NOT compile; here we assert the VM still
# accepts legal deep nesting, which the depth guard must not have broken.

# Legal nesting well under the guard still runs.
assert ((((((((1)))))))) == 1
assert [[[[[[1]]]]]][0][0][0][0][0][0] == 1
assert {"a": {"b": {"c": 3}}}["a"]["b"]["c"] == 3

# A long-but-flat expression is fine (not depth, just length).
x = 1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1
assert x == 20

# Comprehensions and mixed brackets nest fine.
total = 0
for i in [i for i in range(10)]:
    total += i
assert total == 45
assert [[1, 2], [3, 4]][1][0] == 3

print("Parser limit tests passed.")
