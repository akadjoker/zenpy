# expect: stack overflow
def r(n):
    return r(n + 1)
r(0)
