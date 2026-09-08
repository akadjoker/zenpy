import time
def loop():
    for i in range(20000000):
        pass
start = time.perf_counter()
loop()
print("elapsed:", time.perf_counter() - start)
