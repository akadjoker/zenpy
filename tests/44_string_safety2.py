# === Level-5: Deep string safety edge cases ===
# Covers cross-frame upvalues, fibers, default args, complex aliasing.

def assert_eq(a, b, msg):
    if a != b:
        print("FAIL " + msg + ": got " + str(a) + " expected " + str(b))
        return False
    return True

ok = True

# --- Test 1: upvalue read into local + modify ---
# OP_GETUPVAL creates cross-frame alias; += must not corrupt outer.
def t1():
    s = "u_" + str(0)
    saved = s
    def inner():
        local_s = s  # OP_GETUPVAL — reads upvalue into local register
        local_s += "_x"  # local += : A == B — would corrupt outer without shared marking
        return local_s
    r = inner()
    return (assert_eq(saved, "u_0", "t1 saved") and
            assert_eq(s, "u_0", "t1 s") and
            assert_eq(r, "u_0_x", "t1 r"))
ok = t1() and ok
if ok:
    print("t1_upvalue_alias OK")

# --- Test 2: upvalue modify in loop via nonlocal ---
def t2():
    s = "v_" + str(0)
    snapshots = []
    def grow():
        nonlocal s
        snapshots.append(s)
        s = s + "_g"  # nonlocal uses = instead of +=
    i = 0
    while i < 5:
        grow()
        i += 1
    return (assert_eq(snapshots[0], "v_0", "t2 snap0") and
            assert_eq(snapshots[1], "v_0_g", "t2 snap1") and
            assert_eq(snapshots[2], "v_0_g_g", "t2 snap2") and
            assert_eq(s, "v_0_g_g_g_g_g", "t2 final"))
ok = t2() and ok
if ok:
    print("t2_upvalue_loop OK")

# --- Test 3: two closures sharing same upvalue ---
def t3():
    s = "w_" + str(0)
    def reader():
        return s
    def writer():
        nonlocal s
        s = s + "_w"
    snap1 = reader()
    writer()
    snap2 = reader()
    writer()
    snap3 = reader()
    return (assert_eq(snap1, "w_0", "t3 snap1") and
            assert_eq(snap2, "w_0_w", "t3 snap2") and
            assert_eq(snap3, "w_0_w_w", "t3 snap3"))
ok = t3() and ok
if ok:
    print("t3_two_closures OK")

# --- Test 4: return value from function + modify ---
def t4():
    def make():
        return "r_" + str(0)
    a = make()
    b = a
    a += "_mod"
    return assert_eq(b, "r_0", "t4 b") and assert_eq(a, "r_0_mod", "t4 a")
ok = t4() and ok
if ok:
    print("t4_return_alias OK")

# --- Test 5: generator yields string snapshots, each must be independent ---
def t5():
    def gen():
        s = "fib_" + str(0)
        yield s
        s += "_a"
        yield s
        s += "_b"
        yield s

    results = []
    for v in gen():
        results.append(v)
    return (assert_eq(results[0], "fib_0", "t5 r0") and
            assert_eq(results[1], "fib_0_a", "t5 r1") and
            assert_eq(results[2], "fib_0_a_b", "t5 r2"))
ok = t5() and ok
if ok:
    print("t5_fiber_send OK")

# --- Test 6: deep nested upvalue chain ---
def t6():
    s = "deep_" + str(0)
    saved = s
    def level1():
        nonlocal s
        def level2():
            nonlocal s
            s = s + "_2"
        level2()
        s = s + "_1"
    level1()
    return assert_eq(saved, "deep_0", "t6 saved") and assert_eq(s, "deep_0_2_1", "t6 s")
ok = t6() and ok
if ok:
    print("t6_nested_upval OK")

# --- Test 7: string in multiple containers ---
def t7():
    s = "multi_" + str(0)
    arr = [s]
    m = {"k": s}
    class Box:
        def __init__(self, v):
            self.v = v
    b = Box(s)
    s += "_mod"
    return (assert_eq(arr[0], "multi_0", "t7 arr") and
            assert_eq(m["k"], "multi_0", "t7 map") and
            assert_eq(b.v, "multi_0", "t7 box") and
            assert_eq(s, "multi_0_mod", "t7 s"))
ok = t7() and ok
if ok:
    print("t7_multi_container OK")

# --- Test 8: string passed through multiple function calls ---
def t8():
    def f3(s):
        return s + "_3"
    def f2(s):
        return f3(s + "_2")
    def f1(s):
        r = f2(s + "_1")
        return r

    base = "chain_" + str(0)
    saved = base
    result = f1(base)
    return (assert_eq(saved, "chain_0", "t8 saved") and
            assert_eq(base, "chain_0", "t8 base") and
            assert_eq(result, "chain_0_1_2_3", "t8 result"))
ok = t8() and ok
if ok:
    print("t8_deep_call_chain OK")

# --- Test 9: class method += on self.field ---
def t9():
    class Builder:
        def __init__(self):
            self.buf = "b_" + str(0)
        def add(self, part):
            self.buf += part
        def snapshot(self):
            return self.buf

    b = Builder()
    snap1 = b.snapshot()
    b.add("_x")
    snap2 = b.snapshot()
    b.add("_y")
    snap3 = b.snapshot()
    # Each snapshot should be independent
    return (assert_eq(snap1, "b_0", "t9 snap1") and
            assert_eq(snap2, "b_0_x", "t9 snap2") and
            assert_eq(snap3, "b_0_x_y", "t9 snap3"))
ok = t9() and ok
if ok:
    print("t9_method_build OK")

# --- Test 10: string shared between set and local ---
def t10():
    s = "set_" + str(0)
    st = {s}
    s += "_mod"
    return assert_eq("set_0" in st, True, "t10 set contains original") and assert_eq(s, "set_0_mod", "t10 s")
ok = t10() and ok
if ok:
    print("t10_set_alias OK")

# --- Test 11: upvalue read in loop, each read must get fresh snapshot ---
def t11():
    s = "loop_" + str(0)
    snaps = []
    def capture():
        local = s  # OP_GETUPVAL — marks shared
        local += "_snap"  # must not corrupt s
        snaps.append(local)
    i = 0
    while i < 5:
        capture()
        s += "_" + str(i)
        i += 1
    return (assert_eq(snaps[0], "loop_0_snap", "t11 s0") and
            assert_eq(snaps[1], "loop_0_0_snap", "t11 s1") and
            assert_eq(snaps[2], "loop_0_0_1_snap", "t11 s2") and
            assert_eq(s, "loop_0_0_1_2_3_4", "t11 final"))
ok = t11() and ok
if ok:
    print("t11_except_augadd OK")

# --- Test 12: string from array index + modify ---
def t12():
    arr = ["item_" + str(0), "item_" + str(1), "item_" + str(2)]
    x = arr[1]
    x += "_mod"
    return assert_eq(arr[1], "item_1", "t12 arr") and assert_eq(x, "item_1_mod", "t12 x")
ok = t12() and ok
if ok:
    print("t12_array_read_modify OK")

# --- Test 13: string from map value + modify ---
def t13():
    m = {"a": "val_" + str(0)}
    v = m["a"]
    v += "_mod"
    return assert_eq(m["a"], "val_0", "t13 map") and assert_eq(v, "val_0_mod", "t13 v")
ok = t13() and ok
if ok:
    print("t13_map_read_modify OK")

# --- Test 14: string from field + modify ---
def t14():
    class Obj:
        def __init__(self, v):
            self.v = v
    o = Obj("f_" + str(0))
    x = o.v
    x += "_mod"
    return assert_eq(o.v, "f_0", "t14 field") and assert_eq(x, "f_0_mod", "t14 x")
ok = t14() and ok
if ok:
    print("t14_field_read_modify OK")

# --- Test 15: generator yields snapshots during string build ---
def t15():
    def builder(prefix):
        s = prefix
        i = 0
        while i < 5:
            s += "_" + str(i)
            yield s
            i += 1

    results = []
    for v in builder("gen_" + str(0)):
        results.append(v)

    return (assert_eq(results[0], "gen_0_0", "t15 r0") and
            assert_eq(results[1], "gen_0_0_1", "t15 r1") and
            assert_eq(results[2], "gen_0_0_1_2", "t15 r2") and
            assert_eq(len(results), 5, "t15 count"))
ok = t15() and ok
if ok:
    print("t15_generator_build OK")

# --- Test 16: triple alias: local, param, field ---
def t16():
    class Holder:
        pass

    def mutate(s, h):
        h.ref = s
        saved = s
        s += "_mut"
        return assert_eq(saved, h.ref, "t16 saved==ref") and assert_eq(s, saved + "_mut", "t16 s")

    base = "triple_" + str(0)
    h = Holder()
    r = mutate(base, h)
    return r and assert_eq(base, "triple_0", "t16 base") and assert_eq(h.ref, "triple_0", "t16 href")
ok = t16() and ok
if ok:
    print("t16_triple_alias OK")

# --- Test 17: swap strings via tuple unpack ---
def t17():
    a = "sw_" + str(1)
    b = "sw_" + str(2)
    saved_a = a
    saved_b = b
    a, b = b, a
    a += "_x"
    b += "_y"
    return (assert_eq(saved_a, "sw_1", "t17 sa") and
            assert_eq(saved_b, "sw_2", "t17 sb") and
            assert_eq(a, "sw_2_x", "t17 a") and
            assert_eq(b, "sw_1_y", "t17 b"))
ok = t17() and ok
if ok:
    print("t17_swap_alias OK")

# --- Test 18: map key survives value modification ---
def t18():
    m = {}
    i = 0
    while i < 10:
        k = "k_" + str(i)
        m[k] = i
        k += "_mod"  # must not corrupt map key
        i += 1
    # Verify all keys intact
    i = 0
    while i < 10:
        if m["k_" + str(i)] != i:
            print("FAIL t18: key k_" + str(i) + " corrupted")
            return False
        i += 1
    return True
ok = t18() and ok
if ok:
    print("t18_iter_key_modify OK")

# --- Test 19: string concatenation chain with intermediate saves ---
def t19():
    s = "c_" + str(0)
    parts = []
    i = 0
    while i < 10:
        parts.append(s)
        s = s + "_" + str(i)  # non-augmented: A != B -> new_string_concat
        i += 1
    # All parts should be independent snapshots
    r = True
    i = 0
    while i < 10:
        expected = "c_0"
        j = 0
        while j < i:
            expected = expected + "_" + str(j)
            j += 1
        if parts[i] != expected:
            print("FAIL t19: parts[" + str(i) + "]=" + parts[i] + " expected=" + expected)
            r = False
        i += 1
    return r
ok = t19() and ok
if ok:
    print("t19_concat_chain OK")

# --- Test 20: stress - 1000 objects with interleaved string builds ---
def t20():
    class Item:
        def __init__(self, name):
            self.name = name
            self.desc = name + "_desc"

    items = []
    i = 0
    while i < 1000:
        items.append(Item("item_" + str(i)))
        i += 1

    # Verify integrity
    ok_inner = True
    i = 0
    while i < 1000:
        if items[i].name != "item_" + str(i):
            print("FAIL t20: name[" + str(i) + "]=" + items[i].name)
            ok_inner = False
            break
        if items[i].desc != "item_" + str(i) + "_desc":
            print("FAIL t20: desc[" + str(i) + "]=" + items[i].desc)
            ok_inner = False
            break
        i += 1
    return ok_inner
ok = t20() and ok
if ok:
    print("t20_stress_objects OK")

if ok:
    print("=== ALL LEVEL-5 STRING SAFETY TESTS PASSED ===")
else:
    print("=== SOME TESTS FAILED ===")
