# Test async/await

async def add(a, b):
    return a + b

async def twice(n):
    x = await add(n, 1)
    return x * 2

r1 = await add(10, 20)
assert r1 == 30

r2 = await twice(5)
assert r2 == 12

# await pass-through for non-fiber values
v = await 42
assert v == 42

print("All async/await tests passed.")
