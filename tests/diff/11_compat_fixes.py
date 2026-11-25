# Python behaviour a Kinetix2D comparison found broken (2026-11-25).
# Each block runs unchanged under CPython and Zen and prints the same thing.

def counter():
    n = 0
    def inc():
        nonlocal n
        n += 1
        return n
    return inc
c = counter()
c()
print(c())

for i, v in enumerate(["a", "b"], start=1):
    print(i, v)
for i, v in enumerate(["a", "b"], 5):
    print(i, v)

words = ["pear", "fig", "banana"]
words.sort(key=len)
print(words)
print(sorted(["pear", "fig", "banana"], key=len))
print(max(["pear", "fig", "banana"], key=len), min(["pear", "fig", "banana"], key=len))

d = {"a": 1, "b": 2, "c": 3}
d.pop("a")
d["q"] = 9
print(list(d.keys()), d)
d2 = {"x": 1, "y": 2}
del d2["x"]
d2["z"] = 3
d2["x"] = 4
print(list(d2), list(d2.values()))

import math as m
print(m.sqrt(16), m.floor(2.7))

import math, time
print(math.ceil(2.1))

def kind(v):
    match v:
        case 0:
            return "zero"
        case [a, b]:
            return "pair " + str(a) + str(b)
        case "x" | "y":
            return "xy"
        case _:
            return "other"
print(kind(0), kind([1, 2]), kind("y"), kind(5))

print(r"a\nb", len(r"\t"))

class Documented:
    """A class with a docstring."""
    value = 3
    def get(self):
        """A method with one."""
        return self.value
print(Documented().get())

class Empty: pass
e = Empty()
e.a = 5
print(e.a)

class Counted:
    count = 7
print(Counted.count)

def gen():
    yield 1
    yield 2
    yield 3
print(list(gen()), sum(gen()), max(gen()), sorted(gen(), reverse=True))
print(set(gen()) == {1, 2, 3}, any(x > 2 for x in gen()), min(gen()))

t = (1, 2, 3)
a, b, c = t
print(a, b, c)
pair = [4, 5]
x, y = pair
print(x, y)

for i, (p, q) in enumerate([(1, 2), (3, 4)]):
    print(i, p, q)

scale = lambda x, y=2: x * y
print(scale(3), scale(3, 3))

class Box:
    pass
box = Box()
setattr(box, "w", 5)
print(getattr(box, "w"), hasattr(box, "w"), hasattr(box, "h"))

class Base:
    pass
class Child(Base):
    pass
print(issubclass(Child, Base), issubclass(Base, Child))
print(callable(len), callable(abs), callable(3), callable(lambda: 1))
print(__name__)
if __name__ == "__main__":
    print("main")

print(math.floor(2.7), type(math.floor(2.7)).__name__, math.ceil(2.1), type(math.ceil(-2.5)).__name__)
