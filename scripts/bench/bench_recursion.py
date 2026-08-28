# bench_recursion.py
def fib(n):
    if n < 2:
        return n
    return fib(n-1) + fib(n-2)
print("fib:", fib(32))