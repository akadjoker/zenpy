# Global augmented string append — the OP_*GLOBAL_AUG pair keeps a global
# accumulator unshared so += appends in place. These are the aliasing cases
# that must NOT observe mutation.

s = ""
for i in range(1000):
    s += "ab"
assert len(s) == 2000

# 1. reading the global into another global freezes the alias
t = s
s += "XX"
assert len(s) == 2002
assert len(t) == 2000, "t must keep the pre-append value"

# 2. reading the global into a local, then appending via a function
def snapshot():
    return s
u = snapshot()
def grow():
    global s
    s += "YY"
grow()
assert len(s) == 2004
assert len(u) == 2002, "u must keep the value read before grow()"

# 3. self-append: rhs reads the global, so it must copy
g = "base"
for i in range(3):
    g += g
assert len(g) == 32
assert g[0:4] == "base"

# 4. append into a container does not tie the container to the global
acc = ""
box = []
for i in range(5):
    acc += "z"
    box.append(acc)
assert box[0] == "z"
assert box[1] == "zz"
assert box[4] == "zzzzz"
assert acc == "zzzzz"

# 5. numbers through the same pair
n = 0
for i in range(100):
    n += 3
assert n == 300
f = 0.5
f += 1
assert f == 1.5

# 6. interleaved reads keep old snapshots intact
snaps = []
w = ""
for i in range(4):
    w += "q"
    snaps.append(w)
assert snaps[0] == "q" and snaps[3] == "qqqq"

print("All global append tests passed.")
