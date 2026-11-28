# expect: Cannot assign to this expression
def f():
    return 1

x, f() = 1, 2
