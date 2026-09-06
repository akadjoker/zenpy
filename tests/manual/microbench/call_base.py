import time
def loop(n):
    i = 0
    while i < n:
        i = i + 1
start = time.perf_counter()
loop(5000000)
print("elapsed:", time.perf_counter() - start)
