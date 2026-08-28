# Test generators / yield

def count_up(n):
    i = 0
    while i < n:
        yield i
        i = i + 1

for x in count_up(5):
    print(x)
# expected: 0 1 2 3 4

# Fibonacci generator
def fib(n):
    a = 0
    b = 1
    i = 0
    while i < n:
        yield a
        c = a + b
        a = b
        b = c
        i = i + 1

for f in fib(8):
    print(f)
# expected: 0 1 1 2 3 5 8 13

# Generator with arguments
def squares(start, stop):
    i = start
    while i < stop:
        yield i * i
        i = i + 1

for s in squares(3, 7):
    print(s)
# expected: 9 16 25 36

# Mixed: generator + array in same program
for x in [100, 200, 300]:
    print(x)
# expected: 100 200 300
