import time
class C:
    def __init__(self):
        self.a = 1
        self.b = 2
    def run(self, n):
        i = 0
        t = 0
        while i < n:
            x = self.a
            y = self.b
            t = t + x + y
            i = i + 1
        return t
start = time.perf_counter()
C().run(5000000)
print("elapsed:", time.perf_counter() - start)
