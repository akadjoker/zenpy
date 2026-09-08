# Quadtree (2D) and octree (3D): insert random points, then range queries.
# Same file runs under CPython and Zen.
import time

NPOINTS = 20000
NQUERIES = 2000
SIZE = 1024
CAPACITY = 8
MAX_DEPTH = 8

seed = 42
def rnd():
    global seed
    seed = (seed * 16807) % 2147483647
    return seed

class Quad:
    def __init__(self, x, y, w, depth):
        self.x = x
        self.y = y
        self.w = w
        self.depth = depth
        self.px = []
        self.py = []
        self.count = 0
        self.split = False
        self.c0 = None
        self.c1 = None
        self.c2 = None
        self.c3 = None
    def child_for(self, x, y):
        h = self.w // 2
        if y < self.y + h:
            if x < self.x + h:
                return self.c0
            return self.c1
        if x < self.x + h:
            return self.c2
        return self.c3
    def insert(self, x, y):
        node = self
        while node.split:
            node = node.child_for(x, y)
        node.px.append(x)
        node.py.append(y)
        node.count += 1
        if node.count > CAPACITY and node.depth < MAX_DEPTH:
            node.subdivide()
    def subdivide(self):
        h = self.w // 2
        d = self.depth + 1
        self.c0 = Quad(self.x, self.y, h, d)
        self.c1 = Quad(self.x + h, self.y, h, d)
        self.c2 = Quad(self.x, self.y + h, h, d)
        self.c3 = Quad(self.x + h, self.y + h, h, d)
        self.split = True
        px = self.px
        py = self.py
        for i in range(self.count):
            self.child_for(px[i], py[i]).insert(px[i], py[i])
        self.px = []
        self.py = []
        self.count = 0
    def query(self, x0, y0, x1, y1):
        # count points with x0 <= x < x1, y0 <= y < y1
        if x1 <= self.x or y1 <= self.y or x0 >= self.x + self.w or y0 >= self.y + self.w:
            return 0
        if self.split:
            n = self.c0.query(x0, y0, x1, y1) + self.c1.query(x0, y0, x1, y1)
            n += self.c2.query(x0, y0, x1, y1) + self.c3.query(x0, y0, x1, y1)
            return n
        n = 0
        px = self.px
        py = self.py
        for i in range(self.count):
            if px[i] >= x0 and px[i] < x1 and py[i] >= y0 and py[i] < y1:
                n += 1
        return n

class Oct:
    def __init__(self, x, y, z, w, depth):
        self.x = x
        self.y = y
        self.z = z
        self.w = w
        self.depth = depth
        self.px = []
        self.py = []
        self.pz = []
        self.count = 0
        self.split = False
        self.kids = None
    def child_for(self, x, y, z):
        h = self.w // 2
        i = 0
        if x >= self.x + h:
            i += 1
        if y >= self.y + h:
            i += 2
        if z >= self.z + h:
            i += 4
        return self.kids[i]
    def insert(self, x, y, z):
        node = self
        while node.split:
            node = node.child_for(x, y, z)
        node.px.append(x)
        node.py.append(y)
        node.pz.append(z)
        node.count += 1
        if node.count > CAPACITY and node.depth < MAX_DEPTH:
            node.subdivide()
    def subdivide(self):
        h = self.w // 2
        d = self.depth + 1
        kids = []
        for i in range(8):
            ox = self.x
            oy = self.y
            oz = self.z
            if i % 2 == 1:
                ox += h
            if (i // 2) % 2 == 1:
                oy += h
            if i >= 4:
                oz += h
            kids.append(Oct(ox, oy, oz, h, d))
        self.kids = kids
        self.split = True
        px = self.px
        py = self.py
        pz = self.pz
        for i in range(self.count):
            self.child_for(px[i], py[i], pz[i]).insert(px[i], py[i], pz[i])
        self.px = []
        self.py = []
        self.pz = []
        self.count = 0
    def query(self, x0, y0, z0, x1, y1, z1):
        if x1 <= self.x or y1 <= self.y or z1 <= self.z:
            return 0
        if x0 >= self.x + self.w or y0 >= self.y + self.w or z0 >= self.z + self.w:
            return 0
        if self.split:
            n = 0
            kids = self.kids
            for i in range(8):
                n += kids[i].query(x0, y0, z0, x1, y1, z1)
            return n
        n = 0
        px = self.px
        py = self.py
        pz = self.pz
        for i in range(self.count):
            if px[i] >= x0 and px[i] < x1 and py[i] >= y0 and py[i] < y1 and pz[i] >= z0 and pz[i] < z1:
                n += 1
        return n

def run_quad():
    global seed
    seed = 42
    q = Quad(0, 0, SIZE, 0)
    for i in range(NPOINTS):
        q.insert(rnd() % SIZE, rnd() % SIZE)
    total = 0
    for i in range(NQUERIES):
        x = rnd() % SIZE
        y = rnd() % SIZE
        w = 32 + rnd() % 64
        total += q.query(x, y, x + w, y + w)
    return total

def run_oct():
    global seed
    seed = 42
    o = Oct(0, 0, 0, SIZE, 0)
    for i in range(NPOINTS):
        o.insert(rnd() % SIZE, rnd() % SIZE, rnd() % SIZE)
    total = 0
    for i in range(NQUERIES):
        x = rnd() % SIZE
        y = rnd() % SIZE
        z = rnd() % SIZE
        w = 64 + rnd() % 128
        total += o.query(x, y, z, x + w, y + w, z + w)
    return total

t0 = time.perf_counter()
a = run_quad()
t1 = time.perf_counter()
b = run_oct()
t2 = time.perf_counter()
print("quadtree checksum", a)
print("octree checksum", b)
print("quadtree", t1 - t0)
print("octree", t2 - t1)
