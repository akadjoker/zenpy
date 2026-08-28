# === MULTI-RETURN TESTS ===
# Comprehensive testing of functions returning multiple values

# Test 1: 2-element swap (this was the original bug)
def swap_2(x, y):
    return y, x

a, b = swap_2(10, 20)
assert a == 20, "swap_2: a should be 20"
assert b == 10, "swap_2: b should be 10"
print("2-element swap OK")

# Test 2: 2-element normal order
def pair(x, y):
    return x, y

x1, y1 = pair(100, 200)
assert x1 == 100, "pair: x1 should be 100"
assert y1 == 200, "pair: y1 should be 200"
print("2-element normal OK")

# Test 3: 3-element normal order
def triple(x, y, z):
    return x, y, z

a3, b3, c3 = triple(1, 2, 3)
assert a3 == 1, "triple: a3 should be 1"
assert b3 == 2, "triple: b3 should be 2"
assert c3 == 3, "triple: c3 should be 3"
print("3-element normal OK")

# Test 4: 3-element reversal (this is a known bug)
def triple_reversed(x, y, z):
    return z, y, x

r1, r2, r3 = triple_reversed(10, 20, 30)
assert r1 == 30, "triple_reversed: r1 should be 30"
assert r2 == 20, "triple_reversed: r2 should be 20"
assert r3 == 10, "triple_reversed: r3 should be 10"
print("3-element reversal OK")

# Test 5: With class methods
class Point:
    def __init__(self, x, y, z):
        self.x = x
        self.y = y
        self.z = z
    
    def coords(self):
        return self.x, self.y, self.z
    
    def reversed(self):
        return self.z, self.y, self.x

p = Point(5, 10, 15)
p1, p2, p3 = p.coords()
assert p1 == 5 and p2 == 10 and p3 == 15, "Point.coords failed"

pr1, pr2, pr3 = p.reversed()
assert pr1 == 15, "Point.reversed: pr1 should be 15"
assert pr2 == 10, "Point.reversed: pr2 should be 10"
assert pr3 == 5, "Point.reversed: pr3 should be 5"
print("Class methods OK")

print("\nAll multi-return tests passed!")
