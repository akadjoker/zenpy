# Regression: allocations made inside a gc_pause were never collected.
#
# `a = a + [i]` builds a new array each iteration and drops the previous one,
# but the concat runs inside gc_pause/gc_resume, so the threshold check never
# saw an unpaused moment: nothing was ever freed. A 20k-element list peaked at
# 4.7 GB resident and spent most of its time in munmap handing pages back.
#
# Copying is inherent to `a = a + [x]` — CPython is quadratic here too — so
# this does not test speed. It tests that the garbage is bounded: the loop
# below allocates roughly 1.6 GB in total, and must not hold it.

a = []
i = 0
while i < 10000:
    a = a + [i]
    i = i + 1
print(len(a))
print(a[0])
print(a[9999])

# The same shape for strings, which has its own in-place path.
s = ""
i = 0
while i < 5000:
    s = s + "ab"
    i = i + 1
print(len(s))

# List repetition allocates the same way.
b = []
i = 0
while i < 2000:
    b = [0] * 50
    i = i + 1
print(len(b))
