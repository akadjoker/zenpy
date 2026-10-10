# expect: keyword arguments need a function the compiler can see
def f(a, b=1):
    return a + b
g = f
g(1, b=2)
