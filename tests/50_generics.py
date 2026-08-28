# Generic calls pass their type arguments as normal runtime values.

class Transform:
    pass

class Sprite:
    pass

def choose<T>(value):
    assert T == Transform
    return value

def relay<T>(value):
    # Generic parameters can be forwarded to another generic call.
    return choose<T>(value)

def second<T, U>(value):
    assert T == Transform
    assert U == Sprite
    return value

def default_value<T>(value = 78):
    assert T == Transform
    return value

assert choose<Transform>(12) == 12
assert relay<Transform>(34) == 34
assert second<Transform, Sprite>(56) == 56
assert default_value<Transform>() == 78

class Entity:
    def get_component<T>(self):
        return T

entity = Entity()
assert entity.get_component<Transform>() == Transform
assert entity.get_component<Sprite>() == Sprite

# `<` remains the regular comparison operator unless it has generic-call form.
assert 1 < 2
