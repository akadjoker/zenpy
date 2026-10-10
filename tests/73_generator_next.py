# next(g) advances a generator one step. Generators are fibers here, so
# resuming one re-enters the interpreter — which a native cannot do, hence
# an opcode rather than a builtin function.

def gen():
    i = 0
    while i < 3:
        yield i
        i = i + 1

g = gen()
print(next(g), next(g), next(g))

# The point of next(): consuming an endless generator as a stream, which a
# for loop cannot do.
def fib():
    a = 0
    b = 1
    while True:
        yield a
        t = a + b
        a = b
        b = t

f = fib()
out = []
i = 0
while i < 8:
    out.append(next(f))
    i = i + 1
print(out)

# A generator that yields nil must be distinguishable from exhaustion, so
# running out raises rather than returning nil.
def maybe():
    yield None
    yield 1

m = maybe()
print(next(m), next(m))

# The loop forms still work and are unaffected.
def two():
    yield 10
    yield 20

for v in two():
    print(v)
print([v for v in two()])
print(sum(v for v in two()))

# Independent generators do not share state.
g1 = gen()
g2 = gen()
print(next(g1), next(g1), next(g2))
