import time
def f(a):
    if a < 0:
        b1 = 1
        b2 = 2
        b3 = 3
        b4 = 4
        b5 = 5
        b6 = 6
        b7 = 7
        b8 = 8
        b9 = 9
        b10 = 10
        b11 = 11
        b12 = 12
        b13 = 13
        b14 = 14
        b15 = 15
        b16 = 16
        return b1 + b2 + b3 + b4 + b5 + b6 + b7 + b8 + b9 + b10 + b11 + b12 + b13 + b14 + b15 + b16
    return a
def loop(n):
    i = 0
    while i < n:
        f(i)
        i = i + 1
start = time.perf_counter()
loop(5000000)
print("elapsed:", time.perf_counter() - start)
