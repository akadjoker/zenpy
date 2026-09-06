import time
class C:
    def __init__(self):
        self.v = 1
    def m(self):
        return self.v
def run(xs, n):
    total = 0
    i = 0
    while i < n:
        for b in xs:
            total = total + b.m()
        i = i + 1
    return total
xs = []
k = 0
while k < 1000:
    xs.append(C())
    k = k + 1
start = time.perf_counter()
print(run(xs, 2000))
print("elapsed:", time.perf_counter() - start)
