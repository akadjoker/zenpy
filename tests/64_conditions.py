# Conditions compiled as jump chains (and/or/not without a materialised
# boolean), fused ==/!= branches, `not` precedence, chained comparisons, and
# subscript assignment order. Every result must equal CPython's.

def f(a, b, c):
    r = []
    if a and b:
        r.append(1)
    if a or b:
        r.append(2)
    if a and b or c:
        r.append(3)
    if a or b and c:
        r.append(4)
    if not a:
        r.append(5)
    if not a and not b:
        r.append(6)
    if a and not b or not c and a:
        r.append(7)
    x = 0
    while x < 3 and a:
        x += 1
    r.append(x)
    if a < 2 and b != 0:
        r.append(8)
    elif a or c:
        r.append(9)
    else:
        r.append(10)
    if (a if b else c):
        r.append(11)
    if a if b else c:
        r.append(12)
    return r
assert f(1, 1, 1) == [1, 2, 3, 4, 3, 8, 11, 12]
assert f(0, 1, 0) == [2, 5, 0, 8]
assert f(1, 0, 1) == [2, 3, 4, 7, 3, 9, 11, 12]
assert f(0, 0, 0) == [5, 6, 0, 10]
assert f(3, 2, 0) == [1, 2, 3, 4, 7, 3, 9, 11, 12]

def chains(a, b, c):
    r = []
    if a < b < c:
        r.append(1)
    if a != b != c:          # short-circuit lands after the last compare
        r.append(2)
    if not (a < b < c):
        r.append(3)
    if a == b:
        r.append(4)
    if a != b:
        r.append(5)
    if not a == b:           # `not` binds looser than `==`
        r.append(6)
    if not a != b:
        r.append(7)
    if a not in [b, c]:
        r.append(8)
    if not a in [b, c]:
        r.append(9)
    while a != c:
        a += 1
    r.append(a)
    return r
assert chains(1, 0, 5) == [2, 3, 5, 6, 8, 9, 5]
assert chains(1, 2, 3) == [1, 2, 5, 6, 8, 9, 3]
assert chains(2, 2, 2) == [3, 4, 7, 2]
assert chains(0, 1, 1) == [3, 5, 6, 8, 9, 1]

class E:
    def __init__(self, v):
        self.v = v
    def __eq__(self, o):
        return self.v == o.v
def eqs(x, y):
    r = 0
    if x == y:
        r += 1
    if x != y:
        r += 10
    return r
assert eqs(E(1), E(1)) == 1 and eqs(E(1), E(2)) == 10
assert eqs("ab", "a" + "b") == 1 and eqs([1, 2], [1, 2]) == 1 and eqs(1.0, 1) == 1 and eqs(None, 0) == 10

# `not x` on an array element / call result branches on the operand directly
def nots(xs):
    n = 0
    i = 0
    while i < len(xs) and not xs[i]:
        n += 1
        i += 1
    if not len(xs):
        n += 100
    return n
assert nots([0, 0, 1, 0]) == 2 and nots([]) == 100 and nots([1]) == 0

# subscript assignment: RHS first, then the target (locals are not copied)
def order():
    xs = [0, 0, 0]
    i = 1
    def bump():
        nonlocal i
        i = i + 1
        return 7
    xs[i] = bump()
    return xs
assert order() == [0, 0, 7]
def keys_shift(h):
    i = 3
    while i > 0:
        p = (i - 1) // 2
        h[i] = h[p]
        i = p
    return h
assert keys_shift([5, 6, 7, 8]) == [5, 5, 7, 6]

# if without else inside loops (the dead JMP after the body is gone)
def count_evens(n):
    c = 0
    for i in range(n):
        if i % 2 == 0:
            c += 1
        if i == 5:
            continue
        c += 10
    return c
assert count_evens(8) == 4 + 70

print("ok")
