# Vec2 / Vec3 are value types: the floats live inside the Value, no heap, no
# GC. The runtime ERRORS (Vec2 + Vec3, v < w, int(v), Vec2.z, Vec2("a", 1))
# cannot be asserted here (no try/except); they are checked in
# tests/cpp/test_embedding.cpp (test_vec_values).

# Module-level vector: --bytecode runs this file through --dump + .zbc.
g = Vec3(1, 2, 3)

def get_g():
    return g

# --- construction ---------------------------------------------------------
a = Vec2(1, 2)
b = Vec3(1, 2, 3)
assert a.x == 1.0 and a.y == 2.0
assert b.x == 1.0 and b.y == 2.0 and b.z == 3.0
assert typeof(a.x) == "float"            # ints are converted to float
assert Vec2() == Vec2(0, 0)
assert Vec3() == Vec3(0.0, 0.0, 0.0)
assert Vec2(0.5, -1.5).x == 0.5 and Vec2(0.5, -1.5).y == -1.5
assert get_g() == Vec3(1, 2, 3)
assert g.z == 3.0

# --- type queries -----------------------------------------------------------
assert typeof(a) == "Vec2" and typeof(b) == "Vec3"
assert type(a) == "Vec2" and type(b) == "Vec3"
assert isinstance(a, Vec2) and not isinstance(a, Vec3)
assert isinstance(b, Vec3) and not isinstance(b, Vec2)
assert not isinstance(3, Vec2)

# --- arithmetic -------------------------------------------------------------
assert Vec2(1, 2) + Vec2(3, 4) == Vec2(4, 6)
assert Vec2(5, 5) - Vec2(3, 4) == Vec2(2, 1)
assert Vec3(1, 2, 3) + Vec3(4, 5, 6) == Vec3(5, 7, 9)
assert Vec3(5, 7, 9) - Vec3(4, 5, 6) == Vec3(1, 2, 3)
assert Vec2(1, 2) * 3 == Vec2(3, 6)
assert 3 * Vec2(1, 2) == Vec2(3, 6)
assert Vec3(1, 2, 3) * 0.5 == Vec3(0.5, 1.0, 1.5)
assert 0.5 * Vec3(1, 2, 3) == Vec3(0.5, 1.0, 1.5)
assert Vec3(2, 4, 6) / 2 == Vec3(1, 2, 3)
assert Vec2(1, 1) / 4 == Vec2(0.25, 0.25)
assert Vec2(1, 2) * Vec2(3, 4) == Vec2(3, 8)           # component-wise
assert Vec3(1, 2, 3) * Vec3(2, 2, 2) == Vec3(2, 4, 6)
assert -Vec2(1, -2) == Vec2(-1, 2)
assert -Vec3(1, -2, 3) == Vec3(-1, 2, -3)

v = Vec3(1, 1, 1)
v += Vec3(1, 2, 3)
assert v == Vec3(2, 3, 4)
v -= Vec3(1, 1, 1)
assert v == Vec3(1, 2, 3)
v *= 2
assert v == Vec3(2, 4, 6)
v /= 2
assert v == Vec3(1, 2, 3)
w = v
w += Vec3(1, 1, 1)          # value semantics: v is a copy, not an alias
assert v == Vec3(1, 2, 3) and w == Vec3(2, 3, 4)

# --- equality, truthiness ----------------------------------------------------
assert Vec2(1, 2) == Vec2(1, 2)
assert Vec2(1, 2) != Vec2(1, 3)
assert Vec3(1, 2, 3) != Vec3(1, 2, 4)
assert Vec2(1, 2) != Vec3(1, 2, 0)      # different kinds never equal
assert Vec2(1, 2) != 1
assert not (Vec2(1, 2) == [1, 2])
assert not Vec2(0, 0)
assert not Vec3(0, 0, 0)
assert Vec2(0, 1)
assert Vec3(0, 0, 0.5)
assert Vec3(-0.0, 0, 0) == Vec3(0, 0, 0)
if Vec3(0, 0, 0):
    assert False
if not Vec2(0, 1):
    assert False
assert (Vec2(0, 0) or 7) == 7
assert (Vec2(0, 2) or 7) == Vec2(0, 2)

# --- fields on locals (write-back through the register) -----------------------
def fields():
    p = Vec3(1, 2, 3)
    assert p.x == 1.0 and p.y == 2.0 and p.z == 3.0
    p.x = 2.5
    assert p == Vec3(2.5, 2, 3)
    p.y += 1
    assert p == Vec3(2.5, 3, 3)
    p.z = p.x * 2
    assert p == Vec3(2.5, 3, 5)
    q = p                         # copy
    q.x = 100
    assert p.x == 2.5 and q.x == 100.0
    r = Vec2(1, 2)
    r.x = 10
    r.y *= 4
    assert r == Vec2(10, 8)
    return p
assert fields() == Vec3(2.5, 3, 5)

# --- methods -------------------------------------------------------------------
assert Vec2(3, 4).length() == 5.0
assert Vec3(2, 3, 6).length() == 7.0
assert Vec2(3, 4).length_sq() == 25.0
assert Vec3(1, 2, 2).length_sq() == 9.0
assert Vec2(3, 4).normalized() == Vec2(0.6, 0.8)
assert Vec3(0, 0, 5).normalized() == Vec3(0, 0, 1)
assert Vec2(0, 0).normalized() == Vec2(0, 0)          # zero stays zero
assert Vec3(0, 0, 0).normalized() == Vec3(0, 0, 0)
assert Vec2(1, 2).dot(Vec2(3, 4)) == 11.0
assert Vec3(1, 2, 3).dot(Vec3(4, 5, 6)) == 32.0
assert Vec3(1, 0, 0).cross(Vec3(0, 1, 0)) == Vec3(0, 0, 1)
assert Vec3(0, 1, 0).cross(Vec3(1, 0, 0)) == Vec3(0, 0, -1)
assert Vec3(1, 2, 3).cross(Vec3(4, 5, 6)) == Vec3(-3, 6, -3)
assert Vec2(1, 0).cross(Vec2(0, 1)) == 1.0             # 2D cross: scalar
assert Vec2(0, 1).cross(Vec2(1, 0)) == -1.0
assert Vec2(0, 0).distance(Vec2(3, 4)) == 5.0
assert Vec3(1, 1, 1).distance(Vec3(1, 1, 1)) == 0.0
assert Vec3(0, 0, 0).distance_sq(Vec3(1, 2, 2)) == 9.0
assert Vec2(0, 0).lerp(Vec2(10, 20), 0.5) == Vec2(5, 10)
assert Vec3(0, 0, 0).lerp(Vec3(2, 4, 8), 0.25) == Vec3(0.5, 1, 2)
assert Vec2(1, 1).lerp(Vec2(3, 5), 0) == Vec2(1, 1)
assert Vec2(1, 1).lerp(Vec2(3, 5), 1) == Vec2(3, 5)
assert Vec2(-1, 2).abs() == Vec2(1, 2)
assert Vec3(-1, -2, 3).abs() == Vec3(1, 2, 3)
assert Vec2(1, 2).with_x(9) == Vec2(9, 2)
assert Vec2(1, 2).with_y(9) == Vec2(1, 9)
assert Vec3(1, 2, 3).with_x(7) == Vec3(7, 2, 3)
assert Vec3(1, 2, 3).with_y(7) == Vec3(1, 7, 3)
assert Vec3(1, 2, 3).with_z(7) == Vec3(1, 2, 7)
orig = Vec3(1, 2, 3)
orig.with_x(5)                                         # returns a copy, orig is untouched
assert orig == Vec3(1, 2, 3)
assert Vec2(1, 2).to_list() == [1.0, 2.0]
assert Vec3(1, 2, 3).to_list() == [1.0, 2.0, 3.0]
# chained
assert (Vec2(3, 4) * 2).length() == 10.0
assert Vec3(0, 3, 4).normalized().length() > 0.9999

# --- printing -------------------------------------------------------------------
assert str(Vec3(1, 2, 3)) == "Vec3(1.0, 2.0, 3.0)"
assert str(Vec2(1.5, -2)) == "Vec2(1.5, -2.0)"
assert str(Vec2(0.1, 0.2)) == "Vec2(0.1, 0.2)"         # float32-shortest, not 0.10000000149...
assert repr(Vec3(0, 0, 0)) == "Vec3(0.0, 0.0, 0.0)"
assert str([Vec2(1, 2), Vec2(3, 4)]) == "[Vec2(1.0, 2.0), Vec2(3.0, 4.0)]"
assert "p=" + str(Vec2(1, 2)) == "p=Vec2(1.0, 2.0)"
assert "p=" + Vec2(1, 2) == "p=Vec2(1.0, 2.0)"
assert Vec3(1, 2, 3) + "!" == "Vec3(1.0, 2.0, 3.0)!"

# --- containers ---------------------------------------------------------------------
d = {}
d[Vec2(1, 2)] = "a"
d[Vec3(1, 2, 3)] = "b"
d[Vec2(1, 2)] = "c"                                   # equal vectors collide
assert len(d) == 2
assert d[Vec2(1, 2)] == "c" and d[Vec3(1, 2, 3)] == "b"
assert Vec2(1, 2) in d and Vec2(2, 1) not in d
d[Vec3(0, 0, 0)] = 1
assert d[Vec3(-0.0, 0, 0)] == 1                       # -0.0 hashes like 0.0
s = set([Vec2(1, 2), Vec2(1, 2), Vec2(2, 1)])
assert len(s) == 2
assert Vec2(2, 1) in s and Vec2(3, 3) not in s
lst = [Vec2(1, 2), Vec2(3, 4), Vec2(5, 6)]
assert lst[1] == Vec2(3, 4)
assert lst.index_of(Vec2(5, 6)) == 2
assert Vec2(3, 4) in lst
assert lst[0] + lst[2] == Vec2(6, 8)
total = Vec2(0, 0)
for p in lst:
    total += p
assert total == Vec2(9, 12)

# --- args / returns -------------------------------------------------------------------
def scale(v, k):
    return v * k
def mid(a, b):
    return (a + b) * 0.5
def both(a):
    return [a, a.length()]
assert scale(Vec3(1, 2, 3), 2) == Vec3(2, 4, 6)
assert mid(Vec2(0, 0), Vec2(2, 4)) == Vec2(1, 2)
pair = both(Vec2(3, 4))
assert pair[0] == Vec2(3, 4) and pair[1] == 5.0

class Body:
    def __init__(self, pos, vel):
        self.pos = pos
        self.vel = vel
    def step(self, dt):
        self.pos = self.pos + self.vel * dt
    def speed(self):
        return self.vel.length()
body = Body(Vec2(0, 0), Vec2(3, 4))
body.step(0.5)
body.step(0.5)
assert body.pos == Vec2(3, 4)
assert body.speed() == 5.0
assert body.pos.x == 3.0                              # reading a field of a field works

# --- nested field writes: the receiver is a copy, the compiler stores it back ------------
class Mover:
    def __init__(self):
        self.pos = Vec3(1, 2, 3)
        self.vel = Vec2(0, 0)
        self.tag = 0
    def nudge(self, dx):
        self.pos.x += dx
        self.pos.y -= 1
        self.pos.z *= 2
        self.vel.y = dx
    def put(self, v):
        self.pos.x = v
m = Mover()
m.nudge(4)
assert m.pos == Vec3(5, 1, 6), m.pos
assert m.vel == Vec2(0, 4), m.vel
m.put(9)
assert m.pos == Vec3(9, 1, 6)
m.pos.x = 7                                           # same thing from outside the class
m.pos.z += 1
assert m.pos == Vec3(7, 1, 7)

class Holder:
    def __init__(self):
        self.mover = Mover()
    def bump(self):
        self.mover.pos.x += 1                         # object link, then a vector link
h = Holder()
h.bump()
h.bump()
assert h.mover.pos == Vec3(3, 2, 3)
h.mover.pos.y = 50
assert h.mover.pos.y == 50.0

def shove(b: Mover, dx):                              # annotated receiver: checked-index read
    b.pos.x += dx
    b.vel.x = dx
shove(m, 1)
assert m.pos.x == 8.0 and m.vel.x == 1.0

g = Vec2(1, 2)                                        # module-level global
g.x = 10
assert g == Vec2(10, 2)
g.y += 5
assert g == Vec2(10, 7)
def poke_global():
    g.x = 3
    g.y *= 2
poke_global()
assert g == Vec2(3, 14)

pts = [Vec2(1, 2), Vec2(3, 4)]
pts[1].y = 9
assert pts[1] == Vec2(3, 9) and pts[0] == Vec2(1, 2)
i = 0
while i < 2:
    pts[i].x += 1
    i += 1
assert pts[0] == Vec2(2, 2) and pts[1] == Vec2(4, 9)
for k in range(2):
    pts[k].y = pts[k].y * 2
assert pts[0] == Vec2(2, 4) and pts[1] == Vec2(4, 18)
def bump_all(arr):
    j = 0
    while j < len(arr):
        arr[j].x -= 1
        j += 1
bump_all(pts)
assert pts == [Vec2(1, 4), Vec2(3, 18)]
pts[0 + 1].x = 0
assert pts[1] == Vec2(0, 18)

d = {"a": Vec2(0, 0)}
d["a"].x = 5
d["a"].y += 2
assert d["a"] == Vec2(5, 2)
grid = [[Vec3(0, 0, 0), Vec3(1, 1, 1)], [Vec3(2, 2, 2), Vec3(3, 3, 3)]]
grid[1][0].z = 8
assert grid[1][0] == Vec3(2, 2, 8) and grid[0][1] == Vec3(1, 1, 1)

def make_counter():
    c = Vec2(0, 0)                                    # captured local: the receiver is an upvalue
    def inc():
        c.x += 1
        return c.x
    return inc
inc = make_counter()
inc()
assert inc() == 2.0

# a plain heap-object chain is untouched by the write-back
class Node:
    def __init__(self):
        self.item = 0
        self.next = None
a = Node()
a.next = Node()
a.next.item = 3
a.next.item += 4
assert a.next.item == 7 and a.item == 0
pair = [Node(), Node()]
pair[1].item = 5
assert pair[1].item == 5 and pair[0].item == 0
nd = {"n": Node()}
nd["n"].item = 6
assert nd["n"].item == 6

# not written back: a value a call returns is a copy of whatever it read
box = [Vec2(1, 1)]
def first():
    return box[0]
first().x = 99
assert box[0] == Vec2(1, 1)
w = (box[0] if True else box[0]).x                    # reads are never affected
assert w == 1.0

# --- 1M iterations: no allocation, no GC growth ----------------------------------------
def integrate(n):
    p = Vec3(0, 0, 0)
    v = Vec3(1, 2, 3)
    i = 0
    while i < n:
        p = p + v * 0.5
        i += 1
    return p
before = mem_used()
res = integrate(1000000)
after = mem_used()
assert res == Vec3(500000, 1000000, 1500000), res
assert after - before < 65536, after - before

# --- json ---------------------------------------------------------------------------------
import json
assert json.stringify(Vec2(1, 2)) == json.stringify([1.0, 2.0])
assert json.stringify(Vec3(1, 2, 3)) == json.stringify([1.0, 2.0, 3.0])
assert json.stringify(Vec2(0.5, -1.5)) == "[0.5,-1.5]"
assert json.stringify({"p": Vec3(1, 2, 3)}) == json.stringify({"p": [1.0, 2.0, 3.0]})
print("OK")
