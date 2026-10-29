/*
** test_debug_hook.cpp — VM::set_debug_hook in zen_debug, the library with the
** hook in its dispatch: once a line, the frames, the named locals in scope.
*/
#include "vm.h"
#include "compiler.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace zen;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) printf("  %-50s", name)
#define PASS() do { printf("OK\n"); g_tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

static bool run_source(VM &vm, const char *source, const char *name = "<test>")
{
    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, name);
    if (!fn) return false;
    vm.run(fn);
    return true;
}

/* =========================================================
** Debug hook: once a line, the frames and the named locals in scope
** ========================================================= */
struct DebugRecord
{
    std::vector<int> lines;
    int depth_at_6 = -1;
    std::string outer_at_6;
    std::vector<std::string> locals_at_6;
    long long total_at_6 = -1;
};

static void debug_hook(VM *vm, void *user)
{
    DebugRecord *record = (DebugRecord *)user;
    const int line = vm->debug_frame_line(0);
    record->lines.push_back(line);
    if (line != 6 || record->depth_at_6 >= 0)
        return;
    record->depth_at_6 = vm->debug_frame_count();
    ObjFunc *outer = vm->debug_frame_function(1);
    record->outer_at_6 = outer && outer->name ? outer->name->chars : "";
    for (int i = 0; i < vm->debug_local_count(0); i++)
    {
        record->locals_at_6.push_back(vm->debug_local_name(0, i));
        if (std::strcmp(vm->debug_local_name(0, i), "total") == 0)
            record->total_at_6 = vm->debug_local_value(0, i).as.integer;
    }
}

static void test_debug_hook()
{
    printf("\n--- Debug hook ---\n");
    const char *source = "def add(a, b):\n"        /* 1 */
                         "    return a + b\n"      /* 2 */
                         "def run():\n"            /* 3 */
                         "    total = 0\n"         /* 4 */
                         "    total = add(total, 5)\n" /* 5 */
                         "    total = total + 1\n" /* 6 */
                         "    return total\n"      /* 7 */
                         "result = run()\n";       /* 8 */
    VM vm;
    DebugRecord record;
    const bool hooked = vm.set_debug_hook(&debug_hook, &record);
    const bool ran = run_source(vm, source, "debug.zen");
    vm.set_debug_hook(nullptr, nullptr);

    TEST("zen_debug takes the hook");
    CHECK(hooked, "set_debug_hook refused");

    TEST("hook sees each line once, in order");
    const std::vector<int> want = {1, 3, 8, 4, 5, 2, 5, 6, 7, 8};
    CHECK(ran && record.lines == want, "lines differ");

    TEST("frames: run() inside the module");
    CHECK(record.depth_at_6 == 2 && record.outer_at_6 == "<module>", "frame count or outer frame");

    TEST("locals in scope with their values");
    CHECK(record.locals_at_6.size() == 1 && record.locals_at_6[0] == "total" && record.total_at_6 == 5,
          "local names or values");

    TEST("value text as repr writes it");
    char text[16];
    const int length = vm.debug_value_text(val_int(42), text, sizeof text);
    char quoted[8];
    vm.debug_value_text(val_obj((Obj *)vm.make_string("hello world")), quoted, sizeof quoted);
    CHECK(length == 2 && std::strcmp(text, "42") == 0 && std::strcmp(quoted, "'hello ") == 0, "value text");

    TEST("no hook: nothing recorded");
    DebugRecord quiet;
    VM plain;
    CHECK(run_source(plain, source, "debug.zen") && quiet.lines.empty(), "ran with a hook");
}

/* =========================================================
** A local is in scope once its value is in, not from the start of
** its line: before that its register holds a temporary
** ========================================================= */
struct ScopeRecord
{
    std::vector<std::string> seen; /* "line:frame:name" */
};

static void scope_hook(VM *vm, void *user)
{
    ScopeRecord *record = (ScopeRecord *)user;
    const int line = vm->debug_frame_line(0);
    for (int depth = 0; depth < vm->debug_frame_count(); depth++)
        for (int i = 0; i < vm->debug_local_count(depth); i++)
            record->seen.push_back(std::to_string(line) + ":" + std::to_string(depth) + ":" +
                                   vm->debug_local_name(depth, i));
}

static bool seen(const ScopeRecord &record, const char *entry)
{
    for (const std::string &s : record.seen)
        if (s == entry)
            return true;
    return false;
}

static void test_local_scope()
{
    printf("\n--- Locals in scope after their assignment ---\n");
    const char *source = "def twice(x):\n"          /* 1 */
                         "    return x * 2\n"       /* 2 */
                         "def run():\n"             /* 3 */
                         "    a = 1\n"              /* 4 */
                         "    b = twice(a)\n"       /* 5 */
                         "    const c = a + b\n"    /* 6 */
                         "    d: int = c\n"         /* 7 */
                         "    return d\n"           /* 8 */
                         "run()\n";                 /* 9 */
    VM vm;
    ScopeRecord record;
    vm.set_debug_hook(&scope_hook, &record);
    const bool ran = run_source(vm, source, "scope.zen");
    vm.set_debug_hook(nullptr, nullptr);

    TEST("no local before its first assignment");
    CHECK(ran && !seen(record, "4:0:a") && seen(record, "5:0:a"), "a seen on its own line");

    TEST("caller's local hidden while it calls");
    CHECK(seen(record, "2:1:a") && !seen(record, "2:1:b") && seen(record, "6:0:b"), "b seen inside twice()");

    TEST("const and annotated locals the same");
    CHECK(!seen(record, "6:0:c") && seen(record, "7:0:c") && !seen(record, "7:0:d") && seen(record, "8:0:d"),
          "c or d seen before they hold a value");
}

int main()
{
    printf("=== Zen Debug Hook Tests ===\n");
    test_debug_hook();
    test_local_scope();
    printf("\nResults: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
