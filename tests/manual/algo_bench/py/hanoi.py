# Towers of Hanoi (recursive, call-heavy) and flood fill (explicit stack on a
# grid with random walls). Same file runs under CPython and Zen.
import time

DISKS = 20
W = 256
H = 256
N = W * H
WALL_PCT = 35
SEEDS = 60

seed = 42
def rnd():
    global seed
    seed = (seed * 16807) % 2147483647
    return seed

class Pegs:
    def __init__(self):
        self.moves = 0
        self.a = 0
        self.b = 0
        self.c = 0
    def move(self, n, src, dst, tmp):
        if n == 1:
            self.moves += 1
            return
        self.move(n - 1, src, tmp, dst)
        self.moves += 1
        self.move(n - 1, tmp, dst, src)

def hanoi_plain(n, src, dst, tmp):
    if n == 1:
        return 1
    return hanoi_plain(n - 1, src, tmp, dst) + 1 + hanoi_plain(n - 1, tmp, dst, src)

def run_hanoi():
    p = Pegs()
    p.move(DISKS, 0, 2, 1)
    return p.moves + hanoi_plain(DISKS - 2, 0, 2, 1)

def make_grid():
    cells = [0 for i in range(N)]
    for i in range(N):
        if rnd() % 100 < WALL_PCT:
            cells[i] = 1
    return cells

def flood(cells, start, color):
    # 4-connected fill of the 0-region containing start; returns cells painted
    if cells[start] != 0:
        return 0
    stack = [start]
    cells[start] = color
    painted = 0
    while len(stack) > 0:
        cur = stack.pop()
        painted += 1
        x = cur % W
        if x > 0 and cells[cur - 1] == 0:
            cells[cur - 1] = color
            stack.append(cur - 1)
        if x < W - 1 and cells[cur + 1] == 0:
            cells[cur + 1] = color
            stack.append(cur + 1)
        if cur >= W and cells[cur - W] == 0:
            cells[cur - W] = color
            stack.append(cur - W)
        if cur < N - W and cells[cur + W] == 0:
            cells[cur + W] = color
            stack.append(cur + W)
    return painted

def run_flood():
    global seed
    seed = 42
    cells = make_grid()
    total = 0
    regions = 0
    for s in range(SEEDS):
        n = flood(cells, rnd() % N, 2 + s)
        if n > 0:
            regions += 1
            total += n
    return total * 100 + regions

t0 = time.perf_counter()
a = run_hanoi()
t1 = time.perf_counter()
b = run_flood()
t2 = time.perf_counter()
print("hanoi checksum", a)
print("floodfill checksum", b)
print("hanoi", t1 - t0)
print("floodfill", t2 - t1)
