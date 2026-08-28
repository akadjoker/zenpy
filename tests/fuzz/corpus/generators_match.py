def gen():
    yield 1
    yield 2
    yield 3

for v in gen():
    print(v)

x, y = 10, 20
a, _ = 30, 40
print(x + y + a)

match 42:
    case 1:
        print("one")
    case 42:
        print("forty-two")
    case _:
        print("other")
