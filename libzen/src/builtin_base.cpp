/* =========================================================
** builtin_base.cpp — Global builtin functions for Zen
**
** Always available (no import needed):
**   str(val)         → convert to string
**   int(val)         → convert to integer
**   float(val)       → convert to float
**   char(code)       → codepoint → 1-char string
**   ord(str)         → first char → codepoint int
**   typeof(val)      → type name as string
**   input(prompt?)   → read line from stdin
**   assert(cond,msg?)→ runtime error if !cond
**   error(msg)       → raise runtime error
**   range(a,b?,step?)→ array [a..b) with step
** ========================================================= */

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "module.h"
#include "vm.h"
#include "memory.h"
#include "platform_time.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace zen
{
    struct BaseFileData
    {
        ZenFile handle;
        bool closed;
    };

    static void *base_file_ctor(VM *vm, int argc, Value *args)
    {
        if (argc < 1 || !is_string(args[0]))
        {
            vm->runtime_error("File: expected string path");
            return nullptr;
        }
        const char *path = as_cstring(args[0]);
        const char *mode = "r";
        if (argc >= 2 && is_string(args[1]))
            mode = as_cstring(args[1]);

        const ZenCallbacks &cb = vm->callbacks();
        ZenFile h = cb.io.open(path, mode, cb.userdata);
        if (!h)
        {
            vm->runtime_error("File: cannot open '%s' (mode '%s')", path, mode);
            return nullptr;
        }
        BaseFileData *fd = (BaseFileData *)malloc(sizeof(BaseFileData));
        fd->handle = h;
        fd->closed = false;
        return fd;
    }

    static void base_file_dtor(VM *vm, void *data)
    {
        BaseFileData *fd = (BaseFileData *)data;
        if (fd && !fd->closed && fd->handle)
        {
            const ZenCallbacks &cb = vm->callbacks();
            cb.io.close(fd->handle, cb.userdata);
        }
        free(fd);
    }

    static int base_file_read(VM *vm, Value *args, int nargs)
    {
        BaseFileData *fd = zen_instance_data<BaseFileData>(args[-1]);
        if (!fd || fd->closed) { vm->runtime_error("File.read: file is closed"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        long size = -1;
        if (nargs >= 1 && is_int(args[0]))
        {
            size = (long)args[0].as.integer;
        }
        else
        {
            long cur = cb.io.seek(fd->handle, 0, 1, cb.userdata);
            long end = cb.io.seek(fd->handle, 0, 2, cb.userdata);
            size = end - cur;
            cb.io.seek(fd->handle, cur, 0, cb.userdata);
        }
        if (size <= 0) { args[0] = val_obj((Obj *)vm->make_string("", 0)); return 1; }
        if (size > 100 * 1024 * 1024) { vm->runtime_error("File.read: too large"); return -1; }
        char *buf = (char *)malloc((size_t)size + 1);
        if (!buf) { vm->runtime_error("File.read: OOM"); return -1; }
        long n = cb.io.read(fd->handle, buf, size, cb.userdata);
        if (n < 0) n = 0;
        buf[n] = '\0';
        ObjString *s = vm->make_string(buf, (int)n);
        free(buf);
        args[0] = val_obj((Obj *)s);
        return 1;
    }

    static int base_file_readlines(VM *vm, Value *args, int)
    {
        BaseFileData *fd = zen_instance_data<BaseFileData>(args[-1]);
        if (!fd || fd->closed) { vm->runtime_error("File.readlines: file is closed"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();

        long cur = cb.io.seek(fd->handle, 0, 1, cb.userdata);
        long end = cb.io.seek(fd->handle, 0, 2, cb.userdata);
        cb.io.seek(fd->handle, cur, 0, cb.userdata);
        long size = end - cur;

        GC *gc = &vm->get_gc();
        ObjArray *arr = new_array(gc);
        args[0] = val_obj((Obj *)arr); /* root array against GC during string creation */
        if (size <= 0) { return 1; }
        if (size > 100 * 1024 * 1024) { vm->runtime_error("File.readlines: too large"); return -1; }

        char *raw = (char *)malloc((size_t)size + 1);
        if (!raw) { vm->runtime_error("File.readlines: OOM"); return -1; }
        long n = cb.io.read(fd->handle, raw, size, cb.userdata);
        if (n < 0) n = 0;
        raw[n] = '\0';

        const char *p = raw;
        const char *e = raw + n;
        while (p < e)
        {
            const char *nl = (const char *)memchr(p, '\n', (size_t)(e - p));
            if (!nl) nl = e;
            int len = (int)(nl - p);
            if (len > 0 && p[len - 1] == '\r') len--;
            ObjString *line = vm->make_string(p, len);
            array_push(gc, as_array(args[0]), val_obj((Obj *)line));
            p = nl + 1;
        }
        free(raw);
        return 1;
    }

    static int base_file_write(VM *vm, Value *args, int nargs)
    {
        BaseFileData *fd = zen_instance_data<BaseFileData>(args[-1]);
        if (!fd || fd->closed) { vm->runtime_error("File.write: closed"); return -1; }
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("File.write: expected string");
            return -1;
        }
        ObjString *data = as_string(args[0]);
        const ZenCallbacks &cb = vm->callbacks();
        long n = cb.io.write(fd->handle, data->chars, (long)data->length, cb.userdata);
        args[0] = val_int((int64_t)n);
        return 1;
    }

    static int base_file_close(VM *vm, Value *args, int)
    {
        BaseFileData *fd = zen_instance_data<BaseFileData>(args[-1]);
        if (!fd || fd->closed) return 0;
        const ZenCallbacks &cb = vm->callbacks();
        cb.io.close(fd->handle, cb.userdata);
        fd->closed = true;
        return 0;
    }

    static int nat_open(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("open() expects path string and optional mode");
            return -1;
        }
        int file_idx = vm->find_global("File");
        if (file_idx < 0)
        {
            vm->runtime_error("open(): File class is not available");
            return -1;
        }
        Value file_class = vm->get_global(file_idx);
        if (!is_class(file_class))
        {
            vm->runtime_error("open(): File is not a class");
            return -1;
        }

        ObjClass *klass = as_class(file_class);
        ObjInstance *inst = new_instance(&vm->get_gc(), klass);

        ObjClass *ctor_src = klass;
        while (ctor_src && !ctor_src->native_ctor)
            ctor_src = ctor_src->parent;

        if (ctor_src && ctor_src->native_ctor)
        {
            int argc = nargs >= 2 ? 2 : 1;
            inst->native_data = ctor_src->native_ctor(vm, argc, args);
            if (vm->had_error()) return -1;
        }
        args[0] = val_obj((Obj *)inst);
        return 1;
    }


    /* =========================================================
    ** str(val) → string
    ** ========================================================= */
    static void py_append_value(VM *vm, std::string &out, Value v, bool repr);
    static bool kwarg_get(VM *vm, const char *name, Value *out);
    static bool py_truthy(Value v);
    static bool py_collect(VM *vm, Value v, std::vector<Value> &out, const char *who);

    static int nat_str(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1)
        {
            args[0] = val_obj((Obj *)vm->make_string(""));
            return 1;
        }
        Value v = args[0];
        if (is_string(v))
        {
            args[0] = v;
            return 1;
        }
        std::string out;
        py_append_value(vm, out, v, false); /* str(x) == what print(x) shows */
        args[0] = val_obj((Obj *)vm->make_string(out.c_str(), (int)out.size()));
        return 1;
    }

    /* =========================================================
    ** int(val) → integer
    ** ========================================================= */
    static int nat_int(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1)
        {
            args[0] = val_int(0);
            return 1;
        }
        Value v = args[0];
        if (is_int(v))
        {
            args[0] = v;
            return 1;
        }
        if (is_float(v))
        {
            args[0] = val_int((int64_t)v.as.number);
            return 1;
        }
        if (is_bool(v))
        {
            args[0] = val_int(v.as.boolean ? 1 : 0);
            return 1;
        }
        if (is_string(v))
        {
            ObjString *s = as_string(v);
            int base = 10;
            if (nargs >= 2 && is_int(args[1]))
                base = (int)args[1].as.integer;
            const char *p = s->chars;
            while (*p == ' ' || *p == '\t') p++;
            /* Python accepts the 0x/0o/0b prefix when it matches the base */
            bool neg = false;
            if (*p == '-') { neg = true; p++; } else if (*p == '+') p++;
            if (p[0] == '0' && ((base == 16 && (p[1] == 'x' || p[1] == 'X')) ||
                                (base == 8 && (p[1] == 'o' || p[1] == 'O')) ||
                                (base == 2 && (p[1] == 'b' || p[1] == 'B'))))
                p += 2;
            char *end;
            int64_t n = strtoll(p, &end, base);
            if (neg) n = -n;
            if (end == p)
                end = s->chars; /* nothing parsed: same error as before */
            if (end == s->chars)
            {
                vm->runtime_error("int(): cannot convert '%s' to int.", s->chars);
                return -1;
            }
            args[0] = val_int(n);
            return 1;
        }
        if (is_nil(v))
        {
            args[0] = val_int(0);
            return 1;
        }
        vm->runtime_error("int(): unsupported type.");
        return -1;
    }

    /* =========================================================
    ** float(val) → float
    ** ========================================================= */
    static int nat_float(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1)
        {
            args[0] = val_float(0.0);
            return 1;
        }
        Value v = args[0];
        if (is_float(v))
        {
            args[0] = v;
            return 1;
        }
        if (is_int(v))
        {
            args[0] = val_float((double)v.as.integer);
            return 1;
        }
        if (is_bool(v))
        {
            args[0] = val_float(v.as.boolean ? 1.0 : 0.0);
            return 1;
        }
        if (is_string(v))
        {
            ObjString *s = as_string(v);
            char *end;
            double d = strtod(s->chars, &end);
            if (end == s->chars)
            {
                vm->runtime_error("float(): cannot convert '%s' to float.", s->chars);
                return -1;
            }
            args[0] = val_float(d);
            return 1;
        }
        if (is_nil(v))
        {
            args[0] = val_float(0.0);
            return 1;
        }
        vm->runtime_error("float(): unsupported type.");
        return -1;
    }

    /* =========================================================
    ** char(code) → string (1 UTF-8 char from codepoint)
    ** ========================================================= */
    static int nat_char(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1)
        {
            vm->runtime_error("char() expects 1 argument.");
            return -1;
        }
        Value v = args[0];
        int64_t cp;
        if (is_int(v))
            cp = v.as.integer;
        else if (is_float(v))
            cp = (int64_t)v.as.number;
        else
        {
            vm->runtime_error("char() expects an integer codepoint.");
            return -1;
        }

        /* Encode UTF-8 */
        char buf[5];
        int len = 0;
        if (cp < 0x80)
        {
            buf[0] = (char)cp;
            len = 1;
        }
        else if (cp < 0x800)
        {
            buf[0] = (char)(0xC0 | (cp >> 6));
            buf[1] = (char)(0x80 | (cp & 0x3F));
            len = 2;
        }
        else if (cp < 0x10000)
        {
            buf[0] = (char)(0xE0 | (cp >> 12));
            buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
            buf[2] = (char)(0x80 | (cp & 0x3F));
            len = 3;
        }
        else if (cp < 0x110000)
        {
            buf[0] = (char)(0xF0 | (cp >> 18));
            buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
            buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
            buf[3] = (char)(0x80 | (cp & 0x3F));
            len = 4;
        }
        else
        {
            vm->runtime_error("char(): invalid codepoint %lld.", (long long)cp);
            return -1;
        }
        buf[len] = '\0';
        args[0] = val_obj((Obj *)vm->make_string(buf, len));
        return 1;
    }

    /* =========================================================
    ** ord(str) → int (codepoint of first char)
    ** ========================================================= */
    static int nat_ord(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("ord() expects a string argument.");
            return -1;
        }
        ObjString *s = as_string(args[0]);
        if (s->length == 0)
        {
            vm->runtime_error("ord(): empty string.");
            return -1;
        }
        /* Decode first UTF-8 char */
        const uint8_t *p = (const uint8_t *)s->chars;
        int64_t cp;
        if (p[0] < 0x80)
            cp = p[0];
        else if ((p[0] & 0xE0) == 0xC0)
            cp = ((int64_t)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        else if ((p[0] & 0xF0) == 0xE0)
            cp = ((int64_t)(p[0] & 0x0F) << 12) | ((int64_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        else if ((p[0] & 0xF8) == 0xF0)
            cp = ((int64_t)(p[0] & 0x07) << 18) | ((int64_t)(p[1] & 0x3F) << 12) |
                 ((int64_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        else
            cp = p[0]; /* fallback */

        args[0] = val_int(cp);
        return 1;
    }

    /* =========================================================
    ** Type check builtins — direct tag comparison, no string alloc
    ** ========================================================= */
    static int nat_isNil(VM *vm, Value *args, int nargs)
    { args[0] = val_bool(nargs >= 1 && is_nil(args[0])); return 1; }

    static int nat_isBool(VM *vm, Value *args, int nargs)
    { args[0] = val_bool(nargs >= 1 && is_bool(args[0])); return 1; }

    static int nat_isInt(VM *vm, Value *args, int nargs)
    { args[0] = val_bool(nargs >= 1 && is_int(args[0])); return 1; }

    static int nat_isFloat(VM *vm, Value *args, int nargs)
    { args[0] = val_bool(nargs >= 1 && is_float(args[0])); return 1; }

    static int nat_isNumber(VM *vm, Value *args, int nargs)
    { args[0] = val_bool(nargs >= 1 && (is_int(args[0]) || is_float(args[0]))); return 1; }

    static int nat_isString(VM *vm, Value *args, int nargs)
    { args[0] = val_bool(nargs >= 1 && is_string(args[0])); return 1; }

    static int nat_isArray(VM *vm, Value *args, int nargs)
    { args[0] = val_bool(nargs >= 1 && is_array(args[0])); return 1; }

    static int nat_isMap(VM *vm, Value *args, int nargs)
    { args[0] = val_bool(nargs >= 1 && is_map(args[0])); return 1; }

    static int nat_isFunction(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_bool(false); return 1; }
        Value v = args[0];
        bool r = is_obj(v) && (v.as.obj->type == OBJ_FUNC ||
                               v.as.obj->type == OBJ_CLOSURE ||
                               v.as.obj->type == OBJ_NATIVE);
        args[0] = val_bool(r);
        return 1;
    }

    /* =========================================================
    ** typeof(val) → string ("nil","bool","int","float","string",
    **                        "array","map","function","class","instance")
    ** ========================================================= */
    static int nat_typeof(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1)
        {
            args[0] = val_obj((Obj *)vm->make_string("nil"));
            return 1;
        }
        Value v = args[0];
        const char *name;
        switch (v.type)
        {
        case VAL_NIL:
            name = "nil";
            break;
        case VAL_BOOL:
            name = "bool";
            break;
        case VAL_INT:
            name = "int";
            break;
        case VAL_FLOAT:
            name = "float";
            break;
        case VAL_SMALL_STRING:
            name = "string";
            break;
        case VAL_OBJ:
        {
            switch (v.as.obj->type)
            {
            case OBJ_STRING:
                name = "string";
                break;
            case OBJ_ARRAY:
                name = "array";
                break;
            case OBJ_MAP:
                name = "map";
                break;
            case OBJ_FUNC:
            case OBJ_CLOSURE:
            case OBJ_NATIVE:
                name = "function";
                break;
            case OBJ_CLASS:
                name = "class";
                break;
            case OBJ_INSTANCE:
                name = "instance";
                break;
            case OBJ_BUFFER:
                name = "buffer";
                break;
            default:
                name = "object";
                break;
            }
            break;
        }
        default:
            name = "unknown";
            break;
        }
        args[0] = val_obj((Obj *)vm->make_string(name));
        return 1;
    }

    /* =========================================================
    ** input(prompt?) → string (reads line from stdin)
    ** ========================================================= */
    static int nat_input(VM *vm, Value *args, int nargs)
    {
        /* Print prompt if given */
        if (nargs >= 1 && is_string(args[0]))
        {
            ObjString *prompt = as_string(args[0]);
            fwrite(prompt->chars, 1, prompt->length, stdout);
            fflush(stdout);
        }

        /* Read line */
        char buf[4096];
        if (!fgets(buf, sizeof(buf), stdin))
        {
            args[0] = val_nil();
            return 1;
        }
        /* Strip trailing newline */
        int len = (int)strlen(buf);
        if (len > 0 && buf[len - 1] == '\n')
            len--;
        if (len > 0 && buf[len - 1] == '\r')
            len--;

        args[0] = val_obj((Obj *)vm->make_string(buf, len));
        return 1;
    }

    /* =========================================================
    ** assert(cond, msg?) → nil or runtime error
    ** ========================================================= */
    static int nat_assert(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1)
        {
            vm->runtime_error("assert() expects at least 1 argument.");
            return -1;
        }
        if (!is_truthy_full(args[0]))
        {
            if (nargs >= 2 && is_string(args[1]))
                vm->runtime_error("assertion failed: %s", as_string(args[1])->chars);
            else
                vm->runtime_error("assertion failed.");
            return -1;
        }
        args[0] = val_nil();
        return 1;
    }

    /* =========================================================
    ** error(msg) → raises runtime error (never returns normally)
    ** ========================================================= */
    static int nat_error(VM *vm, Value *args, int nargs)
    {
        if (nargs >= 1 && is_string(args[0]))
            vm->runtime_error("%s", as_string(args[0])->chars);
        else
            vm->runtime_error("error() called.");
        return -1;
    }

    /* =========================================================
    ** range(stop) or range(start, stop) or range(start, stop, step)
    ** → array of integers
    ** ========================================================= */
    static int nat_range(VM *vm, Value *args, int nargs)
    {
        int64_t start = 0, stop = 0, step = 1;

        if (nargs == 1)
        {
            stop = to_integer(args[0]);
        }
        else if (nargs == 2)
        {
            start = to_integer(args[0]);
            stop = to_integer(args[1]);
        }
        else if (nargs >= 3)
        {
            start = to_integer(args[0]);
            stop = to_integer(args[1]);
            step = to_integer(args[2]);
        }
        else
        {
            vm->runtime_error("range() expects 1-3 arguments.");
            return -1;
        }

        if (step == 0)
        {
            vm->runtime_error("range(): step cannot be 0.");
            return -1;
        }

        ObjRange *r = new_range(&vm->get_gc(), start, stop, step);
        args[0] = val_obj((Obj *)r);
        return 1;
    }

    /* =========================================================
    ** type(x) → string name of the type
    ** ========================================================= */

    static int nat_type(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        Value v = args[0];
        const char *name = nullptr;
        switch (v.type)
        {
        case VAL_NIL:   name = "NoneType"; break;
        case VAL_BOOL:  name = "bool"; break;
        case VAL_INT:   name = "int"; break;
        case VAL_FLOAT: name = "float"; break;
        case VAL_SMALL_STRING: name = "str"; break;
        case VAL_OBJ:
            switch (v.as.obj->type)
            {
            case OBJ_STRING:  name = "str"; break;
            case OBJ_ARRAY:   name = "list"; break;
            case OBJ_MAP:     name = "dict"; break;
            case OBJ_SET:     name = "set"; break;
            case OBJ_CLASS:   name = "type"; break;
            case OBJ_INSTANCE:
                name = as_instance(v)->klass->name->chars;
                break;
            case OBJ_FUNC: case OBJ_CLOSURE: case OBJ_NATIVE:
                name = "function"; break;
            case OBJ_BUFFER: name = "buffer"; break;
            default: name = "object"; break;
            }
            break;
        default: name = "unknown"; break;
        }
        args[0] = val_obj((Obj *)create_string(&vm->get_gc(), name, (int)strlen(name)));
        return 1;
    }

    /* =========================================================
    ** isinstance(obj, cls) → bool
    ** ========================================================= */

    /* isinstance(x, Cls), isinstance(x, (A, B)), and the builtin type
    ** functions as types: isinstance(1, int), isinstance("s", str), ... */
    static int py_isinstance_one(Value obj, Value cls)
    {
        if (is_class(cls))
        {
            if (!is_instance(obj)) return 0;
            ObjClass *target = as_class(cls);
            for (ObjClass *k = as_instance(obj)->klass; k; k = k->parent)
                if (k == target) return 1;
            return 0;
        }
        if (is_native(cls) && as_native(cls)->name)
        {
            const char *n = as_native(cls)->name->chars;
            if (!strcmp(n, "int")) return is_int(obj) || is_bool(obj);
            if (!strcmp(n, "float")) return is_float(obj);
            if (!strcmp(n, "str")) return is_string(obj);
            if (!strcmp(n, "bool")) return is_bool(obj);
            if (!strcmp(n, "list") || !strcmp(n, "tuple")) return is_array(obj);
            if (!strcmp(n, "dict")) return is_map(obj);
            if (!strcmp(n, "set")) return is_set(obj);
            if (!strcmp(n, "object")) return 1;
        }
        return -1;
    }
    static int nat_isinstance(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2) { vm->runtime_error("isinstance() takes two arguments"); return -1; }
        Value obj = args[0], cls = args[1];
        int r = -1;
        if (is_array(cls))
        {
            ObjArray *arr = as_array(cls);
            r = 0;
            for (int32_t i = 0; i < arr_count(arr) && r == 0; i++)
                r = py_isinstance_one(obj, arr->data[i]);
        }
        else
            r = py_isinstance_one(obj, cls);
        if (r < 0) { vm->runtime_error("isinstance() arg 2 must be a class or a tuple of classes"); return -1; }
        args[0] = val_bool(r == 1);
        return 1;
    }

    /* getattr(obj, name[, default]) / hasattr(obj, name): instance fields. */
    static bool py_get_field(Value obj, ObjString *name, Value *out)
    {
        if (!is_instance(obj)) return false;
        ObjInstance *inst = as_instance(obj);
        ObjClass *k = inst->klass;
        for (int32_t fi = 0; fi < k->num_fields; fi++)
        {
            ObjString *fn = k->field_names[fi];
            if (fn->length == name->length && memcmp(fn->chars, name->chars, (size_t)name->length) == 0)
            {
                *out = fi < inst->num_fields ? inst->fields[fi] : val_nil();
                return true;
            }
        }
        return false;
    }
    static int nat_getattr(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[1])) { vm->runtime_error("getattr() expects (object, name[, default])"); return -1; }
        Value v;
        if (py_get_field(args[0], as_string(args[1]), &v)) { args[0] = v; return 1; }
        if (nargs >= 3) { args[0] = args[2]; return 1; }
        vm->runtime_error("object has no attribute '%s'", as_string(args[1])->chars);
        return -1;
    }
    static int nat_hasattr(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[1])) { vm->runtime_error("hasattr() expects (object, name)"); return -1; }
        Value v;
        args[0] = val_bool(py_get_field(args[0], as_string(args[1]), &v));
        return 1;
    }

    /* =========================================================
    ** format(fmt, ...) → string
    **
    ** C-style format: %d %i %u %x %X %o %f %e %g %s %c %%
    ** Width/precision: %10d %-8s %.2f %04x
    ** ========================================================= */
    static int nat_format(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("format() expects (fmt_string, ...).");
            return -1;
        }

        ObjString *fmt = as_string(args[0]);
        const char *p = fmt->chars;
        const char *end = p + fmt->length;
        int arg_idx = 1; /* next arg to consume */

        /* Output buffer */
        int cap = 256;
        int len = 0;
        char *out = (char *)malloc((size_t)cap);

        auto grow = [&](int need) {
            if (len + need > cap)
            {
                while (cap < len + need)
                    cap *= 2;
                out = (char *)realloc(out, (size_t)cap);
            }
        };

        while (p < end)
        {
            if (*p != '%')
            {
                grow(1);
                out[len++] = *p++;
                continue;
            }
            p++; /* skip '%' */
            if (p >= end)
                break;

            /* %% → literal % */
            if (*p == '%')
            {
                grow(1);
                out[len++] = '%';
                p++;
                continue;
            }

            /* Parse flags: -, +, 0, space, # */
            char spec[32];
            int si = 0;
            spec[si++] = '%';

            while (p < end && (*p == '-' || *p == '+' || *p == '0' || *p == ' ' || *p == '#'))
            {
                if (si < 28)
                    spec[si++] = *p;
                p++;
            }
            /* Width */
            while (p < end && *p >= '0' && *p <= '9')
            {
                if (si < 28)
                    spec[si++] = *p;
                p++;
            }
            /* Precision */
            if (p < end && *p == '.')
            {
                if (si < 28)
                    spec[si++] = *p;
                p++;
                while (p < end && *p >= '0' && *p <= '9')
                {
                    if (si < 28)
                        spec[si++] = *p;
                    p++;
                }
            }

            if (p >= end)
                break;

            char conv = *p++;
            spec[si++] = conv;
            spec[si] = '\0';

            char tmp[128];
            int n = 0;

            switch (conv)
            {
            case 'd':
            case 'i':
            {
                if (arg_idx >= nargs)
                {
                    free(out);
                    vm->runtime_error("format(): not enough arguments for %%d.");
                    return -1;
                }
                /* Replace 'd'/'i' with lld for int64_t */
                spec[si - 1] = '\0';
                char full[36];
                snprintf(full, sizeof(full), "%slld", spec);
                int64_t val = is_int(args[arg_idx]) ? args[arg_idx].as.integer
                            : is_float(args[arg_idx]) ? (int64_t)args[arg_idx].as.number
                            : is_bool(args[arg_idx]) ? (args[arg_idx].as.boolean ? 1 : 0)
                            : 0;
                n = snprintf(tmp, sizeof(tmp), full, (long long)val);
                arg_idx++;
                break;
            }
            case 'u':
            {
                if (arg_idx >= nargs)
                {
                    free(out);
                    vm->runtime_error("format(): not enough arguments for %%u.");
                    return -1;
                }
                spec[si - 1] = '\0';
                char full[36];
                snprintf(full, sizeof(full), "%sllu", spec);
                uint64_t val = is_int(args[arg_idx]) ? (uint64_t)args[arg_idx].as.integer
                             : is_float(args[arg_idx]) ? (uint64_t)args[arg_idx].as.number
                             : 0;
                n = snprintf(tmp, sizeof(tmp), full, (unsigned long long)val);
                arg_idx++;
                break;
            }
            case 'x':
            case 'X':
            case 'o':
            {
                if (arg_idx >= nargs)
                {
                    free(out);
                    vm->runtime_error("format(): not enough arguments for %%%c.", conv);
                    return -1;
                }
                spec[si - 1] = '\0';
                char full[36];
                snprintf(full, sizeof(full), "%sll%c", spec, conv);
                uint64_t val = is_int(args[arg_idx]) ? (uint64_t)args[arg_idx].as.integer
                             : is_float(args[arg_idx]) ? (uint64_t)args[arg_idx].as.number
                             : 0;
                n = snprintf(tmp, sizeof(tmp), full, (unsigned long long)val);
                arg_idx++;
                break;
            }
            case 'f':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            {
                if (arg_idx >= nargs)
                {
                    free(out);
                    vm->runtime_error("format(): not enough arguments for %%%c.", conv);
                    return -1;
                }
                double val = is_float(args[arg_idx]) ? args[arg_idx].as.number
                           : is_int(args[arg_idx]) ? (double)args[arg_idx].as.integer
                           : 0.0;
                n = snprintf(tmp, sizeof(tmp), spec, val);
                arg_idx++;
                break;
            }
            case 's':
            case 'r':
            {
                if (arg_idx >= nargs)
                {
                    free(out);
                    vm->runtime_error("format(): not enough arguments for %%s.");
                    return -1;
                }
                /* %s: the text print() shows; %r: repr (strings quoted) */
                std::string pys;
                py_append_value(vm, pys, args[arg_idx], conv == 'r');
                const char *sv = pys.c_str();
                int sl = (int)pys.size();

                /* If spec is just "%s", skip snprintf overhead */
                if (si == 2)
                {
                    grow(sl);
                    memcpy(out + len, sv, (size_t)sl);
                    len += sl;
                    arg_idx++;
                    continue;
                }
                spec[si - 1] = 's';
                n = snprintf(tmp, sizeof(tmp), spec, sv);
                arg_idx++;
                break;
            }
            case 'c':
            {
                if (arg_idx >= nargs)
                {
                    free(out);
                    vm->runtime_error("format(): not enough arguments for %%c.");
                    return -1;
                }
                int ch = is_int(args[arg_idx]) ? (int)args[arg_idx].as.integer : '?';
                n = snprintf(tmp, sizeof(tmp), spec, ch);
                arg_idx++;
                break;
            }
            default:
                /* Unknown specifier — output as-is */
                grow(si);
                memcpy(out + len, spec, (size_t)si);
                len += si;
                continue;
            }

            if (n > 0)
            {
                grow(n);
                memcpy(out + len, tmp, (size_t)n);
                len += n;
            }
        }

        args[0] = val_obj((Obj *)vm->make_string(out, len));
        free(out);
        return 1;
    }

    /* =========================================================
    ** collect() → int (freed bytes)
    ** Force a garbage collection cycle.
    ** ========================================================= */
    static int nat_collect(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        GC *gc = &vm->get_gc();
        size_t before = gc->bytes_allocated;
        gc_collect(vm);
        int64_t freed = (int64_t)(before - gc->bytes_allocated);
        args[0] = val_int(freed);
        return 1;
    }

    /* =========================================================
    ** gc pause
    ** pause() → nil
    ** ========================================================= */
    static int nat_gc_pause(VM *vm, Value *args, int nargs)
    {
        (void)args; (void)nargs;
        GC *gc = &vm->get_gc();
        gc_pause(gc);
        return 0;
    }

    /* =========================================================
    ** gc resume
    ** resume() → nil
    ** ========================================================= */
    static int nat_gc_resume(VM *vm, Value *args, int nargs)
    {
        (void)args; (void)nargs;
        GC *gc = &vm->get_gc();
        gc_resume(gc);
        return 0;
    }

    /* =========================================================
    ** mem_used() → int (bytes currently allocated by GC)
    ** ========================================================= */
    static int nat_mem_used(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        GC *gc = &vm->get_gc();
        args[0] = val_int((int64_t)gc->bytes_allocated);
        return 1;
    }

    /* =========================================================
    ** mem_info() → map { "used", "next_gc", "objects" }
    ** ========================================================= */
    static int nat_mem_info(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        GC *gc = &vm->get_gc();

        /* Count objects in linked list */
        int64_t obj_count = 0;
        for (Obj *o = gc->objects; o != nullptr; o = o->gc_next)
            obj_count++;

        // map_set(gc, map, val_obj((Obj *)k_used), val_int((int64_t)gc->bytes_allocated));
        // map_set(gc, map, val_obj((Obj *)k_next), val_int((int64_t)gc->next_gc));
        // map_set(gc, map, val_obj((Obj *)k_objs), val_int(obj_count));

        args[0] = val_int(obj_count);
        args[1] = val_int((int64_t)gc->bytes_allocated);
        args[2] = val_int((int64_t)gc->next_gc);
        return 3;
    }

    /* clock() → float seconds since process start (monotonic) */
    static int nat_clock(VM *vm, Value *args, int nargs)
    {
        (void)vm; (void)nargs;
        args[0] = val_float(platform_monotonic_seconds());
        return 1;
    }
 



    /* =========================================================
    ** enumerate(iterable) → [[0,v0],[1,v1],...]
    ** ========================================================= */
    static int nat_enumerate(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        GC *gc = &vm->get_gc();
        Value iterable = args[0];
        ObjArray *result = new_array(gc);

        if (is_obj(iterable) && iterable.as.obj->type == OBJ_ARRAY)
        {
            ObjArray *src = as_array(iterable);
            for (int32_t i = 0; i < arr_count(src); i++)
            {
                ObjArray *pair = new_array(gc);
                array_push(gc, pair, val_int(i));
                array_push(gc, pair, src->data[i]);
                array_push(gc, result, val_obj((Obj *)pair));
            }
        }
        else if (is_obj(iterable) && iterable.as.obj->type == OBJ_STRING)
        {
            ObjString *s = as_string(iterable);
            for (int32_t i = 0; i < (int32_t)s->length; i++)
            {
                ObjArray *pair = new_array(gc);
                array_push(gc, pair, val_int(i));
                ObjString *ch = create_string(gc, s->chars + i, 1);
                array_push(gc, pair, val_obj((Obj *)ch));
                array_push(gc, result, val_obj((Obj *)pair));
            }
        }

        args[0] = val_obj((Obj *)result);
        return 1;
    }

    /* =========================================================
    ** zip(a, b, ...) → [[a0,b0],[a1,b1],...]
    ** ========================================================= */
    static int nat_zip(VM *vm, Value *args, int nargs)
    {
        if (nargs == 0) { args[0] = val_obj((Obj *)new_array(&vm->get_gc())); return 1; }
        GC *gc = &vm->get_gc();

        /* Any iterables (lists, strings, dicts, sets, ranges) */
        std::vector<std::vector<Value>> cols(nargs);
        size_t min_len = SIZE_MAX;
        for (int a = 0; a < nargs; a++)
        {
            if (!py_collect(vm, args[a], cols[a], "zip")) return -1;
            if (cols[a].size() < min_len) min_len = cols[a].size();
        }
        if (min_len == SIZE_MAX) min_len = 0;

        ObjArray *result = new_array(gc);
        for (size_t i = 0; i < min_len; i++)
        {
            ObjArray *tuple = new_array(gc);
            for (int a = 0; a < nargs; a++)
                array_push(gc, tuple, cols[a][i]);
            array_push(gc, result, val_obj((Obj *)tuple));
        }

        args[0] = val_obj((Obj *)result);
        return 1;
    }

    /* =========================================================
    ** map(fn, iterable) → [fn(v0), fn(v1), ...]
    ** ========================================================= */
    static int nat_map(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        GC *gc = &vm->get_gc();
        Value fn = args[0];
        Value iterable = args[1];

        if (!is_obj(iterable) || iterable.as.obj->type != OBJ_ARRAY)
        {
            vm->runtime_error("map: second argument must be an array");
            return 0;
        }

        ObjArray *src = as_array(iterable);
        ObjArray *result = new_array(gc);
        Value call_arg[1];
        for (int32_t i = 0; i < arr_count(src); i++)
        {
            call_arg[0] = src->data[i];
            Value r = vm->call_fn(fn, call_arg, 1);
            array_push(gc, result, r);
        }

        args[0] = val_obj((Obj *)result);
        return 1;
    }

    /* =========================================================
    ** filter(fn, iterable) → [v for v in iterable if fn(v)]
    ** ========================================================= */
    static int nat_filter(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        GC *gc = &vm->get_gc();
        Value fn = args[0];
        Value iterable = args[1];

        if (!is_obj(iterable) || iterable.as.obj->type != OBJ_ARRAY)
        {
            vm->runtime_error("filter: second argument must be an array");
            return 0;
        }

        ObjArray *src = as_array(iterable);
        ObjArray *result = new_array(gc);
        Value call_arg[1];
        for (int32_t i = 0; i < arr_count(src); i++)
        {
            call_arg[0] = src->data[i];
            Value r = vm->call_fn(fn, call_arg, 1);
            if (py_truthy(r))
                array_push(gc, result, src->data[i]);
        }

        args[0] = val_obj((Obj *)result);
        return 1;
    }

    /* =========================================================
    ** Registration table
    ** ========================================================= */

    /* ---- Typed Array constructors ---- */
    /* Int8Array(n), Uint8Array(n), Float32Array([1,2,3]), etc. */

    static int nat_typed_array(VM *vm, Value *args, int nargs, BufferType btype)
    {
        if (nargs != 1) { vm->runtime_error("typed array expects 1 argument"); return -1; }
        GC *gc = &vm->get_gc();
        Value arg = args[0];

        if (is_int(arg)) {
            int32_t count = arg.as.integer;
            if (count < 0) { vm->runtime_error("buffer size must be non-negative"); return -1; }
            ObjBuffer *buf = new_buffer(gc, btype, count);
            args[0] = val_obj((Obj *)buf);
            return 1;
        }
        if (is_array(arg)) {
            ObjArray *src = as_array(arg);
            int32_t count = arr_count(src);
            ObjBuffer *buf = new_buffer(gc, btype, count);
            for (int32_t i = 0; i < count; i++) {
                double v = 0;
                if (is_int(src->data[i])) v = (double)src->data[i].as.integer;
                else if (is_float(src->data[i])) v = src->data[i].as.number;
                buffer_set(buf, i, v);
            }
            args[0] = val_obj((Obj *)buf);
            return 1;
        }
        vm->runtime_error("typed array expects int or array argument");
        return -1;
    }

    static int nat_Int8Array(VM *vm, Value *args, int n)    { return nat_typed_array(vm, args, n, BUF_INT8); }
    static int nat_Int16Array(VM *vm, Value *args, int n)   { return nat_typed_array(vm, args, n, BUF_INT16); }
    static int nat_Int32Array(VM *vm, Value *args, int n)   { return nat_typed_array(vm, args, n, BUF_INT32); }
    static int nat_Uint8Array(VM *vm, Value *args, int n)   { return nat_typed_array(vm, args, n, BUF_UINT8); }
    static int nat_Uint16Array(VM *vm, Value *args, int n)  { return nat_typed_array(vm, args, n, BUF_UINT16); }
    static int nat_Uint32Array(VM *vm, Value *args, int n)  { return nat_typed_array(vm, args, n, BUF_UINT32); }
    static int nat_Float32Array(VM *vm, Value *args, int n) { return nat_typed_array(vm, args, n, BUF_FLOAT32); }
    static int nat_Float64Array(VM *vm, Value *args, int n) { return nat_typed_array(vm, args, n, BUF_FLOAT64); }


    /* =========================================================
    ** Python core builtins: abs, min, max, round, sum, sorted, reversed,
    ** any, all, list, tuple, set, dict, bool, repr, chr, hex, oct, bin,
    ** divmod, pow, callable. Iterables are arrays, strings (characters),
    ** maps (keys) and sets.
    ** ========================================================= */
#define ZEN_INTLIKE_V(v) ((v).type == VAL_INT || (v).type == VAL_BOOL)
    static bool py_truthy(Value v)
    {
        if (is_array(v)) return arr_count(as_array(v)) > 0;
        if (is_string(v)) return as_string(v)->length > 0;
        if (is_map(v)) return as_map(v)->count > 0;
        if (is_set(v)) return as_set(v)->count > 0;
        if (is_range(v))
        {
            ObjRange *r = as_range(v);
            return r->step > 0 ? r->start < r->stop : (r->step < 0 && r->start > r->stop);
        }
        return is_truthy(v);
    }

    static bool py_collect(VM *vm, Value v, std::vector<Value> &out, const char *who)
    {
        if (is_array(v))
        {
            ObjArray *a = as_array(v);
            out.assign(a->data, a->data + arr_count(a));
            return true;
        }
        if (is_string(v))
        {
            ObjString *s = as_string(v);
            for (int i = 0; i < s->length; i++)
                out.push_back(val_obj((Obj *)vm->make_string(s->chars + i, 1)));
            return true;
        }
        if (is_map(v))
        {
            ObjMap *m = as_map(v);
            for (int32_t i = 0; i < m->capacity; i++)
                if (m->nodes[i].hash != 0xFFFFFFFFu)
                    out.push_back(m->nodes[i].key);
            return true;
        }
        if (is_set(v))
        {
            ObjSet *st = as_set(v);
            for (int32_t i = 0; i < st->capacity; i++)
                if (st->nodes[i].hash != 0xFFFFFFFFu)
                    out.push_back(st->nodes[i].key);
            return true;
        }
        if (is_range(v))
        {
            ObjRange *r = as_range(v);
            if (r->step > 0)
                for (int64_t x = r->start; x < r->stop; x += r->step) out.push_back(val_int(x));
            else if (r->step < 0)
                for (int64_t x = r->start; x > r->stop; x += r->step) out.push_back(val_int(x));
            return true;
        }
        vm->runtime_error("%s: object is not iterable", who);
        return false;
    }

    static Value py_array_from(VM *vm, const std::vector<Value> &items)
    {
        GC *gc = &vm->get_gc();
        ObjArray *r = new_array(gc);
        for (const Value &x : items)
            array_push(gc, r, x);
        return val_obj((Obj *)r);
    }

    /* Text of a value exactly as print() shows it (repr: strings quoted). */
    static void py_append_value(VM *vm, std::string &out, Value v, bool repr)
    {
        char buf[64];
        if (is_nil(v)) out += "None";
        else if (is_bool(v)) out += v.as.boolean ? "True" : "False";
        else if (is_int(v)) { snprintf(buf, sizeof buf, "%lld", (long long)v.as.integer); out += buf; }
        else if (is_float(v)) { format_float_py(v.as.number, buf, sizeof buf); out += buf; }
        else if (is_string(v))
        {
            ObjString *str = as_string(v);
            if (repr) out += '\'';
            out.append(str->chars, (size_t)str->length);
            if (repr) out += '\'';
        }
        else if (is_array(v))
        {
            ObjArray *a = as_array(v);
            out += '[';
            for (int32_t i = 0; i < arr_count(a); i++)
            {
                if (i) out += ", ";
                py_append_value(vm, out, a->data[i], true);
            }
            out += ']';
        }
        else if (is_map(v))
        {
            ObjMap *m = as_map(v);
            out += '{';
            bool first = true;
            for (int32_t i = 0; i < m->capacity; i++)
            {
                if (m->nodes[i].hash == 0xFFFFFFFFu) continue;
                if (!first) out += ", ";
                first = false;
                py_append_value(vm, out, m->nodes[i].key, true);
                out += ": ";
                py_append_value(vm, out, m->nodes[i].value, true);
            }
            out += '}';
        }
        else if (is_set(v))
        {
            ObjSet *st = as_set(v);
            if (st->count == 0) { out += "set()"; return; }
            out += '{';
            bool first = true;
            for (int32_t i = 0; i < st->capacity; i++)
            {
                if (st->nodes[i].hash == 0xFFFFFFFFu) continue;
                if (!first) out += ", ";
                first = false;
                py_append_value(vm, out, st->nodes[i].key, true);
            }
            out += '}';
        }
        else if (is_instance(v))
        {
            ObjInstance *inst = as_instance(v);
            Value slot = inst->klass->operator_slots[VM::SLOT_STR];
            if (!is_nil(slot))
            {
                Value r = vm->invoke_operator(v, VM::SLOT_STR, nullptr, 0);
                if (is_string(r)) { out.append(as_string(r)->chars, (size_t)as_string(r)->length); return; }
            }
            out += '<'; out += inst->klass->name->chars; out += " object>";
        }
        else if (is_class(v)) { out += "<class '"; out += as_class(v)->name->chars; out += "'>"; }
        else out += "<object>";
    }

    static int nat_repr(VM *vm, Value *args, int nargs)
    {
        std::string out;
        py_append_value(vm, out, nargs > 0 ? args[0] : val_nil(), true);
        args[0] = val_obj((Obj *)vm->make_string(out.c_str(), (int)out.size()));
        return 1;
    }

    static int nat_abs(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { vm->runtime_error("abs() takes one argument"); return -1; }
        Value v = args[0];
        if (is_int(v)) args[0] = val_int(v.as.integer < 0 ? -v.as.integer : v.as.integer);
        else if (is_float(v)) args[0] = val_float(fabs(v.as.number));
        else if (is_bool(v)) args[0] = val_int(v.as.boolean ? 1 : 0);
        else { vm->runtime_error("abs(): bad operand type"); return -1; }
        return 1;
    }

    static int nat_minmax(VM *vm, Value *args, int nargs, bool want_max)
    {
        std::vector<Value> items;
        if (nargs == 1)
        {
            if (!py_collect(vm, args[0], items, want_max ? "max" : "min")) return -1;
        }
        else
            items.assign(args, args + nargs);
        Value dflt;
        if (items.empty())
        {
            if (kwarg_get(vm, "default", &dflt)) { args[0] = dflt; return 1; }
            vm->runtime_error("%s() arg is an empty sequence", want_max ? "max" : "min");
            return -1;
        }
        Value keyfn;
        bool has_key = kwarg_get(vm, "key", &keyfn) && !is_nil(keyfn);
        Value best = items[0];
        Value best_key = best;
        if (has_key) { best_key = vm->call_fn(keyfn, &best, 1); if (vm->had_error()) return -1; }
        for (size_t i = 1; i < items.size(); i++)
        {
            Value k = items[i];
            if (has_key) { Value arg = items[i]; k = vm->call_fn(keyfn, &arg, 1); if (vm->had_error()) return -1; }
            int c = zen_compare_vm(vm, k, best_key);
            if (want_max ? c > 0 : c < 0) { best = items[i]; best_key = k; }
        }
        args[0] = best;
        return 1;
    }
    static int nat_min(VM *vm, Value *args, int nargs) { return nat_minmax(vm, args, nargs, false); }
    static int nat_max(VM *vm, Value *args, int nargs) { return nat_minmax(vm, args, nargs, true); }

    static int nat_round(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { vm->runtime_error("round() takes at least one argument"); return -1; }
        double x = to_number(args[0]);
        if (nargs >= 2 && !is_nil(args[1]))
        {
            int nd = (int)to_integer(args[1]);
            if (nd >= 0)
            {
                /* printf rounds the exact binary value, as Python does
                ** (round(2.675, 2) == 2.67, not 2.68) */
                char buf[64];
                snprintf(buf, sizeof buf, "%.*f", nd > 30 ? 30 : nd, x);
                double r = strtod(buf, nullptr);
                args[0] = is_int(args[0]) ? val_int((int64_t)r) : val_float(r);
                return 1;
            }
            double scale = pow(10.0, -nd);
            double r = nearbyint(x / scale) * scale;
            args[0] = is_int(args[0]) ? val_int((int64_t)r) : val_float(r);
            return 1;
        }
        if (is_int(args[0])) return 1; /* already an int */
        args[0] = val_int((int64_t)nearbyint(x));
        return 1;
    }

    static int nat_sum(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { vm->runtime_error("sum() takes at least one argument"); return -1; }
        std::vector<Value> items;
        if (!py_collect(vm, args[0], items, "sum")) return -1;
        Value acc = nargs >= 2 ? args[1] : val_int(0);
        for (const Value &x : items)
        {
            if (is_array(acc) && is_array(x))
            {
                std::vector<Value> joined;
                py_collect(vm, acc, joined, "sum");
                ObjArray *xa = as_array(x);
                joined.insert(joined.end(), xa->data, xa->data + arr_count(xa));
                acc = py_array_from(vm, joined);
                continue;
            }
            if (!is_numeric_like(acc) || !is_numeric_like(x)) { vm->runtime_error("sum(): unsupported operand"); return -1; }
            if (ZEN_INTLIKE_V(acc) && ZEN_INTLIKE_V(x))
                acc = val_int(to_integer(acc) + to_integer(x));
            else
                acc = val_float(to_number(acc) + to_number(x));
        }
        args[0] = acc;
        return 1;
    }

    static int nat_sorted(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { vm->runtime_error("sorted() takes one argument"); return -1; }
        std::vector<Value> items;
        if (!py_collect(vm, args[0], items, "sorted")) return -1;
        Value kv;
        bool reverse = (nargs >= 2 && py_truthy(args[1])) || (kwarg_get(vm, "reverse", &kv) && py_truthy(kv));
        Value keyfn;
        if (kwarg_get(vm, "key", &keyfn) && !is_nil(keyfn))
        {
            std::vector<std::pair<Value, Value>> keyed;
            keyed.reserve(items.size());
            for (const Value &x : items)
            {
                Value arg = x;
                keyed.emplace_back(vm->call_fn(keyfn, &arg, 1), x);
                if (vm->had_error()) return -1;
            }
            std::stable_sort(keyed.begin(), keyed.end(), [vm](const std::pair<Value, Value> &a, const std::pair<Value, Value> &b) { return zen_compare_vm(vm, a.first, b.first) < 0; });
            for (size_t i = 0; i < keyed.size(); i++) items[i] = keyed[i].second;
        }
        else
            std::stable_sort(items.begin(), items.end(), [vm](const Value &a, const Value &b) { return zen_compare_vm(vm, a, b) < 0; });
        if (reverse) std::reverse(items.begin(), items.end());
        args[0] = py_array_from(vm, items);
        return 1;
    }

    static int nat_reversed(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { vm->runtime_error("reversed() takes one argument"); return -1; }
        std::vector<Value> items;
        if (!py_collect(vm, args[0], items, "reversed")) return -1;
        std::reverse(items.begin(), items.end());
        args[0] = py_array_from(vm, items);
        return 1;
    }

    static int nat_any(VM *vm, Value *args, int nargs)
    {
        std::vector<Value> items;
        if (nargs < 1 || !py_collect(vm, args[0], items, "any")) return -1;
        for (const Value &x : items) if (py_truthy(x)) { args[0] = val_bool(true); return 1; }
        args[0] = val_bool(false);
        return 1;
    }
    static int nat_all(VM *vm, Value *args, int nargs)
    {
        std::vector<Value> items;
        if (nargs < 1 || !py_collect(vm, args[0], items, "all")) return -1;
        for (const Value &x : items) if (!py_truthy(x)) { args[0] = val_bool(false); return 1; }
        args[0] = val_bool(true);
        return 1;
    }

    static int nat_list(VM *vm, Value *args, int nargs)
    {
        std::vector<Value> items;
        if (nargs >= 1 && !py_collect(vm, args[0], items, "list")) return -1;
        args[0] = py_array_from(vm, items);
        return 1;
    }
    static int nat_set(VM *vm, Value *args, int nargs)
    {
        std::vector<Value> items;
        if (nargs >= 1 && !py_collect(vm, args[0], items, "set")) return -1;
        GC *gc = &vm->get_gc();
        ObjSet *st = new_set(gc);
        for (const Value &x : items) set_add(gc, st, x);
        args[0] = val_obj((Obj *)st);
        return 1;
    }
    static int nat_dict(VM *vm, Value *args, int nargs)
    {
        GC *gc = &vm->get_gc();
        ObjMap *m = new_map(gc);
        if (nargs >= 1)
        {
            if (is_map(args[0]))
            {
                ObjMap *src = as_map(args[0]);
                for (int32_t i = 0; i < src->capacity; i++)
                    if (src->nodes[i].hash != 0xFFFFFFFFu)
                        map_set(gc, m, src->nodes[i].key, src->nodes[i].value);
            }
            else
            {
                std::vector<Value> items;
                if (!py_collect(vm, args[0], items, "dict")) return -1;
                for (const Value &pair : items)
                {
                    if (!is_array(pair) || arr_count(as_array(pair)) != 2)
                    {
                        vm->runtime_error("dict(): each item must be a (key, value) pair");
                        return -1;
                    }
                    map_set(gc, m, as_array(pair)->data[0], as_array(pair)->data[1]);
                }
            }
        }
        if (vm->kwargs()) /* dict(a=1, b=2) */
        {
            ObjMap *kw = vm->kwargs();
            for (int32_t i = 0; i < kw->capacity; i++)
                if (kw->nodes[i].hash != 0xFFFFFFFFu)
                    map_set(gc, m, kw->nodes[i].key, kw->nodes[i].value);
        }
        args[0] = val_obj((Obj *)m);
        return 1;
    }
    static int nat_bool(VM *, Value *args, int nargs) { args[0] = val_bool(nargs >= 1 && py_truthy(args[0])); return 1; }

    static int nat_basefmt(VM *vm, Value *args, int nargs, const char *prefix, int base)
    {
        if (nargs < 1 || !(is_int(args[0]) || is_bool(args[0]))) { vm->runtime_error("%s(): integer argument required", prefix); return -1; }
        int64_t v = to_integer(args[0]);
        char digits[80];
        int n = 0;
        uint64_t u = v < 0 ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
        do { int d = (int)(u % (uint64_t)base); digits[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); u /= (uint64_t)base; } while (u);
        std::string out;
        if (v < 0) out += '-';
        out += prefix;
        while (n) out += digits[--n];
        args[0] = val_obj((Obj *)vm->make_string(out.c_str(), (int)out.size()));
        return 1;
    }
    static int nat_hex(VM *vm, Value *args, int nargs) { return nat_basefmt(vm, args, nargs, "0x", 16); }
    static int nat_oct(VM *vm, Value *args, int nargs) { return nat_basefmt(vm, args, nargs, "0o", 8); }
    static int nat_bin(VM *vm, Value *args, int nargs) { return nat_basefmt(vm, args, nargs, "0b", 2); }

    static int nat_divmod(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2) { vm->runtime_error("divmod() takes two arguments"); return -1; }
        Value a = args[0], b = args[1];
        std::vector<Value> pair;
        if (ZEN_INTLIKE_V(a) && ZEN_INTLIKE_V(b))
        {
            int64_t x = to_integer(a), y = to_integer(b);
            if (y == 0) { vm->runtime_error("divmod(): division by zero"); return -1; }
            int64_t q = x / y, r = x % y;
            if (r != 0 && ((r < 0) != (y < 0))) { q--; r += y; }
            pair.push_back(val_int(q)); pair.push_back(val_int(r));
        }
        else
        {
            double x = to_number(a), y = to_number(b);
            if (y == 0.0) { vm->runtime_error("divmod(): division by zero"); return -1; }
            double q = floor(x / y), r = fmod(x, y);
            if (r != 0.0 && ((r < 0.0) != (y < 0.0))) r += y;
            pair.push_back(val_float(q)); pair.push_back(val_float(r));
        }
        args[0] = py_array_from(vm, pair);
        return 1;
    }

    /* (a * b) % m for int64 without a 128-bit intermediate — __int128 is a
       GCC/Clang extension, not available on MSVC. "Peasant" multiplication:
       double a and conditionally add, reducing mod m at every step so the
       running values never reach m and the doubling itself never overflows
       uint64_t (2*(m-1) < 2^64 for any m that fits in int64_t). Assumes
       0 <= a < m and b >= 0, same as the __int128 version it replaces. */
    static inline int64_t mulmod64(int64_t a_, int64_t b_, int64_t m_)
    {
        uint64_t m = (uint64_t)m_, a = (uint64_t)a_, b = (uint64_t)b_, result = 0;
        while (b > 0)
        {
            if (b & 1) { result += a; if (result >= m) result -= m; }
            a += a; if (a >= m) a -= m;
            b >>= 1;
        }
        return (int64_t)result;
    }

    static int nat_pow(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2) { vm->runtime_error("pow() takes two or three arguments"); return -1; }
        if (nargs >= 3 && ZEN_INTLIKE_V(args[0]) && ZEN_INTLIKE_V(args[1]) && ZEN_INTLIKE_V(args[2]))
        {
            int64_t b = to_integer(args[0]), e = to_integer(args[1]), m = to_integer(args[2]);
            if (m == 0) { vm->runtime_error("pow(): modulus is zero"); return -1; }
            int64_t r = 1; b %= m; if (b < 0) b += m;
            while (e > 0) { if (e & 1) r = mulmod64(r, b, m); b = mulmod64(b, b, m); e >>= 1; }
            args[0] = val_int(r);
            return 1;
        }
        if (ZEN_INTLIKE_V(args[0]) && ZEN_INTLIKE_V(args[1]) && to_integer(args[1]) >= 0)
        {
            int64_t b = to_integer(args[0]), e = to_integer(args[1]), r = 1;
            while (e-- > 0) r *= b;
            args[0] = val_int(r);
            return 1;
        }
        args[0] = val_float(pow(to_number(args[0]), to_number(args[1])));
        return 1;
    }

    static int nat_callable(VM *, Value *args, int nargs)
    {
        Value v = nargs >= 1 ? args[0] : val_nil();
        args[0] = val_bool(is_closure(v) || is_native(v) || is_class(v));
        return 1;
    }

    /* values_compare() plus __lt__ on instances (sorted(), min(), max(), list.sort()). */
    int zen_compare_vm(VM *vm, Value a, Value b)
    {
        if (is_instance(a) || is_instance(b))
        {
            Value inst = is_instance(a) ? a : b;
            if (!is_nil(as_instance(inst)->klass->operator_slots[VM::SLOT_LT]))
            {
                Value arg = b;
                Value r = is_instance(a) ? vm->invoke_operator(a, VM::SLOT_LT, &arg, 1) : val_bool(false);
                if (py_truthy(r)) return -1;
                arg = a;
                r = is_instance(b) ? vm->invoke_operator(b, VM::SLOT_LT, &arg, 1) : val_bool(false);
                if (py_truthy(r)) return 1;
                return 0;
            }
        }
        return values_compare(a, b);
    }

    /* "fmt" % rhs for OP_MOD: rhs is one value or a tuple (array) of them. */
    Value zen_percent_format(VM *vm, Value fmt, Value rhs)
    {
        std::vector<Value> a;
        a.push_back(fmt);
        if (is_array(rhs))
        {
            ObjArray *arr = as_array(rhs);
            a.insert(a.end(), arr->data, arr->data + arr_count(arr));
        }
        else
            a.push_back(rhs);
        int n = nat_format(vm, a.data(), (int)a.size());
        return n > 0 ? a[0] : val_nil();
    }

    /* Keyword arguments of the native call in progress (vm->kwargs()). */
    static bool kwarg_get(VM *vm, const char *name, Value *out)
    {
        ObjMap *kw = vm->kwargs();
        if (!kw)
            return false;
        bool found = false;
        Value v = map_get(kw, val_obj((Obj *)vm->make_string(name)), &found);
        if (found)
            *out = v;
        return found;
    }

    static const NativeReg base_functions[] = {
        {"str", nat_str, 1},
        {"repr", nat_repr, 1},
        {"abs", nat_abs, 1},
        {"min", nat_min, -1},
        {"max", nat_max, -1},
        {"round", nat_round, -1},
        {"sum", nat_sum, -1},
        {"sorted", nat_sorted, -1},
        {"reversed", nat_reversed, 1},
        {"any", nat_any, 1},
        {"all", nat_all, 1},
        {"list", nat_list, -1},
        {"tuple", nat_list, -1},
        {"set", nat_set, -1},
        {"dict", nat_dict, -1},
        {"bool", nat_bool, -1},
        {"chr", nat_char, 1},
        {"hex", nat_hex, 1},
        {"oct", nat_oct, 1},
        {"bin", nat_bin, 1},
        {"divmod", nat_divmod, 2},
        {"pow", nat_pow, -1},
        {"callable", nat_callable, 1},
        {"getattr", nat_getattr, -1},
        {"hasattr", nat_hasattr, 2},
        {"int", nat_int, 1},
        {"float", nat_float, 1},
        {"char", nat_char, 1},
        {"ord", nat_ord, 1},
        {"typeof", nat_typeof, 1},
        {"isNil", nat_isNil, 1},
        {"isBool", nat_isBool, 1},
        {"isInt", nat_isInt, 1},
        {"isFloat", nat_isFloat, 1},
        {"isNumber", nat_isNumber, 1},
        {"isString", nat_isString, 1},
        {"isArray", nat_isArray, 1},
        {"isMap", nat_isMap, 1},
        {"isFunction", nat_isFunction, 1},
        {"input", nat_input, -1},
        {"assert", nat_assert, -1},
        {"error", nat_error, 1},
        {"range", nat_range, -1},
        {"type", nat_type, 1},
        {"isinstance", nat_isinstance, 2},
        {"format", nat_format, -1},
        {"collect", nat_collect, 0},
        {"gc_pause", nat_gc_pause, 0},
        {"gc_resume", nat_gc_resume, 0},
        {"mem_used", nat_mem_used, 0},
        {"mem_info", nat_mem_info, 0},
        {"clock",    nat_clock,    0},
        {"enumerate", nat_enumerate, 1},
        {"zip",      nat_zip,      -1},
        {"map",      nat_map,      2},
        {"filter",   nat_filter,   2},
        {"open",     nat_open,     -1},
        {"Int8Array",    nat_Int8Array,    1},
        {"Int16Array",   nat_Int16Array,   1},
        {"Int32Array",   nat_Int32Array,   1},
        {"Uint8Array",   nat_Uint8Array,   1},
        {"Uint16Array",  nat_Uint16Array,  1},
        {"Uint32Array",  nat_Uint32Array,  1},
        {"Float32Array", nat_Float32Array, 1},
        {"Float64Array", nat_Float64Array, 1},
    };

    /* =========================================================
    ** Signal class — GDScript-style event system.
    **
    **   s = Signal()
    **   s.connect(callback)
    **   s.emit(args...)
    **   s.disconnect(callback)
    **   s.count()
    **
    ** Field 0 ("_listeners") = ObjArray of listener closures.
    ** GC-safe: the array is a normal field, traced automatically.
    ** ========================================================= */

    /* signal.__init__: create the listeners array in field 0 */
    static int signal_init(VM *vm, Value *args, int /*nargs*/)
    {
        ObjInstance *inst = as_instance(args[-1]);
        inst->fields[0] = val_obj((Obj *)new_array(&vm->get_gc()));
        return 0;
    }

    static ObjArray *signal_get_listeners(Value self)
    {
        ObjInstance *inst = as_instance(self);
        return as_array(inst->fields[0]);
    }

    /* signal.connect(fn) */
    static int signal_connect(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || (!is_closure(args[0]) && !is_native(args[0])))
        { vm->runtime_error("Signal.connect: expected callable"); return -1; }
        ObjArray *listeners = signal_get_listeners(args[-1]);
        array_push(&vm->get_gc(), listeners, args[0]);
        return 0;
    }

    /* signal.disconnect(fn) */
    static int signal_disconnect(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { vm->runtime_error("Signal.disconnect: expected callable"); return -1; }
        ObjArray *listeners = signal_get_listeners(args[-1]);
        Value target = args[0];
        int count = arr_count(listeners);
        for (int i = 0; i < count; i++)
        {
            if (listeners->data[i].as.obj == target.as.obj)
            {
                for (int j = i; j < count - 1; j++)
                    listeners->data[j] = listeners->data[j + 1];
                listeners->end--;
                args[0] = val_bool(true);
                return 1;
            }
        }
        args[0] = val_bool(false);
        return 1;
    }

    /* signal.emit(args...) — calls all listeners with forwarded args */
    static int signal_emit(VM *vm, Value *args, int nargs)
    {
        ObjArray *listeners = signal_get_listeners(args[-1]);
        int count = arr_count(listeners);
        for (int i = 0; i < count; i++)
        {
            vm->call_fn(listeners->data[i], args, nargs);
            if (vm->had_error()) return -1;
        }
        return 0;
    }

    /* signal.count() → int */
    static int signal_count(VM * /*vm*/, Value *args, int /*nargs*/)
    {
        ObjArray *listeners = signal_get_listeners(args[-1]);
        args[0] = val_int(arr_count(listeners));
        return 1;
    }

    /* signal.clear() */
    static int signal_clear(VM * /*vm*/, Value *args, int /*nargs*/)
    {
        ObjArray *listeners = signal_get_listeners(args[-1]);
        listeners->end = listeners->data;
        return 0;
    }

    static void base_lib_init(VM *vm)
    {
        vm->def_class("File")
            .ctor(base_file_ctor)
            .dtor(base_file_dtor)
            .method("read",      base_file_read,      -1)
            .method("readlines", base_file_readlines,  0)
            .method("write",     base_file_write,      1)
            .method("close",     base_file_close,      0)
            .end();

        vm->def_class("Signal")
            .field("_listeners")
            .method("__init__",   signal_init,       0)
            .method("connect",    signal_connect,    1)
            .method("disconnect", signal_disconnect,  1)
            .method("emit",       signal_emit,       -1)
            .method("count",      signal_count,       0)
            .method("clear",      signal_clear,       0)
            .end();
    }

    const NativeLib zen_lib_base = {
        "base",
        base_functions,
        (int)(sizeof(base_functions) / sizeof(base_functions[0])), /* num_functions */
        nullptr, /* constants */
        0,    /* num_constants */
        base_lib_init, /* init_func */
    };

} /* namespace zen */
