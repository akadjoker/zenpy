# === STRESS TEST: Crazy edge cases ===

# --- Recursion with reassignment ---
def ackermann(m, n):
    if m == 0:
        return n + 1
    if n == 0:
        return ackermann(m - 1, 1)
    return ackermann(m - 1, ackermann(m, n - 1))

assert ackermann(3, 4) == 125
print("ackermann OK")

# --- Mutual recursion ---
def is_even(n):
    if n == 0:
        return True
    return is_odd(n - 1)

def is_odd(n):
    if n == 0:
        return False
    return is_even(n - 1)

assert is_even(100) == True
assert is_odd(99) == True
assert is_even(77) == False
print("mutual recursion OK")

# --- Closure factory with shared state ---
def make_pair():
    state = [0]
    def get():
        return state[0]
    def set(v):
        state[0] = v
    return [get, set]

pair = make_pair()
getter = pair[0]
setter = pair[1]
setter(42)
assert getter() == 42
setter(getter() + 8)
assert getter() == 50
print("closure pair OK")

# --- Deeply nested closures ---
def nest(n):
    def a():
        def b():
            def c():
                return n * 2
            return c() + 1
        return b() + 1
    return a() + 1

assert nest(10) == 23
assert nest(0) == 3
print("deep closure OK")

# --- String operations ---
s = "hello world"
assert len(s) == 11
assert s[0] == "h"
assert s[10] == "d"
assert "lo wo" in s
assert "xyz" not in s
assert s * 2 == "hello worldhello world"
assert 3 * "ab" == "ababab"
print("string ops OK")

# --- Array stress ---
def build_array(n):
    arr = []
    i = 0
    while i < n:
        arr.append(i * i)
        i = i + 1
    return arr

squares = build_array(20)
assert len(squares) == 20
assert squares[0] == 0
assert squares[4] == 16
assert squares[19] == 361
print("array build OK")

# --- Map stress ---
def build_map(n):
    m = {}
    i = 0
    while i < n:
        m[str(i)] = i * 10
        i = i + 1
    return m

d = build_map(50)
assert len(d) == 50
assert d["0"] == 0
assert d["49"] == 490
assert d["25"] == 250
print("map build OK")

# --- Fibonacci generator ---
def fib_gen(limit):
    a = 0
    b = 1
    results = []
    while a < limit:
        results.append(a)
        temp = a
        a = b
        b = temp + b
    return results

fibs = fib_gen(100)
assert fibs == [0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89]
print("fib sequence OK")

# --- Class with multiple inheritance-like pattern ---
class Animal:
    def __init__(self, name, legs):
        self.name = name
        self.legs = legs
    def describe(self):
        return self.name + " has " + str(self.legs) + " legs"

class Dog(Animal):
    def __init__(self, name):
        super().__init__(name, 4)
    def speak(self):
        return "woof"

class Puppy(Dog):
    def __init__(self, name, toy):
        super().__init__(name)
        self.toy = toy
    def play(self):
        return self.name + " plays with " + self.toy

p = Puppy("Rex", "ball")
assert p.describe() == "Rex has 4 legs"
assert p.speak() == "woof"
assert p.play() == "Rex plays with ball"
assert p.name == "Rex"
assert p.legs == 4
assert p.toy == "ball"
print("deep inheritance OK")

# --- Augmented assign in all forms ---
def aug_all():
    x = 100
    x += 10
    x -= 5
    x *= 2
    x //= 3
    x %= 9
    x **= 3
    return x

assert aug_all() == 343
print("augmented all OK")

# --- Nested loops with break/continue ---
def find_primes(n):
    primes = []
    i = 2
    while i < n:
        is_prime = True
        j = 2
        while j * j <= i:
            if i % j == 0:
                is_prime = False
                break
            j = j + 1
        if is_prime:
            primes.append(i)
        i = i + 1
    return primes

assert find_primes(30) == [2, 3, 5, 7, 11, 13, 17, 19, 23, 29]
print("primes OK")

# --- Default args + keyword-like patterns ---
def make_range(start=0, stop=10, step=1):
    result = []
    i = start
    while i < stop:
        result.append(i)
        i = i + step
    return result

assert make_range() == [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
assert make_range(5) == [5, 6, 7, 8, 9]
assert make_range(0, 20, 5) == [0, 5, 10, 15]
print("default args range OK")

# --- Lots of locals (register pressure) ---
def many_locals():
    a = 1
    b = 2
    c = 3
    d = 4
    e = 5
    f = 6
    g = 7
    h = 8
    i = 9
    j = 10
    k = 11
    l = 12
    m = 13
    n = 14
    o = 15
    p = 16
    q = 17
    r = 18
    s = 19
    t = 20
    return a+b+c+d+e+f+g+h+i+j+k+l+m+n+o+p+q+r+s+t

assert many_locals() == 210
print("many locals OK")

# --- Swap without temp (Python style) ---
def swap_test():
    a = 1
    b = 2
    # Using temp since we don't have tuple unpack in assignment yet
    temp = a
    a = b
    b = temp
    return a * 10 + b

assert swap_test() == 21
print("swap OK")

# --- Recursive data structure (nested arrays) ---
def flatten(lst):
    result = []
    i = 0
    while i < len(lst):
        item = lst[i]
        if type(item) == "list":
            inner = flatten(item)
            j = 0
            while j < len(inner):
                result.append(inner[j])
                j = j + 1
        else:
            result.append(item)
        i = i + 1
    return result

nested = [1, [2, 3], [4, [5, 6]], 7]
assert flatten(nested) == [1, 2, 3, 4, 5, 6, 7]
print("flatten OK")

# --- Counter class with operator-like methods ---
class Counter:
    def __init__(self, start=0):
        self.val = start
    def inc(self):
        self.val = self.val + 1
        return self
    def dec(self):
        self.val = self.val - 1
        return self
    def get(self):
        return self.val

c = Counter(10)
assert c.inc().inc().inc().get() == 13
assert c.dec().get() == 12
c0 = Counter()
assert c0.get() == 0
c0.inc()
assert c0.get() == 1
print("method chain counter OK")

# --- GC pressure: create many objects ---
def gc_torture():
    i = 0
    while i < 1000:
        x = [1, 2, 3, 4, 5]
        y = {"a": 1, "b": 2}
        z = "hello" + str(i)
        i = i + 1
    return i

assert gc_torture() == 1000
print("GC torture OK")

# --- Negative indexing ---
arr = [10, 20, 30, 40, 50]
assert arr[-1] == 50
assert arr[-2] == 40
assert arr[-5] == 10
print("negative index OK")

# --- String methods ---
assert "HELLO".lower() == "hello"
assert "hello".upper() == "HELLO"
assert "  spaces  ".strip() == "spaces"
assert "hello world".split(" ") == ["hello", "world"]
assert ",".join(["a", "b", "c"]) == "a,b,c"
assert "hello".replace("l", "L") == "heLLo"
assert "hello world".find("world") == 6
assert "hello".startswith("hel") == True
assert "hello".endswith("llo") == True
print("string methods OK")

# --- Power edge cases ---
assert 2 ** 0 == 1
assert 2 ** 1 == 2
assert 2 ** 40 == 1099511627776
assert (-2) ** 3 == -8
assert (-1) ** 100 == 1
assert (-1) ** 99 == -1
print("power edges OK")

# --- Truthiness comprehensive ---
assert not None
assert not False
assert not 0
assert not 0.0
assert not ""
assert not []
assert not {}
assert True
assert 1
assert -1
assert 0.1
assert "x"
assert [0]
assert {"a": 1}
print("truthiness full OK")

# --- Comparison chaining ---
def in_range(x, lo, hi):
    return lo <= x and x < hi

assert in_range(5, 0, 10) == True
assert in_range(10, 0, 10) == False
assert in_range(-1, 0, 10) == False
print("range check OK")

# --- Varargs ---
def sum_all(*args):
    total = 0
    i = 0
    while i < len(args):
        total = total + args[i]
        i = i + 1
    return total

assert sum_all(1, 2, 3) == 6
assert sum_all(10) == 10
assert sum_all() == 0
assert sum_all(1, 2, 3, 4, 5, 6, 7, 8, 9, 10) == 55
print("varargs OK")

# --- Lambda ---
double = lambda x: x * 2
assert double(5) == 10
assert double(0) == 0

apply = lambda f, x: f(x)
assert apply(double, 7) == 14
print("lambda OK")

# --- Global modification from nested call ---
result = []
def collect(n):
    global result
    i = 0
    while i < n:
        result.append(i)
        i = i + 1

collect(5)
assert result == [0, 1, 2, 3, 4]
collect(3)
assert result == [0, 1, 2, 3, 4, 0, 1, 2]
print("global mutate OK")

# --- Exception-like patterns (early return) ---
def safe_divide(a, b):
    if b == 0:
        return None
    return a / b

assert safe_divide(10, 2) == 5.0
assert safe_divide(7, 0) == None
assert safe_divide(0, 5) == 0.0
print("safe divide OK")

# --- String formatting via concatenation ---
def greet(name, age):
    return "Hello " + name + ", you are " + str(age) + " years old!"

assert greet("Alice", 30) == "Hello Alice, you are 30 years old!"
print("string format OK")

# --- Nested class instantiation ---
class Node:
    def __init__(self, val, next):
        self.val = val
        self.next = next

def list_sum(node):
    total = 0
    current = node
    while current != None:
        total = total + current.val
        current = current.next
    return total

linked = Node(1, Node(2, Node(3, Node(4, Node(5, None)))))
assert list_sum(linked) == 15
print("linked list OK")

# --- Lots of function calls (call stack depth) ---
def countdown(n):
    if n <= 0:
        return 0
    return 1 + countdown(n - 1)

assert countdown(500) == 500
print("deep recursion OK")

print("\n=== ALL CRAZY EDGE CASES PASSED ===")
