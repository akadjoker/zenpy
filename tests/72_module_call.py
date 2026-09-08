# Module function dispatch: `math.sqrt(x)` re-interned the method name on
# every call — strlen, hash, and a lookup in the intern table — when the
# compiler had already interned it and stored the hash in the selector slot.
# It now reuses that string. This checks the dispatch still resolves.

import math
print(math.floor(2.7))
print(math.sqrt(16.0))
print(math.pow(2.0, 8.0))
print(math.max(3, 7))
print(math.min(3, 7))
print(math.abs(-5))

import json
print(json.stringify([1, 2]))
print(json.parse("[3,4]"))

# The same name on a plain dict must still be a key miss, not a module hit.
d = {"sqrt": 1}
print(d["sqrt"])
print(len(d))

# A method call on a dict that is not a module still reaches map methods.
d2 = {"a": 1}
d2.set("b", 2)
print(d2.get("b"))
print(d2.size())

# Repeated calls in a loop — the path the fix is about.
n = 0.0
i = 0
while i < 1000:
    n = n + math.sqrt(i)
    i = i + 1
print(int(n))
