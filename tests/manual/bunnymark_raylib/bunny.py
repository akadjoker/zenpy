# CPython bunnymark script (same shape as bunny.zen).
import native

class Bunny:
    __slots__ = ("x", "y", "vx", "vy")

    def __init__(self, x, y, vx, vy):
        self.x = x
        self.y = y
        self.vx = vx
        self.vy = vy

    def update(self, dt):
        self.x = self.x + self.vx * dt
        self.y = self.y + self.vy * dt
        if self.x > max_x or self.x < 0:
            self.vx = -self.vx
        if self.y > max_y or self.y < 0:
            self.vy = -self.vy
        native.draw_bunny(self.x, self.y)

bunnies = []
max_x = native.screen_width() - 26
max_y = native.screen_height() - 37

def add_bunnies(n):
    rand = native.rand
    for _ in range(n):
        bunnies.append(Bunny(rand(0, max_x), rand(0, max_y), rand(-250, 250), rand(-250, 250)))

def update_all(dt):
    for b in bunnies:
        b.update(dt)
