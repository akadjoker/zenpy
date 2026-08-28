# Test break, continue, and functions with nested control flow

# Basic break
total = 0
i = 0
while i < 10:
    if i == 5:
        break
    total = total + i
    i = i + 1
print(total)
# expected: 0+1+2+3+4 = 10

# Basic continue
total = 0
i = 0
while i < 10:
    i = i + 1
    if i % 2 == 0:
        continue
    total = total + i
print(total)
# expected: 1+3+5+7+9 = 25

# Break in nested loop
count = 0
i = 0
while i < 5:
    j = 0
    while j < 5:
        if j == 2:
            break
        count = count + 1
        j = j + 1
    i = i + 1
print(count)
# expected: 5 * 2 = 10 (inner breaks at j=2, so counts j=0,j=1 each outer iter)

# Continue in nested loop
count = 0
i = 0
while i < 4:
    j = 0
    while j < 4:
        j = j + 1
        if j == 2:
            continue
        count = count + 1
    i = i + 1
print(count)
# expected: 4 outer * 3 inner (j=1,3,4 counted, j=2 skipped) = 12

# Function with loop + break
def find_first_over(limit):
    i = 0
    while i < 100:
        if i * i > limit:
            return i
        i = i + 1
    return -1

print(find_first_over(50))
# expected: 8 (8*8=64 > 50)

# Function with nested if + loop
def classify(n):
    if n < 0:
        return "negative"
    elif n == 0:
        return "zero"
    else:
        if n < 10:
            return "small"
        elif n < 100:
            return "medium"
        else:
            return "large"

print(classify(-5))
print(classify(0))
print(classify(7))
print(classify(42))
print(classify(999))

# Function with while + continue + break
def sum_odd_until(limit):
    total = 0
    i = 0
    while i < 100:
        i = i + 1
        if i % 2 == 0:
            continue
        if i > limit:
            break
        total = total + i
    return total

print(sum_odd_until(10))
# expected: 1+3+5+7+9 = 25

# Nested functions with control flow
def outer(n):
    def inner(x):
        if x < 0:
            return 0
        return x * 2
    total = 0
    i = 0
    while i < n:
        val = inner(i - 2)
        total = total + val
        i = i + 1
    return total

print(outer(5))
# expected: inner(-2)=0, inner(-1)=0, inner(0)=0, inner(1)=2, inner(2)=4 → 6
