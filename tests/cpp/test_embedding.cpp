 /*
** test_embedding.cpp — Tests C++ <-> Script bidirectional communication.
**
** Tests:
**   1. C++ defines native function, script calls it
**   2. Script defines function, C++ calls it via call_global
**   3. Bidirectional: native calls back into script
**   4. Shared globals between C++ and script
**   5. C++ invokes script method on instance
**   6. Incremental execution (multiple run_source)
**   7. Native struct (C++ pointer in script)
**   8. Script class inherits from C++ class
**   9. StructBuilder — C++ defines record, script uses it
**  10. ClassBuilder with native ctor/dtor + native_data
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "bytecode.h"
#include "zen_bind.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

using namespace zen;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) printf("  %-50s", name)
#define PASS() do { printf("OK\n"); g_tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

/* =========================================================
** Helper: compile + run source in a VM
** ========================================================= */
static bool run_source(VM &vm, const char *source, const char *name = "<test>")
{
    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, name);
    if (!fn) return false;
    vm.run(fn);
    return true;
}

/* =========================================================
** TEST 1: C++ defines native, script calls it
** ========================================================= */
static int native_add(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    int64_t a = args[0].as.integer;
    int64_t b = args[1].as.integer;
    args[0] = val_int(a + b);
    return 1;
}

static int native_concat(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    ObjString *a = as_string(args[0]);
    ObjString *b = as_string(args[1]);

    int len = a->length + b->length;
    char *buf = (char *)malloc(len + 1);
    memcpy(buf, a->chars, a->length);
    memcpy(buf + a->length, b->chars, b->length);
    buf[len] = '\0';

    ObjString *result = intern_string(&vm->get_gc(), buf, len, hash_string(buf, len));
    free(buf);
    args[0] = val_obj((Obj *)result);
    return 1;
}

static void test_native_from_script()
{
    printf("\n[Test 1] C++ native called from script\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    vm.def_native("native_add", native_add, 2);
    vm.def_native("native_concat", native_concat, 2);

    TEST("native_add(10, 20) == 30");
    run_source(vm, "result = native_add(10, 20)");
    Value r = vm.get_global("result");
    CHECK(r.type == VAL_INT && r.as.integer == 30, "expected 30");

    TEST("native_add(100, -50) == 50");
    run_source(vm, "result = native_add(100, -50)");
    r = vm.get_global("result");
    CHECK(r.type == VAL_INT && r.as.integer == 50, "expected 50");

    TEST("native_concat('hello', ' world')");
    run_source(vm, "result = native_concat('hello', ' world')");
    r = vm.get_global("result");
    CHECK(is_string(r) && strcmp(as_string(r)->chars, "hello world") == 0,
          "expected 'hello world'");
}

/* =========================================================
** TEST 1b: C++ -> native boundary hardening
** ========================================================= */
static int native_zero_result(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    args[0] = val_int(42);
    return 1;
}

static int g_native_method_calls = 0;
static int g_cpp_native_was_paused = 0;

static int native_sum_many(VM *vm, Value *args, int nargs)
{
    (void)vm;
    if (!is_instance(args[-1]))
        return -1;
    g_native_method_calls++;
    int64_t total = 0;
    for (int i = 0; i < nargs; i++)
        total += args[i].as.integer;
    args[0] = val_int(total);
    return 1;
}

static int native_gc_safe_probe(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    g_cpp_native_was_paused = vm->get_gc().pause_depth > 0;
    args[0] = val_bool(g_cpp_native_was_paused != 0);
    return 1;
}

static void test_cpp_native_boundary()
{
    printf("\n[Test 1b] C++ -> native call boundary\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_native("zero_result", native_zero_result, 0);
    vm.def_native("gc_safe_probe", native_gc_safe_probe, 0, ZEN_NATIVE_GC_SAFE);

    TEST("zero-arg native returns safely through call_global");
    Value r = vm.call_global("zero_result", nullptr, 0);
    CHECK(!vm.had_error() && is_int(r) && r.as.integer == 42, "expected 42");

    TEST("GC_SAFE native is paused through C++ call_global");
    g_cpp_native_was_paused = 0;
    r = vm.call_global("gc_safe_probe", nullptr, 0);
    CHECK(!vm.had_error() && is_bool(r) && r.as.boolean && g_cpp_native_was_paused,
          "C++ argument storage was not protected from GC");

    ObjClass *klass = vm.def_class("ManyArgs")
        .method("sum_many", native_sum_many, -1)
        .end();
    Value instance = vm.make_instance(klass);
    Value args[20];
    for (int i = 0; i < 20; i++)
        args[i] = val_int(i + 1);

    TEST("native method receives self at args[-1] and all 20 arguments");
    g_native_method_calls = 0;
    r = vm.invoke(instance, "sum_many", args, 20);
    CHECK(!vm.had_error() && is_int(r) && r.as.integer == 210 && g_native_method_calls == 1,
          "receiver convention or arguments beyond 16 were lost");

    TEST("invalid C++ native arguments report an error, not a crash");
    r = vm.invoke(instance, "sum_many", nullptr, 1);
    CHECK(vm.had_error() && is_nil(r) && g_native_method_calls == 1,
          "invalid argument window reached the native");

    TEST("invalid C++ receiver reports an error, not a cast");
    r = vm.invoke(val_int(1), "sum_many", nullptr, 0);
    CHECK(vm.had_error() && is_nil(r), "non-instance receiver was accepted");

    TEST("out-of-range C++ global index reports an error");
    r = vm.call_global(vm.num_globals() + 1, nullptr, 0);
    CHECK(vm.had_error() && is_nil(r), "out-of-range global index was accepted");
}

/* =========================================================
** TEST 2: Script defines function, C++ calls it
** ========================================================= */
static void test_call_script_from_cpp()
{
    printf("\n[Test 2] C++ calls script-defined function\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    run_source(vm, R"(
def square(x):
    return x * x

def greet(name):
    return "Hi " + name

def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

def first_plus_rest(first, *rest):
    return first + len(rest)
)");

    TEST("call_global('square', 7) == 49");
    Value args[1] = { val_int(7) };
    Value result = vm.call_global("square", args, 1);
    CHECK(result.type == VAL_INT && result.as.integer == 49, "expected 49");

    TEST("call_global('square', 12) == 144");
    args[0] = val_int(12);
    result = vm.call_global("square", args, 1);
    CHECK(result.type == VAL_INT && result.as.integer == 144, "expected 144");

    TEST("call_global('greet', 'Zen')");
    ObjString *name = vm.make_string("Zen");
    args[0] = val_obj((Obj *)name);
    result = vm.call_global("greet", args, 1);
    CHECK(is_string(result) && strcmp(as_string(result)->chars, "Hi Zen") == 0,
          "expected 'Hi Zen'");

    TEST("call_global('fib', 10) == 55");
    args[0] = val_int(10);
    result = vm.call_global("fib", args, 1);
    CHECK(result.type == VAL_INT && result.as.integer == 55, "expected 55");

    TEST("call_global packs *args like a script call");
    Value varargs[4] = { val_int(10), val_int(1), val_int(2), val_int(3) };
    result = vm.call_global("first_plus_rest", varargs, 4);
    CHECK(!vm.had_error() && result.type == VAL_INT && result.as.integer == 13,
          "expected first + 3 packed args");

    TEST("call_global validates script arity before entering the VM");
    result = vm.call_global("square", nullptr, 0);
    CHECK(vm.had_error() && is_nil(result), "wrong arity was accepted");
}

/* =========================================================
** TEST 3: Bidirectional — native calls back into script
** ========================================================= */
static int native_apply_twice(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    /* args[0] = callable, args[1] = value
    ** Returns callable(callable(value)) */
    Value fn = args[0];
    Value val = args[1];

    Value call_args[1] = { val };
    Value r1 = vm->call_fn(fn, call_args, 1);
    call_args[0] = r1;
    Value r2 = vm->call_fn(fn, call_args, 1);
    args[0] = r2;
    return 1;
}

static void test_bidirectional_callback()
{
    printf("\n[Test 3] Bidirectional: native calls back into script\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_native("apply_twice", native_apply_twice, 2);

    TEST("apply_twice(double, 3) == 12");
    run_source(vm, R"(
def double(x):
    return x * 2

result = apply_twice(double, 3)
)");
    Value r = vm.get_global("result");
    CHECK(r.type == VAL_INT && r.as.integer == 12, "expected 12");

    TEST("apply_twice(inc, 10) == 12");
    run_source(vm, R"(
def inc(x):
    return x + 1

result = apply_twice(inc, 10)
)");
    r = vm.get_global("result");
    CHECK(r.type == VAL_INT && r.as.integer == 12, "expected 12");

    TEST("C++ -> native -> Zen callback has no synthetic caller frame");
    Value callback_args[2] = { vm.get_global("double"), val_int(5) };
    r = vm.call_global("apply_twice", callback_args, 2);
    CHECK(!vm.had_error() && r.type == VAL_INT && r.as.integer == 20,
          "direct native callback did not return 20");
}

/* =========================================================
** TEST 4: Shared globals between C++ and script
** ========================================================= */
static void test_shared_globals()
{
    printf("\n[Test 4] Shared globals between C++ and script\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    /* C++ sets globals */
    vm.def_global("width", val_int(800));
    vm.def_global("height", val_int(600));
    ObjString *title = vm.make_string("My Game");
    vm.def_global("title", val_obj((Obj *)title));

    TEST("Script reads C++ globals (area)");
    run_source(vm, "area = width * height");
    Value area = vm.get_global("area");
    CHECK(area.type == VAL_INT && area.as.integer == 480000, "expected 480000");

    TEST("Script reads C++ string global");
    run_source(vm, "result = title");
    Value t = vm.get_global("result");
    CHECK(is_string(t) && strcmp(as_string(t)->chars, "My Game") == 0,
          "expected 'My Game'");

    TEST("Script modifies C++ global");
    run_source(vm, "width = 1920");
    Value w = vm.get_global("width");
    CHECK(w.type == VAL_INT && w.as.integer == 1920, "expected 1920");
}

/* =========================================================
** TEST 5: C++ invokes script method on instance
** ========================================================= */
static void test_invoke_method()
{
    printf("\n[Test 5] C++ invokes script method on instance\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    run_source(vm, R"(
class Counter:
    def __init__(self, start=0):
        self.value = start

    def increment(self):
        self.value = self.value + 1
        return self.value

    def add(self, n):
        self.value = self.value + n
        return self.value

c = Counter(10)
)");

    TEST("invoke(c, 'increment') == 11");
    Value c = vm.get_global("c");
    Value result = vm.invoke(c, "increment", nullptr, 0);
    CHECK(result.type == VAL_INT && result.as.integer == 11, "expected 11");

    TEST("invoke(c, 'add', 5) == 16");
    Value add_args[1] = { val_int(5) };
    result = vm.invoke(c, "add", add_args, 1);
    CHECK(result.type == VAL_INT && result.as.integer == 16, "expected 16");

    TEST("invoke(c, 'increment') == 17 (state persists)");
    result = vm.invoke(c, "increment", nullptr, 0);
    CHECK(result.type == VAL_INT && result.as.integer == 17, "expected 17");
}

/* =========================================================
** TEST 6: Multiple run_source calls share state
** ========================================================= */
static void test_incremental_execution()
{
    printf("\n[Test 6] Incremental execution (multiple run_source)\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    TEST("Sequential scripts share globals");
    run_source(vm, "x = 100");
    run_source(vm, "y = x * 2");
    run_source(vm, "z = x + y");
    Value z = vm.get_global("z");
    CHECK(z.type == VAL_INT && z.as.integer == 300, "expected 300");

    TEST("Class defined in one run, used in another");
    run_source(vm, R"(
class Vec2:
    def __init__(self, x, y):
        self.x = x
        self.y = y
    def length_sq(self):
        return self.x * self.x + self.y * self.y
)");
    run_source(vm, "v = Vec2(3, 4)");
    run_source(vm, "result = v.length_sq()");
    Value r = vm.get_global("result");
    CHECK(r.type == VAL_INT && r.as.integer == 25, "expected 25");
}

/* =========================================================
** TEST 7: Native struct (C++ pointer in script)
** ========================================================= */
struct Vec3 {
    float x, y, z;
};

static int vec3_new(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    Vec3 *v = (Vec3 *)malloc(sizeof(Vec3));
    v->x = (float)args[0].as.integer;
    v->y = (float)args[1].as.integer;
    v->z = (float)args[2].as.integer;
    args[0] = val_ptr(v);
    return 1;
}

static int vec3_dot(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    Vec3 *a = (Vec3 *)as_ptr(args[0]);
    Vec3 *b = (Vec3 *)as_ptr(args[1]);
    float dot = a->x * b->x + a->y * b->y + a->z * b->z;
    args[0] = val_float((double)dot);
    return 1;
}

static int vec3_free_fn(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    free(as_ptr(args[0]));
    return 0;
}

static void test_native_struct()
{
    printf("\n[Test 7] Native struct (C++ data in script)\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    vm.def_native("vec3_new", vec3_new, 3);
    vm.def_native("vec3_dot", vec3_dot, 2);
    vm.def_native("vec3_free", vec3_free_fn, 1);

    TEST("vec3 dot product from script");
    run_source(vm, R"(
a = vec3_new(1, 2, 3)
b = vec3_new(4, 5, 6)
result = vec3_dot(a, b)
vec3_free(a)
vec3_free(b)
)");
    Value r = vm.get_global("result");
    /* 1*4 + 2*5 + 3*6 = 4+10+18 = 32 */
    CHECK(r.type == VAL_FLOAT && (int)r.as.number == 32, "expected 32.0");
}

/* =========================================================
** TEST 8: Script class inherits from C++ class
** ========================================================= */

/* Native "Entity" base class:
**   - fields: x, y
**   - methods: move(dx, dy), pos_str()
*/
static int entity_init(VM *vm, Value *args, int nargs)
{
    (void)vm;
    /* ClassBuilder convention: args[-1]=self, args[0..n-1]=arguments */
    ObjInstance *self = as_instance(args[-1]);
    self->fields[0] = (nargs > 0) ? args[0] : val_int(0); /* x */
    self->fields[1] = (nargs > 1) ? args[1] : val_int(0); /* y */
    return 0;
}

static int entity_move(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    /* ClassBuilder convention: args[-1]=self, args[0..n-1]=arguments */
    ObjInstance *self = as_instance(args[-1]);
    self->fields[0] = val_int(self->fields[0].as.integer + args[0].as.integer);
    self->fields[1] = val_int(self->fields[1].as.integer + args[1].as.integer);
    return 0;
}

static int entity_pos_str(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    /* ClassBuilder convention: args[-1]=self */
    ObjInstance *self = as_instance(args[-1]);
    char buf[64];
    snprintf(buf, sizeof(buf), "(%ld, %ld)",
             (long)self->fields[0].as.integer,
             (long)self->fields[1].as.integer);
    ObjString *s = intern_string(&vm->get_gc(), buf, (int)strlen(buf),
                                 hash_string(buf, (int)strlen(buf)));
    args[0] = val_obj((Obj *)s);
    return 1;
}

static void test_script_inherits_cpp_class()
{
    printf("\n[Test 8] Script class inherits from C++ class\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    /* Define C++ base class "Entity" */
    vm.def_class("Entity")
        .field("x")
        .field("y")
        .method("__init__", entity_init, 3)
        .method("move", entity_move, 3)
        .method("pos_str", entity_pos_str, 1)
        .end();

    /* Script defines a class that inherits Entity */
    TEST("Script child uses C++ parent method");
    run_source(vm, R"(
class Player(Entity):
    def __init__(self, x, y, name):
        super().__init__(x, y)
        self.name = name

    def greet(self):
        return "I am " + self.name + " at " + self.pos_str()

p = Player(10, 20, "Hero")
result = p.greet()
)");
    Value r = vm.get_global("result");
    CHECK(is_string(r) && strcmp(as_string(r)->chars, "I am Hero at (10, 20)") == 0,
          "expected 'I am Hero at (10, 20)'");

    TEST("Script child calls C++ parent move()");
    run_source(vm, R"(
p.move(5, -3)
result = p.pos_str()
)");
    r = vm.get_global("result");
    CHECK(is_string(r) && strcmp(as_string(r)->chars, "(15, 17)") == 0,
          "expected '(15, 17)'");

    TEST("Script overrides C++ parent method");
    run_source(vm, R"(
class Enemy(Entity):
    def __init__(self, x, y, hp):
        super().__init__(x, y)
        self.hp = hp

    def pos_str(self):
        return "Enemy[hp=" + str(self.hp) + "]"

e = Enemy(0, 0, 100)
result = e.pos_str()
)");
    r = vm.get_global("result");
    CHECK(is_string(r) && strcmp(as_string(r)->chars, "Enemy[hp=100]") == 0,
          "expected 'Enemy[hp=100]'");

    TEST("C++ parent move() still works on overridden child");
    run_source(vm, R"(
e.move(1, 2)
e.move(3, 4)
result_x = e.x
result_y = e.y
)");
    Value rx = vm.get_global("result_x");
    Value ry = vm.get_global("result_y");
    CHECK(rx.type == VAL_INT && rx.as.integer == 4 &&
          ry.type == VAL_INT && ry.as.integer == 6,
          "expected x=4, y=6");

    TEST("Multiple inheritance levels (C++ -> Script -> Script)");
    run_source(vm, R"(
class Boss(Enemy):
    def __init__(self, x, y, hp, phase):
        super().__init__(x, y, hp)
        self.phase = phase

    def info(self):
        return "Boss phase=" + str(self.phase) + " hp=" + str(self.hp)

b = Boss(0, 0, 999, 2)
b.move(50, 50)
result = b.info()
result_pos = str(b.x) + "," + str(b.y)
)");
    r = vm.get_global("result");
    CHECK(is_string(r) && strcmp(as_string(r)->chars, "Boss phase=2 hp=999") == 0,
          "expected 'Boss phase=2 hp=999'");
    Value rp = vm.get_global("result_pos");
    CHECK(is_string(rp) && strcmp(as_string(rp)->chars, "50,50") == 0,
          "expected '50,50'");
}

/* =========================================================
** TEST 9: NativeStructBuilder — zero-copy C++ struct binding
** ========================================================= */

struct HostVec2f {
    float x, y;
};

struct Color4 {
    uint8_t r, g, b, a;
};

static void vec2f_ctor(VM *vm, void *buffer, int argc, Value *args)
{
    (void)vm;
    HostVec2f *v = (HostVec2f *)buffer;
    v->x = (argc > 0) ? (float)(args[0].type == VAL_FLOAT ? args[0].as.number : (double)args[0].as.integer) : 0.0f;
    v->y = (argc > 1) ? (float)(args[1].type == VAL_FLOAT ? args[1].as.number : (double)args[1].as.integer) : 0.0f;
}

static void color4_ctor(VM *vm, void *buffer, int argc, Value *args)
{
    (void)vm;
    Color4 *c = (Color4 *)buffer;
    c->r = (argc > 0) ? (uint8_t)args[0].as.integer : 0;
    c->g = (argc > 1) ? (uint8_t)args[1].as.integer : 0;
    c->b = (argc > 2) ? (uint8_t)args[2].as.integer : 0;
    c->a = (argc > 3) ? (uint8_t)args[3].as.integer : 255;
}

static void test_native_struct_builder()
{
    printf("\n[Test 9] NativeStructBuilder (zero-copy C++ struct binding)\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    /* Register HostVec2f — maps directly to C++ struct with float x, y */
    vm.register_native_struct("Vec2", sizeof(HostVec2f), vec2f_ctor)
        .f32("x", offsetof(HostVec2f, x))
        .f32("y", offsetof(HostVec2f, y))
        .end();

    /* Register Color4 — maps to C++ struct with uint8 r,g,b,a */
    vm.register_native_struct("Color", sizeof(Color4), color4_ctor)
        .byte("r", offsetof(Color4, r))
        .byte("g", offsetof(Color4, g))
        .byte("b", offsetof(Color4, b))
        .byte("a", offsetof(Color4, a))
        .end();

    TEST("Script creates native struct, reads C++ fields");
    run_source(vm, R"(
v = Vec2(10.5, 20.3)
rx = v.x
ry = v.y
)");
    Value rx = vm.get_global("rx");
    Value ry = vm.get_global("ry");
    CHECK(rx.type == VAL_FLOAT && rx.as.number > 10.4 && rx.as.number < 10.6 &&
          ry.type == VAL_FLOAT && ry.as.number > 20.2 && ry.as.number < 20.4,
          "expected x~10.5, y~20.3");

    TEST("Script modifies native struct fields");
    run_source(vm, R"(
v.x = 100.0
v.y = 200.0
rx = v.x
ry = v.y
)");
    rx = vm.get_global("rx");
    ry = vm.get_global("ry");
    CHECK(rx.type == VAL_FLOAT && (int)rx.as.number == 100 &&
          ry.type == VAL_FLOAT && (int)ry.as.number == 200,
          "expected x=100, y=200");

    TEST("Color struct with byte fields");
    run_source(vm, R"(
c = Color(255, 128, 0, 200)
cr = c.r
cg = c.g
cb = c.b
ca = c.a
)");
    CHECK(vm.get_global("cr").as.integer == 255, "r=255");
    CHECK(vm.get_global("cg").as.integer == 128, "g=128");
    CHECK(vm.get_global("cb").as.integer == 0,   "b=0");
    CHECK(vm.get_global("ca").as.integer == 200, "a=200");

    TEST("Multiple native structs coexist");
    run_source(vm, R"(
p1 = Vec2(1.0, 2.0)
p2 = Vec2(3.0, 4.0)
c = Color(0, 0, 0, 255)
result = p1.x + p2.y + c.a
)");
    Value r = vm.get_global("result");
    /* 1.0 + 4.0 + 255 = 260.0 (float because Vec2 fields are float) */
    CHECK(r.type == VAL_FLOAT && (int)r.as.number == 260,
          "expected 1+4+255=260");

    TEST("Native struct passed as function argument");
    run_source(vm, R"(
def vec2_length_sq(v):
    return v.x * v.x + v.y * v.y

result = vec2_length_sq(Vec2(3.0, 4.0))
)");
    r = vm.get_global("result");
    CHECK(r.type == VAL_FLOAT && (int)r.as.number == 25, "expected 25");

    TEST("C++ creates native struct via make_native_struct");
    {
        /* Find the NativeStructDef by global name */
        Value def_val = vm.get_global("Vec2");
        NativeStructDef *def = as_native_struct_def(def_val);
        Value args[2] = { val_float(7.0), val_float(8.0) };
        Value ns_val = vm.make_native_struct(def, args, 2);
        vm.def_global("cpp_vec", ns_val);
        run_source(vm, R"(
result = cpp_vec.x + cpp_vec.y
)");
        r = vm.get_global("result");
        CHECK(r.type == VAL_FLOAT && (int)r.as.number == 15, "expected 7+8=15");
    }
}

/* =========================================================
** TEST 10: ClassBuilder with native ctor/dtor + native_data
** ========================================================= */
struct SpriteData {
    float x, y;
    int width, height;
    bool visible;
};

static int g_sprites_alive = 0;

static void *sprite_ctor(VM *vm, int argc, Value *args)
{
    (void)vm;
    SpriteData *s = (SpriteData *)malloc(sizeof(SpriteData));
    s->x = (argc > 0) ? (float)args[0].as.number : 0.0f;
    s->y = (argc > 1) ? (float)args[1].as.number : 0.0f;
    s->width = (argc > 2) ? (int)args[2].as.integer : 32;
    s->height = (argc > 3) ? (int)args[3].as.integer : 32;
    s->visible = true;
    g_sprites_alive++;
    return s;
}

static void sprite_dtor(VM *vm, void *data)
{
    (void)vm;
    free(data);
    g_sprites_alive--;
}

static int sprite_move(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    SpriteData *s = zen_instance_data<SpriteData>(args[-1]);
    s->x += (float)args[0].as.number;
    s->y += (float)args[1].as.number;
    return 0;
}

static int sprite_get_x(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    SpriteData *s = zen_instance_data<SpriteData>(args[-1]);
    args[0] = val_float((double)s->x);
    return 1;
}

static int sprite_get_y(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    SpriteData *s = zen_instance_data<SpriteData>(args[-1]);
    args[0] = val_float((double)s->y);
    return 1;
}

static int sprite_set_visible(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    SpriteData *s = zen_instance_data<SpriteData>(args[-1]);
    s->visible = is_truthy(args[0]);
    return 0;
}

static int sprite_is_visible(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    SpriteData *s = zen_instance_data<SpriteData>(args[-1]);
    args[0] = val_bool(s->visible);
    return 1;
}

static int sprite_info(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    SpriteData *s = zen_instance_data<SpriteData>(args[-1]);
    char buf[128];
    snprintf(buf, sizeof(buf), "Sprite(%.0f,%.0f,%dx%d,%s)",
             s->x, s->y, s->width, s->height,
             s->visible ? "visible" : "hidden");
    ObjString *str = intern_string(&vm->get_gc(), buf, (int)strlen(buf),
                                   hash_string(buf, (int)strlen(buf)));
    args[0] = val_obj((Obj *)str);
    return 1;
}

static void test_class_builder_native_data()
{
    printf("\n[Test 10] ClassBuilder with native ctor/dtor + native_data\n");

    g_sprites_alive = 0;

    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);

        vm.def_class("Sprite")
            .ctor(sprite_ctor)
            .dtor(sprite_dtor)
            .method("move", sprite_move, 3)
            .method("get_x", sprite_get_x, 1)
            .method("get_y", sprite_get_y, 1)
            .method("set_visible", sprite_set_visible, 2)
            .method("is_visible", sprite_is_visible, 1)
            .method("info", sprite_info, 1)
            .end();

        TEST("Native ctor allocates data");
        run_source(vm, R"(
s = Sprite(10.0, 20.0, 64, 64)
result = s.info()
)");
        Value r = vm.get_global("result");
        CHECK(is_string(r) && strcmp(as_string(r)->chars, "Sprite(10,20,64x64,visible)") == 0,
              "expected 'Sprite(10,20,64x64,visible)'");

        TEST("Native methods modify C++ data");
        run_source(vm, R"(
s.move(5.0, -3.0)
rx = s.get_x()
ry = s.get_y()
)");
        Value rxv = vm.get_global("rx");
        Value ryv = vm.get_global("ry");
        CHECK(rxv.type == VAL_FLOAT && (int)rxv.as.number == 15 &&
              ryv.type == VAL_FLOAT && (int)ryv.as.number == 17,
              "expected x=15, y=17");

        TEST("Boolean round-trip through native data");
        run_source(vm, R"(
s.set_visible(False)
vis1 = s.is_visible()
s.set_visible(True)
vis2 = s.is_visible()
)");
        Value v1 = vm.get_global("vis1");
        Value v2 = vm.get_global("vis2");
        CHECK(v1.type == VAL_BOOL && !v1.as.boolean &&
              v2.type == VAL_BOOL && v2.as.boolean,
              "expected false then true");

        TEST("Sprites alive during VM lifetime");
        CHECK(g_sprites_alive == 1, "expected 1 sprite alive");

    } /* VM destructor runs, GC frees all instances → dtor called */

    TEST("Native dtor called on GC cleanup");
    CHECK(g_sprites_alive == 0, "expected 0 sprites after VM destroyed");
}

/* =========================================================
** TEST 11: run() from inside a running script
**
** What an instantiate(prefab) does: a native compiles and runs a script of
** its own while the caller's script is still on the stack. run() used to
** rewind the main fiber unconditionally, which threw away the caller's
** frames and left its registers pointing at reused stack.
** ========================================================= */
static int native_spawn(VM *vm, Value *args, int nargs)
{
    if (nargs < 1 || !is_string(args[0]))
    {
        args[0] = val_bool(false);
        return 1;
    }

    const char *source = safe_string_chars(args[0]);
    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm->get_gc(), vm, source, "<spawned>");
    if (!fn)
    {
        args[0] = val_bool(false);
        return 1;
    }
    vm->run(fn);
    args[0] = val_bool(true);
    return 1;
}

static void test_run_from_inside_script()
{
    printf("\n[Test 11] run() from inside a running script\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_native("spawn", native_spawn, 1);

    /* The locals must be live across the native call, and the call has to
    ** happen from inside a function so there are frames to lose. */
    const bool ok = run_source(vm,
        "def outer(x, y):\n"
        "    p = x + y\n"
        "    q = p * 2\n"
        "    done = spawn(\"spawned_value = 99\\n\")\n"
        "    return q + p + x + y\n"
        "result = outer(10, 20)\n"
        "spawned_ok = spawn(\"top_level_value = 7\\n\")\n");

    TEST("outer() compiled and ran");
    CHECK(ok && !vm.had_error(), "outer() did not run cleanly");

    TEST("caller's locals survive the nested run");
    Value r = vm.get_global("result");
    CHECK(r.type == VAL_INT && r.as.integer == 120, "expected 120 (60+30+10+20)");

    TEST("the native's return value reaches the caller");
    Value done = vm.get_global("spawned_ok");
    CHECK(done.type == VAL_BOOL && done.as.boolean, "expected spawn() to report true");

    TEST("the spawned script ran");
    Value spawned = vm.get_global("spawned_value");
    CHECK(spawned.type == VAL_INT && spawned.as.integer == 99, "expected spawned_value == 99");

    /* Twice over, and nested one level deeper: the spawned script spawns. */
    TEST("nested spawn, two levels deep");
    const bool deep = run_source(vm,
        "def middle(n):\n"
        "    a = n * 3\n"
        "    spawn(\"def inner(k):\\n    return k + 1\\ninner_value = inner(41)\\n\")\n"
        "    return a + 1\n"
        "deep_result = middle(7)\n");
    Value d = vm.get_global("deep_result");
    Value inner = vm.get_global("inner_value");
    CHECK(deep && !vm.had_error() && d.type == VAL_INT && d.as.integer == 22 &&
          inner.type == VAL_INT && inner.as.integer == 42,
          "expected deep_result == 22 and inner_value == 42");
}

/* =========================================================
** TEST 12: VM::resume_fiber() — the embedding API a host uses to drive a
** fiber directly (a coroutine stepped once per frame, say), as opposed to
** the in-script `for`/`await` paths which already checked this.
** ========================================================= */
static void test_resume_fiber_error_handling()
{
    printf("\n[Test 12] resume_fiber() reports fiber errors instead of swallowing them\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    const bool compiled = run_source(vm,
        "def gen():\n"
        "    yield 1\n"
        "    error(\"boom\")\n"
        "    yield 2\n"
        "g = gen()\n");

    TEST("generator call produced a suspended fiber");
    Value gv = vm.get_global("g");
    CHECK(compiled && !vm.had_error() && is_fiber(gv), "expected g to be a fiber");

    ObjFiber *fiber = as_fiber(gv);
    ObjFiber *caller_before = vm.current_fiber();

    TEST("first resume yields 1 with no error");
    Value r1 = vm.resume_fiber(fiber, val_nil());
    CHECK(!vm.had_error() && r1.type == VAL_INT && r1.as.integer == 1,
          "expected 1 with had_error() == false");

    TEST("resume_fiber() restores the caller as current_fiber() afterwards");
    CHECK(vm.current_fiber() == caller_before,
          "current_fiber() was left pointing at the target fiber");

    TEST("second resume hits error() inside the fiber");
    Value r2 = vm.resume_fiber(fiber, val_nil());
    CHECK(vm.had_error(), "expected had_error() == true, error() did not propagate");

    TEST("a fiber error does not hand back a stale transfer_value");
    CHECK(is_nil(r2), "expected nil, not the previous yield's stale value");

    TEST("current_fiber() is still restored after an erroring resume");
    CHECK(vm.current_fiber() == caller_before,
          "current_fiber() was left pointing at the errored fiber");

    TEST("the VM keeps working for a fresh run after an errored fiber");
    const bool ran_after = run_source(vm, "post_check = 123\n");
    Value pc = vm.get_global("post_check");
    CHECK(ran_after && !vm.had_error() && pc.type == VAL_INT && pc.as.integer == 123,
          "expected the VM to run cleanly after an errored fiber");
}

/* =========================================================
** TEST 13: ClassBuilder::generic_method() — native reified generics
**
** The motivating use case: entity.get_component<Transform>(), where
** Entity/Transform are native C++ classes (ClassBuilder), not script
** classes. Container stores at most one component per native_data slot
** keyed by ObjClass* (a tiny fixed table is enough for the test); the
** generic method reads its single type argument to pick which slot.
** ========================================================= */
static const int kMaxComponentSlots = 4;
struct ContainerData {
    ObjClass *slot_class[kMaxComponentSlots];
    Value slot_value[kMaxComponentSlots];
    int count;
};

static void *container_ctor(VM *vm, int argc, Value *args)
{
    (void)vm; (void)argc; (void)args;
    ContainerData *c = (ContainerData *)malloc(sizeof(ContainerData));
    c->count = 0;
    return c;
}

static void container_dtor(VM *vm, void *data)
{
    (void)vm;
    free(data);
}

/* container.add_component(instance) — files it under instance's own class. */
static int container_add_component(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    ContainerData *c = zen_instance_data<ContainerData>(args[-1]);
    Value comp = args[0];
    if (!is_instance(comp) || c->count >= kMaxComponentSlots)
    {
        return 0;
    }
    ObjClass *klass = as_instance(comp)->klass;
    for (int i = 0; i < c->count; i++)
    {
        if (c->slot_class[i] == klass)
        {
            c->slot_value[i] = comp; /* replace */
            return 0;
        }
    }
    c->slot_class[c->count] = klass;
    c->slot_value[c->count] = comp;
    c->count++;
    return 0;
}

/* container.get_component<T>() — the reified-generics native entry point. */
static int container_get_component(VM *vm, Value receiver, Value *type_args, int ntype_args,
                                    Value *args, int nargs)
{
    (void)vm; (void)args; (void)nargs;
    if (ntype_args != 1 || !is_class(type_args[0]))
        return 0; /* the VM already validates this before calling us — belt and suspenders */
    ObjClass *wanted = as_class(type_args[0]);
    ContainerData *c = zen_instance_data<ContainerData>(receiver);
    for (int i = 0; i < c->count; i++)
    {
        if (c->slot_class[i] == wanted)
        {
            args[0] = c->slot_value[i];
            return 1;
        }
    }
    args[0] = val_nil();
    return 1;
}

/* Regression: a GenericNativeFn signaling failure (nret < 0, same
** convention as plain NativeFn) must surface as a runtime error, not be
** silently swallowed into a nil result. */
static int container_always_fails(VM *vm, Value receiver, Value *type_args, int ntype_args,
                                   Value *args, int nargs)
{
    (void)vm; (void)receiver; (void)type_args; (void)ntype_args; (void)args; (void)nargs;
    return -1;
}

static void test_generic_native_method()
{
    printf("\n[Test 13] ClassBuilder::generic_method() — entity.get_component<T>()\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    vm.def_class("Container")
        .ctor(container_ctor)
        .dtor(container_dtor)
        .method("add_component", container_add_component, 1)
        .generic_method("get_component", container_get_component, /*generic_arity=*/1, /*arity=*/0)
        .generic_method("always_fails", container_always_fails, /*generic_arity=*/1, /*arity=*/0)
        .end();

    /* Note: `c: Container = Container()` — the explicit class annotation is
    ** what lets the compiler know `c`'s static type at a call site with no
    ** other source of type information (Container is a native class, so
    ** there's no script `def` for the pre-scan to have seen). Without the
    ** annotation, obj.method<T>(...) can never be told apart from chained
    ** comparisons written without spaces — see generic_call_ahead(). */
    TEST("Two different components on the same container round-trip by type");
    run_source(vm, R"(
class Transform:
    def __init__(self, x, y):
        self.x = x
        self.y = y

class Sprite:
    def __init__(self, path):
        self.path = path

c: Container = Container()
# Container keeps its components in plain C++ memory, which the collector
# cannot see: keep them reachable from script globals too, or a collection
# (now possible on any call or loop iteration) frees them under the native.
keep_t = Transform(1, 2)
keep_s = Sprite("hero.png")
c.add_component(keep_t)
c.add_component(keep_s)

t = c.get_component<Transform>()
s = c.get_component<Sprite>()
)");
    Value tv = vm.get_global("t");
    Value sv = vm.get_global("s");
    CHECK(!vm.had_error() && is_instance(tv) && is_instance(sv) && tv.as.obj != sv.as.obj,
          "expected two distinct component instances, no error");

    /* Note: a class-type annotation (`c: Container`) is tracked per
    ** Compiler instance, not persisted anywhere — each run_source() call
    ** here uses a fresh Compiler, so `c`'s annotation from the previous
    ** run_source() call above is gone. Re-annotating in the same script as
    ** the call is the documented way to use this across a fresh
    ** compilation (matches how any other compile-time-only fact in this
    ** compiler works: signatures, class field tables, etc. are also
    ** per-compilation, not persisted across separate compile() calls). */
    TEST("get_component<T>() with no matching component returns nil");
    run_source(vm, R"(
class Nothing:
    pass
c: Container = c
missing = c.get_component<Nothing>()
)");
    Value mv = vm.get_global("missing");
    CHECK(!vm.had_error() && is_nil(mv), "expected nil for a component that was never added");

    TEST("get_component<T>() called with wrong generic arity is a compile error");
    bool bad_arity_compiled = run_source(vm, "bad = c.get_component<Transform, Sprite>()\n");
    CHECK(!bad_arity_compiled || vm.had_error(), "expected a compile/runtime error, not silent success");

    TEST("add_component (plain native method) is unaffected by generics support");
    run_source(vm, R"(
c2: Container = Container()
keep_t2 = Transform(9, 9)
c2.add_component(keep_t2)
t2 = c2.get_component<Transform>()
)");
    Value t2v = vm.get_global("t2");
    CHECK(!vm.had_error() && is_instance(t2v), "expected plain native method + generic method to coexist");

    /* Regression: a GenericNativeFn returning -1 (the established
    ** error-signal convention) must raise a runtime error, not be silently
    ** swallowed into a nil result — see OP_INVOKE_GENERIC's native branch. */
    TEST("A generic native method returning -1 raises a runtime error, not nil");
    run_source(vm, R"(
c3: Container = Container()
oops = c3.always_fails<Transform>()
)");
    CHECK(vm.had_error(), "expected had_error() == true when the native signals failure");

    /* Regression: receiver_static_class() must not misattribute a closure's
    ** captured UPVALUE to an unrelated GLOBAL of the same name that happens
    ** to carry its own class annotation. Container is a native class, so
    ** this can only be exercised through a native generic method — a
    ** script-only equivalent doesn't reach the code path being tested
    ** (method_signature() already resolves script classes without falling
    ** back to the global-hint table at all). */
    TEST("A closure's captured local isn't misattributed to an unrelated global's type hint");
    run_source(vm, R"(
def outer():
    c: Container = Container()
    keep = Transform(1, 2)  # the container's C++ slots are not GC roots
    c.add_component(keep)
    def inner():
        return c.get_component<Transform>()
    return inner()

class Sprite:
    pass
c: Sprite = None

upvalue_result = outer()
)");
    Value uv = vm.get_global("upvalue_result");
    CHECK(!vm.had_error() && is_instance(uv), "expected the captured Container's component, not misresolution via the unrelated global 'c: Sprite'");
}

/* =========================================================
** TEST 14: Script classes are closed after their declaration
** ========================================================= */
static void test_closed_script_class()
{
    printf("\n[Test 14] Closed script classes\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);

    TEST("runtime method replacement is rejected after class declaration");
    bool compiled = run_source(vm, R"(
class Locked:
    def value(self):
        return 1

def replacement(x):
    return 2

Locked.value = replacement
)");
    CHECK(compiled && vm.had_error(), "expected a closed-class runtime error");
}

/* =========================================================
** TEST 15: Native classes get the typed fast paths
** ========================================================= */
static int ent_init(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    ObjInstance *self = as_instance(args[-1]);
    self->fields[0] = val_int(0);
    self->fields[1] = val_int(0);
    return 0;
}

static int ent_move(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    ObjInstance *self = as_instance(args[-1]);
    self->fields[0] = val_int(self->fields[0].as.integer + args[0].as.integer);
    self->fields[1] = val_int(self->fields[1].as.integer + args[1].as.integer);
    return 0;
}

static void test_native_class_inference()
{
    printf("\n[Test 15] Native class inference (IDXC / INVOKE_VT)\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_class("Ent")
        .field("x")
        .field("y")
        .method("__init__", ent_init, 0)
        .method("move", ent_move, 2)
        .end();

    auto geti = [&](const char *name) -> long {
        Value v = vm.get_global(vm.find_global(name));
        return is_int(v) ? (long)v.as.integer : -1;
    };

    TEST("untyped local and module-level native instances");
    run_source(vm, R"(
def f():
    e = Ent()
    e.move(3, 4)
    return e.x + e.y

r1 = f()
g = Ent()
g.move(3, 4)
r1g = g.x + g.y
)");
    CHECK(!vm.had_error() && geti("r1") == 7 && geti("r1g") == 7, "expected 7 and 7");

    TEST("annotated local native instance");
    run_source(vm, R"(
def h():
    e2: Ent = Ent()
    e2.move(1, 2)
    return e2.x * 10 + e2.y

r2 = h()
)");
    CHECK(!vm.had_error() && geti("r2") == 12, "expected 12");

    TEST("script subclass of a native class keeps inherited fields");
    run_source(vm, R"(
class Sub(Ent):
    def extra(self):
        return self.x + 100

s = Sub()
s.move(5, 0)
r3 = s.extra() + s.y
)");
    CHECK(!vm.had_error() && geti("r3") == 105, "expected 105");

    TEST("field write through a typed native receiver");
    run_source(vm, R"(
def w():
    e = Ent()
    e.x = 9
    return e.x

r5 = w()
)");
    CHECK(!vm.had_error() && geti("r5") == 9, "expected 9");

    TEST("global rebound after compile still reads the right field");
    run_source(vm, R"(
class Other:
    def __init__(self):
        self.y = 1
        self.x = 2

def f4():
    e = Ent()
    return e.x

Ent = Other
r4 = f4()
)");
    CHECK(!vm.had_error() && geti("r4") == 2, "expected 2 (by-name fallback)");
}

/* =========================================================
** TEST 16: a native constructor that fails stops the construction
** ========================================================= */
static int g_bad_ctor_calls = 0;
static bool g_bad_ctor_paused = false;

static void *bad_ctor(VM *vm, int, Value *)
{
    g_bad_ctor_calls++;
    g_bad_ctor_paused = vm->get_gc().pause_depth > 0;
    vm->runtime_error("ctor failed");
    return nullptr;
}

static void test_failing_native_ctor()
{
    printf("\n[Test 16] failing native ctor is checked (script and C++)\n");

    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        vm.def_class("Bad").ctor(bad_ctor).end();

        TEST("script Bad(1): error reported, execution stops");
        g_bad_ctor_calls = 0;
        run_source(vm, "x = Bad(1)\nafter = 1\n");
        CHECK(vm.had_error() && g_bad_ctor_calls == 1 && is_nil(vm.get_global("after")) &&
                  is_nil(vm.get_global("x")),
              "VM continued after a failed native ctor");
        TEST("native ctor ran with the GC paused");
        CHECK(g_bad_ctor_paused, "GC was not paused around native_ctor");
    }

    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        vm.def_class("Bad").ctor(bad_ctor).end();
        run_source(vm, "class Sub(Bad):\n    def __init__(self, a):\n"
                       "        self.a = a\n"
                       "s = Sub(1)\nafter = 1\n");
        TEST("script subclass of Bad: error, no instance");
        CHECK(vm.had_error() && is_nil(vm.get_global("after")) && is_nil(vm.get_global("s")),
              "subclass construction continued after a failed native ctor");
    }

    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        ObjClass *bad = vm.def_class("Bad").ctor(bad_ctor).end();
        Value arg = val_int(1);

        TEST("make_instance(klass, args, n) reports the error, returns nil");
        g_bad_ctor_calls = 0;
        Value r = vm.make_instance(bad, &arg, 1);
        CHECK(vm.had_error() && is_nil(r) && g_bad_ctor_calls == 1,
              "make_instance ignored a failed native ctor");
    }

    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        ObjClass *bad = vm.def_class("Bad").ctor(bad_ctor).end();

        TEST("make_instance(klass) reports the error, returns nil");
        Value r = vm.make_instance(bad);
        CHECK(vm.had_error() && is_nil(r), "make_instance(klass) ignored a failed native ctor");
        TEST("GC is resumed after the failure");
        CHECK(vm.get_gc().pause_depth == 0, "gc_pause leaked on the error path");
    }
}

/* =========================================================
** TEST 17: bytecode round trip into a fresh VM
**
** Selector slots are baked into OP_INVOKE* operands and class vtables. The
** loader used to run every registered lib's init_fn (which interns native
** method names such as File.seek) BEFORE reading the file's selector table,
** so the first user method name landed on a slot the lib had already taken
** and the load failed with "selector mismatch at slot 87". Any script that
** called a method on a user class, and every script with an import, hit it.
** ========================================================= */
static void register_roundtrip_libs(VM &vm)
{
    vm.open_lib_globals(&zen_lib_base);
    vm.register_lib(&zen_lib_math);
    vm.register_lib(&zen_lib_time);
    vm.register_lib(&zen_lib_io);
    vm.register_lib(&zen_lib_json);
}

static void test_bytecode_roundtrip_fresh_vm()
{
    printf("\n[Test 17] bytecode round trip into a fresh VM (imports, builtin + user methods)\n");

    const char *path = "test_embedding_roundtrip.zbc";
    const char *source =
        "import math\n"
        "r = math.sqrt(16.0)\n"
        "xs = [3, 1, 2]\n"
        "xs.sort()\n"
        "t = xs[0]\n"
        "parts = \"a,b\".split(\",\")\n"
        "n = len(parts)\n"
        "class Counter:\n"
        "    def __init__(self):\n"
        "        self.v = 1\n"
        "    def bump(self, d):\n"
        "        self.v += d\n"
        "        return self.v\n"
        "c = Counter()\n"
        "u = c.bump(2)\n";

    char err[256] = {0};
    bool dumped = false;
    {
        VM vm;
        register_roundtrip_libs(vm);
        Compiler compiler;
        ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, "<dump>");
        dumped = fn && dump_bytecode_file(&vm, fn, path, false, err, sizeof(err));
    }
    TEST("compile + dump");
    CHECK(dumped, err[0] ? err : "dump failed");
    if (!dumped)
        return;

    std::vector<uint8_t> bytes;
    if (FILE *f = fopen(path, "rb"))
    {
        uint8_t buf[4096];
        size_t got;
        while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
            bytes.insert(bytes.end(), buf, buf + got);
        fclose(f);
    }
    remove(path);

    VM vm;
    register_roundtrip_libs(vm);
    err[0] = '\0';
    ObjFunc *fn = load_bytecode_buffer(&vm, bytes.data(), bytes.size(), err, sizeof(err));
    TEST("load into a fresh VM");
    CHECK(fn != nullptr, err[0] ? err : "load failed");
    if (!fn)
        return;

    vm.run(fn);
    TEST("loaded script ran without error");
    CHECK(!vm.had_error(), "runtime error after load");

    Value r = vm.get_global("r");
    TEST("math.sqrt(16.0) == 4.0 (import resolved)");
    CHECK(is_float(r) && r.as.number == 4.0, "expected 4.0");

    Value t = vm.get_global("t");
    TEST("xs.sort(); xs[0] == 1 (builtin method)");
    CHECK(is_int(t) && t.as.integer == 1, "expected 1");

    Value n = vm.get_global("n");
    TEST("\"a,b\".split(\",\") has 2 parts");
    CHECK(is_int(n) && n.as.integer == 2, "expected 2");

    Value u = vm.get_global("u");
    TEST("user class method c.bump(2) == 3");
    CHECK(is_int(u) && u.as.integer == 3, "expected 3");
}

/* =========================================================
** TEST 18: subscripts beyond 32 bits are out of range, not wrapped
**
** (int32_t)key.as.integer turned xs[4294967296] into xs[0] and
** xs[-4294967297] into xs[-1]. The language has no try/except, so the
** error cases are asserted here through vm.had_error().
** ========================================================= */
static bool script_errors(const char *source)
{
    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    run_source(vm, source);
    return vm.had_error();
}

static void test_index_beyond_32_bits()
{
    printf("\n[Test 18] indices beyond 32 bits raise instead of wrapping\n");

    struct Case { const char *what; const char *src; bool expect_error; };
    static const Case cases[] = {
        /* Reads */
        {"xs[4294967296] (reads xs[0] if wrapped)",    "xs = [1, 2, 3]\nv = xs[4294967296]\n", true},
        {"xs[-4294967297] (reads xs[-1] if wrapped)",  "xs = [1, 2, 3]\nv = xs[-4294967297]\n", true},
        {"xs[4294967297] (reads xs[1] if wrapped)",    "xs = [1, 2, 3]\nv = xs[4294967297]\n", true},
        {"xs[9223372036854775807]",                    "xs = [1, 2, 3]\nv = xs[9223372036854775807]\n", true},
        {"xs[-9223372036854775807]",                   "xs = [1, 2, 3]\nv = xs[-9223372036854775807]\n", true},
        {"\"abc\"[4294967296]",                        "s = \"abc\"\nv = s[4294967296]\n", true},
        {"\"abc\"[-4294967297]",                       "s = \"abc\"\nv = s[-4294967297]\n", true},
        {"Uint32Array read [4294967296]",              "b = Uint32Array(4)\nv = b[4294967296]\n", true},
        {"Uint32Array read [-4294967297]",             "b = Uint32Array(4)\nv = b[-4294967297]\n", true},
        /* Writes and deletes */
        {"xs[4294967296] = 1",                         "xs = [1, 2, 3]\nxs[4294967296] = 1\n", true},
        {"xs[-4294967297] = 1",                        "xs = [1, 2, 3]\nxs[-4294967297] = 1\n", true},
        {"Uint32Array write [4294967296]",             "b = Uint32Array(4)\nb[4294967296] = 1\n", true},
        {"del xs[4294967296]",                         "xs = [1, 2, 3]\ndel xs[4294967296]\n", true},
        {"del xs[-4294967297]",                        "xs = [1, 2, 3]\ndel xs[-4294967297]\n", true},
        {"xs.pop(4294967296)",                         "xs = [1, 2, 3]\nxs.pop(4294967296)\n", true},
        {"xs.remove(4294967296)",                      "xs = [1, 2, 3]\nxs.remove(4294967296)\n", true},
        {"xs.insert(4294967296, 9)",                   "xs = [1, 2, 3]\nxs.insert(4294967296, 9)\n", true},
        {"Uint8Array(4294967296) size",                "b = Uint8Array(4294967296)\n", true},
        /* Valid uses and controls: still fine */
        {"xs[2], xs[-3], s[-1] still work",            "xs = [1, 2, 3]\na = xs[2]\nb = xs[-3]\ns = \"abc\"\nc = s[-1]\n", false},
        {"slice bounds beyond 32 bits clamp",          "xs = [1, 2, 3]\na = xs[:4294967296]\nb = xs[4294967296:]\nc = \"abc\"[:4294967296]\n", false},
        {"slice step zero still errors",               "xs = [1, 2, 3]\na = xs[::0]\n", true},
    };

    for (const Case &c : cases)
    {
        TEST(c.what);
        bool errored = script_errors(c.src);
        CHECK(errored == c.expect_error,
              c.expect_error ? "no error raised (index wrapped?)" : "unexpected error");
    }

    /* Wrapped value must not leak through: the failed read leaves v unset. */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm, "xs = [11, 22, 33]\nv = 0\nv = xs[4294967296]\n");
        Value v = vm.get_global("v");
        TEST("failed xs[2**32] does not yield xs[0]");
        CHECK(vm.had_error() && !(is_int(v) && v.as.integer == 11), "wrapped read returned xs[0]");
    }

    /* char_at / byte_at report out-of-range as nil / 0, never as byte 0. */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm, "s = \"abc\"\nc = s.char_at(4294967296)\nb = s.byte_at(4294967296)\n");
        Value c = vm.get_global("c");
        Value b = vm.get_global("b");
        TEST("char_at/byte_at(2**32) are out of range, not index 0");
        CHECK(!vm.had_error() && is_nil(c) && is_int(b) && b.as.integer == 0,
              "wrapped to index 0");
    }
}

/* =========================================================
** MAIN
** ========================================================= */

/* =========================================================
** TEST 16: keyword arguments reach a native through vm->kwargs()
** (compiler packs name=value into a trailing map, flag 0x40 in nargs).
** ========================================================= */
static int native_config(VM *vm, Value *args, int nargs)
{
    /* config(base, scale=2, name="x") -> base * scale, and records the name */
    int64_t base = nargs > 0 && is_int(args[0]) ? args[0].as.integer : 0;
    int64_t scale = 1;
    ObjMap *kw = vm->kwargs();
    if (kw)
    {
        bool found = false;
        Value v = map_get(kw, val_obj((Obj *)vm->make_string("scale")), &found);
        if (found && is_int(v))
            scale = v.as.integer;
        v = map_get(kw, val_obj((Obj *)vm->make_string("name")), &found);
        if (found && is_string(v))
            vm->set_global("seen_name", v);
    }
    args[0] = val_int(base * scale);
    return 1;
}

static void test_native_keyword_arguments()
{
    printf("\n[Test 19] keyword arguments to natives (vm->kwargs)\n");
    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_native("config", native_config, -1);
    vm.def_global("seen_name", val_nil());

    TEST("Positional only: kwargs() is null");
    run_source(vm, "r1 = config(21)\n");
    Value r1 = vm.get_global("r1");
    CHECK(is_int(r1) && r1.as.integer == 21, "expected 21");

    TEST("name=value pairs arrive in the map");
    run_source(vm, "r2 = config(21, scale=2, name=\"tex\")\n");
    Value r2 = vm.get_global("r2");
    Value nm = vm.get_global("seen_name");
    CHECK(is_int(r2) && r2.as.integer == 42 && is_string(nm) && strcmp(as_string(nm)->chars, "tex") == 0,
          "expected 42 and seen_name == 'tex'");

    TEST("kwargs() is cleared after the call");
    CHECK(vm.kwargs() == nullptr, "kwargs_ must be null outside a native call");

    TEST("Builtin sorted(key=, reverse=) through the same channel");
    run_source(vm, "r3 = sorted([3, 1, 2], reverse=True)\nr4 = sorted([\"bb\", \"a\", \"ccc\"], key=len)\n");
    Value r3 = vm.get_global("r3"), r4 = vm.get_global("r4");
    CHECK(is_array(r3) && as_array(r3)->data[0].as.integer == 3 &&
          is_array(r4) && strcmp(as_string(as_array(r4)->data[0])->chars, "a") == 0,
          "expected [3,2,1] and ['a','bb','ccc']");
}

/* =========================================================
** TEST 17: ClassBuilder::method flags — a GC_SAFE method runs with the
** collector live, is called straight from the vtable slot (INVOKE_VT on a
** typed receiver), and native_data survives heavy allocation in the script.
** ========================================================= */
static int g_counter_alive = 0;
struct CounterData { int64_t n; };
static void *counter_ctor(VM *, int, Value *) { CounterData *c = (CounterData *)malloc(sizeof(CounterData)); c->n = 0; g_counter_alive++; return c; }
static void counter_dtor(VM *, void *d) { free(d); g_counter_alive--; }
static int counter_tick(VM *, Value *args, int) { CounterData *c = zen_instance_data<CounterData>(args[-1]); c->n++; args[0] = val_int(c->n); return 1; }
static int counter_value(VM *, Value *args, int) { args[0] = val_int(zen_instance_data<CounterData>(args[-1])->n); return 1; }

static void test_method_flags_and_gc()
{
    printf("\n[Test 20] ClassBuilder method flags (GC_SAFE) under allocation pressure\n");
    g_counter_alive = 0;
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        vm.def_class("Counter")
            .ctor(counter_ctor)
            .dtor(counter_dtor)
            .method("tick", counter_tick, 0, ZEN_NATIVE_GC_SAFE)
            .method("value", counter_value, 0, ZEN_NATIVE_GC_SAFE)
            .end();

        TEST("GC_SAFE methods on a typed receiver, 200k calls with garbage in between");
        run_source(vm, R"(
c: Counter = Counter()
junk = []
i = 0
while i < 200000:
    c.tick()
    junk.append([i, str(i)])
    if len(junk) > 1000:
        junk = []
    i += 1
v = c.value()
)");
        Value v = vm.get_global("v");
        CHECK(is_int(v) && v.as.integer == 200000, "expected value() == 200000");

        TEST("Untyped receiver goes through INVOKE and reaches the same native");
        run_source(vm, "d = Counter()\nd.tick()\nd.tick()\nw = d.value()\n");
        Value w = vm.get_global("w");
        CHECK(is_int(w) && w.as.integer == 2, "expected 2");

        TEST("Two instances alive");
        CHECK(g_counter_alive == 2, "expected 2 counters alive");
    }
    TEST("Destructors ran when the VM died");
    CHECK(g_counter_alive == 0, "expected 0 counters alive");
}

/* =========================================================
** TEST 18: zen_bind.hpp — thunks generated from C++ signatures: every
** argument/result conversion, VM* injection, arity and type errors with
** the argument index, native classes with ctor<A...>/ctor<&factory>/dtor.
** ========================================================= */
static double b_avg(double a, double b) { return (a + b) / 2; }
static int64_t b_twice(int64_t x) { return x * 2; }
static int b_small(int x, unsigned y) { return x + (int)y; }
static float b_half(float x) { return x / 2; }
static bool b_both(bool a, bool b) { return a && b; }
static const char *b_greet(const char *n) { static char buf[64]; snprintf(buf, sizeof buf, "hi %s", n); return buf; }
static Value b_len(VM *vm, Value v) { (void)vm; return val_int(is_string(v) ? (int64_t)as_string(v)->length : -1); }
static void b_noop(int64_t) {}
static int g_bind_alive = 0;
struct BindCounter
{
    int64_t n;
    explicit BindCounter(int64_t start) : n(start) { g_bind_alive++; }
    ~BindCounter() { g_bind_alive--; }
    void tick(int64_t k) { n += k; }
    int64_t value() const { return n; }
    const char *label(VM *, const char *p) { static char b[64]; snprintf(b, sizeof b, "%s%lld", p, (long long)n); return b; }
};
struct BindGauge { double level; };
static BindGauge *gauge_make(VM *vm, double level)
{
    if (level < 0) { vm->runtime_error("Gauge: level must be >= 0"); return nullptr; }
    BindGauge *g = new BindGauge; g->level = level; return g;
}
static double gauge_level(BindGauge &g) { return g.level; }
static int64_t b_sum(BindCounter *a, BindCounter &b) { return a->n + b.n; }
struct BindPlain { int64_t a = 7; int64_t get() const { return a; } };
static int64_t b_many(int64_t a, int64_t b, int64_t c, double d, bool e, const char *f)
{ return a + b + c + (int64_t)d + (e ? 1 : 0) + (int64_t)f[0]; }
static void b_touch(VM *vm) { vm->set_global("touched", val_int(1)); }

static void test_zen_bind()
{
    printf("\n[Test 21] zen_bind.hpp typed bindings\n");
    g_bind_alive = 0;
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        bind::def_fn<&b_avg>(vm, "avg");
        bind::def_fn<&b_twice>(vm, "twice");
        bind::def_fn<&b_small>(vm, "small");
        bind::def_fn<&b_half>(vm, "half");
        bind::def_fn<&b_both>(vm, "both");
        bind::def_fn<&b_greet>(vm, "greet");
        bind::def_fn<&b_len>(vm, "vlen");
        bind::def_fn<&b_noop>(vm, "noop");
        bind::def_fn<&b_sum>(vm, "csum");
        bind::def_class<BindCounter>(vm, "Counter")
            .ctor<int64_t>()
            .dtor()
            .method<&BindCounter::tick>("tick", ZEN_NATIVE_GC_SAFE)
            .method<&BindCounter::value>("value", ZEN_NATIVE_GC_SAFE)
            .method<&BindCounter::label>("label")
            .end();
        bind::def_class<BindGauge>(vm, "Gauge")
            .ctor<&gauge_make>()
            .dtor()
            .method<&gauge_level>("level")
            .end();

        TEST("Scalar conversions: double, int64, int/unsigned, float, bool, const char*, Value, void");
        run_source(vm, R"(
r_avg = avg(1, 2)
r_twice = twice(21)
r_small = small(3, 4)
r_half = half(5)
r_both = both(True, 0)
r_greet = greet("zen")
r_len = vlen("abcd")
r_len2 = vlen(7)
r_noop = noop(1)
)");
        Value r;
        r = vm.get_global("r_avg");   CHECK(is_float(r) && r.as.number == 1.5, "avg(1,2) == 1.5 (ints accepted as double)");
        r = vm.get_global("r_twice"); CHECK(is_int(r) && r.as.integer == 42, "twice(21) == 42");
        r = vm.get_global("r_small"); CHECK(is_int(r) && r.as.integer == 7, "small(3,4) == 7");
        r = vm.get_global("r_half");  CHECK(is_float(r) && r.as.number == 2.5, "half(5) == 2.5");
        r = vm.get_global("r_both");  CHECK(is_bool(r) && r.as.boolean == false, "both(True, 0) == False (truthiness)");
        r = vm.get_global("r_greet"); CHECK(is_string(r) && strcmp(as_string(r)->chars, "hi zen") == 0, "greet('zen') == 'hi zen'");
        r = vm.get_global("r_len");   CHECK(is_int(r) && r.as.integer == 4, "VM* injected, Value passthrough");
        r = vm.get_global("r_len2");  CHECK(is_int(r) && r.as.integer == -1, "Value accepts anything");
        r = vm.get_global("r_noop");  CHECK(is_nil(r), "void result is nil");

        TEST("Native class: ctor<int64_t>, methods, VM* + const char* method, T*/T& parameters");
        run_source(vm, R"(
c = Counter(5)
c.tick(3)
d: Counter = Counter(10)
d.tick(1)
r_val = c.value()
r_lab = c.label("n=")
r_sum = csum(c, d)
)");
        r = vm.get_global("r_val"); CHECK(is_int(r) && r.as.integer == 8, "value() == 8");
        r = vm.get_global("r_lab"); CHECK(is_string(r) && strcmp(as_string(r)->chars, "n=8") == 0, "label('n=') == 'n=8'");
        r = vm.get_global("r_sum"); CHECK(is_int(r) && r.as.integer == 19, "csum(c, d) == 19");
        CHECK(g_bind_alive == 2, "two counters alive");

        TEST("Factory ctor with VM*, free function taking T& as a method");
        run_source(vm, "g = Gauge(0.75)\nr_lvl = g.level()\n");
        r = vm.get_global("r_lvl"); CHECK(is_float(r) && r.as.number == 0.75, "level() == 0.75");

        TEST("Type error names the function and the argument index");
        run_source(vm, "e1 = 0\ne1 = twice(\"x\")\n");
        CHECK(vm.had_error(), "twice('x') must fail");
        run_source(vm, "e2 = avg(1, None)\n");
        CHECK(vm.had_error(), "avg(1, None) must fail on argument 2");
        run_source(vm, "e3 = csum(c, 3)\n");
        CHECK(vm.had_error(), "csum(c, 3) must fail: argument 2 not a native object");

        TEST("Arity error and factory returning nullptr after runtime_error");
        run_source(vm, "e4 = small(1)\n");
        CHECK(vm.had_error(), "small(1) must fail: expected 2 arguments");
        run_source(vm, "e5 = Gauge(-1)\n");
        CHECK(vm.had_error(), "Gauge(-1) must fail via the factory");

        TEST("Zero-argument ctor, six-argument function, VM-only function");
        bind::def_class<BindPlain>(vm, "Plain").ctor<>().dtor().method<&BindPlain::get>("get").end();
        bind::def_fn<&b_many>(vm, "many");
        bind::def_fn<&b_touch>(vm, "touch");
        vm.def_global("touched", val_int(0));
        run_source(vm, "p = Plain()\nr_p = p.get()\nr_many = many(1, 2, 3, 4.9, True, \"Z\")\ntouch()\n");
        r = vm.get_global("r_p");    CHECK(is_int(r) && r.as.integer == 7, "Plain().get() == 7");
        r = vm.get_global("r_many"); CHECK(is_int(r) && r.as.integer == 101, "many(...) == 101");
        r = vm.get_global("touched");CHECK(is_int(r) && r.as.integer == 1, "VM-only function ran");

        TEST("GC_SAFE typed method under allocation pressure");
        run_source(vm, R"(
k: Counter = Counter(0)
junk = []
i = 0
while i < 100000:
    k.tick(1)
    junk.append([i, str(i)])
    if len(junk) > 1000:
        junk = []
    i += 1
r_k = k.value()
)");
        r = vm.get_global("r_k"); CHECK(is_int(r) && r.as.integer == 100000, "value() == 100000");
    }
    TEST("dtor() ran for every instance when the VM died");
    CHECK(g_bind_alive == 0, "expected 0 counters alive");
}

/* =========================================================
** TEST 22: Vec2 / Vec3 value types
**
** The language has no try/except, so every runtime error is asserted here
** through vm.had_error(). Also: natives taking and returning vectors,
** vectors through vm.call_global, and the BC_VEC2 / BC_VEC3 bytecode tags.
** ========================================================= */
static int native_vec3_scale(VM *vm, Value *args, int nargs)
{
    if (nargs != 2 || !is_vec3(args[0]) || !(is_float(args[1]) || is_int(args[1])))
    {
        vm->runtime_error("vec3_scale(Vec3, number) expected");
        return -1;
    }
    Vec3f v = as_vec3(args[0]);
    float k = (float)to_number(args[1]);
    args[0] = val_vec3(Vec3f{v.x * k, v.y * k, v.z * k});
    return 1;
}

static int native_vec_kind(VM *vm, Value *args, int nargs)
{
    (void)vm; (void)nargs;
    args[0] = val_int(is_vec3(args[0]) ? 3 : is_vec2(args[0]) ? 2 : 0);
    return 1;
}

static void test_vec_values()
{
    printf("\n[Test 22] Vec2 / Vec3 value types\n");

    static_assert(sizeof(Value) == 16, "Value must stay 16 bytes");

    struct Case { const char *what; const char *src; bool expect_error; };
    static const Case cases[] = {
        {"Vec2 + Vec3",              "x = Vec2(1, 2) + Vec3(1, 2, 3)\n", true},
        {"Vec3 - Vec2",              "x = Vec3(1, 2, 3) - Vec2(1, 2)\n", true},
        {"Vec2 * Vec3",              "x = Vec2(1, 2) * Vec3(1, 2, 3)\n", true},
        {"Vec2 < Vec2",              "x = Vec2(1, 2) < Vec2(3, 4)\n", true},
        {"Vec2 <= Vec2",             "x = Vec2(1, 2) <= Vec2(3, 4)\n", true},
        {"Vec2 > Vec2",              "x = Vec2(1, 2) > Vec2(3, 4)\n", true},
        {"Vec2 >= Vec2",             "x = Vec2(1, 2) >= Vec2(3, 4)\n", true},
        {"Vec2 < 3",                 "x = Vec2(1, 2) < 3\n", true},
        {"3 < Vec2",                 "x = 3 < Vec2(1, 2)\n", true},
        {"if a < b (fused jump)",    "a = Vec2(1, 2)\nb = Vec2(3, 4)\nif a < b:\n    x = 1\n", true},
        {"while a <= b (fused jump)","a = Vec2(1, 2)\nb = Vec2(3, 4)\nwhile a <= b:\n    x = 1\n", true},
        {"if v < 3 (immediate jump)","v = Vec3(1, 2, 3)\nif v < 3:\n    x = 1\n", true},
        {"Vec2 + 1",                 "x = Vec2(1, 2) + 1\n", true},
        {"Vec2 - 1",                 "x = Vec2(1, 2) - 1\n", true},
        {"1 + Vec2",                 "x = 1 + Vec2(1, 2)\n", true},
        {"Vec2 / Vec2",              "x = Vec2(1, 2) / Vec2(1, 1)\n", true},
        {"Vec2 / 0",                 "x = Vec2(1, 2) / 0\n", true},
        {"2 / Vec2",                 "x = 2 / Vec2(1, 2)\n", true},
        {"Vec2 // 2",                "x = Vec2(1, 2) // 2\n", true},
        {"Vec2 % 2",                 "x = Vec2(1, 2) % 2\n", true},
        {"Vec2 ** 2",                "x = Vec2(1, 2) ** 2\n", true},
        {"int(Vec2)",                "x = int(Vec2(1, 2))\n", true},
        {"float(Vec3)",              "x = float(Vec3(1, 2, 3))\n", true},
        {"abs(Vec2)",                "x = abs(Vec2(1, 2))\n", true},
        {"round(Vec2)",              "x = round(Vec2(1, 2))\n", true},
        {"Vec2.z",                   "x = Vec2(1, 2).z\n", true},
        {"Vec2.w",                   "x = Vec3(1, 2, 3).w\n", true},
        {"Vec2.foo",                 "x = Vec2(1, 2).foo\n", true},
        {"Vec2.z = 1",               "def f():\n    v = Vec2(1, 2)\n    v.z = 1\nf()\n", true},
        {"Vec2.x = 'a'",             "def f():\n    v = Vec2(1, 2)\n    v.x = 'a'\nf()\n", true},
        {"Vec2(\"a\", 1)",           "x = Vec2(\"a\", 1)\n", true},
        {"Vec2(1)",                  "x = Vec2(1)\n", true},
        {"Vec3(1, 2)",               "x = Vec3(1, 2)\n", true},
        {"Vec3(1, 2, 3, 4)",         "x = Vec3(1, 2, 3, 4)\n", true},
        {"Vec2(None, 1)",            "x = Vec2(None, 1)\n", true},
        {"unknown method",           "x = Vec2(1, 2).nope()\n", true},
        {"dot with wrong kind",      "x = Vec2(1, 2).dot(Vec3(1, 2, 3))\n", true},
        {"dot with a number",        "x = Vec2(1, 2).dot(3)\n", true},
        {"Vec2.cross(Vec3)",         "x = Vec2(1, 2).cross(Vec3(1, 2, 3))\n", true},
        {"lerp with a vector t",     "x = Vec2(1, 2).lerp(Vec2(3, 4), Vec2(1, 1))\n", true},
        {"with_z on Vec2",           "x = Vec2(1, 2).with_z(3)\n", true},
        {"length with an argument",  "x = Vec2(1, 2).length(1)\n", true},
        {"len(Vec2)",                "x = len(Vec2(1, 2))\n", true},
        {"Vec2[0]",                  "x = Vec2(1, 2)[0]\n", true},
        {"for c in Vec2",            "for c in Vec2(1, 2):\n    pass\n", true},
        {"sorted of vectors",        "x = sorted([Vec2(1, 2), Vec2(0, 1)])\n", true},
        {"max of vectors",           "x = max(Vec2(1, 2), Vec2(0, 1))\n", true},
        /* Controls: all of these are fine */
        {"construction, arithmetic, fields, methods",
         "a = Vec3(1, 2, 3)\nb = a + a * 2 - a\nc = a.x + a.y + a.z\nd = a.cross(b).length()\n"
         "e = Vec2() == Vec2(0, 0)\nf = a != Vec3(0, 0, 0)\nif a:\n    g = 1\n", false},
        {"== and != never error",    "x = Vec2(1, 2) == Vec3(1, 2, 0)\ny = Vec2(1, 2) != 5\nz = Vec2(1, 2) == None\n", false},
        {"local field write",        "def f():\n    v = Vec3(1, 2, 3)\n    v.x = 5\n    v.y += 1\n    return v\nx = f()\n", false},
    };
    for (const Case &c : cases)
    {
        TEST(c.what);
        bool errored = script_errors(c.src);
        CHECK(errored == c.expect_error,
              c.expect_error ? "no error raised" : "unexpected error");
    }

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_native("vec3_scale", native_vec3_scale, 2);
    vm.def_native("vec_kind", native_vec_kind, 1);
    Value r;

    TEST("native receives a Vec3 (is_vec3 / as_vec3) and returns val_vec3");
    run_source(vm, "r_scaled = vec3_scale(Vec3(1, 2, 3), 2)\nr_k3 = vec_kind(Vec3())\nr_k2 = vec_kind(Vec2())\nr_k0 = vec_kind(3)\n");
    CHECK(!vm.had_error(), "script errored");
    r = vm.get_global("r_scaled");
    CHECK(is_vec3(r) && r.vx == 2.0f && vec_y(r) == 4.0f && vec_z(r) == 6.0f, "vec3_scale(Vec3(1,2,3), 2) == Vec3(2,4,6)");
    Vec3f sv = as_vec3(r);
    CHECK(sv.x == 2.0f && sv.y == 4.0f && sv.z == 6.0f, "as_vec3 reads the components back");
    r = vm.get_global("r_k3"); CHECK(is_int(r) && r.as.integer == 3, "is_vec3 sees a Vec3");
    r = vm.get_global("r_k2"); CHECK(is_int(r) && r.as.integer == 2, "is_vec2 sees a Vec2");
    r = vm.get_global("r_k0"); CHECK(is_int(r) && r.as.integer == 0, "neither for an int");

    TEST("math.sqrt(Vec2) is an error, math.sqrt(4) is not");
    {
        VM mvm;
        register_roundtrip_libs(mvm);
        run_source(mvm, "import math\nx = math.sqrt(4)\n");
        CHECK(!mvm.had_error(), "math.sqrt(4) must work");
        VM mvm2;
        register_roundtrip_libs(mvm2);
        run_source(mvm2, "import math\nx = math.sqrt(Vec2(1, 2))\n");
        CHECK(mvm2.had_error(), "math.sqrt(Vec2) must fail");
    }

    TEST("a native given a Vec2 where it wants a Vec3 reports an error");
    run_source(vm, "bad = vec3_scale(Vec2(1, 2), 2)\n");
    CHECK(vm.had_error(), "vec3_scale(Vec2, 2) must fail");

    TEST("Vec3 values survive vm.call_global argument passing and return");
    {
        VM vm2;
        vm2.open_lib_globals(&zen_lib_base);
        run_source(vm2, "def length_of(v):\n    return v.length()\n"
                        "def echo(v):\n    return v\n"
                        "def add(a, b):\n    return a + b\n"
                        "def offset(p, d):\n    p.x = p.x + d\n    return p\n");
        Value a3[1] = {val_vec3(2.0f, 3.0f, 6.0f)};
        Value out = vm2.call_global("length_of", a3, 1);
        CHECK(is_float(out) && out.as.number == 7.0, "length_of(Vec3(2,3,6)) == 7.0");
        Value a4[1] = {val_vec3(Vec3f{1.5f, -2.0f, 0.25f})};
        out = vm2.call_global("echo", a4, 1);
        CHECK(is_vec3(out) && out.vx == 1.5f && vec_y(out) == -2.0f && vec_z(out) == 0.25f, "echo returns the same Vec3");
        Value a5[2] = {val_vec2(1.0f, 2.0f), val_vec2(10.0f, 20.0f)};
        out = vm2.call_global("add", a5, 2);
        CHECK(is_vec2(out) && out.vx == 11.0f && vec_y(out) == 22.0f, "add(Vec2, Vec2) == Vec2(11, 22)");
        Value a6[2] = {val_vec3(1.0f, 1.0f, 1.0f), val_int(4)};
        out = vm2.call_global("offset", a6, 2);
        CHECK(is_vec3(out) && out.vx == 5.0f, "field write on a parameter register");
        CHECK(a6[0].vx == 1.0f, "the caller's Value is a copy, untouched");
        CHECK(!vm2.had_error(), "no error after the calls");
    }

    TEST("Value layout: vx in the former padding, payload at offset 8");
    CHECK(offsetof(Value, vx) == 4 && offsetof(Value, as) == 8 && sizeof(Value) == 16, "layout");
    Value z2 = val_vec2(7.0f, 8.0f);
    CHECK(vec_z(z2) == 0.0f, "val_vec2 zeroes the unused z");

    TEST("BC_VEC2 / BC_VEC3 constants round-trip through the bytecode writer and reader");
    {
        const char *path = "test_embedding_vec.zbc";
        char err[256] = {0};
        bool dumped = false;
        {
            VM dvm;
            register_roundtrip_libs(dvm);
            Compiler compiler;
            ObjFunc *fn = compiler.compile(&dvm.get_gc(), &dvm, "x = 1\n", "<dump>");
            if (fn)
            {
                /* Append two vector constants to the main function. */
                int32_t n = fn->const_count;
                Value *grown = (Value *)zen_alloc(&dvm.get_gc(), sizeof(Value) * (size_t)(n + 2));
                for (int32_t k = 0; k < n; k++)
                    grown[k] = fn->constants[k];
                grown[n] = val_vec2(0.1f, -2.5f);
                grown[n + 1] = val_vec3(1.0f, 2.0f, 3.5f);
                fn->constants = grown;
                fn->const_count = n + 2;
                fn->const_capacity = n + 2;
                dumped = dump_bytecode_file(&dvm, fn, path, false, err, sizeof(err));
            }
        }
        CHECK(dumped, err[0] ? err : "dump failed");
        if (dumped)
        {
            std::vector<uint8_t> bytes;
            if (FILE *f = fopen(path, "rb"))
            {
                uint8_t buf[4096];
                size_t got;
                while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
                    bytes.insert(bytes.end(), buf, buf + got);
                fclose(f);
            }
            VM lvm;
            register_roundtrip_libs(lvm);
            err[0] = '\0';
            ObjFunc *fn = load_bytecode_buffer(&lvm, bytes.data(), bytes.size(), err, sizeof(err));
            CHECK(fn != nullptr, err[0] ? err : "load failed");
            if (fn)
            {
                Value c2 = fn->constants[fn->const_count - 2];
                Value c3 = fn->constants[fn->const_count - 1];
                CHECK(is_vec2(c2) && c2.vx == 0.1f && vec_y(c2) == -2.5f, "Vec2 constant round-trips bit for bit");
                CHECK(is_vec3(c3) && c3.vx == 1.0f && vec_y(c3) == 2.0f && vec_z(c3) == 3.5f, "Vec3 constant round-trips bit for bit");
            }
        }
        remove(path);
    }
}

/* =========================================================
** TEST 24: host references (vm.ref / zen::Ref) and bound methods as values
**
** A native stores a callback it was handed. Nothing in the script keeps it
** alive any more, so only the host reference does -- the script then
** generates enough garbage to force several collections, after which C++
** calls it through call_fn.
** ========================================================= */
static int g_cb_handle = -1;
static Ref g_cb_ref;

static int native_on_click(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    g_cb_handle = vm->ref(args[0]);
    return 0;
}

static int native_on_click_ref(VM *vm, Value *args, int nargs)
{
    (void)nargs;
    g_cb_ref = Ref(*vm, args[0]);
    return 0;
}

static const char *kRefGarbageLoop = R"(
junk = None
for i in range(60000):
    junk = [i, str(i), {"k": i}]
collect()
)";

static void test_host_refs_and_bound_methods()
{
    printf("\n[Test 24] host references (vm.ref / zen::Ref) and bound methods\n");

    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    vm.def_native("on_click", native_on_click, 1);
    vm.def_native("on_click_ref", native_on_click_ref, 1);

    const int base_refs = vm.ref_count();

    /* A bound method of a script instance: neither the instance nor the
    ** bound method is reachable from any script variable once the
    ** statement ends. */
    run_source(vm, R"(
class Button:
    def __init__(self, k):
        self.k = k
    def fire(self, x, y=0):
        return x * self.k + y

on_click(Button(3).fire)
)");
    int h = g_cb_handle;

    TEST("vm.ref() returns a handle and bumps ref_count()");
    CHECK(h >= 0 && vm.ref_count() == base_refs + 1, "expected one live reference");

    run_source(vm, kRefGarbageLoop);
    vm.collect();

    TEST("referenced bound method survives collections and is callable from C++");
    Value args[1] = { val_int(14) };
    Value r = vm.call_fn(vm.deref(h), args, 1);
    CHECK(!vm.had_error() && is_int(r) && r.as.integer == 42, "expected fire(14) == 42");

    TEST("bound method call from C++ applies the default argument");
    Value args2[2] = { val_int(2), val_int(100) };
    r = vm.call_fn(vm.deref(h), args2, 2);
    CHECK(!vm.had_error() && is_int(r) && r.as.integer == 106, "expected fire(2, 100) == 106");

    TEST("bound method called with too many arguments reports an error");
    Value args3[3] = { val_int(1), val_int(2), val_int(3) };
    r = vm.call_fn(vm.deref(h), args3, 3);
    CHECK(vm.had_error() && is_nil(r), "expected an arity error");

    TEST("unref() returns ref_count() to its previous value");
    vm.unref(h);
    CHECK(vm.ref_count() == base_refs, "expected the reference to be released");

    TEST("deref() of a released handle is nil");
    CHECK(is_nil(vm.deref(h)) && is_nil(vm.deref(-1)) && is_nil(vm.deref(100000)),
          "expected nil for released / invalid handles");

    /* A plain closure (with an upvalue) held only by zen::Ref. */
    run_source(vm, R"(
def make_adder(k):
    def add(x):
        return x + k
    return add

on_click_ref(make_adder(1000))
)");
    TEST("zen::Ref holds a closure across collections");
    run_source(vm, kRefGarbageLoop);
    Value a1[1] = { val_int(5) };
    r = vm.call_fn(g_cb_ref.get(), a1, 1);
    CHECK(g_cb_ref && !vm.had_error() && is_int(r) && r.as.integer == 1005, "expected add(5) == 1005");

    TEST("moving a Ref keeps the value alive and releases once");
    {
        const int before = vm.ref_count();
        Ref moved(std::move(g_cb_ref));
        Ref again;
        again = std::move(moved);
        run_source(vm, kRefGarbageLoop);
        Value a2[1] = { val_int(1) };
        Value rr = vm.call_fn(again.get(), a2, 1);
        bool ok = !g_cb_ref && !moved && again && vm.ref_count() == before &&
                  is_int(rr) && rr.as.integer == 1001;
        again.reset();
        ok = ok && vm.ref_count() == before - 1 && !again;
        CHECK(ok, "expected one live reference through both moves, released once");
    }

    TEST("double unref does not corrupt the free list");
    {
        const int before = vm.ref_count();
        int h1 = vm.ref(val_int(1));
        int h2 = vm.ref(val_int(2));
        vm.unref(h1);
        vm.unref(h1);               /* second release: ignored */
        int n1 = vm.ref(val_int(10));
        int n2 = vm.ref(val_int(20));
        bool ok = n1 != n2 && n1 != h2 && n2 != h2;
        ok = ok && vm.deref(n1).as.integer == 10 && vm.deref(n2).as.integer == 20 &&
             vm.deref(h2).as.integer == 2 && vm.ref_count() == before + 3;
        vm.unref(n1);
        vm.unref(n2);
        vm.unref(h2);
        ok = ok && vm.ref_count() == before;
        CHECK(ok, "expected distinct handles and a consistent count");
    }

    TEST("bound method of a native-class method is callable from C++");
    run_source(vm, R"(
sig = Signal()
nat_count = sig.count
)");
    Value nb = vm.get_global("nat_count");
    r = vm.call_fn(nb, nullptr, 0);
    CHECK(!vm.had_error() && is_bound_method(nb) && is_int(r) && r.as.integer == 0,
          "expected a bound native method returning 0");

    g_cb_ref.reset();
}

/* =========================================================
** TEST: GC tuning (set_gc_tuning / gc_tuning / paused tuning)
** ========================================================= */
static void test_gc_tuning()
{
    printf("\n--- GC tuning ---\n");
    VM vm;
    vm.open_lib_globals(&zen_lib_base);
    GC &gc = vm.get_gc();
    size_t mn = 0;
    float fac = 0.0f;

    TEST("defaults are 8 MB / 2.0 and the first cycle waits for 8 MB");
    vm.gc_tuning(&mn, &fac);
    CHECK(mn == kGCDefaultMinThreshold && mn == 8u * 1024 * 1024 && fac == 2.0f &&
          gc.next_gc == mn,
          "unexpected defaults");

    TEST("set_gc_tuning / gc_tuning round trip");
    vm.set_gc_tuning(2 * 1024 * 1024, 3.5f);
    vm.gc_tuning(&mn, &fac);
    CHECK(mn == 2u * 1024 * 1024 && fac == 3.5f, "tuning not stored");

    TEST("values are clamped, not rejected");
    vm.set_gc_tuning(1, 0.25f);
    vm.gc_tuning(&mn, &fac);
    bool ok = mn == 64u * 1024 && fac == 1.0f;
    vm.set_gc_tuning(100 * 1024, -3.0f);
    vm.gc_tuning(&mn, &fac);
    ok = ok && mn == 100u * 1024 && fac == 1.0f;
    vm.gc_tuning(nullptr, nullptr); /* null outputs are allowed */
    CHECK(ok, "expected clamping to 64 KB / 1.0");

    TEST("raising the minimum raises next_gc immediately");
    vm.set_gc_tuning(1024 * 1024, 2.0f);
    vm.collect(); /* next_gc = max(live * 2, 1 MB) = 1 MB for this tiny heap */
    size_t before = gc.next_gc;
    vm.set_gc_tuning(16 * 1024 * 1024, 2.0f);
    bool raised = before < 16u * 1024 * 1024 && gc.next_gc == 16u * 1024 * 1024;
    /* lowering the minimum never moves a pending threshold */
    vm.set_gc_tuning(1024 * 1024, 2.0f);
    CHECK(raised && gc.next_gc == 16u * 1024 * 1024,
          "expected next_gc = 16 MB after raising, unchanged after lowering");

    TEST("a collection reschedules from the new tuning");
    vm.set_gc_tuning(4 * 1024 * 1024, 2.0f);
    vm.collect();
    CHECK(gc.next_gc == 4u * 1024 * 1024, "expected next_gc = 4 MB after a cycle");

    TEST("tuning while paused updates the saved threshold");
    vm.set_gc_tuning(1024 * 1024, 2.0f);
    vm.collect();
    gc_pause(&gc);
    bool sentinel = gc.next_gc == (size_t)-1;
    vm.set_gc_tuning(32 * 1024 * 1024, 2.0f);
    bool still_paused = gc.next_gc == (size_t)-1 && gc.pause_saved_next_gc == 32u * 1024 * 1024;
    gc_resume(&gc);
    CHECK(sentinel && still_paused && gc.next_gc == 32u * 1024 * 1024,
          "expected live threshold untouched while paused, 32 MB after resume");

    TEST("gc_stats()/gc_tune() from script see the host tuning");
    vm.set_gc_tuning(5 * 1024 * 1024, 2.0f);
    ok = run_source(vm, R"(
s0 = gc_stats()
gc_tune(6 * 1024 * 1024, 3.0)
s1 = gc_stats()
)");
    vm.gc_tuning(&mn, &fac);
    Value s0 = vm.get_global("s0");
    ok = ok && !vm.had_error() && is_map(s0) && mn == 6u * 1024 * 1024 && fac == 3.0f;
    CHECK(ok, "expected script gc_tune to reach the VM");
}

int main()
{
    printf("=== Zen Embedding Tests (C++ <-> Script) ===\n");

    test_native_from_script();
    test_cpp_native_boundary();
    test_call_script_from_cpp();
    test_bidirectional_callback();
    test_shared_globals();
    test_invoke_method();
    test_incremental_execution();
    test_native_struct();
    test_script_inherits_cpp_class();
    test_native_struct_builder();
    test_class_builder_native_data();
    test_run_from_inside_script();
    test_resume_fiber_error_handling();
    test_generic_native_method();
    test_closed_script_class();
    test_native_class_inference();
    test_failing_native_ctor();
    test_bytecode_roundtrip_fresh_vm();
    test_index_beyond_32_bits();
    test_native_keyword_arguments();
    test_method_flags_and_gc();
    test_zen_bind();
    test_vec_values();
    test_host_refs_and_bound_methods();
    test_gc_tuning();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
