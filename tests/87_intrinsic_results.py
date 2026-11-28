# len(), next() and eval() in a multi-assign took the register of a later
# value as scratch: `b, c, d = len(xs), j, len(xs)` left the list in `c`.
# Runs unchanged under CPython; every line is an assert.

xs = [0, 0, 0, 0]
ys = [1, 2]
j = 2


def counter():
    yield 1
    yield 2
    yield 3
    yield 4


b, c, d = len(xs), j, len(xs)
assert b == 4 and c == 2 and d == 4
b, c, d = j, len(xs), j
assert b == 2 and c == 4 and d == 2
b, c, d, e = len(xs), len(ys), j, len(xs)
assert (b, c, d, e) == (4, 2, 2, 4)

g = counter()
b, c, d = next(g), j, next(g)
assert b == 1 and c == 2 and d == 2
b, c, d = j, next(g), j
assert b == 2 and c == 3 and d == 2

b, c, d = eval("1 + 1"), j, eval("2 + 2")
assert b == 2 and c == 2 and d == 4
b, c, d = j, eval("len(xs)"), j
assert b == 2 and c == 4 and d == 2


def in_function(items, k):
    g = counter()
    x, y, z = len(items), k, len(items)
    p, q, r = next(g), k, next(g)
    s, t, u = eval("3"), k, eval("4")
    return [x, y, z, p, q, r, s, t, u]


assert in_function(xs, 7) == [4, 7, 4, 1, 7, 2, 3, 7, 4]
