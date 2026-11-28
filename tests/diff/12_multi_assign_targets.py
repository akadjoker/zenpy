class Pair:
    pass

p = Pair()
q = Pair()
p.a, p.b = 1, 2
print(p.a, p.b)
p.a, p.b = p.b, p.a
print(p.a, p.b)

def three():
    return 5, 6, 7

p.a, q.a, p.b = three()
print(p.a, q.a, p.b)

xs = [1, 2, 3, 4]
i = 0
xs[i], xs[3] = xs[3], xs[i]
print(xs)
d = {}
d["a"], d["b"] = 1, 2
print(d["a"], d["b"])

n, p.a, xs[1], d["c"] = 1, 2, 3, 4
print(n, p.a, xs, d["c"])
(m, p.b), xs[0] = (7, 8), 9
print(m, p.b, xs[0])
m, [p.a, [p.b, xs[3]]] = 1, [2, [3, 4]]
print(m, p.a, p.b, xs)

log = []
def rhs():
    log.append("rhs")
    return 1, 2
def holder(name):
    log.append(name)
    return p
holder("a").a, holder("b").b = rhs()
print(log)

zs = [0, 0, 0]
t = 0
t, zs[t] = 1, 5
print(t, zs)
zs = [0, 0, 0]
t = 0
zs[t], t = 6, 2
print(t, zs)
c = Pair()
e = Pair()
c, c.v = e, 9
print(c is e, e.v)

def in_function(obj, items):
    first, obj.x, items[0], last = 1, 2, 3, 4
    obj.y, items[1] = first + last, obj.x
    return first, last

box = Pair()
data = [0, 0]
first, last = in_function(box, data)
print(first, last, box.x, box.y, data)

for p.a, xs[1] in [(1, 2), (3, 4), (5, 6)]:
    print(p.a * 10 + xs[1])
for p.n in [7, 8, 9]:
    pass
print(p.n)
for key, p.v in [("a", 1), ("b", 2)]:
    d[key] = p.v
print(d["a"], d["b"], p.v)
for idx, (p.u, xs[0]) in enumerate([(1, 2), (3, 4)]):
    print(idx, p.u, xs[0])
for [p.a, p.b], c in [([1, 2], 3), ([4, 5], 6)]:
    print(p.a, p.b, c)
