import time
class C:
    def __init__(self):
        self.v = 0
    def update(self):
        self.v = self.v + 1
def loop(n):
    o: C = C()
    i = 0
    while i < n:
        o.update()
        i = i + 1
start = time.perf_counter()
loop(5000000)
print("elapsed:", time.perf_counter() - start)
