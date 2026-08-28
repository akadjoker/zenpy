# Test async/await (extended)

# Basic async function
async def add(a, b):
    return a + b

r = await add(10, 20)
assert r == 30

# Async calling async
async def double(n):
    return n * 2

async def add_and_double(a, b):
    s = await add(a, b)
    return await double(s)

r = await add_and_double(3, 4)
assert r == 14

# Await on non-fiber passes through
v = await 42
assert v == 42

v = await "hello"
assert v == "hello"

# Async with conditionals
async def abs_val(n):
    if n < 0:
        return -n
    return n

r = await abs_val(-5)
assert r == 5
r = await abs_val(3)
assert r == 3

# Async in a loop
async def inc(n):
    return n + 1

total = 0
for i in range(5):
    total = await inc(total)

assert total == 5

# Multiple awaits in sequence
async def step(n):
    return n + 10

r = await step(0)
r = await step(r)
r = await step(r)
assert r == 30

# Async returning complex values
async def make_pair(a, b):
    return [a, b]

pair = await make_pair(1, 2)
assert pair[0] == 1
assert pair[1] == 2

# Nested async calls
async def fib_async(n):
    if n <= 1:
        return n
    a = await fib_async(n - 1)
    b = await fib_async(n - 2)
    return a + b

r = await fib_async(0)
assert r == 0
r = await fib_async(1)
assert r == 1
r = await fib_async(5)
assert r == 5
r = await fib_async(8)
assert r == 21

print("All async/await extended tests passed.")
