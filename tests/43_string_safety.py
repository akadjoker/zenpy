# === Level-4: String immutability & shared flag edge cases ===
# Every test verifies that string_append_inplace never corrupts aliases.

def assert_eq(a, b, msg):
    if a != b:
        print("FAIL " + msg + ": got " + str(a) + " expected " + str(b))
        return False
    return True

ok = True

# --- Test 1: param += doesn't corrupt caller ---
def t1_param_modify(s):
    s += "_mod"
    return s

val = "a_" + str(0)
r = t1_param_modify(val)
ok = assert_eq(val, "a_0", "t1 caller") and ok
ok = assert_eq(r, "a_0_mod", "t1 result") and ok
if ok:
    print("t1_param_modify OK")

# --- Test 2: local alias ---
def t2():
    s = "b_" + str(0)
    saved = s
    s += "_mod"
    return assert_eq(saved, "b_0", "t2 saved") and assert_eq(s, "b_0_mod", "t2 s")
ok = t2() and ok
if ok:
    print("t2_local_alias OK")

# --- Test 3: field store protects against += ---
def t3():
    class Box:
        def __init__(self, v):
            self.v = v
    name = "c_" + str(0)
    b = Box(name)
    name += "_mod"
    return assert_eq(b.v, "c_0", "t3 field")
ok = t3() and ok
if ok:
    print("t3_field_store OK")

# --- Test 4: array store protects ---
def t4():
    s = "d_" + str(0)
    arr = [s]
    s += "_mod"
    return assert_eq(arr[0], "d_0", "t4 array")
ok = t4() and ok
if ok:
    print("t4_array_store OK")

# --- Test 5: map value store protects ---
def t5():
    s = "e_" + str(0)
    m = {"key": s}
    s += "_mod"
    return assert_eq(m["key"], "e_0", "t5 map")
ok = t5() and ok
if ok:
    print("t5_map_store OK")

# --- Test 6: multiple params ---
def t6_multi(a, b, c):
    a += "_x"
    b += "_y"
    c += "_z"
    return a + b + c

p = "f_" + str(0)
q = "g_" + str(0)
r2 = "h_" + str(0)
t6_multi(p, q, r2)
ok = assert_eq(p, "f_0", "t6 p") and ok
ok = assert_eq(q, "g_0", "t6 q") and ok
ok = assert_eq(r2, "h_0", "t6 r") and ok
if ok:
    print("t6_multi_params OK")

# --- Test 7: nested function calls ---
def t7_outer(s):
    def inner(s2):
        s2 += "_inner"
        return s2
    r = inner(s)
    s += "_outer"
    return r + "|" + s

val7 = "i_" + str(0)
r7 = t7_outer(val7)
ok = assert_eq(val7, "i_0", "t7 original") and ok
ok = assert_eq(r7, "i_0_inner|i_0_outer", "t7 result") and ok
if ok:
    print("t7_nested_calls OK")

# --- Test 8: recursive string building ---
def t8_rec(s, n):
    if n <= 0:
        return s
    return t8_rec(s + "x", n - 1)

base8 = "j_" + str(0)
r8 = t8_rec(base8, 10)
ok = assert_eq(base8, "j_0", "t8 base") and ok
ok = assert_eq(len(r8), 13, "t8 len") and ok
if ok:
    print("t8_recursive OK")

# --- Test 9: closure captures string, outer modifies ---
def t9():
    s = "k_" + str(0)
    def getter():
        return s
    # s is captured by reference in upvalue
    # += in outer should work, closure sees new value
    s += "_mod"
    return getter()

r9 = t9()
# closure captures by ref, so should see modified value
ok = assert_eq(r9, "k_0_mod", "t9 closure") and ok
if ok:
    print("t9_closure_capture OK")

# --- Test 10: generator yield with string ---
def t10():
    def gen(s):
        s += "_a"
        yield s
        s += "_b"
        yield s

    base = "l_" + str(0)
    results = []
    for v in gen(base):
        results.append(v)
    return assert_eq(base, "l_0", "t10 base") and assert_eq(results[0], "l_0_a", "t10 v1") and assert_eq(results[1], "l_0_a_b", "t10 v2")

ok = t10() and ok
if ok:
    print("t10_generator OK")

# --- Test 11: string in loop with field store each iteration ---
def t11():
    class Node:
        def __init__(self, name):
            self.name = name
            self.data = name + "_data"
    
    nodes = []
    i = 0
    while i < 100:
        n = Node("n_" + str(i))
        nodes.append(n)
        i = i + 1
    
    # verify all nodes kept their original name
    i = 0
    while i < 100:
        if nodes[i].name != "n_" + str(i):
            return False
        if nodes[i].data != "n_" + str(i) + "_data":
            return False
        i = i + 1
    return True

ok = t11() and ok
if ok:
    print("t11_loop_field OK")

# --- Test 12: += in loop doesn't leak across iterations ---
def t12():
    results = []
    i = 0
    while i < 50:
        s = "m_" + str(i)
        s += "_done"
        results.append(s)
        i = i + 1
    # each result must be independent
    i = 0
    while i < 50:
        if results[i] != "m_" + str(i) + "_done":
            return False
        i = i + 1
    return True

ok = t12() and ok
if ok:
    print("t12_loop_append OK")

# --- Test 13: chain of assignments ---
def t13():
    a = "n_" + str(0)
    b = a
    c = b
    a += "_1"
    b += "_2"
    c += "_3"
    return assert_eq(a, "n_0_1", "t13 a") and assert_eq(b, "n_0_2", "t13 b") and assert_eq(c, "n_0_3", "t13 c")

ok = t13() and ok
if ok:
    print("t13_chain_assign OK")

# --- Test 14: string as dict key (interned) ---
def t14():
    m = {}
    i = 0
    while i < 50:
        k = "key_" + str(i)
        m[k] = i
        k += "_modified"  # must not corrupt the key in the map
        i = i + 1
    # verify keys
    i = 0
    while i < 50:
        if m["key_" + str(i)] != i:
            return False
        i = i + 1
    return True

ok = t14() and ok
if ok:
    print("t14_dict_key OK")

# --- Test 15: class __init__ with self.x = param + something ---
def t15():
    class Thing:
        def __init__(self, name):
            self.label = name + "_label"
            self.name = name
            self.tag = name + "_tag"
    
    things = []
    i = 0
    while i < 100:
        t = Thing("t_" + str(i))
        things.append(t)
        i = i + 1
    
    i = 0
    while i < 100:
        expected = "t_" + str(i)
        if things[i].name != expected:
            print("FAIL t15: name=" + things[i].name + " expected=" + expected)
            return False
        if things[i].label != expected + "_label":
            print("FAIL t15: label=" + things[i].label)
            return False
        if things[i].tag != expected + "_tag":
            print("FAIL t15: tag=" + things[i].tag)
            return False
        i = i + 1
    return True

ok = t15() and ok
if ok:
    print("t15_init_order OK")

# --- Test 16: string += inside method ---
def t16():
    class Builder:
        def __init__(self):
            self.buf = ""
        def add(self, s):
            self.buf += s
    
    b = Builder()
    i = 0
    while i < 100:
        b.add("x")
        i = i + 1
    return assert_eq(len(b.buf), 100, "t16 len")

ok = t16() and ok
if ok:
    print("t16_method_append OK")

# --- Test 17: return string from nested call chain ---
def t17():
    def step3(s):
        return s + "_3"
    def step2(s):
        return step3(s + "_2")
    def step1(s):
        return step2(s + "_1")
    
    base = "o_" + str(0)
    r = step1(base)
    return assert_eq(base, "o_0", "t17 base") and assert_eq(r, "o_0_1_2_3", "t17 result")

ok = t17() and ok
if ok:
    print("t17_call_chain OK")

# --- Test 18: string shared between two fields ---
def t18():
    class Pair:
        def __init__(self, a, b):
            self.a = a
            self.b = b
    
    s = "p_" + str(0)
    p = Pair(s, s)  # both fields point to same string
    s += "_mod"
    return assert_eq(p.a, "p_0", "t18 a") and assert_eq(p.b, "p_0", "t18 b")

ok = t18() and ok
if ok:
    print("t18_dual_field OK")

# --- Test 19: stress — many small concats in function ---
def t19():
    def build(prefix, n):
        s = prefix
        i = 0
        while i < n:
            s += "."
            i = i + 1
        return s
    
    r = build("q_" + str(0), 1000)
    return assert_eq(len(r), 1003, "t19 len")

ok = t19() and ok
if ok:
    print("t19_stress_build OK")

# --- Test 20: interleaved builds don't interfere ---
def t20():
    a = "r_" + str(0)
    b = "s_" + str(0)
    i = 0
    while i < 100:
        a += "a"
        b += "b"
        i = i + 1
    return assert_eq(len(a), 103, "t20 a len") and assert_eq(len(b), 103, "t20 b len") and assert_eq(a[0:3], "r_0", "t20 a prefix") and assert_eq(b[0:3], "s_0", "t20 b prefix")

ok = t20() and ok
if ok:
    print("t20_interleaved OK")

if ok:
    print("=== ALL STRING SAFETY TESTS PASSED ===")
