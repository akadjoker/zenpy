# Static method dispatch: a receiver whose class is known at compile time
# (self or an explicit class annotation) uses the class's vtable slot. The
# observable contract is still normal virtual dispatch: a child override wins.

class Base:
    def value(self):
        return 10

    def via_self(self):
        return self.value() + 1

class Child(Base):
    def value(self):
        return 20

b: Base = Base()
c: Child = Child()

assert b.value() == 10
assert b.via_self() == 11
assert c.value() == 20
assert c.via_self() == 21

# A typed base reference must retain virtual dispatch, rather than pinning
# the call to Base.value at compile time.
poly: Base = c
assert poly.value() == 20
assert poly.via_self() == 21

# The direct path must retain the ordinary call convention, including
# filling defaults for calls through both an annotation and self.
class Defaults:
    def plus(self, x=4):
        return x + 1

    def via_self(self):
        return self.plus()

d: Defaults = Defaults()
assert d.plus() == 5
assert d.via_self() == 5

# Varargs still need packing when the compiler selects the direct slot.
class VarArgs:
    def count(self, *values):
        return len(values)

    def via_self(self):
        return self.count(1, 2, 3)

v: VarArgs = VarArgs()
assert v.count(1, 2) == 2
assert v.via_self() == 3

# Array[T] carries T through an indexed read. This is the game-loop shape:
# one collection annotation, then direct virtual calls on elements.
def sum_values(items):
    typed: Array[Base] = items
    return typed[0].value() + typed[1].value()

assert sum_values([Base(), Child()]) == 30

print("static method dispatch OK")
