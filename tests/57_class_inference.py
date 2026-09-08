# `x = ClassName(...)` lets later `x.method()` dispatch through the class's
# vtable slot (OP_INVOKE_VT); a self-returning method keeps the class across
# `x.m().n()`. None of that may change behaviour: reassigning x to anything
# else, subclass overrides, defaults, *args and missing methods all work as
# through a plain dynamic call.

class A:
    def __init__(self, v):
        self.v = v
    def bump(self):
        self.v = self.v + 1
        return self
    def val(self):
        return self.v
    def twice(self):
        if self.v > 100:
            return self
        return self.bump().bump()
    def maybe(self):
        if self.v > 0:
            return self
        return None
    def with_default(self, extra=0):
        return self.v + extra
    def count(self, *values):
        return len(values)
    def declared(self) -> Self:
        return self

class B(A):
    def bump(self):
        self.v = self.v + 10
        return "not self"

a = A(1)
assert a.bump().bump().val() == 3
assert a.declared().declared().val() == 3
assert a.with_default() == 3
assert a.with_default(5) == 8
assert a.count() == 0
assert a.count(1, 2, 3) == 3

# reassigned to other kinds of values: dynamic dispatch again
a = "hello"
assert a.upper() == "HELLO"
a = [3, 1, 2]
a.append(0)
assert a == [3, 1, 2, 0]

# base-typed parameter holding a subclass whose override returns not-self
def use(x: A):
    return x.bump()
assert use(B(1)) == "not self"

b = B(5)
assert b.bump() == "not self"
assert b.val() == 15

m = A(-1)
assert m.maybe() == None
assert A(3).twice().val() == 5

# a global rebound inside a function
g = A(7)
def rebind():
    global g
    g = "str"
rebind()
assert g.upper() == "STR"

# a local rebound by a closure
def outer():
    h = A(1)
    def inner():
        nonlocal h
        h = "zzz"
    inner()
    return h.upper()
assert outer() == "ZZZ"

# loop-carried reassignment to a value of another type
t = A(1)
got = []
i = 0
while i < 2:
    if i == 0:
        got.append(t.val())
    else:
        got.append(t.upper())
    t = "low"
    i = i + 1
assert got == [1, "LOW"]

# a local of the current class inside a method: field and method access
class Tree:
    def __init__(self, d):
        self.left = None
        self.depth = d
        if d > 0:
            other = Tree(d - 1)
            self.left = other
            assert other.depth == d - 1
            assert other.check() == d - 1
    def check(self):
        return self.depth
assert Tree(2).check() == 2

# vararg method through a receiver of unknown type
def dyn(o):
    return o.count(1, 2)
assert dyn(A(0)) == 2

print("OK")
