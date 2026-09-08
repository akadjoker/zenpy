import time
def f(a, b, c):
    return a
def loop(n):
    i = 0
    while i < n:
        f(i, i, i)
        i = i + 1
start = time.perf_counter()
loop(5000000)
print("elapsed:", time.perf_counter() - start)
