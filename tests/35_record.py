# Test record (struct) declaration and usage

record Vec2 { x, y }
record Color { r, g, b, a }
record Point3D { x, y, z }

# Basic construction and field access
v = Vec2(10, 20)
assert v.x == 10
assert v.y == 20

# Modify fields
v.x = 100
v.y = 200
assert v.x == 100
assert v.y == 200

# Multiple instances are independent
p1 = Vec2(1, 2)
p2 = Vec2(3, 4)
assert p1.x == 1
assert p2.x == 3
p1.x = 99
assert p1.x == 99
assert p2.x == 3

# Mixed types in fields
c = Color(255, 128, 0, 1.0)
assert c.r == 255
assert c.g == 128
assert c.b == 0
assert c.a == 1.0

# Three fields
pt = Point3D(1, 2, 3)
assert pt.x == 1
assert pt.y == 2
assert pt.z == 3

# Record as function argument
def length_sq(v):
    return v.x * v.x + v.y * v.y

assert length_sq(Vec2(3, 4)) == 25
assert length_sq(Vec2(0, 5)) == 25

# Record as return value
def make_vec(a, b):
    return Vec2(a * 2, b * 2)

r = make_vec(5, 10)
assert r.x == 10
assert r.y == 20

# Record in array
points = [Vec2(1, 1), Vec2(2, 2), Vec2(3, 3)]
assert points[0].x == 1
assert points[2].y == 3

# Record field arithmetic
a = Vec2(10, 20)
b = Vec2(30, 40)
sum_x = a.x + b.x
sum_y = a.y + b.y
assert sum_x == 40
assert sum_y == 60

# Nested record usage
def add_vec(a, b):
    return Vec2(a.x + b.x, a.y + b.y)

result = add_vec(Vec2(1, 2), Vec2(3, 4))
assert result.x == 4
assert result.y == 6

print("All record tests passed.")
