# A receiver whose class is known statically but is not the class being
# compiled — an annotated parameter, an inferred local, an Array[T] element —
# reads and writes fields through a checked direct index (OP_GETFIELD_IDXC /
# OP_SETFIELD_IDXC). The check compares the field name at that index, so a
# value of another class, a subclass, a dynamic field or a plain dict all
# behave exactly as the by-name access did.

class P:
    def __init__(self):
        self.a = 1
        self.b = 2
        self.c = 3
        self.d = 4
        self.e = 5

class Q:
    def __init__(self):
        self.e = 50
        self.d = 40
        self.c = 30
        self.b = 20
        self.a = 10

class Sub(P):
    def __init__(self):
        super().__init__()
        self.f = 6

def sum5(p: P):
    return p.a + p.b + p.c + p.d + p.e

def set_c(p: P, v):
    p.c = v
    return p.c

assert sum5(P()) == 15
assert sum5(Sub()) == 15      # subclass: same layout prefix
assert sum5(Q()) == 150       # other class, same names at other indices: by name
assert set_c(P(), 7) == 7
q = Q()
assert set_c(q, 33) == 33 and q.c == 33 and q.a == 10

# inferred local at module level
pp = P()
assert pp.d == 4
pp.d = 44
assert pp.d == 44 and sum5(pp) == 55

# inferred local inside a function, then reassigned to another class
def mixed():
    o = P()
    x = o.b
    o = Q()
    y = o.b
    return x, y
mx, my = mixed()
assert mx == 2 and my == 20

# dynamic field added later: the checked form falls back and the by-name
# store creates it, reads find it afterwards
def dyn(p: P):
    p.extra = 99
    return p.extra
assert dyn(P()) == 99

# Array[T] elements
ps: Array[P] = [P(), Sub(), P()]
total = 0
i = 0
while i < len(ps):
    total = total + ps[i].e
    ps[i].a = i
    i = i + 1
assert total == 15
assert ps[0].a == 0 and ps[1].a == 1 and ps[2].a == 2

# an annotation that is simply wrong for the value passed still works
class Other:
    def __init__(self):
        self.zzz = 1
        self.a = 100
def read_a(p: P):
    return p.a
assert read_a(Other()) == 100
assert read_a(Q()) == 10

# self-returning chain keeps the class for field access
class Builder:
    def __init__(self):
        self.n = 0
    def inc(self):
        self.n = self.n + 1
        return self
bd = Builder()
assert bd.inc().inc().n == 2

print("OK")

# builtin annotations are recorded like any other name: a str/int/list
# parameter must keep dynamic behaviour for methods, operators and fields
def builtin_hints(s: str, n: int, xs: list, d: dict):
    out = [s.upper(), n < 3, n + 1, xs[0], len(xs)]
    xs.append(n)
    d["k"] = s
    if n == None:
        out.append("none")
    return out
bh = builtin_hints("ab", 2, [7], {})
assert bh == ["AB", True, 3, 7, 1]

def generic_looking(a: int, b: int):
    return a < b, a <= b
gl1, gl2 = generic_looking(1, 2)
assert gl1 == True and gl2 == True
print("OK2")
