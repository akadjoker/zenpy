def abs(x):
    if x < 0:
        return 0 - x
    return x

def max(a, b):
    if a > b:
        return a
    return b

def clamp(x, lo, hi):
    if x < lo:
        return lo
    elif x > hi:
        return hi
    else:
        return x

print(abs(5))
print(abs(0 - 3))
print(max(10, 20))
print(max(99, 1))
print(clamp(5, 0, 10))
print(clamp(0 - 5, 0, 10))
print(clamp(15, 0, 10))
