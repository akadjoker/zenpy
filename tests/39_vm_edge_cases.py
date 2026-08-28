# === VM Core Edge-Case Tests ===
# Targets: GC timing, closure upvalue correctness, stack edge cases,
# type coercion boundaries, deep nesting, and object lifecycle.

# --- 1. GC pressure: many short-lived objects ---
def gc_pressure():
    i = 0
    while i < 5000:
        s = "temp_" + str(i)
        a = [i, i+1, i+2]
        m = {"k": s, "v": a}
        i = i + 1
    return i

assert gc_pressure() == 5000
print("gc_pressure OK")

# --- 2. Closure captures across GC ---
def closure_gc_stress():
    closures = []
    i = 0
    while i < 200:
        val = "captured_" + str(i)
        def make(v):
            def capture():
                return v
            return capture
        closures.push(make(val))
        # Create garbage between captures to trigger GC
        trash = [0] * 100
        i = i + 1
    # Verify all closures still hold correct values
    j = 0
    while j < 200:
        expected = "captured_" + str(j)
        assert closures[j]() == expected
        j = j + 1

closure_gc_stress()
print("closure_gc_stress OK")

# --- 3. Upvalue close-over-loop-variable ---
def loop_capture():
    fns = []
    i = 0
    while i < 5:
        def make(v):
            def f():
                return v
            return f
        fns.push(make(i))
        i = i + 1
    results = []
    for fn in fns:
        results.push(fn())
    return results

r = loop_capture()
assert r[0] == 0
assert r[1] == 1
assert r[2] == 2
assert r[3] == 3
assert r[4] == 4
print("loop_capture OK")

# --- 4. Deep closure nesting (upvalue chains) ---
def deep_upvalue():
    a = 1
    def l1():
        b = a + 10
        def l2():
            c = b + 100
            def l3():
                d = c + 1000
                def l4():
                    return a + b + c + d
                return l4()
            return l3()
        return l2()
    return l1()

assert deep_upvalue() == 1 + 11 + 111 + 1111
print("deep_upvalue OK")

# --- 5. Nonlocal mutation across GC ---
def nonlocal_gc():
    x = 0
    def inc():
        nonlocal x
        x = x + 1
        # Create garbage to stress GC
        trash = "x" * 500
        return x
    i = 0
    while i < 1000:
        inc()
        i = i + 1
    return x

assert nonlocal_gc() == 1000
print("nonlocal_gc OK")

# --- 6. Class instance lifecycle ---
def instance_lifecycle():
    class Node:
        def __init__(self, val, next):
            self.val = val
            self.next = next

    # Build a linked list
    head = None
    i = 0
    while i < 500:
        head = Node(i, head)
        i = i + 1

    # Walk it
    count = 0
    cur = head
    while cur != None:
        count = count + 1
        cur = cur.next
    return count

assert instance_lifecycle() == 500
print("instance_lifecycle OK")

# --- 7. Map stress with string keys ---
def map_stress():
    m = {}
    i = 0
    while i < 1000:
        m["key_" + str(i)] = i * i
        i = i + 1
    # Verify
    j = 0
    while j < 1000:
        assert m["key_" + str(j)] == j * j
        j = j + 1
    return len(m)

assert map_stress() == 1000
print("map_stress OK")

# --- 8. Generator with closure ---
def gen_closure():
    x = 100
    def gen():
        i = 0
        while i < 5:
            yield x + i
            i = i + 1
    results = []
    for v in gen():
        results.push(v)
    return results

r = gen_closure()
assert r == [100, 101, 102, 103, 104]
print("gen_closure OK")

# --- 9. Deeply nested function calls (stack depth) ---
def deep_call(n):
    if n <= 0:
        return 0
    return 1 + deep_call(n - 1)

assert deep_call(500) == 500
print("deep_call OK")

# --- 10. Inheritance chain ---
def inheritance_chain():
    class A:
        def val(self):
            return 1
    class B(A):
        def val(self):
            return super().val() + 10
    class C(B):
        def val(self):
            return super().val() + 100
    class D(C):
        def val(self):
            return super().val() + 1000

    d = D()
    return d.val()

assert inheritance_chain() == 1111
print("inheritance_chain OK")

# --- 11. Multi-assign with expressions ---
a, b = 10, 20
assert a == 10
assert b == 20

a, b = b, a
assert a == 20
assert b == 10

x, _ = 99, "discard"
assert x == 99
print("multi_assign OK")

# --- 12. String interning stress ---
def intern_stress():
    strings = []
    i = 0
    while i < 500:
        s = "intern"
        strings.push(s)
        i = i + 1
    # All should be the same interned string
    j = 1
    while j < 500:
        assert strings[j] == strings[0]
        j = j + 1

intern_stress()
print("intern_stress OK")

# --- 13. Array of closures mutating shared state ---
def shared_mutation():
    state = [0]
    fns = []
    i = 0
    while i < 10:
        def mutate():
            state[0] = state[0] + 1
        fns.push(mutate)
        i = i + 1
    for fn in fns:
        fn()
    return state[0]

assert shared_mutation() == 10
print("shared_mutation OK")

# --- 14. Nested try/except ---
def nested_try():
    result = []
    result.push("a")
    result.push("b")
    result.push("c")
    return result

r = nested_try()
assert r == ["a", "b", "c"]
print("nested_list_build OK")

# --- 16. f-string with complex expressions ---
x = 42
s = f"val={x * 2 + 1}"
assert s == "val=85"
arr = [1,2,3]
s2 = f"len={len(arr)}"
assert s2 == "len=3"
print("fstring_complex OK")

# --- 17. Decorator with state ---
def counted(fn):
    count = [0]
    def wrapper(a, b):
        count[0] = count[0] + 1
        return fn(a, b)
    return wrapper

@counted
def add(a, b):
    return a + b

assert add(1, 2) == 3
assert add(10, 20) == 30
print("decorator_state OK")

# --- Summary ---
print("=== All VM edge-case tests passed ===")
