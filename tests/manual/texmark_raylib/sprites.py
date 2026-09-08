# CPython texmark script (same shape as sprites.zen).
import native

class Sprite:
    __slots__ = ("tex", "x", "y", "vx", "vy")

    def __init__(self, tex, x, y, vx, vy):
        self.tex = tex
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
        self.tex.draw(self.x, self.y)

wabbit = native.Texture("wabbit_alpha.png")
sprites = []
max_x = native.screen_width() - wabbit.width()
max_y = native.screen_height() - wabbit.height()

def add_sprites(n, x, y):
    rand = native.rand
    for _ in range(n):
        if x < 0:
            sprites.append(Sprite(wabbit, rand(0, max_x), rand(0, max_y), rand(-250, 250), rand(-250, 250)))
        else:
            sprites.append(Sprite(wabbit, x, y, rand(-250, 250), rand(-250, 250)))

def update_all(dt):
    for s in sprites:
        s.update(dt)
