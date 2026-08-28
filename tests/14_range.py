# Test range()

# Basic range(stop)
for i in range(5):
    print(i)
# expected: 0 1 2 3 4

# range(start, stop)
for i in range(2, 7):
    print(i)
# expected: 2 3 4 5 6

# range(start, stop, step)
for i in range(0, 10, 3):
    print(i)
# expected: 0 3 6 9

# Negative step
for i in range(5, 0, -1):
    print(i)
# expected: 5 4 3 2 1

# Sum using range
total = 0
for i in range(1, 101):
    total = total + i
print(total)
# expected: 5050

# Nested range loops
for i in range(3):
    for j in range(3):
        if i == j:
            print(i)
# expected: 0 1 2
