# Game-shaped workloads: SAT polygon collision with per-frame transforms,
# broadphase over 1000 bodies, and an ECS-style sprite update.
#
# These stress what a game actually does every frame — float math, object
# field access, array iteration, method calls on many small objects — rather
# than the recursive/integer shapes the algo benchmarks cover.
#
# Deterministic: one LCG, no wall-clock input, so every language must print
# the same checksums.

NBODIES = 1000
NFRAMES = 60
NSPRITES = 4000
WORLD = 2048

seed = 42
def rnd():
    global seed
    seed = (seed * 16807) % 2147483647
    return seed

# ---------------------------------------------------------------
# 1. SAT: convex polygons, rotated and translated every frame
# ---------------------------------------------------------------

def make_poly(n, radius):
    # Unit polygon centred on origin; the body carries the transform.
    pts = []
    i = 0
    while i < n:
        a = 6.28318530718 * i / n
        # cos/sin without a math module: 7-term Taylor, plenty for a fixed set
        c = 1.0
        s = a
        a2 = a * a
        term_c = 1.0
        term_s = a
        k = 1
        while k <= 5:
            term_c = -term_c * a2 / ((2 * k - 1) * (2 * k))
            c = c + term_c
            term_s = -term_s * a2 / ((2 * k) * (2 * k + 1))
            s = s + term_s
            k = k + 1
        pts.append([c * radius, s * radius])
        i = i + 1
    return pts

class Body:
    def __init__(self, px, py, angle, vx, vy, spin, poly):
        self.px = px
        self.py = py
        self.angle = angle
        self.vx = vx
        self.vy = vy
        self.spin = spin
        self.poly = poly
        self.wx = []
        self.wy = []
        self.minx = 0.0
        self.miny = 0.0
        self.maxx = 0.0
        self.maxy = 0.0
        i = 0
        while i < len(poly):
            self.wx.append(0.0)
            self.wy.append(0.0)
            i = i + 1

    def integrate(self, dt):
        self.px = self.px + self.vx * dt
        self.py = self.py + self.vy * dt
        self.angle = self.angle + self.spin * dt
        if self.px < 0.0:
            self.px = self.px + WORLD
        if self.px > WORLD:
            self.px = self.px - WORLD
        if self.py < 0.0:
            self.py = self.py + WORLD
        if self.py > WORLD:
            self.py = self.py - WORLD

    def transform(self):
        # cos/sin of the current angle, same Taylor as above but wrapped
        a = self.angle
        while a > 3.14159265359:
            a = a - 6.28318530718
        while a < -3.14159265359:
            a = a + 6.28318530718
        a2 = a * a
        c = 1.0
        s = a
        term_c = 1.0
        term_s = a
        k = 1
        while k <= 5:
            term_c = -term_c * a2 / ((2 * k - 1) * (2 * k))
            c = c + term_c
            term_s = -term_s * a2 / ((2 * k) * (2 * k + 1))
            s = s + term_s
            k = k + 1
        n = len(self.poly)
        minx = 1000000.0
        miny = 1000000.0
        maxx = -1000000.0
        maxy = -1000000.0
        i = 0
        while i < n:
            p = self.poly[i]
            x = p[0] * c - p[1] * s + self.px
            y = p[0] * s + p[1] * c + self.py
            self.wx[i] = x
            self.wy[i] = y
            if x < minx:
                minx = x
            if x > maxx:
                maxx = x
            if y < miny:
                miny = y
            if y > maxy:
                maxy = y
            i = i + 1
        self.minx = minx
        self.miny = miny
        self.maxx = maxx
        self.maxy = maxy

def project(wx, wy, n, ax, ay):
    lo = wx[0] * ax + wy[0] * ay
    hi = lo
    i = 1
    while i < n:
        d = wx[i] * ax + wy[i] * ay
        if d < lo:
            lo = d
        if d > hi:
            hi = d
        i = i + 1
    return [lo, hi]

def sat_overlap(a, b):
    # Separating Axis Theorem over both polygons' edge normals.
    na = len(a.wx)
    nb = len(b.wx)
    i = 0
    while i < na:
        j = i + 1
        if j == na:
            j = 0
        ex = a.wx[j] - a.wx[i]
        ey = a.wy[j] - a.wy[i]
        ax = -ey
        ay = ex
        pa = project(a.wx, a.wy, na, ax, ay)
        pb = project(b.wx, b.wy, nb, ax, ay)
        if pa[1] < pb[0] or pb[1] < pa[0]:
            return False
        i = i + 1
    i = 0
    while i < nb:
        j = i + 1
        if j == nb:
            j = 0
        ex = b.wx[j] - b.wx[i]
        ey = b.wy[j] - b.wy[i]
        ax = -ey
        ay = ex
        pa = project(a.wx, a.wy, na, ax, ay)
        pb = project(b.wx, b.wy, nb, ax, ay)
        if pa[1] < pb[0] or pb[1] < pa[0]:
            return False
        i = i + 1
    return True

def run_physics():
    shapes = [make_poly(3, 12.0), make_poly(4, 14.0), make_poly(5, 11.0),
              make_poly(6, 13.0), make_poly(8, 10.0)]
    bodies = []
    i = 0
    while i < NBODIES:
        px = float(rnd() % WORLD)
        py = float(rnd() % WORLD)
        ang = float(rnd() % 628) / 100.0
        vx = float(rnd() % 200 - 100) / 2.0
        vy = float(rnd() % 200 - 100) / 2.0
        spin = float(rnd() % 200 - 100) / 100.0
        bodies.append(Body(px, py, ang, vx, vy, spin, shapes[rnd() % 5]))
        i = i + 1

    # Sweep and prune on x: sort once per frame, then only test overlapping
    # AABB spans, which is what a real broadphase does.
    hits = 0
    frame = 0
    dt = 0.016
    while frame < NFRAMES:
        i = 0
        while i < NBODIES:
            b = bodies[i]
            b.integrate(dt)
            b.transform()
            i = i + 1

        order = sorted(bodies, key=lambda b: b.minx)

        i = 0
        while i < NBODIES:
            a = order[i]
            amaxx = a.maxx
            j = i + 1
            while j < NBODIES:
                b = order[j]
                if b.minx > amaxx:
                    j = NBODIES
                else:
                    if a.miny <= b.maxy and b.miny <= a.maxy:
                        if sat_overlap(a, b):
                            hits = hits + 1
                    j = j + 1
            i = i + 1
        frame = frame + 1
    return hits

# ---------------------------------------------------------------
# 2. ECS-style sprite update: components in parallel arrays,
#    systems iterating them, entities added and removed
# ---------------------------------------------------------------

class World:
    def __init__(self):
        self.px = []
        self.py = []
        self.vx = []
        self.vy = []
        self.life = []
        self.frame = []
        self.alive = []
        self.count = 0

    def spawn(self, px, py, vx, vy, life):
        self.px.append(px)
        self.py.append(py)
        self.vx.append(vx)
        self.vy.append(vy)
        self.life.append(life)
        self.frame.append(0)
        self.alive.append(1)
        self.count = self.count + 1

    def system_move(self, dt):
        i = 0
        n = self.count
        while i < n:
            if self.alive[i] == 1:
                self.px[i] = self.px[i] + self.vx[i] * dt
                self.py[i] = self.py[i] + self.vy[i] * dt
                if self.px[i] < 0.0 or self.px[i] > WORLD:
                    self.vx[i] = -self.vx[i]
                if self.py[i] < 0.0 or self.py[i] > WORLD:
                    self.vy[i] = -self.vy[i]
            i = i + 1

    def system_animate(self):
        i = 0
        n = self.count
        acc = 0
        while i < n:
            if self.alive[i] == 1:
                f = self.frame[i] + 1
                if f >= 8:
                    f = 0
                self.frame[i] = f
                acc = acc + f
            i = i + 1
        return acc

    def system_lifetime(self):
        i = 0
        n = self.count
        killed = 0
        while i < n:
            if self.alive[i] == 1:
                l = self.life[i] - 1
                self.life[i] = l
                if l <= 0:
                    self.alive[i] = 0
                    killed = killed + 1
            i = i + 1
        return killed

def run_ecs():
    w = World()
    i = 0
    while i < NSPRITES:
        w.spawn(float(rnd() % WORLD), float(rnd() % WORLD),
                float(rnd() % 100 - 50), float(rnd() % 100 - 50),
                20 + rnd() % 40)
        i = i + 1

    total = 0
    frame = 0
    while frame < NFRAMES:
        w.system_move(0.016)
        total = total + w.system_animate()
        total = total + w.system_lifetime()
        # Respawn what died, so the arrays keep growing like a real pool
        if frame % 10 == 0:
            k = 0
            while k < 200:
                w.spawn(float(rnd() % WORLD), float(rnd() % WORLD),
                        float(rnd() % 100 - 50), float(rnd() % 100 - 50),
                        20 + rnd() % 40)
                k = k + 1
        frame = frame + 1
    return total

import time
t0 = time.perf_counter()
a = run_physics()
t1 = time.perf_counter()
b = run_ecs()
t2 = time.perf_counter()
print("physics checksum", a)
print("ecs checksum", b)
print("physics", t1 - t0)
print("ecs", t2 - t1)
