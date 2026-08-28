# Class body fields: "class A:" followed by "name = <literal>" gives a field
# a starting value without writing a constructor for it.

# --- 1. Every literal kind ---
class Basics:
    speed = 90.0
    lives = 3
    label = "spin"
    enabled = True
    disabled = False
    offset = -2.5
    count = -7
    nothing = None

    def total(self):
        return self.speed + self.offset

b = Basics()
assert b.speed == 90.0
assert b.lives == 3
assert b.label == "spin"
assert b.enabled == True
assert b.disabled == False
assert b.offset == -2.5
assert b.count == -7
assert b.nothing == None
assert b.total() == 87.5
print("class_field_literals OK")

# --- 2. An int stays an int, a float stays a float, even declared side by
# side: both go through the constant pool, and a pool that deduplicated on
# '==' alone would hand the int the float's slot.
class Kinds:
    i = 3
    f = 3.0
    big = 40000
    bigf = 40000.0

k = Kinds()
assert k.i == 3
assert k.f == 3.0
assert type(k.i) == "int"
assert type(k.f) == "float"
assert type(k.big) == "int"
assert type(k.bigf) == "float"
print("class_field_kinds OK")

# --- 3. Instances do not share the values ---
class Counter:
    n = 0

a = Counter()
c = Counter()
a.n = 5
assert a.n == 5
assert c.n == 0
print("class_field_per_instance OK")

# --- 4. A constructor still runs and wins ---
class WithInit:
    hp = 10
    armour = 1

    def __init__(self):
        self.hp = 99

w = WithInit()
assert w.hp == 99      # __init__ overwrote the declaration
assert w.armour == 1   # and left the rest alone
print("class_field_init_wins OK")

# --- 5. Methods still work alongside declarations ---
class Mixed:
    base = 10

    def add(self, n):
        return self.base + n

    def bump(self):
        self.base = self.base + 1

m = Mixed()
assert m.add(5) == 15
m.bump()
assert m.base == 11
assert m.add(5) == 16
print("class_field_with_methods OK")

# --- 6. Inheritance: a subclass gets the parent's values ---
class Actor:
    hp = 100
    name = "actor"

class Enemy(Actor):
    damage = 7

e = Enemy()
assert e.hp == 100
assert e.name == "actor"
assert e.damage == 7
print("class_field_inherited OK")

# --- 7. A subclass can redeclare one ---
class Boss(Actor):
    hp = 500

boss = Boss()
assert boss.hp == 500
assert boss.name == "actor"
plain = Actor()
assert plain.hp == 100     # the parent is untouched
print("class_field_override OK")

# --- 8. A subclass constructor still reaches the parent's ---
class Base:
    v = 1

    def __init__(self):
        self.v = 2

class Derived(Base):
    w = 3

d = Derived()
assert d.v == 2    # Base.__init__ ran
assert d.w == 3    # and the subclass declaration survived
print("class_field_inherited_init OK")

# --- 9. Many instances, so the values survive collection ---
class Held:
    tag = "held"
    weight = 1.5

items = []
i = 0
while i < 500:
    items.append(Held())
    i = i + 1

assert len(items) == 500
assert items[0].tag == "held"
assert items[499].tag == "held"
assert items[250].weight == 1.5
print("class_field_gc OK")

print("=== All class field tests passed ===")
