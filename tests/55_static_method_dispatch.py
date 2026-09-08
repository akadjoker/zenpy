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

# `self.field * local` is compiled as the fused GETFIELD_MUL form used by
# movement code. It must retain ordinary arithmetic and operator overloads.
class Motion:
    def __init__(self, velocity):
        self.velocity = velocity

    def distance(self, dt):
        return self.velocity * dt

class ScaleBySeven:
    def __mul__(self, value):
        return value * 7

class ObjectMotion:
    def __init__(self):
        self.velocity = ScaleBySeven()

    def distance(self, dt):
        return self.velocity * dt

assert Motion(1.5).distance(2.0) == 3.0
assert ObjectMotion().distance(3) == 21

# The complete movement assignment has its own fusion. A non-numeric x must
# deopt through the untouched four trailing instructions and call __add__.
class StepMotion:
    def __init__(self, x, velocity):
        self.x = x
        self.velocity = velocity

    def step(self, dt):
        self.x = self.x + self.velocity * dt
        return self.x

class AddByFive:
    def __add__(self, value):
        return 5

assert StepMotion(1, 3).step(2) == 7
assert StepMotion(AddByFive(), 3).step(2) == 5

print("static method dispatch OK")
