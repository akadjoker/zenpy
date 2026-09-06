import time
class P:
    def __init__(self):
        self.a = 1
        self.b = 2
        self.c = 3
        self.d = 4
        self.e = 5
def sum5(p: P):
    return p.a + p.b + p.c + p.d + p.e
def loop(n):
    p = P()
    t = 0
    i = 0
    while i < n:
        t = t + sum5(p)
        i = i + 1
    return t
start = time.perf_counter()
print(loop(1000000))
print("elapsed:", time.perf_counter() - start)
