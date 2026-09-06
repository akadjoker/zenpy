import time
def loop():
    sum = 0
    i = 0
    while i < 5000000:
        sum = sum + i
        i = i + 1
    return sum
start = time.perf_counter()
print(loop())
print("elapsed:", time.perf_counter() - start)
