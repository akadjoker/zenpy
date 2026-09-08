# `for i in range(...)` compiles to OP_FORPREP/OP_FORLOOP when `range` is the
# builtin. The schedule is fixed on entry, like iterating a range object.

def f():
    s = 0
    for i in range(10):
        if i == 3:
            continue
        if i == 8:
            break
        s += i
    assert s == 0 + 1 + 2 + 4 + 5 + 6 + 7

    out = []
    for i in range(5, 0, -2):
        out.append(i)
    assert out == [5, 3, 1]

    for i in range(0):
        assert False
    for i in range(3, 3):
        assert False
    for i in range(3, 1):
        assert False
    for i in range(1, 3, -1):
        assert False

    # rebinding the loop variable does not change the schedule
    seen = []
    for i in range(2, 9, 3):
        seen.append(i)
        i = 100
    assert seen == [2, 5, 8]

    neg = []
    for i in range(-3, -10, -4):
        neg.append(i)
    assert neg == [-3, -7]

    total = 0
    for a in range(3):
        for b in range(2):
            total += a * 10 + b
    assert total == 0 + 1 + 10 + 11 + 20 + 21

    # arguments are arbitrary expressions
    n = 4
    acc = []
    for k in range(n - 3, n * 2, n // 2):
        acc.append(k)
    assert acc == [1, 3, 5, 7]

    # a shadowing local keeps the generic path
    def shadow():
        range = [7, 8]
        got = []
        for x in range:
            got.append(x)
        return got
    assert shadow() == [7, 8]
    return True

assert f()

# module level (globals as arguments)
lim = 5
acc = []
for k in range(1, lim):
    acc.append(k)
assert acc == [1, 2, 3, 4]

big = 0
for k in range(0, 1000000, 250000):
    big += k
assert big == 0 + 250000 + 500000 + 750000

print("OK")
