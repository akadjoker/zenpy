# evaluation order, aliasing, truthiness, identity, scoping corners
log = []
def t(x):
    log.append(x)
    return x
r = t(1) + t(2) * t(3); print(r, log)
log = []
_ = [t(1), t(2)][t(0)]; print(log)
log = []
d = {t("k"): t("v")}; print(log)
log = []
_ = t(1) < t(2) < t(0); print(log)
log = []
_ = t(0) and t(1); _ = t(1) or t(2); print(log)
a = [1, 2, 3]; b = a; a += [4]; print(a, b, a is b)
c = (1, 2); dd = c; c += (3,); print(c, dd, c is dd)
s = "ab"; s2 = s; s += "c"; print(s, s2)
x = y = [0]; x.append(1); print(x, y)
i = j = 5; i += 1; print(i, j)
def f(lst=[]):
    lst.append(1)
    return lst
print(f(), f(), f([]))
print(1 is 1, "a" is "a", None is None, [] is [], () == (), [1] == [1], [1] is [1])
print(0 == 0.0, 0 == "", "1" == 1, None == 0, None == None, True == 1, True is 1)
print(bool([]), bool([[]]), bool({}), bool(""), bool(" "), bool(0), bool(0.0), bool(-1), bool(None), bool(range(0)))
print(1 or 2, 0 or 2, 1 and 2, 0 and 2, None or "d", "" or None, [] or {}, 0 or 0.0)
print(not 1, not 0, not not 5, not [], not [0])
x = 10
def scope1():
    return x
def scope2():
    x = 20
    return x
print(scope1(), scope2(), x)
def scope3():
    global x
    x += 1
    return x
print(scope3(), x)
def outer():
    v = 1
    def inner():
        return v + 1
    v = 5
    return inner()
print(outer())
def loop_var():
    for i in range(3):
        pass
    return i
print(loop_var())
lst = [1, 2, 3]
for v in lst:
    v *= 2
print(lst)
for idx in range(len(lst)):
    lst[idx] *= 2
print(lst)
m = [[0] * 2] * 2; m[0][0] = 1; print(m)
n = [[0] * 2 for _ in range(2)]; n[0][0] = 1; print(n)
print(2 ** -2, 10 // 3 * 3 + 10 % 3, -(5 // 2), (-5) // 2, 5 // -2, -5 % 2, 5 % -2, 5.0 // 2, -5.0 % 2)
print(1 / 2 + 1 / 2 == 1, 3 * 0.1 == 0.3, abs(3 * 0.1 - 0.3) < 1e-9, 1e308 * 10, -1e308 * 10)
print("a" * 3 + "b", "-" * 0, "x" + str(1), str(1) + str(2), "%d" % 3 + "!", 3 * "ab")
def counter_factory():
    total = [0]
    def add(n):
        total[0] += n
        return total[0]
    return add
add = counter_factory(); add(5); print(add(10))
print(type(1 + 1.0).__name__, type(2 * 3).__name__, type(4 / 2).__name__, type(4 // 2).__name__, type(2 ** 2).__name__, type(2 ** -1).__name__, type(True + 1).__name__)
print(1 + True, "a" < "b" < "c", (1, 2) < (1, 3), (1, 2) < (1, 2, 0), [1, 2] == (1, 2), (1,) == (1,))
z = None
print(z is None, z == None, z is not None, z != None, type(z).__name__, str(z), z or "default")
big = 10 ** 18; print(big + 1, big * 10, -big, big // 3, big % 7, (big + 1) - big)
print(0.1 + 0.7, 0.3 - 0.1, 1.1 * 1.1, 1 / 3 * 3, 2.0 ** 0.5, 10.0 / 3.0, 7 / 7, 6 / 4, 2 ** 31)
ab = "x"
ab += "y"; ab *= 2; print(ab)
lst2 = [1]; lst2 *= 3; lst2 += [2]; print(lst2)
tup = (1,); tup *= 2; tup += (3,); print(tup)
print(sorted([True, False, 2, 1.5, -1]), [1, "a"] * 2, 3 in [1, 3.0], "3" in ["3"], 3.0 in {3: 1})
