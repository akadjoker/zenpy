# === GC Torture Tests ===
# Designed to expose GC safety bugs under stress GC mode.
# Each test creates heavy allocation pressure while using results.

# --- 1. String method chains under pressure ---
def test_string_methods():
    i = 0
    while i < 500:
        s = "  Hello, World!  "
        s = s.strip()
        s = s.lower()
        s = s.upper()
        s = s.replace("WORLD", "ZEN")
        s = s.replace("HELLO", "HI")
        assert s == "HI, ZEN!"
        parts = s.split(",")
        assert parts[0] == "HI"
        assert parts[1].strip() == "ZEN!"
        joined = "-".join(parts)
        assert joined == "HI- ZEN!"
        rev = s.reverse()
        assert rev == "!NEZ ,IH"
        assert s.startswith("HI")
        assert s.endswith("!")
        assert s.contains("ZEN")
        assert s.find("ZEN") == 4
        assert s.count("!") == 1
        padded = s.pad_left(15, ".")
        assert padded.len() == 15
        i = i + 1

test_string_methods()
print("string_methods OK")

# --- 2. Nested data structure construction ---
def test_nested_structures():
    i = 0
    while i < 300:
        outer = []
        j = 0
        while j < 10:
            inner = {"idx": j, "data": [j, j+1, j+2], "name": "item_" + str(j)}
            outer.push(inner)
            j = j + 1
        # Verify everything is intact
        assert len(outer) == 10
        assert outer[0]["idx"] == 0
        assert outer[9]["name"] == "item_9"
        assert outer[5]["data"][2] == 7
        i = i + 1

test_nested_structures()
print("nested_structures OK")

# --- 3. Class creation storm ---
class Node:
    def __init__(self, val, children):
        self.val = val
        self.children = children

    def sum(self):
        s = self.val
        for c in self.children:
            s = s + c.sum()
        return s

def test_class_trees():
    i = 0
    while i < 200:
        # Build a small tree
        leaves = []
        j = 0
        while j < 5:
            leaves.push(Node(j, []))
            j = j + 1
        mid = [Node(100, [leaves[0], leaves[1]]),
               Node(200, [leaves[2], leaves[3], leaves[4]])]
        root = Node(1000, mid)
        total = root.sum()
        # 1000 + (100 + 0 + 1) + (200 + 2 + 3 + 4) = 1310
        assert total == 1310
        i = i + 1

test_class_trees()
print("class_trees OK")

# --- 4. Closure factory with heavy allocation ---
def test_closure_factory():
    makers = []
    i = 0
    while i < 500:
        val = [i, "str_" + str(i), {"k": i}]
        def make(v):
            def getter():
                return v
            return getter
        makers.push(make(val))
        i = i + 1
    # Verify all closures retained correct values
    j = 0
    while j < 500:
        v = makers[j]()
        assert v[0] == j
        assert v[1] == "str_" + str(j)
        assert v[2]["k"] == j
        j = j + 1

test_closure_factory()
print("closure_factory OK")

# --- 5. Map grow/shrink/rehash stress ---
def test_map_stress():
    i = 0
    while i < 100:
        m = {}
        j = 0
        while j < 100:
            m["key_" + str(j)] = [j, j*2, "v_" + str(j)]
            j = j + 1
        # Verify
        assert len(m) == 100
        assert m["key_0"][0] == 0
        assert m["key_99"][2] == "v_99"
        # Overwrite all values (triggers rehash with same keys)
        j = 0
        while j < 100:
            m["key_" + str(j)] = j * 3
            j = j + 1
        assert m["key_50"] == 150
        i = i + 1

test_map_stress()
print("map_stress OK")

# --- 6. Array slice + reverse + concatenation ---
def test_array_ops():
    i = 0
    while i < 300:
        arr = []
        j = 0
        while j < 50:
            arr.push(j)
            j = j + 1
        sliced = arr.slice(10, 20)
        assert len(sliced) == 10
        assert sliced[0] == 10
        assert sliced[9] == 19
        # reverse is in-place
        copy = arr.slice(0, len(arr))
        copy.reverse()
        assert copy[0] == 49
        assert copy[49] == 0
        # Original untouched
        assert arr[0] == 0
        i = i + 1

test_array_ops()
print("array_ops OK")

# --- 7. String split edge cases under pressure ---
def test_split_edges():
    i = 0
    while i < 500:
        # Empty string split
        r = "".split(",")
        assert r == [""]
        # Single char splits
        r2 = "a,b,c".split(",")
        assert r2 == ["a", "b", "c"]
        # Adjacent separators
        r3 = ",,a,,".split(",")
        assert r3 == ["", "", "a", "", ""]
        # No separator found
        r4 = "hello".split(",")
        assert r4 == ["hello"]
        # Whitespace split
        r5 = "  one  two   three  ".split()
        assert r5 == ["one", "two", "three"]
        # Empty string per-char split
        r6 = "abc".split("")
        assert r6 == ["a", "b", "c"]
        i = i + 1

test_split_edges()
print("split_edges OK")

# --- 8. Recursive data building ---
def build_nested(depth):
    if depth == 0:
        return {"leaf": True, "data": "leaf_data_" + str(depth)}
    left = build_nested(depth - 1)
    right = build_nested(depth - 1)
    return {"leaf": False, "left": left, "right": right, "depth": depth}

def count_leaves(node):
    if node["leaf"]:
        return 1
    return count_leaves(node["left"]) + count_leaves(node["right"])

def test_recursive_build():
    i = 0
    while i < 50:
        tree = build_nested(7)  # 128 leaves
        assert count_leaves(tree) == 128
        assert tree["depth"] == 7
        i = i + 1

test_recursive_build()
print("recursive_build OK")

# --- 9. Mixed types in arrays ---
def test_mixed_arrays():
    i = 0
    while i < 500:
        arr = [1, 2.5, "hello", True, False, None, [1,2], {"a": 1}]
        assert arr[0] == 1
        assert arr[1] == 2.5
        assert arr[2] == "hello"
        assert arr[3] == True
        assert arr[4] == False
        assert arr[5] == None
        assert arr[6][1] == 2
        assert arr[7]["a"] == 1
        i = i + 1

test_mixed_arrays()
print("mixed_arrays OK")

# --- 10. Interleaved string + array + map allocation ---
def test_interleaved_alloc():
    results = []
    i = 0
    while i < 300:
        s = "item_" + str(i)
        a = [i, s]
        m = {"idx": i, "arr": a, "str": s}
        results.push(m)
        i = i + 1
    # Verify everything survived GC cycles
    j = 0
    while j < 300:
        assert results[j]["idx"] == j
        assert results[j]["str"] == "item_" + str(j)
        assert results[j]["arr"][0] == j
        j = j + 1

test_interleaved_alloc()
print("interleaved_alloc OK")

# --- 11. Heavy string building ---
def test_string_building():
    i = 0
    while i < 500:
        parts = []
        j = 0
        while j < 20:
            parts.push("word_" + str(j))
            j = j + 1
        result = ",".join(parts)
        back = result.split(",")
        assert len(back) == 20
        assert back[0] == "word_0"
        assert back[19] == "word_19"
        i = i + 1

test_string_building()
print("string_building OK")

# --- 12. Inheritance chain with method calls ---
class Animal:
    def __init__(self, name):
        self.name = name
    def speak(self):
        return self.name + " speaks"

class Dog(Animal):
    def __init__(self, name, breed):
        super().__init__(name)
        self.breed = breed
    def speak(self):
        return self.name + " barks"

class Puppy(Dog):
    def __init__(self, name, breed, toy):
        super().__init__(name, breed)
        self.toy = toy
    def speak(self):
        return self.name + " yips with " + self.toy

def test_inheritance():
    i = 0
    while i < 500:
        a = Animal("cat_" + str(i))
        d = Dog("dog_" + str(i), "lab")
        p = Puppy("pup_" + str(i), "beagle", "ball_" + str(i))
        assert a.speak() == "cat_" + str(i) + " speaks"
        assert d.speak() == "dog_" + str(i) + " barks"
        assert p.speak() == "pup_" + str(i) + " yips with ball_" + str(i)
        assert p.breed == "beagle"
        i = i + 1

test_inheritance()
print("inheritance OK")

# --- 13. Generator stress ---
def counter(n):
    i = 0
    while i < n:
        yield i
        i = i + 1

def test_generators():
    i = 0
    while i < 200:
        total = 0
        for v in counter(50):
            total = total + v
        assert total == 1225  # sum(0..49)
        i = i + 1

test_generators()
print("generators OK")

# --- 14. String repeat + pad under pressure ---
def test_string_repeat_pad():
    i = 0
    while i < 500:
        s = "ab".repeat(50)
        assert s.len() == 100
        assert s.startswith("ab")
        assert s.endswith("ab")
        p = str(i).pad_left(10, "0")
        assert p.len() == 10
        p2 = str(i).pad_right(10, ".")
        assert p2.len() == 10
        i = i + 1

test_string_repeat_pad()
print("string_repeat_pad OK")

# --- 15. Large map with string keys ---
def test_large_map():
    m = {}
    i = 0
    while i < 1000:
        m["key_" + str(i)] = "val_" + str(i)
        i = i + 1
    assert len(m) == 1000
    # Random access
    assert m["key_0"] == "val_0"
    assert m["key_500"] == "val_500"
    assert m["key_999"] == "val_999"
    # Iterate keys
    keys = m.keys()
    assert len(keys) == 1000
    vals = m.values()
    assert len(vals) == 1000

test_large_map()
print("large_map OK")

# --- 16. Decorator + closure combo ---
def trace(fn):
    def wrapper(*args):
        return fn(*args)
    return wrapper

@trace
def add(a, b):
    return a + b

def test_decorator():
    i = 0
    while i < 500:
        r = add(i, i + 1)
        assert r == 2 * i + 1
        i = i + 1

test_decorator()
print("decorator OK")

# --- 17. For-in with many iterables ---
def test_for_in():
    i = 0
    while i < 200:
        total = 0
        for x in [1, 2, 3, 4, 5]:
            total = total + x
        assert total == 15
        
        chars = []
        s = "hello"
        ci = 0
        while ci < s.len():
            chars.push(s.char_at(ci))
            ci = ci + 1
        assert chars == ["h", "e", "l", "l", "o"]
        
        for k in range(10):
            pass  # just iterate
        i = i + 1

test_for_in()
print("for_in OK")

# --- 18. Varargs stress ---
def variadic(*args):
    total = 0
    for a in args:
        total = total + a
    return total

def test_varargs():
    i = 0
    while i < 500:
        assert variadic(1) == 1
        assert variadic(1, 2) == 3
        assert variadic(1, 2, 3, 4, 5) == 15
        assert variadic() == 0
        i = i + 1

test_varargs()
print("varargs OK")

# --- 19. String formatting / concatenation storm ---
def test_string_concat():
    i = 0
    while i < 500:
        s = ""
        j = 0
        while j < 20:
            s = s + str(j) + ","
            j = j + 1
        parts = s.split(",")
        # Last element is "" (trailing comma)
        assert parts[0] == "0"
        assert parts[19] == "19"
        assert parts[20] == ""
        i = i + 1

test_string_concat()
print("string_concat OK")

# --- 20. Rapid create/discard cycle ---
def test_rapid_lifecycle():
    i = 0
    while i < 2000:
        x = [1, 2, 3]
        y = {"a": x, "b": "hello"}
        z = y["a"]
        assert z[0] == 1
        # x, y, z all become garbage next iteration
        i = i + 1

test_rapid_lifecycle()
print("rapid_lifecycle OK")

print("=== ALL GC TORTURE TESTS PASSED ===")
