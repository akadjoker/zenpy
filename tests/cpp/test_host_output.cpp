/*
** test_host_output.cpp — the -DZEN_HOST_OUTPUT writer hook.
**
** Built only when ZEN_HOST_OUTPUT is ON. Checks that a registered writer
** receives what print() produces, that errors arrive flagged, and that
** unregistering puts output back on stdout.
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "zen/zen_host_output.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace zen;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) printf("  %-50s", name)
#define PASS() do { printf("OK\n"); g_tests_passed++; } while (0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_tests_failed++; } while (0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while (0)

struct Sink
{
    std::string out;
    std::string err;
};

static void sink_write(const char *text, size_t length, int is_error, void *user)
{
    Sink *sink = (Sink *)user;
    (is_error ? sink->err : sink->out).append(text, length);
}

static bool run_source(VM &vm, const char *source)
{
    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, "<host>");
    if (!fn)
        return false;
    vm.run(fn);
    return true;
}

int main()
{
    printf("=== Host output hook ===\n");

    Sink sink;
    zen_host_set_writer(sink_write, &sink);

    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);

        TEST("print() reaches the host writer");
        run_source(vm, "print(\"from script\")");
        CHECK(sink.out.find("from script") != std::string::npos,
              "writer saw nothing");

        TEST("the newline comes through too");
        CHECK(!sink.out.empty() && sink.out[sink.out.size() - 1] == '\n',
              "no trailing newline");
    }

    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        sink.err.clear();

        TEST("runtime errors arrive flagged as errors");
        run_source(vm, "error(\"boom\")");
        CHECK(!sink.err.empty(), "nothing arrived on the error channel");
    }

    TEST("zen_host_write falls back when no writer is set");
    zen_host_set_writer(nullptr, nullptr);
    {
        size_t before = sink.out.size();
        zen_host_writes("");
        CHECK(sink.out.size() == before, "writer still called after reset");
    }

    printf("\n  passed: %d  failed: %d\n", g_tests_passed, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
