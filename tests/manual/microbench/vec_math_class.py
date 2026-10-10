# Same loop as vec_math.py on a script class: every + and * allocates an
# instance and runs a script __add__ / __mul__ call.
import time

class V3:
    def __init__(self, x, y, z):
        self.x = x
        self.y = y
        self.z = z
    def __add__(self, o):
        return V3(self.x + o.x, self.y + o.y, self.z + o.z)
    def __mul__(self, k):
        return V3(self.x * k, self.y * k, self.z * k)

def run(n):
    p = V3(0.0, 0.0, 0.0)
    v = V3(1.0, 2.0, 3.0)
    i = 0
    while i < n:
        p = p + v * 0.016
        i += 1
    return p

start = time.perf_counter()
p = run(5000000)
print("result:", p.x, p.y, p.z)
print("elapsed:", time.perf_counter() - start)
