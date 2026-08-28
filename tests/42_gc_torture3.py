# Level-3 GC Torture Tests — extreme combinations
# Each test exercises multiple GC-dangerous patterns simultaneously

# --- 1. Recursive closures capturing recursive results ---
def test_recursive_closure_chain():
    def build(depth):
        if depth == 0:
            return {"val": "leaf_" + str(depth), "children": []}
        left = build(depth - 1)
        right = build(depth - 1)
        node = {"val": "node_" + str(depth), "children": [left, right]}
        def get_val():
            return node["val"]
        node["getter"] = get_val
        return node

    i = 0
    while i < 50:
        tree = build(6)
        assert tree["val"] == "node_6"
        assert tree["children"][0]["val"] == "node_5"
        assert tree["getter"]() == "node_6"
        # Walk down left spine
        n = tree
        d = 6
        while d > 0:
            assert n["val"] == "node_" + str(d)
            assert n["getter"]() == "node_" + str(d)
            n = n["children"][0]
            d = d - 1
        assert n["val"] == "leaf_0"
        i = i + 1

test_recursive_closure_chain()
print("recursive_closure_chain OK")

# --- 2. Generator yielding closures that capture complex state ---
def test_generator_closure_yield():
    def make_items(n):
        i = 0
        while i < n:
            tag = "gen_" + str(i) + "_" + str(i * 3)
            data = {"tag": tag, "nums": [i, i+1, i+2], "nested": {"x": i}}
            def capture(d, t):
                def get():
                    return [d, t]
                return get
            fn = capture(data, tag)
            yield fn
            i = i + 1

    results = []
    gen = make_items(200)
    for fn in gen:
        pair = fn()
        results.push(pair)
    assert len(results) == 200
    assert results[0][1] == "gen_0_0"
    assert results[199][1] == "gen_199_597"
    assert results[50][0]["tag"] == "gen_50_150"
    assert results[50][0]["nums"][2] == 52

test_generator_closure_yield()
print("generator_closure_yield OK")

# --- 3. Class instances holding closures that reference other instances ---
def test_instance_closure_web():
    class Node:
        def __init__(self, name):
            self.name = name
            self.neighbors = []
            self.data = {"created": name + "_data"}

        def add_neighbor(self, other):
            self.neighbors.push(other)

        def make_walker(self):
            node = self
            def walk(depth):
                if depth == 0:
                    return [node.name]
                result = [node.name]
                for nb in node.neighbors:
                    for item in nb.make_walker()(depth - 1):
                        result.push(item)
                return result
            return walk

    i = 0
    while i < 100:
        a = Node("a_" + str(i))
        b = Node("b_" + str(i))
        c = Node("c_" + str(i))
        a.add_neighbor(b)
        a.add_neighbor(c)
        b.add_neighbor(c)
        walker = a.make_walker()
        names = walker(2)
        assert names[0] == "a_" + str(i)
        assert len(names) >= 4
        i = i + 1

test_instance_closure_web()
print("instance_closure_web OK")

# --- 4. Deeply nested map/array construction with string interning pressure ---
def test_deep_nested_construction():
    i = 0
    while i < 100:
        obj = {"level": 0, "id": "root_" + str(i)}
        current = obj
        j = 1
        while j < 15:
            child = {
                "level": j,
                "id": "node_" + str(i) + "_" + str(j),
                "tags": [str(j), str(j*2), ("tag_" + str(j)).upper()],
                "parent_id": current["id"]
            }
            current["child"] = child
            current = child
            j = j + 1
        # Walk back and verify
        current = obj
        j = 0
        while j < 15:
            assert current["level"] == j
            assert current["id"].startswith("node_") or current["id"].startswith("root_")
            if j < 14:
                current = current["child"]
            j = j + 1
        i = i + 1

test_deep_nested_construction()
print("deep_nested_construction OK")

# --- 5. Closure capturing loop variable with heavy allocation between captures ---
def test_closure_loop_capture_stress():
    makers = []
    i = 0
    while i < 300:
        val = {"idx": i, "str": "item_" + str(i), "arr": [i, i+1, i+2]}
        def make(v):
            def get():
                return v
            return get
        makers.push(make(val))
        # Heavy allocation pressure between captures
        trash1 = [0] * 20
        trash2 = {"a": str(i), "b": str(i*2), "c": [i]}
        trash3 = ("x" + str(i)).upper() + "_end"
        i = i + 1
    # Verify all captures survived
    j = 0
    while j < 300:
        v = makers[j]()
        assert v["idx"] == j
        assert v["str"] == "item_" + str(j)
        assert v["arr"][2] == j + 2
        j = j + 1

test_closure_loop_capture_stress()
print("closure_loop_capture_stress OK")

# --- 6. Multiple generators interleaved ---
def test_interleaved_generators():
    def counter(prefix, n):
        i = 0
        while i < n:
            yield prefix + "_" + str(i)
            i = i + 1

    results = []
    i = 0
    while i < 50:
        g1 = counter("alpha", 10)
        g2 = counter("beta", 10)
        g3 = counter("gamma", 10)
        for a in g1:
            for b in g2:
                pass
            for c in g3:
                pass
            results.push(a)
        i = i + 1
    assert len(results) == 500
    assert results[0] == "alpha_0"
    assert results[499] == "alpha_9"

test_interleaved_generators()
print("interleaved_generators OK")

# --- 7. Class with __str__ and complex method chains ---
def test_method_chain_alloc():
    class Builder:
        def __init__(self):
            self.parts = []
            self.meta = {}

        def add(self, part):
            self.parts.push(part)
            self.meta["last"] = part
            self.meta["count"] = len(self.parts)
            return self

        def build(self):
            result = ""
            for p in self.parts:
                if len(result) > 0:
                    result = result + "," + p
                else:
                    result = p
            return {"result": result, "meta": self.meta}

    i = 0
    while i < 200:
        b = Builder()
        j = 0
        while j < 10:
            b.add("part_" + str(j) + "_iter" + str(i))
            j = j + 1
        obj = b.build()
        assert obj["meta"]["count"] == 10
        assert "part_0_iter" + str(i) in obj["result"]
        i = i + 1

test_method_chain_alloc()
print("method_chain_alloc OK")

# --- 8. Recursive fibonacci with memoization (map + closures) ---
def test_fib_memo():
    i = 0
    while i < 100:
        cache = {}
        def fib(n):
            if n < 2:
                return n
            key = str(n)
            if key in cache:
                return cache[key]
            result = fib(n - 1) + fib(n - 2)
            cache[key] = result
            return result
        assert fib(20) == 6765
        assert len(cache) == 19
        i = i + 1

test_fib_memo()
print("fib_memo OK")

# --- 9. Array of arrays of maps with string keys, random-like access ---
def test_matrix_of_maps():
    rows = []
    i = 0
    while i < 30:
        row = []
        j = 0
        while j < 30:
            cell = {
                "r": i, "c": j,
                "id": "cell_" + str(i) + "_" + str(j),
                "val": (i * 30 + j)
            }
            row.push(cell)
            j = j + 1
        rows.push(row)
        i = i + 1
    # Access pattern that creates temporaries
    total = 0
    i = 0
    while i < 30:
        j = 0
        while j < 30:
            cell = rows[i][j]
            assert cell["r"] == i
            assert cell["c"] == j
            assert cell["id"] == "cell_" + str(i) + "_" + str(j)
            total = total + cell["val"]
            j = j + 1
        i = i + 1
    assert total == 30 * 30 * (30 * 30 - 1) / 2

test_matrix_of_maps()
print("matrix_of_maps OK")

# --- 10. Everything at once: classes + generators + closures + recursion ---
def test_ultimate_stress():
    class TreeNode:
        def __init__(self, val):
            self.val = val
            self.left = nil
            self.right = nil
            self.tag = "node_" + str(val)

    def build_tree(depth, base):
        if depth == 0:
            return nil
        node = TreeNode(base)
        node.left = build_tree(depth - 1, base * 2)
        node.right = build_tree(depth - 1, base * 2 + 1)
        return node

    def tree_to_list(node):
        if node == nil:
            yield_hack = []
            return yield_hack
        result = [{"val": node.val, "tag": node.tag}]
        for item in tree_to_list(node.left):
            result.push(item)
        for item in tree_to_list(node.right):
            result.push(item)
        return result

    def make_checker(expected_val):
        def check(item):
            return item["val"] == expected_val
        return check

    i = 0
    while i < 30:
        tree = build_tree(5, 1)
        items = tree_to_list(tree)
        assert len(items) == 31  # 2^5 - 1
        assert items[0]["val"] == 1
        assert items[0]["tag"] == "node_1"
        checker = make_checker(1)
        assert checker(items[0])
        # Create garbage between iterations
        trash = []
        j = 0
        while j < 50:
            trash.push({"k": str(j), "v": [j, j+1]})
            j = j + 1
        i = i + 1

test_ultimate_stress()
print("ultimate_stress OK")

print("=== ALL LEVEL-3 TORTURE TESTS PASSED ===")
