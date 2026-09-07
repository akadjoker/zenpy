for i in range(3):
    if i == 0:
        print("zero")
    elif i == 1:
        print("one")
    else:
        print("other", i)
n = 0
while n < 5:
    n += 1
    if n == 2:
        continue
    if n == 4:
        break
    print("n", n)
for i in range(3):
    pass
else:
    print("for-else ran")
for i in range(3):
    if i == 1:
        break
else:
    print("not printed")
k = 0
while k < 2:
    k += 1
else:
    print("while-else", k)
print(1 if True else 2, "a" if 0 else "b", (1 if False else 2) + 1, "x" if "" else "y" if None else "z")
a = 0 or "" or [] or None or "last"; b = 1 and 2 and 3; c = 1 and 0 and 3; d = None or 0
print(a, b, c, d, not 0, not "s", not [], not None, bool(0.0), bool("0"), bool([0]))
print(1 < 2 == 2 != 3 >= 3, 1 < 2 > 1, not 1 == 2, not (1 == 2), 1 == 1 == 1)
x = 5
print(x > 3 and x < 10, x > 3 and x < 4 or x == 5, (x > 3 and x < 4) or x == 5, not x > 3 or x == 5)
for i, ch in enumerate("ab"):
    print(i, ch, end="|")
print()
for a, b in [(1, 2), (3, 4)]:
    print(a + b, end=" ")
print()
for a, (b, c) in [(1, (2, 3))]:
    print(a, b, c)
total = 0
for i in range(1, 101):
    total += i
print(total)
i = 10
while True:
    i -= 3
    if i < 0:
        break
print(i)
def grade(s):
    if s >= 90: return "A"
    if s >= 80: return "B"
    return "C"
print(grade(95), grade(85), grade(10))
outer = 0
for i in range(3):
    for j in range(3):
        if j == 2:
            break
        outer += 1
print(outer)
match_val = 3
if match_val in (1, 2): print("small")
elif match_val in [3, 4]: print("medium")
else: print("large")
print([i for i in range(20) if i % 2 == 0 if i % 3 == 0], [(i, j) for i in range(2) for j in range(2) if i != j])
val = None
if val is None: print("none")
if val is not None: print("no")
if not val: print("falsy")
print(0 == False, 1 == True, [] == False, None == False, "" == False, 0.0 == False)
z = 3
z = z if z > 5 else -z
print(z, abs(z) if z < 0 else z)
print("yes" if [] == [] else "no", "t" if (1, 2) < (1, 3) else "f", "eq" if "a" * 2 == "aa" else "ne")
count = 0
for _ in range(5):
    count += 1
print(count)
i = 0
while i < 3: i += 1
print(i)
if 1: print("one-line if")
for c in "ab": print(c, end="")
print()
