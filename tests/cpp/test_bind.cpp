/*
** test_bind.cpp — zen_bind.hpp: member-function pointers become script methods.
**
** Covers constructor/method/property generation, a leading VM* parameter,
** wrapping host-owned objects (and proving the GC never deletes them),
** type errors that report instead of casting, the class check on C* / C&
** parameters, integer range checks, script subclasses of a bound class, and
** free functions.
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "zen_bind.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace zen;
using zen::bind::def_class;
using zen::bind::def_fn;
using zen::bind::wrap;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) printf("  %-52s", name)
#define PASS() do { printf("OK\n"); g_tests_passed++; } while (0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_tests_failed++; } while (0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while (0)

/* Runtime errors land here instead of stderr so tests can read them. */
static char g_err[2048];
static void capture_err(const char *s, int len, void *)
{
    size_t have = strlen(g_err);
    size_t room = sizeof(g_err) - 1 - have;
    size_t n = (size_t)len < room ? (size_t)len : room;
    memcpy(g_err + have, s, n);
    g_err[have + n] = 0;
}

static void setup(VM &vm)
{
    ZenCallbacks cb = zen_default_callbacks();
    cb.print_err = capture_err;
    vm.set_callbacks(cb);
    g_err[0] = 0;
    vm.open_lib_globals(&zen_lib_base);
}

static bool run_source(VM &vm, const char *source)
{
    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, "<bind>");
    if (!fn)
        return false;
    vm.run(fn);
    return !vm.had_error();
}

/* ---- the bound types ---- */

static int g_body_ctors = 0, g_body_dtors = 0;

struct Body
{
    float x, y;
    float z = 0;
    int hits = 0;
    Body(float x_, float y_) : x(x_), y(y_) { g_body_ctors++; }
    ~Body() { g_body_dtors++; }
    void move(float dx, float dy) { x += dx; y += dy; }
    float speed() const { return std::sqrt(x * x + y * y); }
    bool alive() const { return hits < 10; }
    const char *name() const { return "hero"; }
    int tag(VM *vm, int k) { (void)vm; return k * 10 + hits; }
    float dist_to(const Body &o) const { return std::hypot(x - o.x, y - o.y); }
    bool same(Body *o) const { return o == this; }
    void move_by(Vec3f d) { x += d.x; y += d.y; z += d.z; }
    Vec3f position() const { return Vec3f{x, y, z}; }
    float dot_xy(const Vec2f &v) const { return x * v.x + y * v.y; }
    Vec2f flat() const { return Vec2f{x, y}; }
};

struct World
{
    Body hero{3, 4};
    Body *find(const char *name) { return strcmp(name, "hero") == 0 ? &hero : nullptr; }
};

static double dist(double a, double b) { return std::sqrt(a * a + b * b); }
static int add_ints(VM *, int a, int b) { return a + b; }

static void bind_all(VM &vm)
{
    def_class<Body>(vm, "Body")
        .ctor<float, float>()
        .dtor()
        .method<&Body::move>("move")
        .method<&Body::speed>("speed")
        .method<&Body::alive>("alive")
        .method<&Body::name>("name")
        .method<&Body::tag>("tag")
        .method<&Body::dist_to>("dist_to")
        .method<&Body::same>("same")
        .method<&Body::move_by>("move_by")
        .method<&Body::position>("position")
        .method<&Body::dot_xy>("dot_xy")
        .method<&Body::flat>("flat")
        .property<&Body::hits>("hits")
        .end();
}

static bool near_to(Value v, double want)
{
    return is_float(v) && std::fabs(v.as.number - want) < 1e-4;
}

int main()
{
    printf("=== zen_bind.hpp ===\n");

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("ctor + methods + property");
        bool ok = run_source(vm,
            "b = Body(3, 4)\n"
            "b.move(1, 1)\n"
            "r_speed = b.speed()\n"
            "r_alive = b.alive()\n"
            "r_name = b.name()\n"
            "b.set_hits(7)\n"
            "r_hits = b.hits()\n"
            "r_none = b.move(0, 0)\n"
            "r_far = b.dist_to(Body(4, 5))\n"
            "r_same = b.same(b)\n");
        Value hits = vm.get_global("r_hits"), name = vm.get_global("r_name");
        CHECK(ok && near_to(vm.get_global("r_speed"), std::sqrt(41.0)) &&
              is_bool(vm.get_global("r_alive")) && vm.get_global("r_alive").as.boolean &&
              is_string(name) && strcmp(safe_string_chars(name), "hero") == 0 &&
              is_int(hits) && hits.as.integer == 7 &&
              is_nil(vm.get_global("r_none")) &&
              near_to(vm.get_global("r_far"), 0.0) &&
              vm.get_global("r_same").as.boolean,
              "wrong value from a generated thunk");
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("VM* first parameter is not a script argument");
        bool ok = run_source(vm,
            "b = Body(1, 1)\n"
            "b.set_hits(3)\n"
            "r = b.tag(5)\n");
        Value r = vm.get_global("r");
        CHECK(ok && is_int(r) && r.as.integer == 53, "tag(5) should be 5*10+3 with arity 1");
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        World world;
        def_class<World>(vm, "World").method<&World::find>("find").constructable(false).end();
        vm.def_global("world", wrap<World>(vm, &world));

        TEST("host-owned: find returns a wrapped Body");
        bool ok = run_source(vm,
            "w = world.find(\"hero\")\n"
            "r = w.speed()\n"
            "n = world.find(\"nobody\")\n");
        CHECK(ok && near_to(vm.get_global("r"), 5.0) && is_nil(vm.get_global("n")),
              "find(hero) should wrap, find(nobody) should be None");

        TEST("World cannot be constructed from script");
        VM vm2;
        setup(vm2);
        def_class<World>(vm2, "World").method<&World::find>("find").constructable(false).end();
        CHECK(!run_source(vm2, "World()\n"), "constructable(false) ignored");

        TEST("GC never deletes a wrapped host object");
        int dtors_before = g_body_dtors;
        bool ok2 = run_source(vm,
            "for i in range(2000):\n"
            "    w = world.find(\"hero\")\n"
            "    w.set_hits(i)\n");
        vm.collect();
        vm.collect();
        CHECK(ok2 && g_body_dtors == dtors_before && world.hero.hits == 1999 &&
              near_to(vm.get_global("r"), 5.0),
              "wrapper destructor ran on host memory");
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("owned instances are deleted by the GC");
        int ctors = g_body_ctors, dtors = g_body_dtors;
        bool ok = run_source(vm,
            "for i in range(100):\n"
            "    b = Body(1, 2)\n"
            "b = None\n");
        vm.collect();
        vm.collect();
        CHECK(ok && g_body_ctors - ctors == 100 && g_body_dtors - dtors >= 99,
              "dtor did not run for GC-owned Body");
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("wrong argument type: error, no crash");
        bool ok = run_source(vm, "b = Body(3, 4)\nb.move(\"x\", 1)\n");
        CHECK(!ok && vm.had_error() && strstr(g_err, "move: argument 1") &&
              strstr(g_err, "must be a number") && strstr(g_err, "got string"),
              "type error not reported with function name and argument index");
        printf("      msg: %.*s\n", (int)strcspn(g_err, "\n"), g_err);
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("instance argument of the wrong class is rejected");
        bool ok = run_source(vm, "b = Body(3, 4)\nb.dist_to(5)\n");
        CHECK(!ok && strstr(g_err, "dist_to: argument 1") && strstr(g_err, "must be Body") &&
              strstr(g_err, "got int"),
              "dist_to(5) should report argument 1 must be Body");
        printf("      msg: %.*s\n", (int)strcspn(g_err, "\n"), g_err);

        TEST("None and a foreign native class are rejected too");
        World world;
        def_class<World>(vm, "World").method<&World::find>("find").constructable(false).end();
        vm.def_global("world", wrap<World>(vm, &world));
        g_err[0] = 0;
        bool none_rejected = !run_source(vm, "b = Body(3, 4)\nb.dist_to(None)\n") &&
                             strstr(g_err, "must be Body") && strstr(g_err, "got None");
        g_err[0] = 0;
        bool world_rejected = !run_source(vm, "b2 = Body(3, 4)\nb2.dist_to(world)\n") &&
                              strstr(g_err, "must be Body") && strstr(g_err, "got World");
        CHECK(none_rejected && world_rejected, "dist_to(None) / dist_to(world) must not reach the cast");
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("constructor type error is reported");
        bool ok = run_source(vm, "b = Body(\"a\", 2)\n");
        CHECK(!ok && strstr(g_err, "Body: argument 1") && strstr(g_err, "got string"),
              "ctor accepted a string for float");
        printf("      msg: %.*s\n", (int)strcspn(g_err, "\n"), g_err);
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("script subclass inherits native ctor and methods");
        bool ok = run_source(vm,
            "class Hero(Body):\n"
            "    def special(self):\n"
            "        return self.speed() * 2\n"
            "\n"
            "h = Hero(6, 8)\n"
            "r = h.special()\n");
        CHECK(ok && near_to(vm.get_global("r"), 20.0), "Hero(6, 8).special() should be 20");

        TEST("subclass __init__ runs after the native ctor");
        bool ok2 = run_source(vm,
            "class Boss(Body):\n"
            "    def __init__(self, x, y):\n"
            "        self.rage = 3\n"
            "\n"
            "bo = Boss(3, 4)\n"
            "r2 = bo.speed()\n"
            "r3 = bo.rage\n");
        CHECK(ok2 && near_to(vm.get_global("r2"), 5.0) && vm.get_global("r3").as.integer == 3,
              "native ctor should run first, then __init__");
    }

    {
        VM vm;
        setup(vm);
        def_fn<&dist>(vm, "dist");
        def_fn<&add_ints>(vm, "add_ints");
        TEST("def_fn: free functions");
        bool ok = run_source(vm, "r = dist(3, 4)\nr2 = add_ints(2, 3)\n");
        CHECK(ok && near_to(vm.get_global("r"), 5.0) && vm.get_global("r2").as.integer == 5,
              "dist(3, 4) should be 5.0, add_ints(2, 3) 5");

        TEST("free function type error");
        VM vm2;
        setup(vm2);
        def_fn<&dist>(vm2, "dist");
        CHECK(!run_source(vm2, "dist(3, None)\n") && strstr(g_err, "dist: argument 2") &&
                  strstr(g_err, "got None"),
              "None accepted as double");
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("integer out of range is an error, not a truncation");
        bool ok = run_source(vm, "b = Body(1, 1)\nb.set_hits(5000000000)\n");
        CHECK(!ok && strstr(g_err, "set_hits: argument 1") && strstr(g_err, "int in range"),
              "5000000000 was silently narrowed to int");
        printf("      msg: %.*s\n", (int)strcspn(g_err, "\n"), g_err);
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("Vec3 / Vec2 arguments and results (value types, no heap)");
        bool ok = run_source(vm,
            "b = Body(1, 1)\n"
            "b.move_by(Vec3(1, 2, 3))\n"
            "p = b.position()\n"
            "d = b.dot_xy(Vec2(2, 3))\n"
            "f = b.flat()\n");
        Value p = vm.get_global("p"), f = vm.get_global("f");
        CHECK(ok && is_vec3(p) && p.vx == 2.0f && p.as.v.y == 3.0f && p.as.v.z == 3.0f,
              "position() should be Vec3(2, 3, 3) after move_by(Vec3(1, 2, 3))");
        CHECK(is_vec2(f) && as_vec2(f).x == 2.0f && as_vec2(f).y == 3.0f,
              "flat() should be Vec2(2, 3)");
        CHECK(near_to(vm.get_global("d"), 13.0), "dot_xy(Vec2(2, 3)) on (2, 3) should be 13.0");
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("wrong vector type is an error naming Vec3 / Vec2");
        bool ok = run_source(vm, "b = Body(1, 1)\nb.move_by(1)\n");
        CHECK(!ok && strstr(g_err, "move_by: argument 1") && strstr(g_err, "must be Vec3") &&
                  strstr(g_err, "got int"),
              "move_by(1) should report argument 1 must be Vec3");
        printf("      msg: %.*s\n", (int)strcspn(g_err, "\n"), g_err);
        g_err[0] = 0;
        ok = run_source(vm, "b2 = Body(1, 1)\nb2.move_by(Vec2(1, 2))\n");
        CHECK(!ok && strstr(g_err, "must be Vec3") && strstr(g_err, "got Vec2"),
              "a Vec2 must not pass for a Vec3");
        g_err[0] = 0;
        ok = run_source(vm, "b3 = Body(1, 1)\nb3.dot_xy(Vec3(1, 2, 3))\n");
        CHECK(!ok && strstr(g_err, "must be Vec2") && strstr(g_err, "got Vec3"),
              "a Vec3 must not pass for a const Vec2 &");
    }

    printf("\n  passed: %d  failed: %d\n", g_tests_passed, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
