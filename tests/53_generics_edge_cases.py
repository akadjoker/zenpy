# Reified generics: <T> is a REAL type-argument list, separate from value
# arguments — not sugar for f(T, ...) anymore. These are the positive
# (must-pass) edge cases; the corresponding error cases (arity mismatch,
# non-generic function called with <...>, non-class type argument, etc.)
# are compile/runtime errors and are exercised by a separate script since
# this suite only holds must-succeed tests.

class Transform:
    pass

class Sprite:
    pass

# --- Basic shape: 1 type arg, 0 value args ---
def create<T>():
    return T()

t = create<Transform>()
assert isinstance(t, Transform)

# --- 1 type arg, N value args (the realistic C++/game-engine shape) ---
def create_with<T>(a, b, c):
    inst = T()
    return (a, b, c)

assert create_with<Transform>(1, 2, 3) == (1, 2, 3)

# --- Type argument used inside the body without being a value argument ---
def type_of<T>(value):
    assert T != None
    return value

assert type_of<Transform>(42) == 42

# --- Two type parameters really are two, not folded into value arity ---
def pair<T, U>(a, b):
    assert T == Transform
    assert U == Sprite
    return a + b

assert pair<Transform, Sprite>(1, 2) == 3

# --- Generic forwarding: a generic function calling another generic
# function with the same type argument. ---
def relay<T>(value):
    return create_with<T>(value, value, value)

assert relay<Transform>(9) == (9, 9, 9)

# --- Generic METHOD on a class (the get_component<T>-style shape) ---
class Entity:
    def get_component<T>(self):
        return T

e = Entity()
assert e.get_component<Transform>() == Transform
assert e.get_component<Sprite>() == Sprite

# --- Generic call with a default value on the value parameter ---
def with_default<T>(value = 78):
    assert T == Transform
    return value

assert with_default<Transform>() == 78
assert with_default<Transform>(5) == 5

# --- `<` and `>` remain ordinary comparisons everywhere else, including
# right next to a real generic call, and even when the compared names are
# classes/instances (adjacency + "callee is a known generic" both matter —
# a class name alone does not make '<' generic call syntax). ---
assert 1 < 2
assert not (5 < 3)
assert (2 > 1) == True

# A non-generic, non-adjacent comparison whose shape could *look* like a
# type-argument list if read carelessly — this must stay a plain boolean
# expression, never a call.
x = 1
y = 2
assert (x < y) == True

print("Generics edge cases (must-pass) OK")
