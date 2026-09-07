# expect: argument unpacking requires a list
def f(*a):
    return len(a)
f(*5)
