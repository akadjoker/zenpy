print([x * x for x in range(5)], [x for x in range(10) if x % 2], [(x, y) for x in range(2) for y in "ab"])
print({x: x * x for x in range(3)}, sorted({x % 3 for x in range(9)}), [[y for y in range(x)] for x in range(3)])
print([x for x in [1, 2, 3] if x > 1 if x < 3], [x if x > 1 else -x for x in range(3)], [x for row in [[1, 2], [3]] for x in row])
def gen(n):
    for i in range(n):
        yield i * 10
print(list(gen(3)), sum(gen(4)), [x for x in gen(2)], list(gen(0)), max(gen(5)))
def fib_gen():
    a, b = 0, 1
    while True:
        yield a
        a, b = b, a + b
g = fib_gen()
print([next(g) for _ in range(10)], next(g), next(g))
def echo():
    received = yield "ready"
    while received is not None:
        received = yield received * 2
e = echo(); print(next(e), e.send(5), e.send(21))
def countdown(n):
    while n > 0:
        yield n
        n -= 1
    return "done"
print(list(countdown(3)), " ".join(str(x) for x in countdown(3)), sum(x for x in countdown(4)))
gexp = (x * 2 for x in range(3)); print(list(gexp), list(gexp))
print(any(x > 5 for x in range(10)), all(x < 5 for x in range(10)), sum(1 for _ in "hello"), max(len(w) for w in ["a", "bbb", "cc"]))
def take(n, it):
    out = []
    for x in it:
        if len(out) >= n:
            break
        out.append(x)
    return out
print(take(3, fib_gen()), take(0, fib_gen()), take(2, gen(5)))
def chain(*its):
    for it in its:
        for x in it:
            yield x
print(list(chain([1, 2], "ab", range(2))), list(chain()))
def flatten(xs):
    for x in xs:
        if isinstance(x, list):
            yield from flatten(x)
        else:
            yield x
print(list(flatten([1, [2, [3, [4, 5]], 6], 7])))
def pairs(xs):
    for i in range(len(xs)):
        for j in range(i + 1, len(xs)):
            yield xs[i], xs[j]
print(list(pairs([1, 2, 3])), len(list(pairs(range(5)))))
nested = [[1, 2], [3, 4]]
print([x for row in nested for x in row], [[row[i] for row in nested] for i in range(2)], [sum(row) for row in nested])
words = ["apple", "bob", "cat"]
print({w: len(w) for w in words}, [w.upper() for w in words if len(w) > 3], sorted(words, key=lambda w: w[-1]))
matrix = [[i * j for j in range(3)] for i in range(3)]
print(matrix, [matrix[i][i] for i in range(3)], list(zip(*matrix)))
print([i for i in range(3)] == list(range(3)), type(x * 2 for x in range(1)).__name__)
squares = {}
for n in range(5): squares[n] = n * n
print(squares == {n: n * n for n in range(5)}, list(squares), sum(squares.values()))
def evens():
    n = 0
    while True:
        yield n
        n += 2
ev = evens(); print([next(ev) for _ in range(4)])
print(list(x for x in [3, 1, 2] if x != 1), [str(i) for i in range(3)], "".join(chr(97 + i) for i in range(5)))
