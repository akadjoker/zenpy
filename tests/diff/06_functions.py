def add(a, b=10, *args, **kwargs):
    return a + b + sum(args) + sum(kwargs.values())
print(add(1), add(1, 2), add(1, 2, 3, 4), add(1, b=5), add(1, 2, 3, x=10, y=20), add(b=1, a=2))
def rec(n):
    return 1 if n <= 1 else n * rec(n - 1)
print(rec(5), rec(10), rec(0))
def fib(n):
    a, b = 0, 1
    for _ in range(n):
        a, b = b, a + b
    return a
print(fib(10), fib(50), [fib(i) for i in range(8)])
def make_counter():
    count = 0
    def inc():
        nonlocal count
        count += 1
        return count
    return inc
c1 = make_counter(); c2 = make_counter()
print(c1(), c1(), c1(), c2(), c1())
def make_adders():
    return [lambda x, i=i: x + i for i in range(3)]
print([f(10) for f in make_adders()])
def late():
    fs = []
    for i in range(3):
        fs.append(lambda: i)
    return [f() for f in fs]
print(late())
g = 1
def read_global():
    return g
def write_global():
    global g
    g = 2
print(read_global()); write_global(); print(g, read_global())
def multi():
    return 1, 2, 3
a, b, c = multi(); print(a, b, c, multi(), multi()[1], type(multi()).__name__)
def default_mut(x, acc=None):
    if acc is None:
        acc = []
    acc.append(x)
    return acc
print(default_mut(1), default_mut(2), default_mut(3, [0]))
def kw_only(a, *, b, c=3):
    return a + b + c
print(kw_only(1, b=2), kw_only(1, c=1, b=1))
def star(*a, **k):
    return len(a), sorted(k.items())
print(star(), star(1, 2, x=1), star(*[1, 2, 3], **{"z": 26}))
sq = lambda x: x * x
print(sq(4), (lambda a, b: a * b)(3, 4), (lambda: "nil")(), list(map(sq, [1, 2, 3])))
def compose(f, g):
    return lambda x: f(g(x))
print(compose(sq, lambda x: x + 1)(3), compose(str, sq)(5))
def outer():
    x = "outer"
    def middle():
        def inner():
            return x
        return inner()
    return middle()
print(outer())
def shadow(x):
    x = x * 2
    return x
v = 5; print(shadow(v), v)
def mutate(lst):
    lst.append(99)
l = [1]; mutate(l); print(l)
def doc():
    """docstring"""
    return 1
print(doc(), doc.__name__)
def apply_n(f, n, x):
    for _ in range(n):
        x = f(x)
    return x
print(apply_n(lambda x: x * 2, 10, 1), apply_n(str.upper if False else (lambda s: s + "!"), 3, "hi"))
def gen_args(*args):
    return args
print(gen_args(), gen_args(1), gen_args(1, 2), gen_args(*"ab"))
def ret_none():
    pass
print(ret_none(), ret_none() is None, type(ret_none()))
def early(x):
    if x < 0:
        return "neg"
    for i in range(x):
        if i == 2:
            return "found"
    return "none"
print(early(-1), early(5), early(1))
print((lambda *a: sum(a))(1, 2, 3), (lambda **k: k)(a=1), (lambda x=5: x)())
def hanoi(n, a, b, c):
    if n == 0:
        return 0
    return hanoi(n - 1, a, c, b) + 1 + hanoi(n - 1, c, b, a)
print(hanoi(10, "a", "b", "c"))
