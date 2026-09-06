import time
def loop():
    s = 0
    for i in range(5000000):
        s = s + i
    return s
start = time.perf_counter()
print(loop())
print("elapsed:", time.perf_counter() - start)
