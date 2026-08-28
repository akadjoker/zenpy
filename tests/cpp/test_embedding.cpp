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

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

struct Vec2f {
    float x, y;
};

struct Color4 {
    uint8_t r, g, b, a;
};

static void vec2f_ctor(VM *vm, void *buffer, int argc, Value *args)
{
    (void)vm;
    Vec2f *v = (Vec2f *)buffer;
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

    /* Register Vec2f — maps directly to C++ struct with float x, y */
    vm.register_native_struct("Vec2", sizeof(Vec2f), vec2f_ctor)
        .f32("x", offsetof(Vec2f, x))
        .f32("y", offsetof(Vec2f, y))
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
** MAIN
** ========================================================= */
int main()
{
    printf("=== Zen Embedding Tests (C++ <-> Script) ===\n");

    test_native_from_script();
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

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
