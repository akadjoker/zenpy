# Test nested indentation: loops + if/elif/else

x = 0
i = 0
while i < 5:
    if i == 0:
        x = x + 1
    elif i == 2:
        x = x + 10
    else:
        x = x + 100
    i = i + 1

print(x)
# expected: 1 + 100 + 10 + 100 + 100 = 311

# Nested loops
total = 0
i = 0
while i < 3:
    j = 0
    while j < 3:
        if i == j:
            total = total + 1
        j = j + 1
    i = i + 1

print(total)
# expected: 3 (diagonal: 0,0  1,1  2,2)

# Triple nesting
result = 0
a = 0
while a < 2:
    b = 0
    while b < 2:
        c = 0
        while c < 2:
            result = result + 1
            c = c + 1
        b = b + 1
    a = a + 1

print(result)
# expected: 8 (2*2*2)

# If inside if inside while
val = 0
n = 0
while n < 10:
    if n > 2:
        if n < 7:
            val = val + n
    n = n + 1

print(val)
# expected: 3+4+5+6 = 18
