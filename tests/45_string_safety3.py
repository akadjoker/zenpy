# === Level-6: Adversarial call-path string safety tests ===
# Targets OP_CALLGLOBAL, OP_INVOKE, OP_INVOKE_VT, OP_SUPER_INVOKE
# and other subtle aliasing scenarios.

def assert_eq(a, b, msg):
    if a != b:
        print("FAIL " + msg + ": got " + str(a) + " expected " + str(b))
        return False
    return True

ok = True

# --- Test 1: global function modifies string param (OP_CALLGLOBAL) ---
def global_modify(s):
    s += "_gmod"
    return s

def t1():
    base = "cg_" + str(0)
    saved = base
    r = global_modify(base)
    return assert_eq(saved, "cg_0", "t1 saved") and assert_eq(r, "cg_0_gmod", "t1 result")
ok = t1() and ok
if ok:
    print("t1_callglobal OK")

# --- Test 2: method modifies string param (OP_INVOKE) ---
def t2():
    class Processor:
        def transform(self, s):
            s += "_proc"
            return s

    base = "inv_" + str(0)
    saved = base
    p = Processor()
    r = p.transform(base)
    return assert_eq(saved, "inv_0", "t2 saved") and assert_eq(r, "inv_0_proc", "t2 result")
ok = t2() and ok
if ok:
    print("t2_invoke OK")

# --- Test 3: inherited method modifies string param (OP_SUPER_INVOKE) ---
def t3():
    class Base:
        def process(self, s):
            s += "_base"
            return s
    class Child(Base):
        def process(self, s):
            r = super().process(s)
            return r + "_child"

    base = "sup_" + str(0)
    saved = base
    c = Child()
    r = c.process(base)
    return assert_eq(saved, "sup_0", "t3 saved") and assert_eq(r, "sup_0_base_child", "t3 result")
ok = t3() and ok
if ok:
    print("t3_super_invoke OK")

# --- Test 4: multiple string args to global func ---
def global_multi(a, b, c):
    a += "_1"
    b += "_2"
    c += "_3"
    return a + b + c

def t4():
    x = "m_" + str(0)
    y = "m_" + str(1)
    z = "m_" + str(2)
    sx = x
    sy = y
    sz = z
    r = global_multi(x, y, z)
    return (assert_eq(sx, "m_0", "t4 sx") and
            assert_eq(sy, "m_1", "t4 sy") and
            assert_eq(sz, "m_2", "t4 sz"))
ok = t4() and ok
if ok:
    print("t4_multi_args OK")

# --- Test 5: method chain, each method does += ---
def t5():
    class Builder:
        def __init__(self, s):
            self.s = s
        def add_a(self, part):
            part += "_a"
            self.s += part
            return self
        def add_b(self, part):
            part += "_b"
            self.s += part
            return self

    tag = "bld_" + str(0)
    saved = tag
    b = Builder("start")
    b.add_a(tag)
    b.add_b(tag)
    return (assert_eq(saved, "bld_0", "t5 saved") and
            assert_eq(tag, "bld_0", "t5 tag") and
            assert_eq(b.s, "startbld_0_abld_0_b", "t5 builder"))
ok = t5() and ok
if ok:
    print("t5_method_chain OK")

# --- Test 6: arr[i] += "x" (container element augmented assign) ---
def t6():
    arr = []
    i = 0
    while i < 5:
        arr.append("e_" + str(i))
        i += 1
    # Augmented assign on container element
    arr[0] += "_mod"
    arr[2] += "_mod"
    return (assert_eq(arr[0], "e_0_mod", "t6 arr0") and
            assert_eq(arr[1], "e_1", "t6 arr1") and
            assert_eq(arr[2], "e_2_mod", "t6 arr2") and
            assert_eq(arr[3], "e_3", "t6 arr3"))
ok = t6() and ok
if ok:
    print("t6_arr_augassign OK")

# --- Test 7: obj.field += "x" (field augmented assign) ---
def t7():
    class Box:
        def __init__(self, v):
            self.v = v
    b = Box("fld_" + str(0))
    saved = b.v
    b.v = b.v + "_mod"  # no b.v += outside method
    return assert_eq(saved, "fld_0", "t7 saved") and assert_eq(b.v, "fld_0_mod", "t7 field")
ok = t7() and ok
if ok:
    print("t7_field_augassign OK")

# --- Test 8: map[key] += "x" (map value augmented assign) ---
def t8():
    m = {"k": "mv_" + str(0)}
    saved = m["k"]
    m["k"] += "_mod"
    return assert_eq(saved, "mv_0", "t8 saved") and assert_eq(m["k"], "mv_0_mod", "t8 map")
ok = t8() and ok
if ok:
    print("t8_map_augassign OK")

# --- Test 9: global func called in tight loop ---
def append_x(s):
    s += "x"
    return s

def t9():
    s = "loop_" + str(0)
    original = s
    i = 0
    while i < 100:
        s = append_x(s)
        i += 1
    return (assert_eq(original, "loop_0", "t9 original") and
            assert_eq(len(s), 106, "t9 len"))
ok = t9() and ok
if ok:
    print("t9_global_func_loop OK")

# --- Test 10: method called in tight loop ---
def t10():
    class Acc:
        def add(self, s, part):
            s += part
            return s

    a = Acc()
    s = "ml_" + str(0)
    original = s
    i = 0
    while i < 100:
        s = a.add(s, "_" + str(i))
        i += 1
    return (assert_eq(original, "ml_0", "t10 original") and
            assert_eq(len(s) > 100, True, "t10 length"))
ok = t10() and ok
if ok:
    print("t10_method_loop OK")

# --- Test 11: string passed to global, stored in array, modified ---
collected = []
def collect_and_modify(s):
    collected.append(s)
    s += "_x"
    return s

def t11():
    collected.clear()
    s = "col_" + str(0)
    collect_and_modify(s)
    collect_and_modify(s)
    collect_and_modify(s)
    return (assert_eq(collected[0], "col_0", "t11 c0") and
            assert_eq(collected[1], "col_0", "t11 c1") and
            assert_eq(collected[2], "col_0", "t11 c2") and
            assert_eq(s, "col_0", "t11 s"))
ok = t11() and ok
if ok:
    print("t11_collect_modify OK")

# --- Test 12: same string as two different method args ---
def t12():
    class Dual:
        def process(self, a, b):
            a += "_first"
            b += "_second"
            return a + "|" + b

    s = "dup_" + str(0)
    saved = s
    d = Dual()
    r = d.process(s, s)
    return (assert_eq(saved, "dup_0", "t12 saved") and
            assert_eq(r, "dup_0_first|dup_0_second", "t12 result"))
ok = t12() and ok
if ok:
    print("t12_same_arg_twice OK")

# --- Test 13: inherited method with string field ---
def t13():
    class Animal:
        def __init__(self, name):
            self.name = name
        def speak(self):
            return self.name + " says hello"
    class Dog(Animal):
        def speak(self):
            base = super().speak()
            return base + " (woof)"

    name = "dog_" + str(0)
    saved = name
    d = Dog(name)
    r = d.speak()
    return (assert_eq(saved, "dog_0", "t13 saved") and
            assert_eq(d.name, "dog_0", "t13 field") and
            assert_eq(r, "dog_0 says hello (woof)", "t13 result"))
ok = t13() and ok
if ok:
    print("t13_inherit_field OK")

# --- Test 14: varargs with string args ---
def varfunc(*args):
    results = []
    for a in args:
        a += "_v"
        results.append(a)
    return results

def t14():
    a = "va_" + str(0)
    b = "va_" + str(1)
    c = "va_" + str(2)
    sa = a
    sb = b
    sc = c
    r = varfunc(a, b, c)
    return (assert_eq(sa, "va_0", "t14 sa") and
            assert_eq(sb, "va_1", "t14 sb") and
            assert_eq(sc, "va_2", "t14 sc") and
            assert_eq(r[0], "va_0_v", "t14 r0") and
            assert_eq(r[1], "va_1_v", "t14 r1"))
ok = t14() and ok
if ok:
    print("t14_varargs OK")

# --- Test 15: nested global calls, string flows through ---
def g_outer(s):
    s += "_out"
    return g_inner(s)

def g_inner(s):
    s += "_in"
    return s

def t15():
    base = "nest_" + str(0)
    saved = base
    r = g_outer(base)
    return (assert_eq(saved, "nest_0", "t15 saved") and
            assert_eq(r, "nest_0_out_in", "t15 result"))
ok = t15() and ok
if ok:
    print("t15_nested_global OK")

# --- Test 16: return same string from method, caller modifies ---
def t16():
    class Container:
        def __init__(self):
            self.val = "cnt_" + str(0)
        def get(self):
            return self.val

    c = Container()
    v = c.get()
    v += "_mod"
    return assert_eq(c.val, "cnt_0", "t16 field") and assert_eq(v, "cnt_0_mod", "t16 v")
ok = t16() and ok
if ok:
    print("t16_return_field OK")

# --- Test 17: stress - 500 global func calls with string args ---
def identity_mod(s):
    s += "."
    return s

def t17():
    s = "stress_" + str(0)
    saved = s
    i = 0
    while i < 500:
        r = identity_mod(s)
        i += 1
    return assert_eq(saved, "stress_0", "t17 saved") and assert_eq(s, "stress_0", "t17 s")
ok = t17() and ok
if ok:
    print("t17_stress_global OK")

# --- Test 18: interleaved method and global func calls ---
def gfunc(s):
    s += "_g"
    return s

def t18():
    class Obj:
        def mfunc(self, s):
            s += "_m"
            return s

    o = Obj()
    s = "inter_" + str(0)
    saved = s
    r1 = gfunc(s)
    r2 = o.mfunc(s)
    r3 = gfunc(s)
    return (assert_eq(saved, "inter_0", "t18 saved") and
            assert_eq(s, "inter_0", "t18 s") and
            assert_eq(r1, "inter_0_g", "t18 r1") and
            assert_eq(r2, "inter_0_m", "t18 r2") and
            assert_eq(r3, "inter_0_g", "t18 r3"))
ok = t18() and ok
if ok:
    print("t18_interleaved OK")

# --- Test 19: arr.pop() returns shared string, modify it ---
def t19():
    arr = ["pop_" + str(0), "pop_" + str(1)]
    a = arr.pop()
    a += "_mod"
    return assert_eq(a, "pop_1_mod", "t19 popped") and assert_eq(arr[0], "pop_0", "t19 remaining")
ok = t19() and ok
if ok:
    print("t19_pop_modify OK")

# --- Test 20: grand stress - class hierarchy + methods + globals + closures ---
def t20():
    class Base:
        def __init__(self, name):
            self.name = name
        def tag(self):
            return self.name + "_base"
    class Mid(Base):
        def __init__(self, name):
            self.name = name
        def tag(self):
            return self.name + "_base_mid"
    class Leaf(Mid):
        def __init__(self, name):
            self.name = name
        def tag(self):
            return self.name + "_base_mid_leaf"

    items = []
    i = 0
    while i < 100:
        name = "obj_" + str(i)
        items.append(Leaf(name))
        i += 1

    # Verify all names survived
    ok_inner = True
    i = 0
    while i < 100:
        if items[i].name != "obj_" + str(i):
            print("FAIL t20: name[" + str(i) + "]=" + items[i].name)
            ok_inner = False
            break
        expected_tag = "obj_" + str(i) + "_base_mid_leaf"
        actual = items[i].tag()
        if actual != expected_tag:
            print("FAIL t20: tag[" + str(i) + "]=" + actual + " expected=" + expected_tag)
            ok_inner = False
            break
        i += 1
    return ok_inner
ok = t20() and ok
if ok:
    print("t20_grand_stress OK")

if ok:
    print("=== ALL LEVEL-6 ADVERSARIAL TESTS PASSED ===")
else:
    print("=== SOME TESTS FAILED ===")
