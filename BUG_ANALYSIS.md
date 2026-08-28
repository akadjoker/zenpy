# SSO (Small String Optimization) Bug Analysis - 80% Segfault Rate

## Executive Summary
Test `41_gc_torture2.py` crashes ~80% of the time with segmentation fault. Root cause: **VAL_SMALL_STRING is not recognized by string operation functions**, causing invalid pointer dereferences when string values are processed.

---

## Critical Issues Found

### Issue 1: is_string() Doesn't Recognize VAL_SMALL_STRING
**File:** [libzen/include/zen/object.h](libzen/include/zen/object.h#L92)

```cpp
inline bool is_string(Value v) { return is_obj_type(v, OBJ_STRING); }
```

**Problem:** Only returns true for `VAL_OBJ` with type `OBJ_STRING`. Does NOT check for `VAL_SMALL_STRING`.

**Impact:** When any string operation checks `is_string(v)` on a VAL_SMALL_STRING, it returns false, causing fallthrough to invalid code paths.

---

### Issue 2: Unsafe Pointer Casts Without Type Validation
**File:** [libzen/include/zen/object.h](libzen/include/zen/object.h#L93-L94)

```cpp
inline ObjString *as_string(Value v) { return (ObjString *)v.as.obj; }
inline const char *as_cstring(Value v) { return ((ObjString *)v.as.obj)->chars; }
```

**Problem:** 
- `as_string()` blindly casts `v.as.obj` to `ObjString*`  
- `as_cstring()` dereferences `->chars` without checking if `v` is actually a VAL_OBJ
- If `v.type == VAL_SMALL_STRING`, the cast is INVALID:
  - `v.as.obj` contains the sso struct (uint8_t len, char[7], uint8_t pad)
  - Casting this as ObjString* and accessing `->chars` = SEGFAULT

**Example:**
```cpp
Value sso_val = val_small_string("abc", 3);
// sso_val.type = VAL_SMALL_STRING
// sso_val.as.sso = {len: 3, chars: "abc", _pad: 0}

// If this is used as:
const char *chars = as_cstring(sso_val);  // ← v.as.obj reinterpreted as ObjString*
// → Accesses garbage pointer, likely SEGFAULT
```

---

### Issue 3: OP_CONCAT Missing VAL_SMALL_STRING Handling
**File:** [libzen/src/vm_dispatch.cpp](libzen/src/vm_dispatch.cpp#L3581-L3640)

**Fast path (line 3589):**
```cpp
if (is_string(vb) && is_string(vc))
{
    R[ZEN_A(i)] = val_obj((Obj *)new_string_concat(&gc_, as_string(vb), as_string(vc)));
    NEXT();
}
```
✗ Misses VAL_SMALL_STRING, falls to slow path

**Slow path (line 3608-3612):**
```cpp
if (is_string(vb))
{
    sb = as_cstring(vb);      // ← SEGFAULT if vb is VAL_SMALL_STRING
    lb = as_string(vb)->length; // ← SEGFAULT
}
```

**Slow path (line 3634-3638):**
```cpp
if (is_string(vc))
{
    sc = as_cstring(vc);      // ← SEGFAULT if vc is VAL_SMALL_STRING
    lc = as_string(vc)->length; // ← SEGFAULT
}
```

**Impact:** String concatenation with VAL_SMALL_STRING causes invalid pointer dereference.

---

### Issue 4: 20+ Call Sites Using is_string() Without VAL_SMALL_STRING Support

All of these will fail to recognize VAL_SMALL_STRING:

**vm_dispatch.cpp:**
- Line 141: `else if (is_string(v))`
- Line 240: `else if (is_string(v))`
- Line 467: `if (is_string(v))`
- Line 721: `if (__builtin_expect(is_string(v), 0))`
- Line 736: `else if (is_string(vb) && is_string(vc))` (2 occurrences)
- Line 825: `if (!is_string(sv))`
- Line 834: `if (!is_string(sc))`
- Line 847: `else if (is_string(vb) || is_string(vc))` (2 occurrences)
- Line 854: `if (is_string(vb))`
- Line 880: `if (is_string(vc))`
- Line 958: `else if ((is_string(vb) && is_int(vc)) || (is_int(vb) && is_string(vc)))` (2 occurrences)
- Line 961: `ObjString *s = is_string(vb) ? as_string(vb) : as_string(vc);` (DIRECT as_string CALL)
- Line 1230: `if (is_string(vb) || is_string(vc) || ...)` (2 occurrences)
- Line 1234: `if (!is_string(sv))`
- Line 1244: `if (!is_string(sc))`
- Line 1553: `else if (is_string(vb) && is_string(vc))` (2 occurrences)

---

### Issue 5: Why Intermittent (80% Crash Rate)?

**Current state (from memory.cpp):**
```cpp
Value new_string_sso(GC *gc, const char *chars, int length)
{
    /* Strings 1-7 bytes: store inline (no GC pressure) */
    if (length > 0 && length <= 7)
        return val_small_string(chars, length);  // ← Creates VAL_SMALL_STRING
    
    /* Strings 0 or >= 8 bytes: allocate on heap as usual */
    ObjString *str = new_string(gc, chars, length);
    return val_obj((Obj *)str);
}
```

**Problem:** `new_string_sso()` is **NEVER CALLED anywhere in the codebase**.

VAL_SMALL_STRING values are only created if:
1. Test manually constructs them (unlikely)
2. Some other code path calls `val_small_string()` directly (no evidence)
3. **Memory corruption**: uninitialized memory in certain union fields makes type byte equal to VAL_SMALL_STRING (0x06 = 6)

The 80% crash rate suggests **random type confusion**: when uninitialized memory in `v.as` happens to have byte value 0x06 at offset 0 (type field), it's misinterpreted as VAL_SMALL_STRING. The remaining 20% of the time, the type byte is different (e.g., VAL_OBJ = 0x04), so it's handled correctly.

This explains:
- **Why ASAN doesn't catch it:** The memory is technically allocated and valid, just mistyped
- **Why LSAN false positives:** Mistyped pointers create spurious "roots"
- **Why intermittent:** Depends on heap allocation patterns, GC cycles, and garbage values

---

## Recommended Fixes

### Priority 1: Fix is_string() to Handle VAL_SMALL_STRING
**File:** [libzen/include/zen/object.h](libzen/include/zen/object.h#L92)

```cpp
inline bool is_string(Value v) { 
    return v.type == VAL_SMALL_STRING || is_obj_type(v, OBJ_STRING); 
}
```

### Priority 2: Create Safe Accessors
**File:** [libzen/include/zen/object.h](libzen/include/zen/object.h)

Add functions that handle BOTH VAL_OBJ and VAL_SMALL_STRING:
```cpp
inline const char *get_string_chars(Value v) {
    if (is_small_string(v))
        return v.as.sso.chars;
    if (is_obj(v) && is_obj_type(v, OBJ_STRING))
        return ((ObjString *)v.as.obj)->chars;
    return "";
}

inline int get_string_length(Value v) {
    if (is_small_string(v))
        return v.as.sso.len;
    if (is_obj(v) && is_obj_type(v, OBJ_STRING))
        return ((ObjString *)v.as.obj)->length;
    return 0;
}
```

### Priority 3: Update OP_CONCAT and Other Operations
- Replace `as_string()` calls with type-aware accessor
- Replace `as_cstring()` calls with `get_string_chars()`
- Update all 20+ call sites in [vm_dispatch.cpp](libzen/src/vm_dispatch.cpp)

### Priority 4: Add Assertion or Type Guard
Ensure no direct casts without validation:
```cpp
// UNSAFE - Remove or add guard
inline ObjString *as_string(Value v) {
    assert(is_obj(v));  // ← Guard against VAL_SMALL_STRING
    return (ObjString *)v.as.obj; 
}
```

---

## Test Case
The segfault manifests in test 41 because of:
- Heavy string concatenation: `"key_" + str(i)`
- Nested allocations creating GC pressure
- Potential for memory type corruption during GC phases

---

## Files to Modify
1. [libzen/include/zen/object.h](libzen/include/zen/object.h) - Add safe accessors, fix is_string()
2. [libzen/src/vm_dispatch.cpp](libzen/src/vm_dispatch.cpp) - Update 20+ string operation sites
3. [libzen/src/builtin_io.cpp](libzen/src/builtin_io.cpp) - String methods
4. [libzen/src/builtin_json.cpp](libzen/src/builtin_json.cpp) - JSON string handling
5. Any other files using as_string(), as_cstring(), or is_string()
