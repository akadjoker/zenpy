# Round 3: reflected __gt__/__ge__, `x == 0` branches, augmented assignment on
# any receiver's field, and field classes inferred from constructors.

# --- comparisons on instances reach the right operator ---
class V:                       # compares against numbers
    def __init__(self, v):
        self.v = v
    def __lt__(self, o):
        return self.v < o
    def __le__(self, o):
        return self.v <= o
    def __gt__(self, o):
        return self.v > o
    def __ge__(self, o):
        return self.v >= o
v3 = V(3)
assert (v3 > 2) == True and (v3 > 3) == False
assert (v3 >= 3) == True and (v3 >= 4) == False
assert (2 < v3) == True and (3 < v3) == False
assert (2 <= v3) == True and (4 <= v3) == False
assert (v3 < 2) == False and (v3 <= 3) == True
def branch_gt(x):
    if x > 2:
        return "gt"
    elif x >= 2:
        return "ge"
    return "lt"
assert branch_gt(V(3)) == "gt"
assert branch_gt(V(2)) == "ge"
assert branch_gt(V(1)) == "lt"

class W:                       # compares against W instances
    def __init__(self, v):
        self.v = v
    def __lt__(self, o):
        return self.v < o.v
    def __gt__(self, o):
        return self.v > o.v
assert (W(3) > W(2)) == True and (W(2) > W(3)) == False
assert (W(2) < W(3)) == True and (W(3) < W(2)) == False

# only __gt__ defined: `a > b` still finds it (reflected from the swapped LT)
class OnlyGt:
    def __init__(self, v):
        self.v = v
    def __gt__(self, o):
        return self.v > o
assert (OnlyGt(5) > 4) == True
assert (OnlyGt(5) > 6) == False
assert (4 < OnlyGt(5)) == True

# --- equality against a literal as a branch ---
def eqz(x):
    out = []
    if x == 0:
        out.append("eq0")
    if x != 0:
        out.append("ne0")
    if x == -1:
        out.append("eqm1")
    if x == 200:
        out.append("eq200")
    return out
assert eqz(0) == ["eq0"]
assert eqz(0.0) == ["eq0"]
assert eqz(1) == ["ne0"]
assert eqz(-1) == ["ne0", "eqm1"]
assert eqz(200) == ["ne0", "eq200"]
assert eqz(None) == ["ne0"]
assert eqz("0") == ["ne0"]
assert eqz([]) == ["ne0"]
class EqAny:
    def __eq__(self, o):
        return True
ea = eqz(EqAny())
assert "eq0" in ea and "eqm1" in ea and "ne0" not in ea
def loop_until_zero(n):
    steps = 0
    while n != 0:
        n = n - 1
        steps = steps + 1
    return steps
assert loop_until_zero(5) == 5

# --- augmented assignment on a parameter's field ---
class P:
    def __init__(self):
        self.hp = 10
        self.f = 2.0
        self.s = "a"
    def selfaug(self):
        self.hp //= 3
        self.hp **= 2
        return self.hp
def hit(p: P, dmg):
    p.hp -= dmg
    p.hp -= 1
    p.hp += 2
    p.f *= 2
    p.f /= 4
    p.s += "b"
    return p.hp
p = P()
assert hit(p, 3) == 8 and p.f == 1.0 and p.s == "ab"
def hit_dyn(p):
    p.hp -= 3
    p.hp %= 5
    return p.hp
assert hit_dyn(P()) == 2
assert P().selfaug() == 9
class Q:
    def __init__(self):
        self.x = 1
        self.s = "q"
        self.hp = 100
        self.f = 8.0
q2 = Q()
assert hit(q2, 5) == 96 and q2.f == 4.0 and q2.s == "qb"   # different layout: falls back by name

# --- field classes inferred from constructors ---
class Node:
    def __init__(self, depth):
        self.left = None
        self.right = None
        self.depth = depth
        if depth > 0:
            self.left = Node(depth - 1)
            self.right = Node(depth - 1)
    def count(self):
        if self.left == None:
            return 1
        return 1 + self.left.count() + self.right.count()
    def leftmost_depth(self):
        if self.left == None:
            return self.depth
        return self.left.leftmost_depth()
n = Node(3)
assert n.count() == 15
assert n.leftmost_depth() == 0
assert n.left.left.depth == 1
# a field later holding something else: still works (checked forms fall back)
class Other:
    def __init__(self):
        self.depth = 42
    def count(self):
        return 100
n.left = Other()
assert n.count() == 1 + 100 + 7
assert n.left.depth == 42
def via_param(nd: Node):
    return nd.right.count()
assert via_param(n) == 7
print("OK")
