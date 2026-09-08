# `local = <expr>` / `return <expr>` write the value from its producing
# instruction instead of through a MOVE. That must not change any result:
# captured registers, branches inside the RHS, operands that are the target.

def captured_return(x):
    g = lambda: x
    r = x + 1
    return x + 1, g()
cr_a, cr_b = captured_return(4)
assert cr_a == 5 and cr_b == 4

def captured_return2(x):
    def g():
        return x
    return x * 2
assert captured_return2(21) == 42

def ternary_assign(c):
    a = 1
    a = 10 if c else 20
    return a
assert ternary_assign(True) == 10
assert ternary_assign(False) == 20

def ternary_return(c):
    return 10 if c else 20
assert ternary_return(True) == 10
assert ternary_return(False) == 20

def and_or(c):
    a = 5
    a = c and 7
    b = c or 9
    return a, b
ao_a, ao_b = and_or(True)
assert ao_a == 7 and ao_b == True
ao_a, ao_b = and_or(False)
assert ao_a == False and ao_b == 9

def swap_operands():
    a = 3
    b = 10
    a = b + a
    b = a - b
    return a, b
sw_a, sw_b = swap_operands()
assert sw_a == 13 and sw_b == 3

class P:
    def __init__(self):
        self.x = 4
        self.items = [1, 2, 3]
    def getx(self):
        return self.x
    def item(self, i):
        return self.items[i]
    def name(self):
        return "P" + "!"
    def neg(self):
        return -self.x
    def cmp(self, v):
        return self.x < v
    def count(self):
        return len(self.items)
def fields():
    p = P()
    a = p
    a = a.x
    b = p
    b = b.items[1]
    c = 7
    c = c
    return [a, b, c, p.getx(), p.item(2), p.name(), p.neg(), p.cmp(5), p.count()]
assert fields() == [4, 2, 7, 4, 3, "P!", -4, True, 3]

def upvalue_target():
    n = 1
    def inc():
        nonlocal n
        n = n + 1
        return n
    inc()
    inc()
    return n
assert upvalue_target() == 3

def comprehension_assign():
    xs = [1, 2, 3]
    xs = [x * 2 for x in xs]
    return xs
assert comprehension_assign() == [2, 4, 6]

def call_result():
    v = len("abc")
    v = str(v) + "!"
    return v
assert call_result() == "3!"

# while with a hoisted literal: body declares locals, nested loops, continue
def hoisted():
    i = 0
    total = 0
    while i < 10:
        j = 0
        while j <= 2:
            j = j + 1
            if j == 2:
                continue
            total = total + j
        k = i * 2
        i = i + 1
        total = total + k
    return total
assert hoisted() == 10 * 4 + 90

def hoisted_eq():
    i = 0
    while i == 0:
        i = i + 1
    return i
assert hoisted_eq() == 1

s = ""
n = 0
while n < 3:
    s = s + "x"
    n += 1
assert s == "xxx"

def strings_fused(a, b):
    if a < b:
        return "lt"
    elif a <= b:
        return "le"
    return "gt"
assert strings_fused("a", "b") == "lt"
assert strings_fused("b", "b") == "le"
assert strings_fused("c", "b") == "gt"

class Cmp:
    def __init__(self, v):
        self.v = v
    def __lt__(self, o):
        return self.v < o.v
    def __le__(self, o):
        return self.v <= o.v
def overloaded(a, b):
    if a < b:
        return 1
    elif a <= b:
        return 2
    return 3
assert overloaded(Cmp(1), Cmp(2)) == 1
assert overloaded(Cmp(2), Cmp(2)) == 2
assert overloaded(Cmp(3), Cmp(2)) == 3

def float_hoist():
    x = 0.0
    while x < 2.5:
        x = x + 1
    return x
assert float_hoist() == 3.0

print("OK")
