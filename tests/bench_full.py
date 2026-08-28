import time

# --- 1. Fibonacci recursivo ---
def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

t0 = time.perf_counter()
r = fib(30)
t1 = time.perf_counter()
print("fib(30) = " + str(r) + "  time: " + str(t1 - t0))

# --- 2. While loop aritmético ---
t0 = time.perf_counter()
s = 0
i = 0
while i < 2000000:
    s = s + i
    i = i + 1
t1 = time.perf_counter()
print("sum(2M) = " + str(s) + "  time: " + str(t1 - t0))

# --- 3. Class method calls ---
class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y
    def dist(self):
        return self.x * self.x + self.y * self.y

t0 = time.perf_counter()
total = 0.0
i = 0
while i < 500000:
    p = Point(i, i + 1)
    total = total + p.dist()
    i = i + 1
t1 = time.perf_counter()
print("class ops(500K)  time: " + str(t1 - t0))

# --- 4. Array/list push + sum ---
t0 = time.perf_counter()
arr = []
i = 0
while i < 500000:
    arr.append(i)
    i = i + 1
s = 0
i = 0
while i < 500000:
    s = s + arr[i]
    i = i + 1
t1 = time.perf_counter()
print("list ops(500K) = " + str(s) + "  time: " + str(t1 - t0))
