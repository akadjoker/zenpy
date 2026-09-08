import time

def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

start = time.perf_counter()
for i in range(1, 6):
    print(fib(28))
print("elapsed:", time.perf_counter() - start)
