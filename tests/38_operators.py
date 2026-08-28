# 38_operators.py — operator overloading for classes
# Tests: __add__, __sub__, __mul__, __div__, __mod__, __neg__,
#        __eq__, __lt__, __le__, __str__
#
# Note: print(obj) calls __str__, but str(obj) does NOT.
# So we test __str__ via print() and verify values via fields.

# ── Vec2: arithmetic + comparison ──────────────────────────────────

class Vec2:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    def __add__(self, o):
        return Vec2(self.x + o.x, self.y + o.y)

    def __sub__(self, o):
        return Vec2(self.x - o.x, self.y - o.y)

    def __mul__(self, s):
        return Vec2(self.x * s, self.y * s)

    def __div__(self, s):
        return Vec2(self.x / s, self.y / s)

    def __mod__(self, o):
        return Vec2(self.x % o.x, self.y % o.y)

    def __neg__(self):
        return Vec2(-self.x, -self.y)

    def __eq__(self, o):
        return self.x == o.x and self.y == o.y

    def __lt__(self, o):
        return self.x * self.x + self.y * self.y < o.x * o.x + o.y * o.y

    def __le__(self, o):
        return self.x * self.x + self.y * self.y <= o.x * o.x + o.y * o.y

    def __str__(self):
        return "(" + str(self.x) + ", " + str(self.y) + ")"

a = Vec2(1, 2)
b = Vec2(3, 4)

# __add__
r = a + b
assert r.x == 4 and r.y == 6, "add failed"
print(r)

# __sub__
r = a - b
assert r.x == -2 and r.y == -2, "sub failed"
print(r)

# __mul__ (scalar)
r = a * 3
assert r.x == 3 and r.y == 6, "mul failed"
print(r)

# __div__ (scalar)
r = Vec2(10, 20) / 5
assert r.x == 2 and r.y == 4, "div failed"
print(r)

# __mod__
r = Vec2(10, 7) % Vec2(3, 4)
assert r.x == 1 and r.y == 3, "mod failed"
print(r)

# __neg__
r = -a
assert r.x == -1 and r.y == -2, "neg failed"
print(r)

# __eq__
assert a == Vec2(1, 2), "eq same failed"
assert not (a == b), "eq diff failed"
print("eq: ok")

# __lt__ (by magnitude)
assert a < b, "lt failed"
assert not (b < a), "lt reverse failed"
assert not (a < Vec2(1, 2)), "lt equal failed"
print("lt: ok")

# __le__
assert a <= b, "le less failed"
assert a <= Vec2(1, 2), "le equal failed"
assert not (b <= a), "le greater failed"
print("le: ok")

# __str__ (print calls __str__, str() builtin does not)
print(a)

# ── chaining ───────────────────────────────────────────────────────

r = (a + b) * 2
assert r.x == 8 and r.y == 12, "chain failed"
print(r)

r = -(a + b)
assert r.x == -4 and r.y == -6, "neg chain failed"
print(r)

# ── double negation ───────────────────────────────────────────────

r = -(-a)
assert r.x == 1 and r.y == 2, "double neg failed"
print(r)

# ── identity ──────────────────────────────────────────────────────

zero = Vec2(0, 0)
r = a + zero
assert r == a, "add zero failed"
r = a * 1
assert r == a, "mul one failed"
print("identity: ok")

# ── Matrix2x2: another class ─────────────────────────────────────

class Mat2:
    def __init__(self, a, b, c, d):
        self.a = a
        self.b = b
        self.c = c
        self.d = d

    def __add__(self, o):
        return Mat2(self.a + o.a, self.b + o.b, self.c + o.c, self.d + o.d)

    def __mul__(self, o):
        return Mat2(
            self.a * o.a + self.b * o.c,
            self.a * o.b + self.b * o.d,
            self.c * o.a + self.d * o.c,
            self.c * o.b + self.d * o.d
        )

    def __eq__(self, o):
        return self.a == o.a and self.b == o.b and self.c == o.c and self.d == o.d

    def __str__(self):
        return "[" + str(self.a) + " " + str(self.b) + "; " + str(self.c) + " " + str(self.d) + "]"

identity = Mat2(1, 0, 0, 1)
m = Mat2(1, 2, 3, 4)

# multiply by identity
r = m * identity
assert r == m, "mat mul identity failed"
print(r)

# addition
r = m + Mat2(4, 3, 2, 1)
assert r == Mat2(5, 5, 5, 5), "mat add failed"
print(r)

# actual multiplication
r = Mat2(1, 2, 3, 4) * Mat2(5, 6, 7, 8)
assert r.a == 19 and r.b == 22 and r.c == 43 and r.d == 50, "mat mul failed"
print(r)

# ── Counter: __eq__ with different type guard ─────────────────────

class Counter:
    def __init__(self, n):
        self.n = n

    def __add__(self, o):
        return Counter(self.n + o.n)

    def __eq__(self, o):
        return self.n == o.n

    def __lt__(self, o):
        return self.n < o.n

    def __le__(self, o):
        return self.n <= o.n

    def __str__(self):
        return "Counter(" + str(self.n) + ")"

c1 = Counter(5)
c2 = Counter(10)
c3 = c1 + c2
assert c3.n == 15, "counter add failed"
assert c1 < c2, "counter lt failed"
assert c1 <= c1, "counter le failed"
assert not (c2 < c1), "counter lt reverse failed"
print(c3)

# ── all passed ────────────────────────────────────────────────────
print("all operator tests passed")
