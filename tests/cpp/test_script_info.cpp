/*
** test_script_info.cpp — the load-time introspection a script host runs.
**
** Covers:
**   1. find_script_class finds the one class inheriting the base
**   2. no script class  → error diagnostic
**   3. two script classes → error diagnostic
**   4. properties read off the class body: types kept, '_' skipped
**   5. wrong hook arity → error at load, not on the first frame
**   6. on_updte → "did you mean on_update?"
**   7. a similar name is NOT flagged when the real hook is also defined
**   8. hook the host never calls → warning
**   9. a clean class → zero diagnostics
*/

#include "vm.h"
#include "compiler.h"
#include "module.h"
#include "memory.h"
#include "zen/zen_script_info.h"

#include <cstdio>
#include <cstring>

using namespace zen;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) printf("  %-52s", name)
#define PASS() do { printf("OK\n"); g_tests_passed++; } while (0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_tests_failed++; } while (0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while (0)

static bool run_source(VM &vm, const char *source)
{
    Compiler compiler;
    ObjFunc *fn = compiler.compile(&vm.get_gc(), &vm, source, "<info>");
    if (!fn)
        return false;
    vm.run(fn);
    return true;
}

/* The contract a typical 2D host would declare. Arity is what the host
** passes — self not counted: on_update(self, dt) → 1. */
static const ScriptHook kHooks[] = {
    {"on_start",   0, true},
    {"on_update",  1, true},
    {"on_draw",    0, false}, /* known, ported, never called by this host */
    {"on_destroy", 0, true},
};
static const int kNumHooks = 4;

static bool has_diag(const ScriptDiagnostic *diags, int n, ScriptDiagCode code)
{
    for (int i = 0; i < n; i++)
        if (diags[i].code == code)
            return true;
    return false;
}

int main()
{
    printf("=== Script load-time info ===\n");

    /* ---- 1: the happy path ---- */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm,
            "class ScriptComponent:\n"
            "    pass\n"
            "class Player(ScriptComponent):\n"
            "    speed = 200\n"
            "    def on_update(self, dt):\n"
            "        pass\n");

        ScriptDiagnostic diags[8];
        int nd = 0;
        TEST("finds the one script class");
        ObjClass *klass = find_script_class(vm, "ScriptComponent", diags, 8, &nd);
        CHECK(klass && strcmp(klass->name->chars, "Player") == 0 && nd == 0,
              "expected Player, no diagnostics");
    }

    /* ---- 2: no class ---- */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm,
            "class ScriptComponent:\n"
            "    pass\n"
            "x = 1\n");

        ScriptDiagnostic diags[8];
        int nd = 0;
        TEST("no script class is an error");
        ObjClass *klass = find_script_class(vm, "ScriptComponent", diags, 8, &nd);
        CHECK(!klass && has_diag(diags, nd, SCRIPT_DIAG_NO_CLASS),
              "expected NO_CLASS");
    }

    /* ---- 3: two classes ---- */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm,
            "class ScriptComponent:\n"
            "    pass\n"
            "class A(ScriptComponent):\n"
            "    pass\n"
            "class B(ScriptComponent):\n"
            "    pass\n");

        ScriptDiagnostic diags[8];
        int nd = 0;
        TEST("two script classes is an error");
        ObjClass *klass = find_script_class(vm, "ScriptComponent", diags, 8, &nd);
        CHECK(!klass && has_diag(diags, nd, SCRIPT_DIAG_MULTIPLE_CLASSES),
              "expected MULTIPLE_CLASSES");
    }

    /* ---- 4: properties ---- */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm,
            "class ScriptComponent:\n"
            "    pass\n"
            "class Player(ScriptComponent):\n"
            "    speed = 200\n"
            "    jump = 380.5\n"
            "    tag = \"hero\"\n"
            "    armed = True\n"
            "    _phase = 0.0\n");

        ScriptDiagnostic diags[8];
        int nd = 0;
        ObjClass *klass = find_script_class(vm, "ScriptComponent", diags, 8, &nd);
        ScriptPropertyInfo props[8];
        int np = script_class_properties(klass, props, 8);

        TEST("four exported properties, underscore skipped");
        CHECK(np == 4, "expected 4");

        bool speed_int = false, jump_float = false, tag_str = false, armed_bool = false;
        for (int i = 0; i < np; i++)
        {
            if (strcmp(props[i].name, "speed") == 0) speed_int = is_int(props[i].value) && props[i].value.as.integer == 200;
            if (strcmp(props[i].name, "jump") == 0) jump_float = is_float(props[i].value);
            if (strcmp(props[i].name, "tag") == 0) tag_str = is_string(props[i].value);
            if (strcmp(props[i].name, "armed") == 0) armed_bool = is_bool(props[i].value);
        }
        TEST("types survive: int stays int, float stays float");
        CHECK(speed_int && jump_float && tag_str && armed_bool, "a type was lost");
    }

    /* ---- 5: wrong arity ---- */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm,
            "class ScriptComponent:\n"
            "    pass\n"
            "class Player(ScriptComponent):\n"
            "    def on_update(self):\n"
            "        pass\n");

        ScriptDiagnostic diags[8];
        int nd = 0;
        ObjClass *klass = find_script_class(vm, "ScriptComponent", diags, 8, &nd);
        nd = check_script_contract(klass, kHooks, kNumHooks, diags, 8, nd);

        TEST("on_update(self) without dt is caught at load");
        CHECK(has_diag(diags, nd, SCRIPT_DIAG_WRONG_ARITY), "expected WRONG_ARITY");
    }

    /* ---- 6: the typo ---- */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm,
            "class ScriptComponent:\n"
            "    pass\n"
            "class Player(ScriptComponent):\n"
            "    def on_updte(self, dt):\n"
            "        pass\n");

        ScriptDiagnostic diags[8];
        int nd = 0;
        ObjClass *klass = find_script_class(vm, "ScriptComponent", diags, 8, &nd);
        nd = check_script_contract(klass, kHooks, kNumHooks, diags, 8, nd);

        TEST("on_updte suggests on_update");
        CHECK(has_diag(diags, nd, SCRIPT_DIAG_SUSPECT_NAME) &&
              strstr(diags[0].message, "on_update") != nullptr,
              "expected SUSPECT_NAME mentioning on_update");
    }

    /* ---- 7: helper beside the real hook is not flagged ---- */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm,
            "class ScriptComponent:\n"
            "    pass\n"
            "class Player(ScriptComponent):\n"
            "    def on_update(self, dt):\n"
            "        self.on_update2(dt)\n"
            "    def on_update2(self, dt):\n"
            "        pass\n");

        ScriptDiagnostic diags[8];
        int nd = 0;
        ObjClass *klass = find_script_class(vm, "ScriptComponent", diags, 8, &nd);
        nd = check_script_contract(klass, kHooks, kNumHooks, diags, 8, nd);

        TEST("on_update2 next to on_update is left alone");
        CHECK(!has_diag(diags, nd, SCRIPT_DIAG_SUSPECT_NAME), "false positive");
    }

    /* ---- 8: hook the host never calls ---- */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm,
            "class ScriptComponent:\n"
            "    pass\n"
            "class Player(ScriptComponent):\n"
            "    def on_draw(self):\n"
            "        pass\n");

        ScriptDiagnostic diags[8];
        int nd = 0;
        ObjClass *klass = find_script_class(vm, "ScriptComponent", diags, 8, &nd);
        nd = check_script_contract(klass, kHooks, kNumHooks, diags, 8, nd);

        TEST("on_draw on a host with no draw pass warns");
        CHECK(has_diag(diags, nd, SCRIPT_DIAG_UNCALLED_HOOK), "expected UNCALLED_HOOK");
    }

    /* ---- 9: a clean class ---- */
    {
        VM vm;
        vm.open_lib_globals(&zen_lib_base);
        run_source(vm,
            "class ScriptComponent:\n"
            "    pass\n"
            "class Player(ScriptComponent):\n"
            "    speed = 200\n"
            "    def on_start(self):\n"
            "        pass\n"
            "    def on_update(self, dt):\n"
            "        pass\n"
            "    def on_destroy(self):\n"
            "        pass\n"
            "    def fire(self, power):\n"
            "        pass\n");

        ScriptDiagnostic diags[8];
        int nd = 0;
        ObjClass *klass = find_script_class(vm, "ScriptComponent", diags, 8, &nd);
        nd = check_script_contract(klass, kHooks, kNumHooks, diags, 8, nd);

        TEST("a correct class draws no diagnostics");
        CHECK(klass != nullptr && nd == 0, "unexpected diagnostics");
    }

    printf("\n  passed: %d  failed: %d\n", g_tests_passed, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
