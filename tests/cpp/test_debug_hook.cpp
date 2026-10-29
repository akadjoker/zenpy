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


int main()
{
    printf("=== Zen Debug Hook Tests ===\n");
    test_debug_hook();
    printf("\nResults: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
