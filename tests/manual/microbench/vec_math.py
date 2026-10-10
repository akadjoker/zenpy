# Vec3 value type: 5M iterations of p = p + v * 0.016. No allocation, no GC.
# Compare with vec_math_class.py (the same loop on a 3-field script class).
import time

def run(n):
    p = Vec3(0, 0, 0)
    v = Vec3(1, 2, 3)
    i = 0
    while i < n:
        p = p + v * 0.016
        i += 1
    return p

start = time.perf_counter()
p = run(5000000)
print("result:", p)
print("elapsed:", time.perf_counter() - start)
