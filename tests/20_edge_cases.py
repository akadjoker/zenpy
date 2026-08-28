# Edge case stress test

# --- Closures capturing loop variables ---
funcs = []
for i in range(5):
    def make(n):
        def f():
            return n
        return f
    funcs.append(make(i))
assert funcs[0]() == 0
assert funcs[4]() == 4
print("closure capture OK")

# --- Nested closures ---
def outer(x):
    def middle(y):
        def inner(z):
            return x + y + z
        return inner
    return middle
assert outer(1)(2)(3) == 6
print("nested closure OK")

# --- Recursion depth ---
def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)
assert fib(20) == 6765
print("fib(20) OK")

# --- String edge cases ---
assert "" + "" == ""
assert "a" * 0 == ""
assert len("") == 0
assert len("hello") == 5
assert "hello"[0] == "h"
assert "hello"[-1] == "o"
print("string edges OK")

# --- Array edge cases ---
a = []
a.append(1)
a.append(2)
a.append(3)
assert len(a) == 3
assert a[-1] == 3
assert a[0] == 1
a.pop()
assert len(a) == 2

# Nested arrays
b = [[1, 2], [3, 4]]
assert b[0][0] == 1
assert b[1][1] == 4
print("array edges OK")

# --- Map edge cases ---
m = {}
m["key"] = "value"
assert m["key"] == "value"
assert len(m) == 1
m["key"] = "new"
assert m["key"] == "new"
assert len(m) == 1

# Numeric keys
m[42] = "answer"
assert m[42] == "answer"
print("map edges OK")

# --- Class edge cases ---
class Empty:
    pass

e = Empty()
e.x = 10
assert e.x == 10
print("dynamic field OK")

# --- Multiple assignment ---
a, b, c = 1, 2, 3
assert a == 1
assert b == 2
assert c == 3

# Swap
x, y = 10, 20
x, y = y, x
assert x == 20
assert y == 10
print("multi-assign OK")

# --- Boolean/None edge cases ---
assert not None
assert not False
assert not 0
assert not ""
assert not []
assert 1
assert "x"
assert [1]
print("truthiness OK")

# --- Comparison chaining ---
assert 1 < 2
assert 2 > 1
assert 1 <= 1
assert 1 >= 1
assert 1 == 1
assert 1 != 2
print("comparison OK")

# --- Default arguments ---
def greet(name, greeting="Hello"):
    return greeting + " " + name
assert greet("World") == "Hello World"
assert greet("Zen", "Hi") == "Hi Zen"
print("default args OK")

# --- Variadic unpacking / *args (if supported) ---
# (skip if not supported yet)

# --- Global vs local scope ---
g = 100
def modify():
    g = 200
    return g
assert modify() == 200
assert g == 100  # global unchanged
print("scope OK")

# --- GC stress: create many short-lived objects ---
for i in range(10000):
    s = str(i) + " " + str(i * 2)
print("GC stress OK")

# --- Large recursion ---
def sum_to(n):
    if n == 0:
        return 0
    return n + sum_to(n - 1)
assert sum_to(500) == 125250
print("recursion OK")

# --- Method returning self (chaining) ---
class Builder:
    def __init__(self):
        self.parts = []
    def add(self, p):
        self.parts.append(p)
        return self
    def build(self):
        return ",".join(self.parts)
result = Builder().add("a").add("b").add("c").build()
assert result == "a,b,c"
print("method chain OK")

# --- Inheritance + override ---
class Base:
    def value(self):
        return 10
class Child(Base):
    def value(self):
        return 20
class GrandChild(Child):
    pass
assert Base().value() == 10
assert Child().value() == 20
assert GrandChild().value() == 20
print("inherit override OK")

# --- Integer overflow-ish ---
big = 2 ** 40
assert big == 1099511627776
assert big * 2 == 2199023255552
print("big int OK")

print("\n=== ALL EDGE CASES PASSED ===")
