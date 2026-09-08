# `x is not None` (was parsed as `x is (not None)`), class-body field
# annotations feeding static dispatch, and comprehensions on OP_FOR_NEXT.

# --- is not ---
def isn(x):
    if x is not None:
        return "some"
    return "none"
assert isn(None) == "none" and isn(0) == "some" and isn(False) == "some" and isn("") == "some"
def isn_expr(x):
    return x is not None
assert isn_expr(None) == False and isn_expr(0) == True
def isn_while(xs):
    n = 0
    cur = xs
    while cur is not None:
        n = n + 1
        cur = None
    return n
assert isn_while(1) == 1 and isn_while(None) == 0
a = [1]
b = a
assert (a is not b) == False and (a is not [1]) == True and (a is b) == True
class Eq:
    def __eq__(self, o):
        return True     # `is` / `is not` must never consult __eq__
e = Eq()
assert (e is not None) == True and (e is None) == False
def isn_branch_eq(x):
    if x is not None:
        return 1
    return 0
assert isn_branch_eq(Eq()) == 1

# --- class body field annotations ---
class Node:
    item: int = 0
    left: Node? = None
    right: Node | None = None
    tag: str
    def __init__(self, item):
        self.item = item
    def check(self):
        n = self.item
        if self.left is not None:
            n = n + self.left.check()
        if self.right is not None:
            n = n + self.right.check()
        return n
    def leftmost(self):
        if self.left is not None:
            return self.left.leftmost()
        return self.item
root = Node(1)
root.left = Node(2)
root.right = Node(3)
root.left.left = Node(4)
assert root.check() == 10
assert root.leftmost() == 4
assert root.tag == None and Node(5).left == None
class Other:
    def __init__(self):
        self.item = 100
    def check(self):
        return self.item
root.left = Other()          # the annotation is a hint: a wrong one still runs correctly
assert root.check() == 1 + 100 + 3
class Sub(Node):
    def __init__(self, item):
        super().__init__(item)
        self.extra = 1
s = Sub(7)
s.left = Node(1)
assert s.check() == 8 and s.extra == 1

# --- comprehensions (FOR_NEXT) ---
assert [x * 2 for x in [1, 2, 3]] == [2, 4, 6]
assert [x for x in [1, 2, 3, 4, 5, 6] if x % 2 == 0] == [2, 4, 6]
assert [x for x in range(4)] == [0, 1, 2, 3]
assert [x for x in range(10, 0, -3)] == [10, 7, 4, 1]
assert [c for c in "abc"] == ["a", "b", "c"]
assert [x for x in []] == []
def gen():
    yield 1
    yield 2
assert [x for x in gen()] == [1, 2]
d = {k: k * k for k in [1, 2, 3]}
assert d[2] == 4 and len(d) == 3
d2 = {k: 1 for k in range(3) if k != 1}
assert len(d2) == 2 and 1 not in d2
st = {x % 3 for x in range(9)}
assert len(st) == 3
xs = [[y for y in range(x)] for x in range(3)]
assert xs == [[], [0], [0, 1]]
def comp_in_fn(xs):
    total = 0
    for v in [x + 1 for x in xs if x > 0]:
        total = total + v
    return total
assert comp_in_fn([1, -1, 2]) == 5

print("ok")
