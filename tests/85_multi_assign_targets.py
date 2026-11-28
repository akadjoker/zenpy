# A multi-assign whose targets are attributes, indices and groups of them,
# not only names (2026-11-28). Runs unchanged under CPython; every line is
# an assert.

def two():
    return 3, 4


def three():
    return 5, 6, 7


class Box:
    def __init__(self):
        self.x = 0
        self.y = 0
        self.items = [0, 0, 0]
        self.table = {}

    def load(self, pair):
        self.x, self.y = pair

    def from_call(self):
        self.x, self.y = two()

    def swap(self):
        self.x, self.y = self.y, self.x

    def spread(self):
        self.items[0], self.items[2] = 7, 9
        self.table["a"], self.table["b"] = self.x, self.y

    def mixed(self):
        n, self.x, self.items[1] = 1, 2, 3
        return n

    def nested(self):
        (self.x, self.y), self.items[0] = (8, 9), 10


b = Box()
b.from_call()
assert b.x == 3 and b.y == 4
b.swap()
assert b.x == 4 and b.y == 3
b.load([1, 2])
assert b.x == 1 and b.y == 2
b.load((5, 6))
assert b.x == 5 and b.y == 6
b.spread()
assert b.items == [7, 0, 9] and b.table == {"a": 5, "b": 6}
assert b.mixed() == 1 and b.x == 2 and b.items == [7, 3, 9]
b.nested()
assert b.x == 8 and b.y == 9 and b.items == [10, 3, 9]


class Pair:
    pass


p = Pair()
q = Pair()
p.a, p.b = 1, 2
assert p.a == 1 and p.b == 2
p.a, p.b = p.b, p.a
assert p.a == 2 and p.b == 1
p.a, q.a, p.b = three()
assert p.a == 5 and q.a == 6 and p.b == 7
p.a, p.b = [10, 20]
assert p.a == 10 and p.b == 20
p.a, p.b = "xy"
assert p.a == "x" and p.b == "y"

xs = [1, 2, 3, 4]
i = 0
j = 3
xs[i], xs[j] = xs[j], xs[i]
assert xs == [4, 2, 3, 1]
xs[1], xs[2] = two()
assert xs == [4, 3, 4, 1]
xs[-1], xs[-2] = 0, 9
assert xs == [4, 3, 9, 0]
xs[i + 1], xs[j - 1] = "m", "n"
assert xs == [4, "m", "n", 0]

d = {}
d["a"], d["b"] = 1, 2
assert d == {"a": 1, "b": 2}
k = "c"
d[k], d["a"] = two()
assert d == {"a": 4, "b": 2, "c": 3}

n, p.a, xs[0], d["z"] = 1, 2, 3, 4
assert n == 1 and p.a == 2 and xs[0] == 3 and d["z"] == 4

(m, p.b), xs[1] = (7, 8), 9
assert m == 7 and p.b == 8 and xs[1] == 9
[m, p.a], xs[2] = [1, 2], 3
assert m == 1 and p.a == 2 and xs[2] == 3
m, (p.a, d["q"]) = 5, [6, 7]
assert m == 5 and p.a == 6 and d["q"] == 7
(m, p.a), (xs[0], d["r"]) = two(), [8, 9]
assert m == 3 and p.a == 4 and xs[0] == 8 and d["r"] == 9
m, [p.a, [p.b, xs[3]]] = 1, [2, [3, 4]]
assert m == 1 and p.a == 2 and p.b == 3 and xs[3] == 4

rows = [[0, 0], [0, 0]]
rows[0][0], rows[1][1] = 1, 2
assert rows == [[1, 0], [0, 2]]
p.items = [0, 0]
p.items[0], p.items[1] = two()
assert p.items == [3, 4]
p.inner = Pair()
p.inner.u, p.inner.v = 1, 2
assert p.inner.u == 1 and p.inner.v == 2


# The right-hand side runs first, then each target left to right.
log = []


def rhs():
    log.append("rhs")
    return 1, 2


def holder(name):
    log.append(name)
    return p


holder("a").a, holder("b").b = rhs()
assert log == ["rhs", "a", "b"]
assert p.a == 1 and p.b == 2

log = []
ys = [0, 0, 0]


def at(i):
    log.append(i)
    return i


ys[at(2)], ys[at(0)] = rhs()
assert log == ["rhs", 2, 0] and ys == [2, 0, 1]

# A name assigned earlier in the statement is already new when a later
# target reads it, and an index read earlier is still the old one.
zs = [0, 0, 0]
t = 0
t, zs[t] = 1, 5
assert t == 1 and zs == [0, 5, 0]
zs = [0, 0, 0]
t = 0
zs[t], t = 6, 2
assert t == 2 and zs == [6, 0, 0]
c = Pair()
e = Pair()
c, c.v = e, 9
assert c is e and e.v == 9


# The same inside functions: new names are locals, targets read the
# parameters, and a closure holds the object.
def in_function(obj, items):
    first, obj.x, items[0], last = 1, 2, 3, 4
    obj.y, items[1] = first + last, obj.x
    return first, last


box = Box()
data = [0, 0]
assert in_function(box, data) == (1, 4)
assert box.x == 2 and box.y == 5 and data == [3, 2]


def make():
    store = Pair()
    arr = [0, 0]

    def set_all(u, v):
        store.u, arr[0], store.w, arr[1] = u, v, u + v, u - v
        return store, arr
    return set_all


s, arr = make()(5, 2)
assert s.u == 5 and s.w == 7 and arr == [2, 3]

g = Pair()
garr = [0, 0]


def to_globals():
    g.a, garr[1] = two()


to_globals()
assert g.a == 3 and garr == [0, 4]


class Base:
    def __init__(self):
        self.a, self.b = 1, 2


class Derived(Base):
    def __init__(self):
        Base.__init__(self)
        self.c, self.d = self.b, self.a

    def total(self):
        self.a, self.b = self.a + 10, self.b + 10
        return self.a + self.b + self.c + self.d


dd = Derived()
assert (dd.a, dd.b, dd.c, dd.d) == (1, 2, 2, 1)
assert dd.total() == 12 + 11 + 2 + 1


class Late:
    def make(self):
        self.p, self.q = 1, 2
        return self


late = Late().make()
assert late.p == 1 and late.q == 2

total = 0
for r in range(5):
    p.a, p.b = r, r * 2
    total += p.a + p.b
assert total == 30

more = Pair()
more.a, more.b, more.c, more.d, more.e, more.f, more.g, more.h, more.i = [1, 2, 3, 4, 5, 6, 7, 8, 9]
assert more.a == 1 and more.i == 9 and more.e == 5


# for loops: the targets are stored from the element at the top of each
# iteration, by the same rules.
seen = []
for p.a, xs[1] in [(1, 2), (3, 4), (5, 6)]:
    seen.append(p.a * 10 + xs[1])
assert seen == [12, 34, 56] and p.a == 5 and xs[1] == 6

for p.n in [7, 8, 9]:
    pass
assert p.n == 9

d = {}
for key, p.v in [("a", 1), ("b", 2)]:
    d[key] = p.v
assert d == {"a": 1, "b": 2} and p.v == 2

seen = []
for idx, (p.u, xs[0]) in enumerate([(1, 2), (3, 4)]):
    seen.append((idx, p.u, xs[0]))
assert seen == [(0, 1, 2), (1, 3, 4)]

seen = []
for [p.a, p.b], c in [([1, 2], 3), ([4, 5], 6)]:
    seen.append(p.a + p.b + c)
assert seen == [6, 15]

for d["k"], d["j"] in [(1, 2), (3, 4)]:
    pass
assert d["k"] == 3 and d["j"] == 4


def loop_in_function(obj, items):
    s = 0
    for obj.x, items[0], y in [(1, 2, 3), (4, 5, 6)]:
        s += obj.x + items[0] + y
    return s


holder_obj = Pair()
holder_list = [0]
assert loop_in_function(holder_obj, holder_list) == 21
assert holder_obj.x == 4 and holder_list[0] == 5


class Walker:
    def __init__(self):
        self.i = 0
        self.t = [0, 0]

    def run(self):
        out = []
        for self.i, self.t[1] in [(1, 2), (3, 4)]:
            out.append(self.i * self.t[1])
        return out


w = Walker()
assert w.run() == [2, 12] and w.i == 3 and w.t == [0, 4]

count = 0
for p.a, xs[2] in [(1, 2), (3, 4), (5, 6), (7, 8)]:
    if p.a == 3:
        continue
    if p.a == 7:
        break
    count += 1
assert count == 2 and p.a == 7

for p.a, p.b in []:
    assert False
