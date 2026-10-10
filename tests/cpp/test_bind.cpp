/*
** test_bind.cpp — zen_bind.hpp: member-function pointers become script methods.
**
** Covers constructor/method/property generation (attribute syntax b.x, b.x = v,
** b.x.y += 1 reaching the C++ member; accessor pairs; read-only properties; the
** methods x() / set_x() kept alongside), a leading VM* parameter,
** wrapping host-owned objects (and proving the GC never deletes them, also for
** a class with .dtor()),
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
using zen::bind::ClassOf;

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
    float mass = 2.5f;
    Vec3f vel{0, 0, 0};
    const char *label = "ship";
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
    void setPosition(const Vec3f &p) { x = p.x; y = p.y; z = p.z; }
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
        .getter<&Body::speed>("speed") /* read-only property + speed() */
        .method<&Body::alive>("alive")
        .method<&Body::name>("name")
        .method<&Body::tag>("tag")
        .method<&Body::dist_to>("dist_to")
        .method<&Body::same>("same")
        .method<&Body::move_by>("move_by")
        .property<&Body::position, &Body::setPosition>("position") /* accessor pair */
        .method<&Body::dot_xy>("dot_xy")
        .method<&Body::flat>("flat")
        .property<&Body::hits>("hits")
        .property<&Body::mass>("mass")
        .property<&Body::vel>("vel")
        .property<&Body::label>("label") /* const char * member: read-only */
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

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("attribute syntax: float / int members read and write the C++ member");
        bool ok = run_source(vm,
            "b = Body(3, 4)\n"
            "r0 = b.mass\n"
            "b.mass = 4.0\n"
            "b.mass += 1\n"
            "r1 = b.mass\n"
            "r2 = b.mass()\n"
            "b.hits = 7\n"
            "b.hits += 2\n"
            "r3 = b.hits\n"
            "b.set_hits(1)\n"
            "r4 = b.hits\n"
            "r5 = b.hits()\n");
        Body *host = ok ? zen_instance_data<Body>(vm.get_global("b")) : nullptr;
        CHECK(ok && host && near_to(vm.get_global("r0"), 2.5) && near_to(vm.get_global("r1"), 5.0) &&
                  near_to(vm.get_global("r2"), 5.0) && host->mass == 5.0f && vm.get_global("r3").as.integer == 9 &&
                  vm.get_global("r4").as.integer == 1 && vm.get_global("r5").as.integer == 1 && host->hits == 1,
              "float/int attribute forms did not reach the member");

        TEST("attribute syntax: Vec3f member (value type), b.vel.x += 5 writes back");
        ok = run_source(vm,
            "b.vel = Vec3(1, 2, 3)\n"
            "b.vel.x += 5\n"
            "b.vel.z = 9.5\n"
            "v = b.vel\n"
            "vx = b.vel.x\n"
            "v2 = b.vel()\n");
        Value v = vm.get_global("v");
        CHECK(ok && host && host->vel.x == 6 && host->vel.y == 2 && host->vel.z == 9.5f && is_vec3(v) &&
                  as_vec3(v).x == 6 && near_to(vm.get_global("vx"), 6.0) && is_vec3(vm.get_global("v2")) &&
                  as_vec3(vm.get_global("v2")).z == 9.5f,
              "Vec3f member attribute forms wrong");

        TEST("accessor pair property<&Body::position, &Body::setPosition>");
        ok = run_source(vm,
            "b.position = Vec3(10, 20, 30)\n"
            "b.position.y += 1\n"
            "p = b.position\n"
            "p2 = b.position()\n"
            "b.set_position(Vec3(1, 1, 1))\n"
            "p3 = b.position\n");
        CHECK(ok && host && host->x == 1 && host->y == 1 && host->z == 1 && is_vec3(vm.get_global("p")) &&
                  as_vec3(vm.get_global("p")).x == 10 && as_vec3(vm.get_global("p")).y == 21 &&
                  as_vec3(vm.get_global("p2")).z == 30 && as_vec3(vm.get_global("p3")).x == 1,
              "accessor pair wrong");

        TEST("getter<&Body::speed>: read-only property from a function, speed() kept");
        ok = run_source(vm, "b.position = Vec3(3, 4, 0)\nsp = b.speed\nsp2 = b.speed()\n");
        CHECK(ok && near_to(vm.get_global("sp"), 5.0) && near_to(vm.get_global("sp2"), 5.0), "speed property wrong");

        TEST("property read as a value is the value; a plain method read is still a bound method");
        ok = run_source(vm, "m = b.move\nm(1, 0)\nq = b.mass\n");
        CHECK(ok && host && host->x == 4 && near_to(vm.get_global("q"), 5.0), "bound method / property value wrong");
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("read-only property: writing a function-getter property is an error");
        bool ok = run_source(vm, "b = Body(3, 4)\nb.speed = 3\n");
        CHECK(!ok && strstr(g_err, "'Body.speed' is read-only"), "write to a getter-only property accepted");
        printf("      msg: %.*s\n", (int)strcspn(g_err, "\n"), g_err);
        TEST("const char * member property is read-only");
        VM vm2;
        setup(vm2);
        bind_all(vm2);
        bool r1 = run_source(vm2, "b = Body(3, 4)\nl = b.label\n");
        bool r2 = !run_source(vm2, "b.label = \"x\"\n") && strstr(g_err, "'Body.label' is read-only");
        CHECK(r1 && r2 && is_string(vm2.get_global("l")) && strcmp(safe_string_chars(vm2.get_global("l")), "ship") == 0,
              "label should read \"ship\" and reject writes");
        TEST("no script field is created by a rejected write");
        Value b = vm2.get_global("b");
        bool shadow = false;
        for (int i = 0; i < as_instance(b)->klass->num_fields; i++)
            shadow |= strcmp(as_instance(b)->klass->field_names[i]->chars, "label") == 0;
        CHECK(!shadow, "a script field named label appeared");
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("attribute write of the wrong type / out of range: error, member unchanged");
        bool a = !run_source(vm, "b = Body(3, 4)\nb.vel = 5\n") && strstr(g_err, "must be Vec3") && strstr(g_err, "got int");
        printf("\n      msg: %.*s\n", (int)strcspn(g_err, "\n"), g_err);
        g_err[0] = 0;
        bool c = !run_source(vm, "b.mass = \"heavy\"\n") && strstr(g_err, "must be a number") && strstr(g_err, "got string");
        g_err[0] = 0;
        bool d = !run_source(vm, "b.hits = 5000000000\n") && strstr(g_err, "int in range");
        Body *host = zen_instance_data<Body>(vm.get_global("b"));
        CHECK(a && c && d && host && host->mass == 2.5f && host->hits == 0 && host->vel.x == 0, "bad write accepted");
    }

    {
        VM vm;
        setup(vm);
        bind_all(vm);
        TEST("script subclass of a bound class inherits the properties (self.mass, self.vel.y)");
        bool ok = run_source(vm,
            "class Hero(Body):\n"
            "    def boost(self):\n"
            "        self.mass += 1\n"
            "        self.vel.y = 9\n"
            "        self.position = self.position + Vec3(1, 1, 1)\n"
            "        return self.mass\n"
            "\n"
            "class Boss(Hero):\n"
            "    def rage(self):\n"
            "        self.vel = self.vel * 2\n"
            "\n"
            "h = Hero(6, 8)\n"
            "r = h.boost()\n"
            "k = Boss(1, 1)\n"
            "k.vel = Vec3(1, 2, 3)\n"
            "k.rage()\n");
        Body *h = ok ? zen_instance_data<Body>(vm.get_global("h")) : nullptr;
        Body *k = ok ? zen_instance_data<Body>(vm.get_global("k")) : nullptr;
        bool no_phantom = true;
        ObjClass *hk = as_class(vm.get_global("Hero"));
        for (int i = 0; i < hk->num_fields; i++)
            no_phantom &= strcmp(hk->field_names[i]->chars, "mass") != 0 && strcmp(hk->field_names[i]->chars, "vel") != 0 &&
                          strcmp(hk->field_names[i]->chars, "position") != 0;
        CHECK(ok && h && k && near_to(vm.get_global("r"), 3.5) && h->vel.y == 9 && h->x == 7 && h->y == 9 && h->z == 1 &&
                  k->vel.x == 2 && k->vel.z == 6 && no_phantom,
              "subclass property access did not reach the C++ member");
    }

    {
        /* wrap<T>() of a class WITH .dtor(): borrowed, so the GC never deletes the host object */
        VM vm;
        setup(vm);
        bind_all(vm);
        Body host(1, 2);
        int dtors = g_body_dtors;
        vm.def_global("kept", wrap<Body>(vm, &host));
        TEST("wrap<T> of a class with .dtor(): borrowed, survives collect()");
        bool ok = run_source(vm,
            "for i in range(500):\n"
            "    t = kept\n"
            "kept.mass = 8.0\n");
        Value stray = wrap<Body>(vm, &host); /* a second wrapper that nothing roots */
        (void)stray;
        vm.collect();
        vm.collect();
        bool borrowed = is_instance(vm.get_global("kept")) && as_instance(vm.get_global("kept"))->borrowed &&
                        as_instance(vm.get_global("kept"))->klass == ClassOf<Body>::klass;
        CHECK(ok && borrowed && g_body_dtors == dtors && host.mass == 8.0f, "wrapper destructor ran on host memory");
        TEST("no hidden $ref class is registered any more");
        CHECK(vm.find_global("Body$ref") < 0, "Body$ref still defined");
        TEST("make_instance_borrowed on a script subclass of a .dtor() class");
        vm.run(Compiler().compile(&vm.get_gc(), &vm, "class Pilot(Body):\n    def f(self):\n        return self.mass\n", "<p>"));
        Value pv = vm.make_instance_borrowed(as_class(vm.get_global("Pilot")), &host);
        Value out = vm.invoke(pv, "f", nullptr, 0);
        pv = val_nil();
        vm.collect();
        vm.collect();
        CHECK(near_to(out, 8.0) && g_body_dtors == dtors, "script subclass of Body ran the dtor on a borrowed instance");
    }

    printf("\n  passed: %d  failed: %d\n", g_tests_passed, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
