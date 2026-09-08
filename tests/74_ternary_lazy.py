# `a if c else b` must not evaluate the branch it does not take. Python
# guarantees it and ordinary code leans on it — the whole point of writing
# `n // d if d else 0` is that the division never runs when d is zero.
#
# The syntax is what made this hard: the value comes before the condition,
# so the parser has already compiled `a` when it reaches the `if`.

def boom():
    print("BOOM")
    return 1

x = boom() if False else 2
print(x)

y = 2 if True else boom()
print(y)

# The cases people actually write.
d = 0
print(10 // d if d else 0)

empty = []
print(empty[0] if empty else "empty")

s = ""
print(s[0] if s else "empty")
print(len(s) if s else -1)

# Nesting associates to the right.
print(1 if True else 2 if True else 3)
print(1 if False else 2 if False else 3)

# The taken branch runs exactly once — re-parsing must not double a side
# effect, and neither must the condition.
c = 0
def inc():
    global c
    c = c + 1
    return c
print(inc() if True else 99, c)

n = 0
def side():
    global n
    n = n + 1
    return True
print("a" if side() else "b", n)

# Ordinary shapes that must keep working.
v = 5
print(v * 2 if v > 3 else v * 10)
print([1, 2][0] if [1, 2] else "e")
print((1 if True else 2) + (10 if False else 20))
print([1 if i % 2 == 0 else 0 for i in range(4)])

def f(k):
    return k if k > 0 else -k
print(f(3), f(-3))

m = {"a": 1}
print(m["a"] if "a" in m else 0)
