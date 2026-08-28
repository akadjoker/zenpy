# === BRUTAL EDGE CASES ===
# Tests designed to break register-based VMs, closures, scoping, and GC

# --- Closure captures mutable variable ---
def make_counter():
    count = 0
    def inc():
        nonlocal count
        count = count + 1
        return count
    def get():
        nonlocal count
        return count
    return inc, get

inc, get = make_counter()
assert inc() == 1
assert inc() == 2
assert inc() == 3
assert get() == 3
print("closure mutable capture OK")

# --- Multiple closures sharing same upvalue ---
def shared_state():
    x = 10
    def add(n):
        nonlocal x
        x = x + n
    def sub(n):
        nonlocal x
        x = x - n
    def val():
        nonlocal x
        return x
    return add, sub, val

add, sub, val = shared_state()
add(5)
assert val() == 15
sub(3)
assert val() == 12
add(100)
sub(50)
assert val() == 62
print("shared upvalue OK")

# --- Deeply nested closures ---
def outer():
    a = 1
    def mid():
        b = 2
        def inner():
            c = 3
            return a + b + c
        return inner
    return mid

f = outer()()
assert f() == 6
print("deep nested closure OK")

# --- Closure in loop (use factory to capture value) ---
def make_funcs():
    def make_capture(val):
        def capture():
            return val
        return capture
    funcs = []
    i = 0
    while i < 5:
        funcs.append(make_capture(i))
        i = i + 1
    return funcs

fns = make_funcs()
assert fns[0]() == 0
assert fns[1]() == 1
assert fns[2]() == 2
assert fns[3]() == 3
assert fns[4]() == 4
print("closure in loop OK")

# --- Recursive closure ---
def make_fib():
    def fib(n):
        if n <= 1:
            return n
        return fib(n - 1) + fib(n - 2)
    return fib

my_fib = make_fib()
assert my_fib(10) == 55
assert my_fib(0) == 0
assert my_fib(1) == 1
print("recursive closure OK")

# --- Class with closure methods ---
class Accumulator:
    def __init__(self, start=0):
        self.total = start

    def add(self, x):
        self.total = self.total + x
        return self

    def mul(self, x):
        self.total = self.total * x
        return self

    def get(self):
        return self.total

a = Accumulator(1)
assert a.add(2).mul(3).add(4).get() == 13
assert Accumulator().get() == 0
assert Accumulator(100).add(-50).get() == 50
print("accumulator class OK")

# --- Inheritance with default args ---
class Shape:
    def __init__(self, name="unknown"):
        self.name = name

    def area(self):
        return 0

class Circle(Shape):
    def __init__(self, radius=1):
        super().__init__("circle")
        self.radius = radius

    def area(self):
        return 3.14159 * self.radius * self.radius

c1 = Circle()
c2 = Circle(5)
assert c1.name == "circle"
assert c1.radius == 1
assert c2.radius == 5
assert c2.area() > 78.0
assert c2.area() < 79.0
s = Shape()
assert s.name == "unknown"
assert s.area() == 0
print("inheritance defaults OK")

# --- Empty collections edge cases ---
assert [] == []
assert {} == {}
assert len([]) == 0
assert len({}) == 0
empty = []
empty.append(1)
assert empty == [1]
empty.pop()
assert empty == []
print("empty collections OK")

# --- Nested arrays deep equality ---
a = [[1, 2], [3, [4, 5]]]
b = [[1, 2], [3, [4, 5]]]
c = [[1, 2], [3, [4, 6]]]
assert a == b
assert a != c
assert [[], [[]]] == [[], [[]]]
assert [[1]] != [[2]]
print("nested array equality OK")

# --- Map equality ---
m1 = {"a": 1, "b": 2, "c": 3}
m2 = {"c": 3, "a": 1, "b": 2}
m3 = {"a": 1, "b": 99}
assert m1 == m2
assert m1 != m3
assert {} == {}
assert {"x": [1, 2]} == {"x": [1, 2]}
assert {"x": [1, 2]} != {"x": [1, 3]}
print("map equality OK")

# --- String edge cases ---
assert "" == ""
assert len("") == 0
assert "a" * 5 == "aaaaa"
assert "" * 100 == ""
assert "hello"[0] == "h"
assert "hello"[-1] == "o"
assert "abc".find("") == 0
assert "abc".find("z") == -1
assert "".split(",") == [""]
assert "a,,b".split(",") == ["a", "", "b"]
print("string edge OK")

# --- Numeric edge cases ---
assert 0 == 0.0
assert -0 == 0
assert 1 / 2 == 0.5
assert -7 // 2 == -4
assert 7 // -2 == -4
assert -7 % 2 == 1
assert 7 % -2 == -1
assert 0 ** 0 == 1
assert 1 ** 1000000 == 1
print("numeric edge OK")

# --- Boolean edge cases ---
assert (True and True) == True
assert (True and False) == False
assert (False or True) == True
assert (False or False) == False
assert (not True) == False
assert (not False) == True
assert (not None) == True
assert (not 0) == True
assert (not "") == True
assert (not []) == True
assert (1 and 2) == 2
assert (0 or 3) == 3
assert (None or "x") == "x"
assert (1 and 0) == 0
print("boolean edge OK")

# --- Short-circuit evaluation ---
def bomb():
    assert False

assert (True or bomb()) == True
assert (False and bomb()) == False
# bomb() should never be called due to short-circuit
print("short circuit OK")

# --- Nested function redefinition ---
def redefine():
    def helper():
        return 1
    a = helper()
    def helper():
        return 2
    b = helper()
    return a + b

assert redefine() == 3
print("redefine OK")

# --- Variable shadowing across scopes ---
x = 100
def shadow():
    x = 200
    def inner():
        x = 300
        return x
    return x + inner()

assert shadow() == 500
assert x == 100
print("shadow OK")

# --- While with complex conditions ---
def collatz(n):
    steps = 0
    while n != 1:
        if n % 2 == 0:
            n = n // 2
        else:
            n = 3 * n + 1
        steps = steps + 1
    return steps

assert collatz(1) == 0
assert collatz(2) == 1
assert collatz(27) == 111
print("collatz OK")

# --- Array as stack ---
def balanced(s):
    stack = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == "(" or c == "[" or c == "{":
            stack.append(c)
        else:
            if len(stack) == 0:
                return False
            top = stack.pop()
            if c == ")" and top != "(":
                return False
            if c == "]" and top != "[":
                return False
            if c == "}" and top != "{":
                return False
        i = i + 1
    return len(stack) == 0

assert balanced("()") == True
assert balanced("([{}])") == True
assert balanced("((()))") == True
assert balanced("(]") == False
assert balanced("((") == False
assert balanced("") == True
print("balanced parens OK")

# --- Fibonacci iterative (large) ---
def fib_iter(n):
    if n <= 1:
        return n
    a = 0
    b = 1
    i = 2
    while i <= n:
        temp = b
        b = a + b
        a = temp
        i = i + 1
    return b

assert fib_iter(50) == 12586269025
assert fib_iter(60) == 1548008755920
print("fib large OK")

# --- Multiple inheritance depth ---
class A:
    def who(self):
        return "A"
    def base(self):
        return "base"

class B(A):
    def who(self):
        return "B"

class C(B):
    def who(self):
        return "C"

class D(C):
    pass

d = D()
assert d.who() == "C"
assert d.base() == "base"
print("deep inherit OK")

# --- Method with many defaults ---
class Config:
    def __init__(self, host="localhost", port=8080, debug=False, timeout=30):
        self.host = host
        self.port = port
        self.debug = debug
        self.timeout = timeout

c = Config()
assert c.host == "localhost"
assert c.port == 8080
assert c.debug == False
assert c.timeout == 30

c2 = Config("0.0.0.0", 3000, True, 60)
assert c2.host == "0.0.0.0"
assert c2.port == 3000
assert c2.debug == True
assert c2.timeout == 60

c3 = Config("example.com")
assert c3.host == "example.com"
assert c3.port == 8080
print("many defaults OK")

# --- Recursive array building ---
def build_tree(depth):
    if depth == 0:
        return 0
    return [build_tree(depth - 1), build_tree(depth - 1)]

tree = build_tree(4)
assert tree[0][0][0][0] == 0
assert tree[1][1][1][1] == 0
assert tree[0][1][0][1] == 0
print("recursive tree OK")

# --- String building in loop ---
def repeat_str(s, n):
    result = ""
    i = 0
    while i < n:
        result = result + s
        i = i + 1
    return result

assert repeat_str("ab", 4) == "abababab"
assert repeat_str("", 100) == ""
assert repeat_str("x", 1) == "x"
print("string build OK")

# --- Map operations ---
def word_count(words):
    counts = {}
    i = 0
    while i < len(words):
        w = words[i]
        if w in counts:
            counts[w] = counts[w] + 1
        else:
            counts[w] = 1
        i = i + 1
    return counts

wc = word_count(["a", "b", "a", "c", "b", "a"])
assert wc["a"] == 3
assert wc["b"] == 2
assert wc["c"] == 1
print("word count OK")

# --- Chained comparisons via and ---
def clamp(val, lo, hi):
    if val < lo:
        return lo
    if val > hi:
        return hi
    return val

assert clamp(5, 0, 10) == 5
assert clamp(-5, 0, 10) == 0
assert clamp(15, 0, 10) == 10
assert clamp(0, 0, 0) == 0
print("clamp OK")

# --- Nested maps ---
data = {"users": {"alice": {"age": 30}, "bob": {"age": 25}}}
assert data["users"]["alice"]["age"] == 30
assert data["users"]["bob"]["age"] == 25
data["users"]["alice"]["age"] = 31
assert data["users"]["alice"]["age"] == 31
print("nested maps OK")

# --- Array slicing patterns ---
def take(arr, n):
    result = []
    i = 0
    while i < n and i < len(arr):
        result.append(arr[i])
        i = i + 1
    return result

def drop(arr, n):
    result = []
    i = n
    while i < len(arr):
        result.append(arr[i])
        i = i + 1
    return result

assert take([1,2,3,4,5], 3) == [1,2,3]
assert take([1,2], 10) == [1,2]
assert take([], 5) == []
assert drop([1,2,3,4,5], 2) == [3,4,5]
assert drop([1,2], 10) == []
print("take/drop OK")

# --- Return from nested if/while ---
def find_first(arr, pred):
    i = 0
    while i < len(arr):
        if pred(arr[i]):
            return arr[i]
        i = i + 1
    return None

assert find_first([1,2,3,4,5], lambda x: x > 3) == 4
assert find_first([1,2,3], lambda x: x > 10) == None
assert find_first([], lambda x: True) == None
print("find first OK")

# --- GC stress with closures ---
def gc_closures():
    results = []
    i = 0
    while i < 200:
        val = i * i
        def make():
            nonlocal val
            return val
        results.append(make())
        i = i + 1
    return results[-1]

assert gc_closures() == 199 * 199
print("GC closures OK")

# --- Self-reference in class ---
class LinkedList:
    def __init__(self, val, next=None):
        self.val = val
        self.next = next

    def to_array(self):
        result = []
        node = self
        while node != None:
            result.append(node.val)
            node = node.next
        return result

    def length(self):
        count = 0
        node = self
        while node != None:
            count = count + 1
            node = node.next
        return count

ll = LinkedList(1, LinkedList(2, LinkedList(3, LinkedList(4))))
assert ll.to_array() == [1, 2, 3, 4]
assert ll.length() == 4
assert LinkedList(99).to_array() == [99]
assert LinkedList(99).length() == 1
print("linked list class OK")

# --- Passing functions as arguments ---
def apply_twice(f, x):
    return f(f(x))

def double(x):
    return x * 2

def inc(x):
    return x + 1

assert apply_twice(double, 3) == 12
assert apply_twice(inc, 10) == 12
assert apply_twice(lambda x: x * x, 2) == 16
print("higher order OK")

# --- Complex expression evaluation order ---
def side_effect_test():
    log = []
    def a():
        log.append("a")
        return 1
    def b():
        log.append("b")
        return 2
    def c():
        log.append("c")
        return 3
    result = a() + b() * c()
    assert result == 7
    return log

log = side_effect_test()
assert len(log) == 3
print("eval order OK")

# --- Many parameters ---
def many_params(a, b, c, d, e, f, g, h):
    return a + b + c + d + e + f + g + h

assert many_params(1, 2, 3, 4, 5, 6, 7, 8) == 36
assert many_params(0, 0, 0, 0, 0, 0, 0, 1) == 1
print("many params OK")

# --- While True with break ---
def find_sqrt(n):
    i = 0
    while True:
        if i * i >= n:
            return i
        i = i + 1

assert find_sqrt(0) == 0
assert find_sqrt(1) == 1
assert find_sqrt(4) == 2
assert find_sqrt(16) == 4
assert find_sqrt(17) == 5
print("while true break OK")

# --- Nested while with continue ---
def sum_odd_cols(matrix):
    total = 0
    r = 0
    while r < len(matrix):
        c = 0
        while c < len(matrix[r]):
            if c % 2 == 0:
                c = c + 1
                continue
            total = total + matrix[r][c]
            c = c + 1
        r = r + 1
    return total

m = [[1,2,3,4], [5,6,7,8], [9,10,11,12]]
assert sum_odd_cols(m) == 2 + 4 + 6 + 8 + 10 + 12
print("nested continue OK")

# --- Large map ---
def large_map():
    m = {}
    i = 0
    while i < 500:
        m[str(i)] = i * i
        i = i + 1
    assert m["0"] == 0
    assert m["499"] == 249001
    assert m["250"] == 62500
    return len(m)

assert large_map() == 500
print("large map OK")

# --- Array reverse ---
def reverse(arr):
    result = []
    i = len(arr) - 1
    while i >= 0:
        result.append(arr[i])
        i = i - 1
    return result

assert reverse([1,2,3,4,5]) == [5,4,3,2,1]
assert reverse([]) == []
assert reverse([42]) == [42]
print("reverse OK")

# --- Mutual recursion (top-level, works because globals are late-bound) ---
def is_even(n):
    if n == 0:
        return True
    return is_odd(n - 1)

def is_odd(n):
    if n == 0:
        return False
    return is_even(n - 1)

assert is_even(0) == True
assert is_even(10) == True
assert is_odd(7) == True
assert is_even(7) == False
print("mutual recursion OK")

print("\n=== ALL BRUTAL EDGE CASES PASSED ===")
