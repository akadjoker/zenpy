# Multi-assign into the components of Vec values: each target writes the
# modified copy back to where the vector was read from, as a single
# `v.x = 1` does. Zen only (Vec2/Vec3 are not Python).

g = Vec3(1, 2, 3)


class Body:
    def __init__(self):
        self.pos = Vec2(0, 0)
        self.vel = Vec3(0, 0, 0)
        self.trail = [Vec2(0, 0), Vec2(0, 0)]

    def place(self, x, y):
        self.pos.x, self.pos.y = x, y

    def push(self):
        self.vel.x, self.vel.z = 5, 6


b = Body()
b.pos.x, b.pos.y = 3, 4
assert b.pos == Vec2(3, 4)
b.place(7, 8)
assert b.pos == Vec2(7, 8)
b.push()
assert b.vel == Vec3(5, 0, 6)
b.trail[1].x, b.trail[1].y = 1, 2
assert b.trail[0] == Vec2(0, 0) and b.trail[1] == Vec2(1, 2)
g.x, g.z = 9, 8
assert g == Vec3(9, 2, 8)


def local_vec():
    v = Vec2(1, 1)
    v.x, v.y = v.y + 1, v.x + 5
    return v


assert local_vec() == Vec2(2, 6)
