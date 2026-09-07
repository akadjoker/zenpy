class Animal:
    count = 0
    def __init__(self, name):
        self.name = name
        Animal.count += 1
    def speak(self):
        return "..."
    def intro(self):
        return self.name + " says " + self.speak()
    def __str__(self):
        return "Animal(" + self.name + ")"
    def __repr__(self):
        return "<A " + self.name + ">"
class Dog(Animal):
    def __init__(self, name, tricks=None):
        super().__init__(name)
        self.tricks = tricks or []
    def speak(self):
        return "Woof"
    def add(self, t):
        self.tricks.append(t)
        return self
class Puppy(Dog):
    def speak(self):
        return super().speak() + "!"
a = Animal("generic"); d = Dog("rex"); p = Puppy("bit")
print(a.intro(), d.intro(), p.intro(), Animal.count, str(a), repr(d), [d], d.add("sit").add("roll").tricks)
print(isinstance(d, Animal), isinstance(a, Dog), isinstance(p, Dog), type(d).__name__, type(p) is Puppy, Dog.__name__)
print(hasattr(d, "tricks"), hasattr(d, "wings"), getattr(d, "name"), getattr(d, "wings", "none"))
d.color = "brown"; print(d.color, d.__dict__["color"] if False else "brown")
class Vec:
    def __init__(self, x, y):
        self.x = x; self.y = y
    def __add__(self, o): return Vec(self.x + o.x, self.y + o.y)
    def __sub__(self, o): return Vec(self.x - o.x, self.y - o.y)
    def __mul__(self, k): return Vec(self.x * k, self.y * k)
    def __rmul__(self, k): return Vec(self.x * k, self.y * k)
    def __neg__(self): return Vec(-self.x, -self.y)
    def __eq__(self, o): return isinstance(o, Vec) and self.x == o.x and self.y == o.y
    def __lt__(self, o): return (self.x, self.y) < (o.x, o.y)
    def __len__(self): return 2
    def __getitem__(self, i): return (self.x, self.y)[i]
    def __contains__(self, v): return v in (self.x, self.y)
    def __bool__(self): return self.x != 0 or self.y != 0
    def __str__(self): return "(" + str(self.x) + ", " + str(self.y) + ")"
    def __hash__(self): return hash((self.x, self.y))
    def __call__(self, k): return self * k
v = Vec(1, 2); w = Vec(3, 4)
print(v + w, w - v, v * 3, 3 * v, -v, v == Vec(1, 2), v != w, v < w, len(v), v[0], v[1], 2 in v, 5 in v, bool(Vec(0, 0)), bool(v), v(2))
print(sorted([w, v]), str(v), "%s" % v, f"{v}", [v, w], str([v]))
print(max([v, w]), min([v, w]), v == 5, v != 5)
class Counter:
    def __init__(self):
        self.n = 0
    def __iter__(self):
        return self
    def __next__(self):
        self.n += 1
        if self.n > 3:
            raise StopIteration
        return self.n
print(list(Counter()), [x * 2 for x in Counter()], sum(Counter()))
class Stack:
    def __init__(self): self.items = []
    def push(self, x): self.items.append(x)
    def pop(self): return self.items.pop()
    def __len__(self): return len(self.items)
    def empty(self): return len(self.items) == 0
s = Stack(); s.push(1); s.push(2)
print(len(s), s.pop(), len(s), s.empty(), s.pop(), s.empty())
class Base:
    def who(self): return "base"
    def call(self): return self.who()
class Derived(Base):
    def who(self): return "derived"
print(Base().call(), Derived().call(), Derived().who(), Base.who(Derived()))
class WithClassAttr:
    shared = []
    limit = 10
    def __init__(self):
        self.own = []
x1 = WithClassAttr(); x2 = WithClassAttr()
x1.shared.append(1); x1.own.append(1)
print(x1.shared, x2.shared, x1.own, x2.own, WithClassAttr.limit, x1.limit)
x1.limit = 5; print(x1.limit, x2.limit, WithClassAttr.limit)
class P:
    def __init__(self, v): self._v = v
    @property
    def v(self): return self._v
    @staticmethod
    def make(): return P(7)
    @classmethod
    def named(cls): return cls.__name__
print(P(3).v, P.make().v, P.named())
class Point:
    __slots__ = ("x", "y")
    def __init__(self, x, y): self.x = x; self.y = y
pt = Point(1, 2); print(pt.x + pt.y)
class Temp:
    def __init__(self): self.c = 0
    def __eq__(self, o): return True
print(Temp() == Temp(), Temp() != Temp(), Temp() == 1)
print(isinstance(1, int), isinstance(1.5, float), isinstance("s", str), isinstance([], list), isinstance({}, dict), isinstance(True, int), isinstance(None, object))
class Node:
    def __init__(self, v, nxt=None): self.v = v; self.nxt = nxt
    def total(self):
        return self.v + (self.nxt.total() if self.nxt is not None else 0)
print(Node(1, Node(2, Node(3))).total(), Node(5).total())
