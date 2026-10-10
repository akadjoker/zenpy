#ifndef ZEN_COMMON_H
#define ZEN_COMMON_H

/*
** common.h — Base types and VM configuration.
*/

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef _MSC_VER
#ifndef __builtin_expect
#define __builtin_expect(expr, value) (expr)
#endif
#ifndef __builtin_unreachable
#define __builtin_unreachable() __assume(0)
#endif
#ifndef __builtin_prefetch
#define __builtin_prefetch(address, read_write, locality) ((void)0)
#endif
#endif

namespace zen
{

    /* VM limits */
    constexpr int kMaxRegs = 250;                   /* registers per call frame */
    constexpr int kMaxFrames = 2000;                /* max call depth */
    constexpr int kMaxFiberDepth = 64;              /* max nested fiber resumes (C stack) */
    constexpr int kMaxConstants = 65536;            /* constant pool per function */
    constexpr int kOperatorSlotCount = 18;          /* fixed object operator slots */
    /* Width of the table when bytecode minor was 1-2 (before __len__ became
    ** a slot). Frozen forever — it describes what those files already have
    ** on disk, not what the VM supports today. See bytecode.cpp read_class. */
    constexpr int kOperatorSlotCountV1 = 15;
    /* Globals are stored in a dynamic array (grows on demand).
    ** kInitGlobalCapacity = how big the table is allocated up front.
    ** kMaxGlobalsHard   = absolute ceiling (OP_GETGLOBAL/SETGLOBAL encode
    **                     the index as a 16-bit Bx field). */
    constexpr int kInitGlobalCapacity = 1024;
    constexpr int kMaxGlobalsHard = 65536;
    /* Backwards-compat alias: previously this was the hard cap. Kept so
    ** external code that referenced it still compiles. New code should use
    ** kMaxGlobalsHard for the ceiling, kInitGlobalCapacity for sizing. */
    constexpr int MAX_GLOBALS = kInitGlobalCapacity;
    /* GC tuning defaults (runtime-configurable: VM::set_gc_tuning, gc_tune()).
    ** The first collection runs at the minimum threshold; after a cycle the
    ** next one is scheduled at max(live_bytes * grow_factor, min_threshold).
    ** kGCFloorThreshold is the lowest minimum the setter accepts. */
    constexpr size_t kGCDefaultMinThreshold = 8 * 1024 * 1024; /* 8 MB */
    constexpr float kGCDefaultGrowFactor = 2.0f;               /* Lua pause 200% */
    constexpr size_t kGCFloorThreshold = 64 * 1024;            /* 64 KB */

    /* Forward declarations */
    class VM;
    struct Obj;
    struct ObjString;
    struct ObjFunc;
    struct ObjUpvalue;
    struct ObjClosure;
    struct ObjFiber;
    struct ObjArray;
    struct ObjMap;
    struct ObjSet;
    struct ObjClass;
    struct ObjInstance;
    struct NativeStructDef;
    struct ObjNativeStruct;

} /* namespace zen */


/* Keep a cold path out of its caller's body — used for the rare branches of
** the interpreter's big handlers, which otherwise inflate execute() and cost
** every opcode register pressure. */
#if defined(__GNUC__) || defined(__clang__)
#define ZEN_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define ZEN_NOINLINE __declspec(noinline)
#else
#define ZEN_NOINLINE
#endif

#endif /* ZEN_COMMON_H */
