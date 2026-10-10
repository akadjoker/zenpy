/* =========================================================
** builtin_xml.cpp — "xml" module for Zen, on ct::Xml (third_party/ct).
**
** A document is plain script data, the same way json.parse() returns
** dicts and lists — no native class to learn:
**
**   import xml
**   doc = xml.parse('<scene name="lvl1"><obj id="1">hi</obj></scene>')
**   doc["tag"]                    # "scene"
**   doc["attrs"]["name"]          # "lvl1"
**   doc["children"][0]["text"]    # "hi"
**   xml.stringify(doc)            # '<scene name="lvl1"><obj id="1">hi</obj></scene>'
**   xml.stringify(doc, 2)         # indented
**
** Node layout: {"tag": str, "attrs": {name: value}, "text": str,
**               "children": [node, ...]}
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <ct/xml.hpp>
#include <cstring>
#include <cstdio>

namespace zen
{
    static Value zstr(VM *vm, const ct::String &s)
    {
        return val_obj((Obj *)vm->make_string(s.c_str(), (int)s.size()));
    }
    static Value zkey(VM *vm, const char *s)
    {
        return val_obj((Obj *)vm->make_string(s, (int)strlen(s)));
    }

    static Value xml_to_value(VM *vm, const ct::Xml &x)
    {
        GC *gc = &vm->get_gc();
        ObjMap *node = new_map(gc);
        map_set(gc, node, zkey(vm, "tag"), zstr(vm, x.tag()));

        ObjMap *attrs = new_map(gc);
        const ct::Xml::Attributes &as = x.attributes();
        for (size_t i = 0; i < as.size(); i++)
            map_set(gc, attrs, zstr(vm, as[i].name), zstr(vm, as[i].value));
        map_set(gc, node, zkey(vm, "attrs"), val_obj((Obj *)attrs));

        map_set(gc, node, zkey(vm, "text"), zstr(vm, x.text()));

        ObjArray *children = new_array(gc);
        const ct::Xml::Children &cs = x.children();
        for (size_t i = 0; i < cs.size(); i++)
            array_push(gc, children, xml_to_value(vm, cs[i]));
        map_set(gc, node, zkey(vm, "children"), val_obj((Obj *)children));
        return val_obj((Obj *)node);
    }

    /* Attribute values may be numbers/bools in the script: rendered as text. */
    static bool value_to_text(Value v, ct::String &out)
    {
        char buf[64];
        if (is_string(v)) { out.assign(as_string(v)->chars, (size_t)as_string(v)->length); return true; }
        if (is_int(v)) { snprintf(buf, sizeof buf, "%lld", (long long)v.as.integer); out = buf; return true; }
        if (is_float(v)) { format_float_py(v.as.number, buf, sizeof buf); out = buf; return true; }
        if (is_bool(v)) { out = v.as.boolean ? "true" : "false"; return true; }
        if (is_nil(v)) { out = ""; return true; }
        return false;
    }

    static Value map_lookup(ObjMap *m, VM *vm, const char *key)
    {
        bool found = false;
        Value v = map_get(m, zkey(vm, key), &found);
        return found ? v : val_nil();
    }

    static bool value_to_xml(VM *vm, Value v, ct::Xml &out, int depth)
    {
        if (!is_map(v))
        {
            vm->runtime_error("xml.stringify: a node must be a dict with a \"tag\"");
            return false;
        }
        if (depth > 200)
        {
            vm->runtime_error("xml.stringify: nesting too deep");
            return false;
        }
        ObjMap *m = as_map(v);
        Value tag = map_lookup(m, vm, "tag");
        if (!is_string(tag))
        {
            vm->runtime_error("xml.stringify: node \"tag\" must be a string");
            return false;
        }
        out.set_tag(ct::String(as_string(tag)->chars, (size_t)as_string(tag)->length));

        Value attrs = map_lookup(m, vm, "attrs");
        if (is_map(attrs))
        {
            ObjMap *am = as_map(attrs);
            for (int32_t i = 0; i < am->capacity; i++)
            {
                if (am->nodes[i].hash == 0xFFFFFFFFu) continue;
                ct::String name, value;
                if (!is_string(am->nodes[i].key) || !value_to_text(am->nodes[i].value, value))
                {
                    vm->runtime_error("xml.stringify: attribute names must be strings and values text-like");
                    return false;
                }
                name.assign(as_string(am->nodes[i].key)->chars, (size_t)as_string(am->nodes[i].key)->length);
                out.set_attr(name, value);
            }
        }
        Value text = map_lookup(m, vm, "text");
        ct::String t;
        if (!is_nil(text) && value_to_text(text, t))
            out.set_text(t);

        Value children = map_lookup(m, vm, "children");
        if (is_array(children))
        {
            ObjArray *arr = as_array(children);
            for (int32_t i = 0; i < arr_count(arr); i++)
            {
                ct::Xml child;
                if (!value_to_xml(vm, arr->data[i], child, depth + 1))
                    return false;
                out.add_child(child);
            }
        }
        return true;
    }

    /* xml.parse(text) → node dict */
    static int nat_xml_parse(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("xml.parse() expects a string");
            return -1;
        }
        ObjString *s = as_string(args[0]);
        ct::Xml::Error err;
        ct::Xml doc = ct::Xml::parse(s->chars, (size_t)s->length, &err);
        if (err.message)
        {
            vm->runtime_error("xml.parse: %s (offset %zu)", err.message, (size_t)err.offset);
            return -1;
        }
        args[0] = xml_to_value(vm, doc);
        return 1;
    }

    /* xml.stringify(node[, indent]) → text */
    static int nat_xml_stringify(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1)
        {
            vm->runtime_error("xml.stringify() expects a node");
            return -1;
        }
        int indent = -1;
        if (nargs >= 2)
        {
            if (is_int(args[1])) indent = (int)args[1].as.integer;
            else if (is_bool(args[1]) && args[1].as.boolean) indent = 2;
        }
        ct::Xml doc;
        if (!value_to_xml(vm, args[0], doc, 0))
            return -1;
        ct::String out = doc.dump(indent);
        args[0] = val_obj((Obj *)vm->make_string(out.c_str(), (int)out.size()));
        return 1;
    }

    static const NativeReg xml_functions[] = {
        {"parse", nat_xml_parse, 1},
        {"stringify", nat_xml_stringify, -1},
        {nullptr, nullptr, 0}
    };

    extern const NativeLib zen_lib_xml = {
        "xml",
        xml_functions,
        2,
        nullptr, 0, nullptr
    };

} /* namespace zen */
