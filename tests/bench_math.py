import math
import time

N = 1000000
acc = 0.0
t0 = time.perf_counter()
i = 0
while i < N:
    acc = acc + math.sin(0.5)
    i = i + 1
t1 = time.perf_counter()
print(acc)
print(t1 - t0)
