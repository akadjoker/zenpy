# Python semantics fixed by the CPython differential pass (tools/diff_cpython.py).
# This file must run unchanged under CPython and Zen and print the same thing.

# arithmetic / equality on bools and mixed numbers
assert True + True == 2 and 1 + True == 2 and True * 3 == 3 and False + 1 == 1
assert True == 1 and 0 == False and 1.0 == 1 and {1: "a", True: "b"}[1] == "b" and {1.0: "x"}[1] == "x"
assert -5.0 % 2 == 1.0 and 5.0 % -2 == -1.0 and -7 % 3 == 2 and 7 // -2 == -4

# lists
assert [1, 2] + [3] == [1, 2, 3] and [0] * 3 == [0, 0, 0] and 2 * [1] == [1, 1]
assert [1] < [1, 0] and [1, 2] < [1, 3] and not ([2] < [1, 9]) and (1, 2) <= (1, 2)
for a, b in [(1, 2), (3, 4)]:      # literal list of tuples: first element was skipped
    assert a + 1 == b
xs = [3, 1, 3]
xs.extend([4, 5]); assert xs.count(3) == 2 and xs.copy() == xs and xs.pop(0) == 3 and xs == [1, 3, 4, 5]
xs.sort(reverse=True); assert xs == [5, 4, 3, 1]
xs.sort(key=lambda x: -x); assert xs == [5, 4, 3, 1]
ys = ["bb", "a", "ccc"]; ys.sort(key=len); assert ys == ["a", "bb", "ccc"]

# strings
assert "xxhixx".strip("x") == "hi" and "hello".find("l", 3) == 3 and "aaa".replace("a", "b", 2) == "bba"
assert "hello world".title() == "Hello World" and "abc".isalpha() and "12".isdigit() and "AB".isupper()
assert "hi".center(6, "*") == "**hi**" and "7".zfill(3) == "007" and "-7".zfill(4) == "-007"
assert "aXbXc".rsplit("X", 1) == ["aXb", "c"] and "a\nb\r\nc".splitlines() == ["a", "b", "c"]
assert "k=v=w".partition("=") == ("k", "=", "v=w") and "hello".rfind("l") == 3 and "hello".index("e") == 1
assert "%d %s %.2f|%5d|%-3s|" % (42, "s", 3.14159, 7, "a") == "42 s 3.14|    7|a  |"
assert "%s|%r|%i" % (None, "s", True) == "None|'s'|1" and "%d" % 3.7 == "3"
assert int("0x1f", 16) == 31 and int("101", 2) == 5 and int("  42  ") == 42
assert str([1, "a"]) == "[1, 'a']" and str(None) == "None" and str(True) == "True" and str({"k": 1}) == "{'k': 1}"
assert str(1 / 3) == "0.3333333333333333" and str(3.0) == "3.0" and str(1e-5) == "1e-05" and str(0.1 + 0.2) == "0.30000000000000004"

# dicts and sets
d = {"a": 1}
d.update({"b": 2}); assert d.setdefault("c", 3) == 3 and d.setdefault("a", 0) == 1 and d.pop("b") == 2 and d.pop("zz", "dflt") == "dflt"
assert sorted(k for k in d) == ["a", "c"] and d.copy() == d
s = {1, 2, 3}
s.discard(1); s.discard(42)
assert s == {2, 3} and sorted(s | {5}) == [2, 3, 5] and sorted(s & {2, 9}) == [2] and sorted(s - {2}) == [3] and sorted(s ^ {3, 7}) == [2, 7]
assert {1}.issubset({1, 2}) and {1, 2}.isdisjoint({3}) and sorted({1}.union([2], {3})) == [1, 2, 3] and set([1, 2]) == {1, 2}

# builtins
assert abs(-5) == 5 and min(3, 1, 2) == 1 and max([4, 2, 8]) == 8 and sum([1, 2, 3]) == 6 and sum([0.5, 0.25]) == 0.75
assert sorted([3, 1, 2]) == [1, 2, 3] and sorted("cba") == ["a", "b", "c"] and sorted([3, 1, 2], reverse=True) == [3, 2, 1]
assert sorted(["bb", "a", "ccc"], key=len) == ["a", "bb", "ccc"] and max([1, 5, 3], key=lambda x: -x) == 1 and max([], default=0) == 0
assert list(reversed([1, 2, 3])) == [3, 2, 1] and any([0, 1]) and not all([1, 0]) and not any([]) and all([])
assert list("ab") == ["a", "b"] and list(range(3)) == [0, 1, 2] and dict([(1, 2)]) == {1: 2} and dict(a=1) == {"a": 1}
assert round(2.5) == 2 and round(3.5) == 4 and round(2.675, 2) == 2.67 and round(123, -1) == 120 and round(1.25, 1) == 1.2
assert divmod(17, 5) == (3, 2) and divmod(-17, 5) == (-4, 3) and pow(2, 10) == 1024 and pow(2, 10, 7) == 2
assert hex(255) == "0xff" and oct(8) == "0o10" and bin(5) == "0b101" and hex(-1) == "-0x1"
assert repr("hi") == "'hi'" and repr([1, "x"]) == "[1, 'x']" and bool([]) is False and bool([0]) is True and bool(range(0)) is False
assert list(filter(lambda x: x % 2, [1, 2, 3, 4])) == [1, 3] and list(map(lambda x: x * 2, [1, 2])) == [2, 4]
assert list(zip("ab", range(2))) == [("a", 0), ("b", 1)] and list(enumerate("ab")) == [(0, "a"), (1, "b")]
assert isinstance(1, int) and isinstance(True, int) and isinstance("s", (int, str)) and isinstance([], list) and not isinstance(1.5, int)
assert type(1).__name__ == "int" and type("s").__name__ == "str"

# functions and closures
def compose(f, g):
    return lambda x: f(g(x))
sq = lambda x: x * x
assert compose(sq, lambda x: x + 1)(3) == 16       # lambda over enclosing parameters
def make_adders():
    return [lambda x: x + i for i in range(3)]
assert [f(10) for f in make_adders()] == [12, 12, 12]
def fn():
    pass
assert fn.__name__ == "fn"
class A:
    def __init__(self):
        self.x = 1
a = A()
assert hasattr(a, "x") and not hasattr(a, "y") and getattr(a, "x") == 1 and getattr(a, "y", "none") == "none" and A.__name__ == "A"
class V:
    def __init__(self, x):
        self.x = x
    def __lt__(self, o):
        return self.x < o.x
vs = [V(3), V(1), V(2)]
vs.sort()
assert [v.x for v in vs] == [1, 2, 3] and max(vs).x == 3 and min(vs).x == 1 and [v.x for v in sorted(vs, reverse=True)] == [3, 2, 1]

# comprehensions, generators as arguments, ternary chains, kwargs
assert [x * y for x in [1, 2] for y in [10, 20]] == [10, 20, 20, 40]
assert [x for x in range(10) if x % 2 == 0 if x > 2] == [4, 6, 8]
assert [(k, v) for k, v in [(1, 2), (3, 4)]] == [(1, 2), (3, 4)] and {k: v for k, v in [("a", 1)]} == {"a": 1}
assert [x if x > 1 else -x for x in range(3)] == [0, -1, 2] and ("x" if "" else "y" if None else "z") == "z"
assert sum(x * x for x in range(4)) == 14 and " ".join(str(x) for x in range(3)) == "0 1 2" and max(len(w) for w in ["a", "bbb"]) == 3
count = 0
for _ in range(5):
    count += 1
assert count == 5
x = 5
x <<= 1; x |= 8; x &= 12; x ^= 5; x >>= 1
assert x == 6
for k in {"p": 1}:
    assert k == "p"
print("ok")
