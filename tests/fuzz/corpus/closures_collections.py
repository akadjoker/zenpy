def make_counter():
    n = 0
    def inc():
        nonlocal n
        n = n + 1
        return n
    return inc

c = make_counter()
print(c())
print(c())
a = [1,2,3]
a.push(4)
for x in a:
    print(x)
m = {"a": 1, "b": 2}
for k in m:
    print(k)
