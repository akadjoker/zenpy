import time

def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

start = time.perf_counter()
i = 1
while i <= 5:
    print(fib(28))
    i = i + 1
print("elapsed:", time.perf_counter() - start)
