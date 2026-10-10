#ifndef ZEN_VALUE_H
#define ZEN_VALUE_H

#include <cmath>
#include "common.h"

namespace zen
{

    /*
    ** Value — tagged union, 16 bytes.
    **
    ** Tipos imediatos (vivem dentro do Value, sem GC):
    **   NIL, BOOL, INT, FLOAT, VEC2, VEC3
    **
    ** Vec2/Vec3 are value types: their float components live inside the
    ** Value itself (x in the 4 bytes that used to be padding after the
    ** tag, y and z in the payload), so a vector costs no allocation and
    ** is invisible to the GC. The numeric types are the contiguous range
    ** NIL..FLOAT: `type <= VAL_FLOAT` ("is_numeric_type") is the one
    ** compare the arithmetic fast paths use, so a vector (or object)
    ** operand falls out of them into the slow branch.
    **
    ** Tipos heap (apontam para GCObject, geridos pelo GC):
    **   OBJ → ObjString, ObjFunc, ObjArray, ObjMap, ObjClass, ObjInstance
    **
    */

    enum ValueType : uint8_t
    {
        VAL_NIL,
        VAL_BOOL,
        VAL_INT,
        VAL_FLOAT,
        VAL_OBJ, /* qualquer objecto no heap (string, func, array...) */
        VAL_PTR, /* raw pointer (não gerido pelo GC) */
        VAL_VEC2, /* 2 x float, inline (vx, as.v.y) */
        VAL_VEC3, /* 3 x float, inline (vx, as.v.y, as.v.z) */
    };

    struct Value
    {
        ValueType type; /* offset 0 */
        float vx;       /* offset 4: x of VAL_VEC2 / VAL_VEC3 (former padding) */
        union
        {
            bool boolean;
            int64_t integer;
            double number;
            Obj *obj;
            void *pointer;
            struct
            {
                float y, z;
            } v; /* y, z of VAL_VEC2 (z == 0) / VAL_VEC3 */
        } as;    /* offset 8 */
    };
    static_assert(sizeof(Value) == 16, "Value must stay 16 bytes");
    static_assert(offsetof(Value, as) == 8, "Value payload must sit at offset 8");
    static_assert(offsetof(Value, vx) == 4, "Value::vx must sit in the padding");

    /* Plain C++ vectors for hosts: what a native takes with as_vec2/as_vec3
    ** and returns with val_vec2/val_vec3. */
    struct Vec2f
    {
        float x, y;
    };
    struct Vec3f
    {
        float x, y, z;
    };

    /* Constructores inline — sem overhead */
    inline Value val_nil()
    {
        Value v;
        v.type = VAL_NIL;
        return v;
    }
    inline Value val_bool(bool b)
    {
        Value v;
        v.type = VAL_BOOL;
        v.as.boolean = b;
        return v;
    }
    inline Value val_int(int64_t i)
    {
        Value v;
        v.type = VAL_INT;
        v.as.integer = i;
        return v;
    }
    inline Value val_float(double d)
    {
        Value v;
        v.type = VAL_FLOAT;
        v.as.number = d;
        return v;
    }
    inline Value val_obj(Obj *o)
    {
        Value v;
        v.type = VAL_OBJ;
        v.as.obj = o;
        return v;
    }
    inline Value val_ptr(void *p)
    {
        Value v;
        v.type = VAL_PTR;
        v.as.pointer = p;
        return v;
    }

    inline Value val_vec2(float x, float y)
    {
        Value v;
        v.type = VAL_VEC2;
        v.vx = x;
        v.as.v.y = y;
        v.as.v.z = 0.0f; /* deterministic: equality and hashing read all three */
        return v;
    }
    inline Value val_vec3(float x, float y, float z)
    {
        Value v;
        v.type = VAL_VEC3;
        v.vx = x;
        v.as.v.y = y;
        v.as.v.z = z;
        return v;
    }
    inline Value val_vec2(Vec2f f) { return val_vec2(f.x, f.y); }
    inline Value val_vec3(Vec3f f) { return val_vec3(f.x, f.y, f.z); }

    /* Type checks */
    inline bool is_nil(Value v) { return v.type == VAL_NIL; }
    inline bool is_bool(Value v) { return v.type == VAL_BOOL; }
    inline bool is_int(Value v) { return v.type == VAL_INT; }
    inline bool is_float(Value v) { return v.type == VAL_FLOAT; }
    inline bool is_obj(Value v) { return v.type == VAL_OBJ; }
    inline bool is_ptr(Value v) { return v.type == VAL_PTR; }
    inline void *as_ptr(Value v) { return v.as.pointer; }
    inline bool is_vec2(Value v) { return v.type == VAL_VEC2; }
    inline bool is_vec3(Value v) { return v.type == VAL_VEC3; }
    inline bool is_vec(Value v) { return v.type >= VAL_VEC2; }
    inline float vec_x(Value v) { return v.vx; }
    inline float vec_y(Value v) { return v.as.v.y; }
    inline float vec_z(Value v) { return v.as.v.z; }
    inline Vec2f as_vec2(Value v) { return Vec2f{v.vx, v.as.v.y}; }
    inline Vec3f as_vec3(Value v) { return Vec3f{v.vx, v.as.v.y, v.as.v.z}; }
    /* nil, bool, int, float: the types that coerce to a number. Objects,
    ** pointers and vectors are above VAL_FLOAT. */
    inline bool is_numeric_type(Value v) { return v.type <= VAL_FLOAT; }

    bool objects_equal(Obj *a, Obj *b);

    /* Truthiness — nil e false são falsy, tudo o resto truthy */
    inline bool is_truthy(Value v)
    {
        switch (v.type)
        {
        case VAL_NIL:   return false;
        case VAL_BOOL:  return v.as.boolean;
        case VAL_INT:   return v.as.integer != 0;
        case VAL_FLOAT: return v.as.number != 0.0;
        case VAL_VEC2:  return v.vx != 0.0f || v.as.v.y != 0.0f;
        case VAL_VEC3:  return v.vx != 0.0f || v.as.v.y != 0.0f || v.as.v.z != 0.0f;
        default:        return true; /* objects: refined in is_truthy_full() */
        }
    }

    /* Conversão numérica */
    inline double to_number(Value v)
    {
        if (v.type == VAL_INT)
            return (double)v.as.integer;
        if (v.type == VAL_FLOAT)
            return v.as.number;
        if (v.type == VAL_BOOL)
            return v.as.boolean ? 1.0 : 0.0; /* bool is a number, as in Python */
        return 0.0;
    }

    /* NaN/inf tests without <cmath>'s std:: overloads. */
    inline bool zen_isnan(double d) { return d != d; }
    inline bool zen_isinf(double d) { return d == HUGE_VAL || d == -HUGE_VAL; }
    inline bool zen_isfinite(double d) { return d == d && d != HUGE_VAL && d != -HUGE_VAL; }

    /* int, float or bool: the operands arithmetic and == accept as numbers. */
    inline bool is_numeric_like(Value v)
    {
        return v.type == VAL_INT || v.type == VAL_FLOAT || v.type == VAL_BOOL;
    }
    /* Ordering of two values as Python defines it for numbers, strings and
    ** lists (lexicographic, recursive). 0 for incomparable values. */
    int values_compare(Value a, Value b);
    /* Python's repr of a float: the shortest string that reads back to the
    ** same double, always with a '.' or an exponent (1.0, 0.1, 1e-05, inf). */
    int format_float_py(double d, char *buf, size_t cap);
    /* Same for a float32: the shortest string that reads back to the same
    ** float (0.1f prints 0.1, not 0.10000000149011612). */
    int format_float32_py(float f, char *buf, size_t cap);
    /* "Vec2(1.0, 2.0)" / "Vec3(1.0, 2.0, 3.0)" for a vector value. */
    int format_vec_py(Value v, char *buf, size_t cap);

    /* Conversão para inteiro (bitwise ops) */
    inline int64_t to_integer(Value v)
    {
        if (v.type == VAL_INT)
            return v.as.integer;
        if (v.type == VAL_FLOAT)
            return (int64_t)v.as.number;
        if (v.type == VAL_BOOL)
            return v.as.boolean ? 1 : 0;
        return 0;
    }

    /* Comparação de igualdade — fast path for int (most common in tables) */
    inline bool values_equal(Value a, Value b)
    {
        if (a.type == b.type) {
            if (__builtin_expect(a.type == VAL_INT, 1))
                return a.as.integer == b.as.integer;
            switch (a.type)
            {
            case VAL_NIL:
                return true;
            case VAL_BOOL:
                return a.as.boolean == b.as.boolean;
            case VAL_INT:
                __builtin_unreachable();
            case VAL_FLOAT:
                return a.as.number == b.as.number;
            case VAL_OBJ:
                return objects_equal(a.as.obj, b.as.obj);
            case VAL_PTR:
                return a.as.pointer == b.as.pointer;
            case VAL_VEC2:
                return a.vx == b.vx && a.as.v.y == b.as.v.y;
            case VAL_VEC3:
                return a.vx == b.vx && a.as.v.y == b.as.v.y && a.as.v.z == b.as.v.z;
            }
        }
        /* Mixed numbers: int/float by value, bool as 0/1 (True == 1 in Python) */
        if (is_numeric_like(a) && is_numeric_like(b))
            return to_number(a) == to_number(b);
        return false;
    }

    /* Deep equality for == operator (compares array/map contents recursively) */
    bool values_deep_equal(Value a, Value b);

} /* namespace zen */

#endif /* ZEN_VALUE_H */
