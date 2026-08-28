/* =========================================================
** zen_script_info.h — load-time introspection for script hosts
**
** An engine that embeds Zen loads a .py, finds "the script class" in it and
** calls a fixed set of hooks on instances (on_start, on_update, ...). All the
** facts a host needs for that are already recorded on ObjClass at compile
** time — methods with arities, class-body field defaults — but each host has
** been reading them by hand. This header is the shared reader, plus the
** load-time checks that turn today's silent failures into one diagnostic:
**
**   - def on_updte(self, dt):  compiles, registers, never runs. Nothing
**     complains — a hook the host does not call is just a method nobody
**     invokes. Reported here as a suspect name.
**   - def on_update(self):     wrong arity; today it errors on the first
**     frame, every frame. Reported once at load instead.
**   - a hook this host knows but never calls (on_draw ported to a host
**     with no draw pass): runs nothing, says nothing. Reported here.
**
** Everything returned borrows from the VM: names point into interned
** ObjStrings and stay valid while the class is alive. Nothing allocates.
** ========================================================= */

#ifndef ZEN_SCRIPT_INFO_H
#define ZEN_SCRIPT_INFO_H

#include "value.h"

namespace zen
{
    class VM;
    struct ObjClass;

    /* One hook the host may call on a script class. arity is the number of
    ** arguments the host passes — self not counted, matching both the VM's
    ** method arity and the host's own invoke(instance, slot, args, nargs):
    ** on_update(self, dt) → arity 1. A hook the host recognises but never
    ** invokes (kept for cross-engine portability) is listed with
    ** called = false, so defining it draws a warning instead of running
    ** nothing in silence. */
    struct ScriptHook
    {
        const char *name;
        int arity;
        bool called;
    };

    enum ScriptDiagSeverity
    {
        SCRIPT_DIAG_WARNING = 0,
        SCRIPT_DIAG_ERROR,
    };

    enum ScriptDiagCode
    {
        SCRIPT_DIAG_NO_CLASS = 0,     /* no class inherits the base */
        SCRIPT_DIAG_MULTIPLE_CLASSES, /* more than one class does */
        SCRIPT_DIAG_WRONG_ARITY,      /* hook declared with the wrong parameter count */
        SCRIPT_DIAG_SUSPECT_NAME,     /* method a typo away from a hook name */
        SCRIPT_DIAG_UNCALLED_HOOK,    /* hook defined; this host never calls it */
    };

    struct ScriptDiagnostic
    {
        ScriptDiagSeverity severity;
        ScriptDiagCode code;
        char message[192];
    };

    /* One property the Inspector can show: a class-body field with a literal
    ** default (bool, int, float or string) whose name has no leading '_'.
    ** value keeps the Zen type, so int stays int and float stays float —
    ** that distinction is what picks the widget. */
    struct ScriptPropertyInfo
    {
        const char *name;
        Value value;
    };

    /* Find the one script class in the VM's globals: a class whose parent
    ** chain reaches base_name (the base itself does not count). Returns it,
    ** or NULL after appending a NO_CLASS / MULTIPLE_CLASSES diagnostic.
    ** *num_diags is read as the count already in diags and updated. */
    ObjClass *find_script_class(VM &vm, const char *base_name,
                                ScriptDiagnostic *diags, int max_diags, int *num_diags);

    /* Check the class's own methods against the host's hook list. Appends to
    ** diags starting at num_diags, returns the new count. Dunder names are
    ** skipped; inherited methods are the parent's to check. */
    int check_script_contract(const ObjClass *klass,
                              const ScriptHook *hooks, int num_hooks,
                              ScriptDiagnostic *diags, int max_diags, int num_diags);

    /* Read the exported properties off the class body. Returns how many were
    ** written to out (at most max_out). */
    int script_class_properties(const ObjClass *klass,
                                ScriptPropertyInfo *out, int max_out);

} /* namespace zen */

#endif /* ZEN_SCRIPT_INFO_H */
