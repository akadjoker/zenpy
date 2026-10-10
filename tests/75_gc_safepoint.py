# Regression: the collector only ran inside buffer allocation (zen_alloc /
# zen_realloc). A script that only creates instances or strings never
# triggered it: 5 million small instances in a loop grew to 520 MB. The VM
# now also checks the GC threshold at safe points (loop back edges and
# function entry), where every live value sits in a register or a global.
#
# What must not break: values that are still referenced (held in a list, a
# global, a frame register, or an upvalue) must survive those collections.

class P:
    def __init__(self, x, y):
        self.x = x
        self.y = y

import time

LIMIT = 64 * 1024 * 1024

# Under a stress-GC build (collect at every back edge and call) a loop of
# millions of iterations would never finish. Probe the speed with a tiny loop
# over a live set big enough to make every forced collection cost something,
# and scale the iteration counts down there. The memory ceiling is only
# meaningful at full size, so it is checked only when the run is full size.
def probe():
    live = []
    for k in range(2000):
        live.append(P(k, k))
    t0 = time.perf_counter()
    k = 0
    while k < 300:
        P(k, k)
        k = k + 1
    return time.perf_counter() - t0

FULL = probe() < 0.05
SCALE = 1 if FULL else 200
RC = 3000 if FULL else 20  # recursion rounds

def check_mem():
    if FULL:
        assert mem_used() < LIMIT, mem_used()

# --- 1. 2M instances, nothing retained, no list growth (while loop) ---
def churn_instances(n):
    i = 0
    acc = 0
    while i < n:
        p = P(i, i + 1)
        acc = acc + p.x + p.y
        i = i + 1
    return acc

# sum over i of (2*i + 1) for i in 0..n-1 == n*n
N1 = 2000000 // SCALE
r = churn_instances(N1)
assert r == N1 * N1, r
check_mem()

# --- 2. Same through a counted for loop (FORLOOP back edge) ---
def churn_for(n):
    acc = 0
    for i in range(n):
        p = P(i, 2)
        acc = acc + p.x - p.y
    return acc

N2 = 1000000 // SCALE
r = churn_for(N2)
assert r == int(N2 * (N2 - 1) / 2) - 2 * N2, r
check_mem()

# --- 3. Strings only: every iteration builds a fresh heap string ---
def churn_strings(n):
    total = 0
    for i in range(n):
        s = "item_number_" + str(i) + "_with_a_long_suffix_to_avoid_inline_strings"
        total = total + len(s)
    return total

t = churn_strings(500000 // SCALE)
assert t > 0
check_mem()

# --- 4. Function entry: recursion that allocates but never loops ---
def rec(n):
    if n == 0:
        return 0
    p = P(n, n)
    return p.x + rec(n - 1) - p.y + 1

k = 0
acc = 0
while k < RC:
    acc = acc + rec(200)
    k = k + 1
assert acc == (RC) * 200, acc
check_mem()

# --- 5. Live data survives the collections happening around it ---
keep = []
j = 0
while j < 1000:
    keep.append(P(j, j * 2))
    j = j + 1

holder = P(7, P(8, "nested"))

def counter():
    c = P(0, 0)
    def inc():
        c.x = c.x + 1
        return c.x
    return inc

inc = counter()

churn_instances(1500000 // SCALE)

for q in range(1000):
    assert keep[q].x == q
    assert keep[q].y == q * 2
assert holder.x == 7
assert holder.y.x == 8
assert holder.y.y == "nested"

n_inc = 0
while n_inc < 300000 // SCALE:
    inc()
    n_inc = n_inc + 1
    if n_inc % 3 == 0:
        P(n_inc, n_inc)
assert inc() == 300000 // SCALE + 1
check_mem()

print("PASS")
