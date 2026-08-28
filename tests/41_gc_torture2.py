# === GC Torture Tests - Level 2 ===
# These target the EXACT patterns that cause GC bugs:
# - Temporaries between sub-expressions (never rooted)
# - Nested allocating calls: foo(bar()) where bar() result is unrooted during foo()
# - Multiple allocations in single expression
# - String interning edge cases

# --- 1. Unrooted temporaries in expressions ---
def test_unrooted_temps():
    i = 0
    while i < 1000:
        # create_string result passed directly to array_push
        # The string from str(i) is unrooted when "key_" + str(i) allocates
        a = ["key_" + str(i), "val_" + str(i)]
        assert a[0] == "key_" + str(i)
        assert a[1] == "val_" + str(i)
        i = i + 1

test_unrooted_temps()
print("unrooted_temps OK")

# --- 2. Nested allocating calls ---
def make_list(a, b, c):
    return [a, b, c]

def wrap(x):
    return {"wrapped": x}

def test_nested_alloc_calls():
    i = 0
    while i < 500:
        # wrap() allocates a map; make_list() allocates an array
        # The array from make_list is unrooted during wrap()'s map allocation
        r = wrap(make_list(i, "s_" + str(i), [i]))
        assert r["wrapped"][0] == i
        assert r["wrapped"][1] == "s_" + str(i)
        assert r["wrapped"][2][0] == i
        i = i + 1

test_nested_alloc_calls()
print("nested_alloc_calls OK")

# --- 3. Chained method calls on temporaries ---
def test_chained_methods():
    i = 0
    while i < 500:
        # "hello world" → split → element → upper → find
        # Each method creates a new object; previous is unrooted
        r = "hello world foo bar".split(" ")
        u = r[1].upper()
        assert u == "WORLD"

        # Chain: string concat → split → join → split again
        s = ("a,b,c," + "d,e,f").split(",")
        assert len(s) == 6
        j = "-".join(s)
        assert j == "a-b-c-d-e-f"
        s2 = j.split("-")
        assert s2[0] == "a"
        assert s2[5] == "f"
        i = i + 1

test_chained_methods()
print("chained_methods OK")

# --- 4. Map literal with allocating values ---
def test_map_alloc_values():
    i = 0
    while i < 500:
        # Each value is an allocation; keys are also allocations
        # During map construction, earlier k/v pairs may be unrooted
        m = {
            "k1": [1, 2, 3],
            "k2": {"nested": True},
            "k3": "str_" + str(i),
            "k4": [i, i+1, i+2]
        }
        assert m["k1"][2] == 3
        assert m["k2"]["nested"] == True
        assert m["k3"] == "str_" + str(i)
        assert m["k4"][0] == i
        i = i + 1

test_map_alloc_values()
print("map_alloc_values OK")

# --- 5. Return value immediately used in allocation ---
def make_str(n):
    return "item_" + str(n) + "_data"

def test_return_into_alloc():
    i = 0
    while i < 500:
        # make_str returns a string, which is immediately used as
        # an element in array literal — string is unrooted during array alloc
        arr = [make_str(i), make_str(i+1), make_str(i+2)]
        assert arr[0] == "item_" + str(i) + "_data"
        assert arr[1] == "item_" + str(i+1) + "_data"
        assert arr[2] == "item_" + str(i+2) + "_data"
        i = i + 1

test_return_into_alloc()
print("return_into_alloc OK")

# --- 6. f-string with multiple allocating expressions ---
def test_fstring_stress():
    i = 0
    while i < 500:
        x = i * 3
        y = "hello_" + str(i)
        # f-string must concatenate multiple sub-strings
        s = f"x={x}, y={y}, i={i}"
        assert f"x={x}" in s
        assert f"i={i}" in s
        assert y in s
        i = i + 1

test_fstring_stress()
print("fstring_stress OK")

# --- 7. Multiple string concats in one expression ---
def test_multi_concat():
    i = 0
    while i < 500:
        # Each + creates a new temporary string
        # a + b = t1, t1 + c = t2, t2 + d = t3, etc.
        # Earlier temps are unrooted
        s = "a" + str(i) + "b" + str(i*2) + "c" + str(i*3) + "d" + str(i*4)
        assert s.startswith("a" + str(i))
        assert s.endswith("d" + str(i*4))
        i = i + 1

test_multi_concat()
print("multi_concat OK")

# --- 8. Array of arrays of maps (deep nesting, many allocs) ---
def test_deep_nesting_alloc():
    i = 0
    while i < 200:
        outer = []
        j = 0
        while j < 10:
            row = []
            k = 0
            while k < 5:
                row.push({"i": i, "j": j, "k": k, "s": str(i) + "_" + str(j) + "_" + str(k)})
                k = k + 1
            outer.push(row)
            j = j + 1
        # Verify deep access
        assert outer[3][2]["k"] == 2
        assert outer[9][4]["j"] == 9
        expected = str(i) + "_5_3"
        assert outer[5][3]["s"] == expected
        i = i + 1

test_deep_nesting_alloc()
print("deep_nesting_alloc OK")

# --- 9. Closure capturing allocating expression ---
def test_closure_capture_alloc():
    fns = []
    i = 0
    while i < 300:
        # The value captured is freshly allocated
        data = {"idx": i, "arr": [i, i+1], "name": "n_" + str(i)}
        def make(d):
            def get():
                return d
            return get
        fns.push(make(data))
        i = i + 1
    # Force GC by creating lots of garbage
    j = 0
    while j < 1000:
        trash = [j, j, j, j, j]
        j = j + 1
    # Now verify all closures
    k = 0
    while k < 300:
        d = fns[k]()
        assert d["idx"] == k
        assert d["arr"][0] == k
        assert d["name"] == "n_" + str(k)
        k = k + 1

test_closure_capture_alloc()
print("closure_capture_alloc OK")

# --- 10. Map keys() / values() while map grows ---
def test_map_keys_values_stress():
    i = 0
    while i < 200:
        m = {}
        j = 0
        while j < 50:
            m["k" + str(j)] = j
            j = j + 1
        # keys() allocates array + strings, while m is alive
        ks = m.keys()
        assert len(ks) == 50
        # values() same
        vs = m.values()
        assert len(vs) == 50
        # Verify a few
        found = False
        for k in ks:
            if k == "k25":
                found = True
        assert found
        i = i + 1

test_map_keys_values_stress()
print("map_keys_values OK")

# --- 11. String replace chains ---
def test_replace_chain():
    i = 0
    while i < 500:
        s = "the quick brown fox jumps over the lazy dog"
        s = s.replace("the", str(i))
        s = s.replace("fox", "cat_" + str(i))
        s = s.replace("dog", "bird_" + str(i))
        s = s.replace("lazy", "fast")
        assert ("cat_" + str(i)) in s
        assert ("bird_" + str(i)) in s
        assert "fast" in s
        i = i + 1

test_replace_chain()
print("replace_chain OK")

# --- 12. Recursive function returning freshly allocated data ---
def build_chain(n):
    if n == 0:
        return {"val": 0, "next": None}
    return {"val": n, "next": build_chain(n - 1)}

def chain_sum(node):
    s = 0
    while node != None:
        s = s + node["val"]
        node = node["next"]
    return s

def test_recursive_alloc():
    i = 0
    while i < 100:
        chain = build_chain(20)
        assert chain_sum(chain) == 210  # sum(0..20)
        assert chain["val"] == 20
        assert chain["next"]["val"] == 19
        i = i + 1

test_recursive_alloc()
print("recursive_alloc OK")

# --- 13. Interleaved generators and allocations ---
def pairs(n):
    i = 0
    while i < n:
        yield [i, "v_" + str(i)]
        i = i + 1

def test_generator_alloc():
    i = 0
    while i < 200:
        result = []
        for p in pairs(20):
            result.push({"key": p[0], "val": p[1]})
        assert len(result) == 20
        assert result[0]["key"] == 0
        assert result[0]["val"] == "v_0"
        assert result[19]["val"] == "v_19"
        i = i + 1

test_generator_alloc()
print("generator_alloc OK")

# --- 14. eval() under GC pressure ---
def test_eval_pressure():
    i = 0
    while i < 200:
        r = eval("1 + 2 + 3")
        assert r == 6
        r2 = eval("[1, 2, 3]")
        assert r2 == [1, 2, 3]
        # Create garbage between evals
        trash = {"a": [1,2,3], "b": "x" * 100}
        i = i + 1

test_eval_pressure()
print("eval_pressure OK")

# --- 15. Class __init__ allocates complex structures ---
class Container:
    def __init__(self, n):
        self.items = []
        self.index = {}
        i = 0
        while i < n:
            name = "item_" + str(i)
            self.items.push(name)
            self.index[name] = i
            i = i + 1

def test_class_init_alloc():
    i = 0
    while i < 200:
        c = Container(20)
        assert len(c.items) == 20
        assert c.items[0] == "item_0"
        assert c.index["item_19"] == 19
        i = i + 1

test_class_init_alloc()
print("class_init_alloc OK")

# --- 16. Multiple return values from expressions ---
def make_pair():
    return ["key_" + str(42), {"v": [1,2,3]}]

def test_multi_return():
    i = 0
    while i < 500:
        p = make_pair()
        assert p[0] == "key_42"
        assert p[1]["v"][2] == 3
        # Immediately use return in another alloc
        m = {"pair": make_pair(), "idx": i}
        assert m["pair"][0] == "key_42"
        i = i + 1

test_multi_return()
print("multi_return OK")

# --- 17. super().__init__ chains with allocations ---
class Base:
    def __init__(self):
        self.base_data = [1, 2, 3]
        self.base_name = "base"

class Mid(Base):
    def __init__(self, x):
        super().__init__()
        self.mid_arr = [x, x+1]
        self.mid_map = {"x": x}

class Top(Mid):
    def __init__(self, x, y):
        super().__init__(x)
        self.top_str = "top_" + str(y)
        self.top_list = [self.base_data, self.mid_arr]

def test_super_chain_alloc():
    i = 0
    while i < 300:
        t = Top(i, i * 2)
        assert t.base_data == [1, 2, 3]
        assert t.mid_arr[0] == i
        assert t.mid_map["x"] == i
        assert t.top_str == "top_" + str(i * 2)
        assert t.top_list[0] == [1, 2, 3]
        i = i + 1

test_super_chain_alloc()
print("super_chain_alloc OK")

# --- 18. Many small strings interned/deduped ---
def test_string_interning():
    i = 0
    while i < 500:
        # Same string created different ways — should be equal
        a = "hello"
        b = "hel" + "lo"
        c = "hello world".split(" ")[0]
        assert a == b
        assert a == c
        assert b == c
        # Different strings
        d = "hello" + str(i)
        e = "hello" + str(i)
        assert d == e
        i = i + 1

test_string_interning()
print("string_interning OK")

# --- 19. Array as function argument (not rooted in caller during call) ---
def process(arr, idx):
    # This allocates while arr is only rooted in caller's register
    result = {"data": arr, "idx": idx, "name": "p_" + str(idx)}
    return result

def test_array_arg_rooting():
    i = 0
    while i < 500:
        r = process([i, i+1, i+2], i)
        assert r["data"][0] == i
        assert r["data"][2] == i + 2
        assert r["name"] == "p_" + str(i)
        i = i + 1

test_array_arg_rooting()
print("array_arg_rooting OK")

# --- 20. Stress: all patterns mixed ---
def test_everything_mixed():
    results = []
    i = 0
    while i < 200:
        # Nested alloc in expression
        entry = {
            "str": "item_" + str(i) + "_" + str(i*2),
            "arr": [i, make_list(i, i+1, i+2), {"inner": True}],
            "split": ("a,b,c," + str(i)).split(","),
            "upper": ("hello_" + str(i)).upper()
        }
        results.push(entry)

        # Interleave with closure creation
        val = i
        def make(v):
            def get():
                return v
            return get
        fn = make(entry)

        # Create garbage
        ti = 0
        while ti < 50:
            trash = [ti, ti, ti]
            ti = ti + 1

        # Verify closure still holds entry
        e = fn()
        assert e["str"] == "item_" + str(i) + "_" + str(i*2)
        assert e["arr"][0] == i
        assert e["arr"][1] == [i, i+1, i+2]
        assert e["arr"][2]["inner"] == True

        i = i + 1

    # Final verification
    assert len(results) == 200
    assert results[0]["arr"][0] == 0
    assert results[199]["upper"] == "HELLO_199"

test_everything_mixed()
print("everything_mixed OK")

print("=== ALL LEVEL-2 TORTURE TESTS PASSED ===")
