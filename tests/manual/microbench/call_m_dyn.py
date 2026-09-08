import time
class C:
    def __init__(self):
        self.v = 0
    def m(self):
        return self.v
def make():
    return C()
def loop(n):
    o = make()
    i = 0
    while i < n:
        o.m()
        i = i + 1
start = time.perf_counter()
loop(5000000)
print("elapsed:", time.perf_counter() - start)
