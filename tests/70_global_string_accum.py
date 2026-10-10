# Regression: `g = g + x` on a global built the whole string again on every
# append — 200k appends took 51 seconds, quadratic, while the identical
# `g += x` took 0.011. The compiler now emits the same non-marking AUG pair
# for both, so the accumulator stays unshared and ADD appends in place.
#
# What must not break is why the marking existed: any second reference to
# the string has to keep seeing the old value.

s = ""
i = 0
while i < 20000:
    s = s + "x"
    i = i + 1
print(len(s))

# A second name taken before the append must not see it.
a = "a"
b = a
a = a + "b"
print(a)
print(b)

# Same through a function that reads the global.
g = "x"
def peek():
    return g
before = peek()
g = g + "y"
print(g)
print(before)

# Same when the string is also held by a container.
lst = []
u = "p"
lst.append(u)
u = u + "q"
print(u)
print(lst[0])

# Self-reference on the right-hand side.
v = "m"
v = v + v
print(v)

# The rewrite must only fire for the same global, and must leave arithmetic
# and every other shape alone.
n = 1
n = n + 2
print(n)

p = 10
q = 3
p = q + 1
print(p)

f = 2.5
f = f * 2.0
print(f)
