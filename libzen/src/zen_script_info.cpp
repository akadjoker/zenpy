/* =========================================================
** zen_script_info.cpp — see include/zen/zen_script_info.h
** ========================================================= */

#include "zen_script_info.h"
#include "vm.h"
#include "object.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace zen
{
    /* =========================================================
    ** Diagnostics
    ** ========================================================= */

    static void push_diag(ScriptDiagnostic *diags, int max_diags, int *num_diags,
                          ScriptDiagSeverity severity, ScriptDiagCode code,
                          const char *fmt, ...)
    {
        if (*num_diags >= max_diags)
            return;
        ScriptDiagnostic &d = diags[(*num_diags)++];
        d.severity = severity;
        d.code = code;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(d.message, sizeof(d.message), fmt, ap);
        va_end(ap);
    }

    /* =========================================================
    ** find_script_class
    ** ========================================================= */

    static bool inherits_from(const ObjClass *klass, const char *base_name)
    {
        for (const ObjClass *p = klass->parent; p; p = p->parent)
            if (p->name && strcmp(p->name->chars, base_name) == 0)
                return true;
        return false;
    }

    ObjClass *find_script_class(VM &vm, const char *base_name,
                                ScriptDiagnostic *diags, int max_diags, int *num_diags)
    {
        ObjClass *found = nullptr;
        for (int i = 0; i < vm.num_globals(); i++)
        {
            Value v = vm.get_global(i);
            if (!is_class(v))
                continue;
            ObjClass *klass = as_class(v);
            if (!inherits_from(klass, base_name))
                continue;
            if (found)
            {
                push_diag(diags, max_diags, num_diags,
                          SCRIPT_DIAG_ERROR, SCRIPT_DIAG_MULTIPLE_CLASSES,
                          "script defines more than one %s class ('%s' and '%s') — "
                          "one file, one script class",
                          base_name, found->name ? found->name->chars : "?",
                          klass->name ? klass->name->chars : "?");
                return nullptr;
            }
            found = klass;
        }
        if (!found)
            push_diag(diags, max_diags, num_diags,
                      SCRIPT_DIAG_ERROR, SCRIPT_DIAG_NO_CLASS,
                      "script defines no class inheriting %s", base_name);
        return found;
    }

    /* =========================================================
    ** check_script_contract
    ** ========================================================= */

    /* Levenshtein with early exit — hook names are short, so two stack rows
    ** cover it. Returns a distance capped at limit + 1. */
    static int edit_distance(const char *a, const char *b, int limit)
    {
        int la = (int)strlen(a), lb = (int)strlen(b);
        if (la - lb > limit || lb - la > limit)
            return limit + 1;
        int prev[64], curr[64];
        if (lb >= 63)
            return limit + 1;
        for (int j = 0; j <= lb; j++)
            prev[j] = j;
        for (int i = 1; i <= la; i++)
        {
            curr[0] = i;
            int row_min = curr[0];
            for (int j = 1; j <= lb; j++)
            {
                int sub = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
                int del = prev[j] + 1;
                int ins = curr[j - 1] + 1;
                int m = sub < del ? sub : del;
                curr[j] = m < ins ? m : ins;
                if (curr[j] < row_min)
                    row_min = curr[j];
            }
            if (row_min > limit)
                return limit + 1;
            memcpy(prev, curr, sizeof(int) * (lb + 1));
        }
        return prev[lb];
    }

    /* A method value in the class map is an ObjFunc, a closure over one, or a
    ** native. Only script functions carry an arity worth checking here. */
    static const ObjFunc *method_func(Value v)
    {
        if (is_func(v))
            return as_func(v);
        if (is_closure(v))
            return as_closure(v)->func;
        return nullptr;
    }

    static bool is_dunder(const char *name)
    {
        return name[0] == '_' && name[1] == '_';
    }

    static const ScriptHook *hook_named(const ScriptHook *hooks, int num_hooks,
                                        const char *name)
    {
        for (int i = 0; i < num_hooks; i++)
            if (strcmp(hooks[i].name, name) == 0)
                return &hooks[i];
        return nullptr;
    }

    static bool class_defines(const ObjClass *klass, const char *name)
    {
        const ObjMap *m = klass->methods;
        if (!m)
            return false;
        for (int32_t i = 0; i < m->capacity; i++)
        {
            if (m->nodes[i].hash == 0xFFFFFFFFu)
                continue;
            const char *key = safe_string_chars(m->nodes[i].key);
            if (key && strcmp(key, name) == 0)
                return true;
        }
        return false;
    }

    int check_script_contract(const ObjClass *klass,
                              const ScriptHook *hooks, int num_hooks,
                              ScriptDiagnostic *diags, int max_diags, int num_diags)
    {
        const ObjMap *m = klass ? klass->methods : nullptr;
        if (!m)
            return num_diags;
        const char *cls = klass->name ? klass->name->chars : "?";

        for (int32_t i = 0; i < m->capacity; i++)
        {
            if (m->nodes[i].hash == 0xFFFFFFFFu)
                continue;
            const char *name = safe_string_chars(m->nodes[i].key);
            if (!name || is_dunder(name))
                continue;
            const ObjFunc *fn = method_func(m->nodes[i].value);
            if (!fn)
                continue;

            const ScriptHook *hook = hook_named(hooks, num_hooks, name);
            if (hook)
            {
                if (!hook->called)
                    push_diag(diags, max_diags, &num_diags,
                              SCRIPT_DIAG_WARNING, SCRIPT_DIAG_UNCALLED_HOOK,
                              "%s.%s: this host never calls %s — the method will not run",
                              cls, name, name);

                /* arity < 0 is variadic — anything goes. Defaults widen the
                ** acceptable range downwards. */
                if (fn->arity >= 0)
                {
                    int max_a = fn->arity;
                    int min_a = fn->arity - fn->default_count;
                    if (hook->arity < min_a || hook->arity > max_a)
                        push_diag(diags, max_diags, &num_diags,
                                  SCRIPT_DIAG_ERROR, SCRIPT_DIAG_WRONG_ARITY,
                                  "%s.%s takes %d parameter(s) after self, but the "
                                  "host passes %d",
                                  cls, name, fn->arity, hook->arity);
                }
                continue;
            }

            /* Not a hook: is it one typo away from one the class does not
            ** already define? (If the real hook is present too, a similar
            ** name is far more likely a deliberate helper.) */
            if ((int)strlen(name) < 4)
                continue;
            for (int h = 0; h < num_hooks; h++)
            {
                int d = edit_distance(name, hooks[h].name, 2);
                if (d >= 1 && d <= 2 && !class_defines(klass, hooks[h].name))
                {
                    push_diag(diags, max_diags, &num_diags,
                              SCRIPT_DIAG_WARNING, SCRIPT_DIAG_SUSPECT_NAME,
                              "%s.%s is never called — did you mean %s?",
                              cls, name, hooks[h].name);
                    break;
                }
            }
        }
        return num_diags;
    }

    /* =========================================================
    ** script_class_properties
    ** ========================================================= */

    static bool exportable(Value v)
    {
        return is_bool(v) || is_int(v) || is_float(v) || is_string(v);
    }

    int script_class_properties(const ObjClass *klass,
                                ScriptPropertyInfo *out, int max_out)
    {
        int n = 0;
        if (!klass)
            return 0;
        for (int32_t i = 0; i < klass->num_field_defaults && n < max_out; i++)
        {
            if (i >= klass->num_fields || !klass->field_names[i])
                continue;
            const char *name = klass->field_names[i]->chars;
            if (name[0] == '_')
                continue;
            Value v = klass->field_defaults[i];
            if (!exportable(v))
                continue;
            out[n].name = name;
            out[n].value = v;
            n++;
        }
        return n;
    }

} /* namespace zen */
