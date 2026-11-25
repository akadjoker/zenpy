# Python behaviour a comparison against CPython found broken (2026-11-25).
# Runs unchanged under CPython; every line is an assert.
import math as m, time
from math import sqrt as root, pi


def counter():
    n = 0
    s = "a"
    def inc():
        nonlocal n, s
        n += 1
        n *= 3
        s += "b"
        return n
    def text():
        return s
    return inc, text
inc, text = counter()
inc()
assert inc() == 12 and text() == "abb"

assert [i for i, _ in enumerate("ab", start=1)] == [1, 2]
assert [i for i, _ in enumerate("ab", 5)] == [5, 6]

words = ["pear", "fig", "banana"]
words.sort(key=len)
assert words == ["fig", "pear", "banana"]
assert sorted(["pear", "fig", "banana"], key=len) == ["fig", "pear", "banana"]
assert max(["pear", "fig", "banana"], key=len) == "banana" and min(["pear", "fig", "banana"], key=len) == "fig"
assert callable(len) and list(map(len, ["a", "bcd"])) == [1, 3]

d = {"a": 1, "b": 2, "c": 3}
d.pop("a")
d["q"] = 9
assert list(d) == ["b", "c", "q"]
big = {}
for i in range(40):
    big[i] = i
for i in range(0, 40, 2):
    del big[i]
for i in range(100, 130):
    big[i] = 1
assert list(big)[:3] == [1, 3, 5] and list(big)[-1] == 129 and len(big) == 50
for k in range(500):
    big["k" + str(k)] = k
    del big["k" + str(k)]
assert len(big) == 50 and list(big)[0] == 1

assert m.sqrt(16) == 4.0 and root(9) == 3.0 and pi > 3 and time.time() > 0
assert m.floor(2.7) == 2 and type(m.floor(2.7)).__name__ == "int" and m.ceil(-2.5) == -2

assert r"a\nb" == "a\\nb" and len(r"\t") == 2
def raw_default(p=r"c:\new", q="""tri
ple"""):
    return p + q
assert raw_default() == "c:\\newtri\nple"


class Documented:
    """A class with a docstring."""
    value = 3
    def get(self):
        """A method with one."""
        return self.value
assert Documented().get() == 3


class Empty: pass
e = Empty()
setattr(e, "w", 5)
assert e.w == 5 and getattr(e, "w") == 5 and hasattr(e, "w") and not hasattr(e, "h")


class Counted:
    count = 7
    def __init__(self):
        Counted.count += 1
    def hello(self, end):
        return "hi" + end
Counted()
Counted()
assert Counted.count == 9 and Counted.hello(Counted(), "!") == "hi!"


class Base:
    def __init__(self, n):
        self.n = n
class Kid(Base):
    def __init__(self, n):
        Base.__init__(self, n * 10)
assert Kid(2).n == 20 and issubclass(Kid, Base) and not issubclass(Base, Kid)


def gen():
    yield 1
    yield 2
    yield 3
assert list(gen()) == [1, 2, 3] and sum(gen()) == 6 and max(gen()) == 3 and min(gen()) == 1
assert sorted(gen(), reverse=True) == [3, 2, 1] and set(gen()) == {1, 2, 3}
assert [i for i, v in enumerate(gen(), start=10)] == [10, 11, 12]


def pair():
    return 3, 4
def listed():
    return [1, 2]
a, b = listed()
c, dd = pair()
both = pair()
assert a + b == 3 and c + dd == 7 and list(both) == [3, 4] and both[1] == 4
t = (1, 2, 3)
x, y, z = t
seq = [4, 5]
p, q = seq
assert x + y + z == 6 and p * q == 20


def fib(n):
    if n < 2:
        return n, 1
    f1, c1 = fib(n - 1)
    f2, c2 = fib(n - 2)
    return f1 + f2, c1 + c2
assert list(fib(15)) == [610, 987]


leaked_a = "outer"
def no_leak():
    leaked_a, leaked_b = 1, 2
    return leaked_a + leaked_b
assert no_leak() == 3 and leaked_a == "outer"

assert [p + q for i, (p, q) in enumerate([(1, 2), (3, 4)])] == [3, 7]

scale = lambda v, w=2: v * w
assert scale(3) == 6 and scale(3, 3) == 9

assert __name__ == "__main__"


class Color:
    RED = 1


def kind(v):
    match v:
        case 0:
            return "zero"
        case [first, second]:
            return "pair" + str(first + second)
        case "x" | "y":
            return "xy"
        case Color.RED:
            return "red"
        case n if isinstance(n, int) and n > 100:
            return "big"
        case _:
            return "other"
assert [kind(0), kind([1, 2]), kind("y"), kind(1), kind(500), kind(50)] == ["zero", "pair3", "xy", "red", "big", "other"]

print("ok")
