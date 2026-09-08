# Grid pathfinding: A* and Dijkstra on a W x H grid with random walls.
# Same file runs under CPython and Zen (subset: no tuple-swap on subscripts,
# no abs/min/max builtins, lists built by comprehension).
import time

W = 128
H = 128
N = W * H
BIG = 1000000000
QUERIES = 40
WALL_PCT = 30

seed = 42
def rnd():
    global seed
    seed = (seed * 16807) % 2147483647
    return seed

def iabs(x):
    if x < 0:
        return -x
    return x

class Heap:
    # binary min-heap of (key, value) stored flat: keys[i], vals[i]
    def __init__(self):
        self.keys = []
        self.vals = []
        self.n = 0
    def push(self, key, val):
        keys = self.keys
        vals = self.vals
        i = self.n
        self.n = i + 1
        keys.append(key)
        vals.append(val)
        while i > 0:
            p = (i - 1) // 2
            if keys[p] <= key:
                break
            keys[i] = keys[p]
            vals[i] = vals[p]
            i = p
        keys[i] = key
        vals[i] = val
    def pop(self):
        # returns the value with the smallest key
        keys = self.keys
        vals = self.vals
        top = vals[0]
        last_key = keys.pop()
        last_val = vals.pop()
        n = self.n - 1
        self.n = n
        if n > 0:
            i = 0
            while True:
                l = 2 * i + 1
                if l >= n:
                    break
                r = l + 1
                m = l
                if r < n and keys[r] < keys[l]:
                    m = r
                if keys[m] >= last_key:
                    break
                keys[i] = keys[m]
                vals[i] = vals[m]
                i = m
            keys[i] = last_key
            vals[i] = last_val
        return top

def make_grid():
    blocked = [0 for i in range(N)]
    for i in range(N):
        if rnd() % 100 < WALL_PCT:
            blocked[i] = 1
    return blocked

def search(blocked, start, goal, use_heur):
    # returns path length in steps, or -1
    gx = goal % W
    gy = goal // W
    dist = [BIG for i in range(N)]
    closed = [0 for i in range(N)]
    heap = Heap()
    dist[start] = 0
    heap.push(0, start)
    expanded = 0
    while heap.n > 0:
        cur = heap.pop()
        if closed[cur]:
            continue
        closed[cur] = 1
        expanded += 1
        if cur == goal:
            return dist[cur]
        cx = cur % W
        cy = cur // W
        d = dist[cur] + 1
        # 4 neighbours, unrolled
        if cx > 0:
            nb = cur - 1
            if not blocked[nb] and d < dist[nb]:
                dist[nb] = d
                if use_heur:
                    heap.push(d + iabs(cx - 1 - gx) + iabs(cy - gy), nb)
                else:
                    heap.push(d, nb)
        if cx < W - 1:
            nb = cur + 1
            if not blocked[nb] and d < dist[nb]:
                dist[nb] = d
                if use_heur:
                    heap.push(d + iabs(cx + 1 - gx) + iabs(cy - gy), nb)
                else:
                    heap.push(d, nb)
        if cy > 0:
            nb = cur - W
            if not blocked[nb] and d < dist[nb]:
                dist[nb] = d
                if use_heur:
                    heap.push(d + iabs(cx - gx) + iabs(cy - 1 - gy), nb)
                else:
                    heap.push(d, nb)
        if cy < H - 1:
            nb = cur + W
            if not blocked[nb] and d < dist[nb]:
                dist[nb] = d
                if use_heur:
                    heap.push(d + iabs(cx - gx) + iabs(cy + 1 - gy), nb)
                else:
                    heap.push(d, nb)
    return -1

def run(use_heur):
    global seed
    seed = 42
    blocked = make_grid()
    total = 0
    found = 0
    for q in range(QUERIES):
        s = rnd() % N
        g = rnd() % N
        while blocked[s]:
            s = rnd() % N
        while blocked[g]:
            g = rnd() % N
        r = search(blocked, s, g, use_heur)
        if r >= 0:
            found += 1
            total += r
    return total * 1000 + found

t0 = time.perf_counter()
a = run(True)
t1 = time.perf_counter()
b = run(False)
t2 = time.perf_counter()
print("astar checksum", a)
print("dijkstra checksum", b)
print("astar", t1 - t0)
print("dijkstra", t2 - t1)
