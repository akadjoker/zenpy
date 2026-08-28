#ifndef ZEN_MODULE_H
#define ZEN_MODULE_H

#include "value.h"

namespace zen
{
    class VM; /* forward */

    typedef int (*NativeFn)(VM *vm, Value *args, int nargs);

    /* Native flags. GC_SAFE promises the function roots every allocation it
    ** must keep (vm->root(v), or storing into an already-reachable object)
    ** before the next allocation. The VM then leaves the GC running while the
    ** function executes, instead of pausing it for the whole call — see
    ** call_native in vm_dispatch.cpp. Applies to script-made calls, whose
    ** argument window lives on the marked fiber stack; C++-side invoke()
    ** paths keep the pause (their arg window is a C array the GC never sees). */
    enum NativeFlags
    {
        ZEN_NATIVE_GC_SAFE = 1,
    };

    struct NativeReg
    {
        const char *name;
        NativeFn fn;
        int arity; /* -1 = variadic */
        int flags; /* NativeFlags; brace-inits without it read as 0 */
    };

    struct NativeConst
    {
        const char *name;
        Value value;
    };

    /* Called once when a lib is opened — use to register native structs/classes.
    ** Called before functions/constants are put in globals, so types are available
    ** when init_fn returns. VM is fully alive at this point. */
    typedef void (*NativeLibInitFn)(VM *vm);

    struct NativeLib
    {
        const char *name;            /* "math", "os" */
        const NativeReg *functions;  /* null-terminated array */
        int num_functions;
        const NativeConst *constants; /* null-terminated array (can be nullptr) */
        int num_constants;
        NativeLibInitFn init_fn;     /* optional — register structs/classes here */
    };

    /* Builtin library openers */
    extern const NativeLib zen_lib_base;  /* always open — no import needed */
    extern const NativeLib zen_lib_math;  /* import math */
    extern const NativeLib zen_lib_time;  /* import time */
    extern const NativeLib zen_lib_struct; /* import struct */
    extern const NativeLib zen_lib_numpy;  /* import numpy */
    extern const NativeLib zen_lib_io;     /* import io */
    extern const NativeLib zen_lib_os;     /* import os */
    extern const NativeLib zen_lib_path;   /* import path */
    extern const NativeLib zen_lib_json;   /* import json */
    extern const NativeLib zen_lib_net;    /* import net */
    extern const NativeLib zen_lib_http;   /* import http */

} /* namespace zen */

#endif /* ZEN_MODULE_H */
