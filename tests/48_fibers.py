# Test fibers: generators (yield/for-in) and async/await coroutines

ok = True

def assert_eq(a, b, tag):
    global ok
    if a != b:
        print("FAIL " + tag + ": got " + str(a) + " expected " + str(b))
        ok = False
        return False
    return True

# --- Test 1: basic generator, yield sequence ---
def t1():
    def counter(n):
        i = 0
        while i < n:
            yield i
            i += 1
    result = []
    for v in counter(5):
        result.append(v)
    return (assert_eq(len(result), 5, "t1 len") and
            assert_eq(result[0], 0, "t1[0]") and
            assert_eq(result[4], 4, "t1[4]"))
ok = t1() and ok
if ok: print("t1_basic_generator OK")

# --- Test 2: generator with parameters, yield expressions ---
def t2():
    def range_by(start, stop, step):
        i = start
        while i < stop:
            yield i
            i += step
    result = []
    for v in range_by(0, 10, 3):
        result.append(v)
    return (assert_eq(len(result), 4, "t2 len") and
            assert_eq(result[0], 0, "t2[0]") and
            assert_eq(result[1], 3, "t2[1]") and
            assert_eq(result[2], 6, "t2[2]") and
            assert_eq(result[3], 9, "t2[3]"))
ok = t2() and ok
if ok: print("t2_generator_params OK")

# --- Test 3: fibonacci generator ---
def t3():
    def fib():
        a = 0
        b = 1
        while True:
            yield a
            c = a + b
            a = b
            b = c
    result = []
    count = 0
    for v in fib():
        result.append(v)
        count += 1
        if count >= 8:
            break
    return (assert_eq(result[0], 0, "t3[0]") and
            assert_eq(result[1], 1, "t3[1]") and
            assert_eq(result[2], 1, "t3[2]") and
            assert_eq(result[3], 2, "t3[3]") and
            assert_eq(result[4], 3, "t3[4]") and
            assert_eq(result[5], 5, "t3[5]") and
            assert_eq(result[6], 8, "t3[6]") and
            assert_eq(result[7], 13, "t3[7]"))
ok = t3() and ok
if ok: print("t3_fib_generator OK")

# --- Test 4: generator state is independent per call ---
def t4():
    def make_gen(start):
        i = start
        while i < start + 3:
            yield i
            i += 1
    r1 = []
    for v in make_gen(0):
        r1.append(v)
    r2 = []
    for v in make_gen(10):
        r2.append(v)
    return (assert_eq(r1[0], 0, "t4 r1[0]") and
            assert_eq(r1[2], 2, "t4 r1[2]") and
            assert_eq(r2[0], 10, "t4 r2[0]") and
            assert_eq(r2[2], 12, "t4 r2[2]"))
ok = t4() and ok
if ok: print("t4_independent_state OK")

# --- Test 5: generator with upvalue capture ---
def t5():
    acc = 0
    def gen():
        nonlocal acc
        acc = acc + 1
        yield acc
        acc = acc + 10
        yield acc
        acc = acc + 100
        yield acc
    result = []
    for v in gen():
        result.append(v)
    return (assert_eq(result[0], 1,   "t5[0]") and
            assert_eq(result[1], 11,  "t5[1]") and
            assert_eq(result[2], 111, "t5[2]") and
            assert_eq(acc, 111, "t5 acc"))
ok = t5() and ok
if ok: print("t5_upvalue_capture OK")

# --- Test 6: generator yields strings (SSO safety) ---
def t6():
    def words():
        yield "hello"
        yield "world"
        yield "fiber"
    result = []
    for w in words():
        result.append(w)
    return (assert_eq(result[0], "hello", "t6[0]") and
            assert_eq(result[1], "world", "t6[1]") and
            assert_eq(result[2], "fiber", "t6[2]"))
ok = t6() and ok
if ok: print("t6_string_yield OK")

# --- Test 7: nested generator inside loop ---
def t7():
    def squares(n):
        i = 1
        while i <= n:
            yield i * i
            i += 1
    total = 0
    for sq in squares(4):
        total += sq
    # 1 + 4 + 9 + 16 = 30
    return assert_eq(total, 30, "t7 total")
ok = t7() and ok
if ok: print("t7_sum_of_squares OK")

# --- Test 8: generator passed to function ---
def t8():
    def evens(n):
        i = 0
        while i <= n:
            yield i
            i += 2
    def collect(gen):
        result = []
        for v in gen:
            result.append(v)
        return result
    g = evens(8)
    result = collect(g)
    return (assert_eq(len(result), 5, "t8 len") and
            assert_eq(result[0], 0, "t8[0]") and
            assert_eq(result[4], 8, "t8[4]"))
ok = t8() and ok
if ok: print("t8_generator_as_arg OK")

# --- Test 9: async def / await basic ---
async def async_add(a, b):
    return a + b

def t9():
    r = await async_add(3, 7)
    return assert_eq(r, 10, "t9 basic await")
ok = t9() and ok
if ok: print("t9_async_basic OK")

# --- Test 10: async calling async ---
async def async_double(n):
    return n * 2

async def async_pipeline(n):
    a = await async_add(n, 1)
    b = await async_double(a)
    return b

def t10():
    r = await async_pipeline(4)
    # (4+1)*2 = 10
    return assert_eq(r, 10, "t10 pipeline")
ok = t10() and ok
if ok: print("t10_async_pipeline OK")

# --- Test 11: async with conditional ---
async def async_abs(n):
    if n < 0:
        return -n
    return n

def t11():
    r1 = await async_abs(-7)
    r2 = await async_abs(3)
    return (assert_eq(r1, 7, "t11 neg") and
            assert_eq(r2, 3, "t11 pos"))
ok = t11() and ok
if ok: print("t11_async_conditional OK")

# --- Test 12: async fibonacci ---
async def async_fib(n):
    if n <= 1:
        return n
    a = await async_fib(n - 1)
    b = await async_fib(n - 2)
    return a + b

def t12():
    return (assert_eq(await async_fib(0), 0, "t12 fib0") and
            assert_eq(await async_fib(1), 1, "t12 fib1") and
            assert_eq(await async_fib(5), 5, "t12 fib5") and
            assert_eq(await async_fib(7), 13, "t12 fib7"))
ok = t12() and ok
if ok: print("t12_async_fib OK")

# --- Test 13: generator empty (yields nothing) ---
def t13():
    def empty():
        if False:
            yield 1
    result = []
    for v in empty():
        result.append(v)
    return assert_eq(len(result), 0, "t13 empty")
ok = t13() and ok
if ok: print("t13_empty_generator OK")

# --- Test 14: generator single yield ---
def t14():
    def single():
        yield 42
    result = []
    for v in single():
        result.append(v)
    return (assert_eq(len(result), 1, "t14 len") and
            assert_eq(result[0], 42, "t14[0]"))
ok = t14() and ok
if ok: print("t14_single_yield OK")

# --- Test 15: multiple generators running in sequence ---
def t15():
    def gen_a():
        yield 1
        yield 2
    def gen_b():
        yield 10
        yield 20
    result = []
    for v in gen_a():
        result.append(v)
    for v in gen_b():
        result.append(v)
    return (assert_eq(result[0], 1,  "t15[0]") and
            assert_eq(result[1], 2,  "t15[1]") and
            assert_eq(result[2], 10, "t15[2]") and
            assert_eq(result[3], 20, "t15[3]"))
ok = t15() and ok
if ok: print("t15_sequential_generators OK")

if ok:
    print("All fiber tests passed.")
else:
    print("Some fiber tests FAILED.")
