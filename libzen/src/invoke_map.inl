/*
** invoke_map.inl — Map method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_MAP.
**
** Available variables: base, arg_count, receiver, mname, mlen, args, R, K,
** sel_slot (compile-time-resolved method selector — see BuiltinSelectors in
** vm.h and docs/plano-selector-dispatch-builtins.md).
*/

ObjMap *map = as_map(receiver);

/* Modules are ObjMaps but should NOT expose built-in map methods.
** Modules never expose the builtin map methods, so force the switch to its
** default (the key lookup below) rather than jumping over vm_method's
** initialisation — a goto across it does not compile. */
const uint8_t vm_method = map->is_module ? (uint8_t)MAP_NONE : map_method(sel_slot);
switch (vm_method)
{
case MAP_SET:
{
    /* map.set(key, val) → sets key, returns val */
    if (arg_count != 2)
    {
        RT_ERROR("set() expects (key, value)");
    }
    map_set(&gc_, map, args[0], args[1]);
    R[base] = args[1];
    break;
}
case MAP_GET:
{
    /* map.get(key) or map.get(key, default) → value or nil/default */
    if (arg_count < 1)
    {
        RT_ERROR("get() expects a key");
    }
    bool found;
    Value result = map_get(map, args[0], &found);
    if (found)
        R[base] = result;
    else
        R[base] = (arg_count >= 2) ? args[1] : val_nil();
    break;
}
case MAP_HAS:
{
    /* map.has(key) → bool */
    if (arg_count != 1)
    {
        RT_ERROR("has() expects a key");
    }
    R[base] = val_bool(map_contains(map, args[0]));
    break;
}
case MAP_DELETE:
{
    /* map.delete(key) → removes key, returns true if existed */
    if (arg_count != 1)
    {
        RT_ERROR("delete() expects a key");
    }
    R[base] = val_bool(map_delete(map, args[0]));
    break;
}
case MAP_KEYS:
{
    /* map.keys() → array of keys */
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result);
    map_keys(&gc_, map, as_array(R[base]));
    break;
}
case MAP_VALUES:
{
    /* map.values() → array of values */
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result);
    map_values(&gc_, map, as_array(R[base]));
    break;
}
case MAP_ITEMS:
{
    /* map.items() → array of [key, value] pairs */
    gc_pause(&gc_);
    ObjArray *result = new_array(&gc_);
    for (int32_t mi = 0; mi < map->capacity; mi++)
    {
        if (map->nodes[mi].hash != 0xFFFFFFFFu)
        {
            ObjArray *pair = new_array(&gc_);
            array_push(&gc_, pair, map->nodes[mi].key);
            array_push(&gc_, pair, map->nodes[mi].value);
            array_push(&gc_, result, val_obj((Obj *)pair));
        }
    }
    R[base] = val_obj((Obj *)result);
    gc_resume(&gc_);
    break;
}
case MAP_SIZE:
{
    /* map.size() → number of entries */
    R[base] = val_int(map->count);
    break;
}
case MAP_CLEAR:
{
    /* map.clear() → remove all entries */
    map_clear(&gc_, map);
    R[base] = val_nil();
    break;
}
case MAP_DUMP:
{
    /* map.dump() → pretty-print contents recursively */
    dump_value_rec(receiver, 0);
    putchar('\n');
    R[base] = val_nil();
    break;
}
    /* ---- Python dict methods ---- */
    case MAP_UPDATE:
    {
        if (arg_count != 1 || !is_map(args[0])) RT_ERROR("update() expects a dict");
        ObjMap *src = as_map(args[0]);
        gc_pause(&gc_);
        for (int32_t mi = 0; mi < src->capacity; mi++)
            if (src->nodes[mi].hash != 0xFFFFFFFFu)
                map_set(&gc_, map, src->nodes[mi].key, src->nodes[mi].value);
        gc_resume(&gc_);
        R[base] = val_nil();
        break;
    }
    case MAP_SETDEFAULT:
    {
        if (arg_count < 1) RT_ERROR("setdefault() expects a key");
        bool found;
        Value cur = map_get(map, args[0], &found);
        if (found)
            R[base] = cur;
        else
        {
            Value dflt = arg_count >= 2 ? args[1] : val_nil();
            map_set(&gc_, map, args[0], dflt);
            R[base] = dflt;
        }
        break;
    }
    case MAP_POP:
    {
        if (arg_count < 1) RT_ERROR("pop() expects a key");
        bool found;
        Value cur = map_get(map, args[0], &found);
        if (found)
        {
            map_delete(map, args[0]);
            R[base] = cur;
        }
        else if (arg_count >= 2)
            R[base] = args[1];
        else
            RT_ERROR("pop(): key not found");
        break;
    }
    case MAP_COPY:
    {
        gc_pause(&gc_);
        ObjMap *copy = new_map(&gc_);
        for (int32_t mi = 0; mi < map->capacity; mi++)
            if (map->nodes[mi].hash != 0xFFFFFFFFu)
                map_set(&gc_, copy, map->nodes[mi].key, map->nodes[mi].value);
        gc_resume(&gc_);
        R[base] = val_obj((Obj *)copy);
        break;
    }
default:
{
    /* Not a built-in map method — check if the map contains a callable
    ** with this name (module function dispatch: math.sin(x)).
    **
    ** The key is the selector's own interned string when we have one: the
    ** compiler already interned that name and its hash is stored. Rebuilding
    ** it here meant strlen + hash_string + a lookup in the intern table on
    ** every single call, which is why `math.sqrt(x)` in a loop cost nearly
    ** twice what `from math import sqrt` did. */
    ObjString *key = (sel_slot >= 0) ? selector_obj(sel_slot) : nullptr;
    if (!key)
        key = intern_string(&gc_, mname, (int)strlen(mname),
                            hash_string(mname, (int)strlen(mname)));
    bool found;
    Value callable = map_get(map, val_obj((Obj *)key), &found);
    if (found && is_native(callable))
    {
        ObjNative *nat = as_native(callable);
        /* Module functions don't take receiver (no self) */
        int nret = call_native(this, nat, args, arg_count);
        if (nret >= 0)
            copy_native_results(&R[base], args, nret, nresults);
        else
            RT_ERROR("native function '%s' returned error", mname);
        break;
    }
    if (found && is_closure(callable))
    {
        /* Module-level closure — call without self */
        ObjClosure *cl = as_closure(callable);
        ObjFunc *fn = cl->func;
        if (fn->arity >= 0 && arg_count != fn->arity)
            RT_ERROR("%s() expects %d args but got %d", mname, fn->arity, arg_count);
        if (fiber->frame_count >= kMaxFrames)
            RT_ERROR("stack overflow");
        /* Shift args down: args[0..nargs-1] become base[0..nargs-1] */
        for (int ai = 0; ai < arg_count; ai++)
            R[base + ai] = args[ai];
        ++ip;
        SAVE_IP();
        CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
        new_frame->closure = cl;
        new_frame->func = fn;
        new_frame->ip = fn->code;
        new_frame->base = &R[base];
        new_frame->ret_reg = base;
        new_frame->ret_count = 1;
        fiber->stack_top = new_frame->base + fn->num_regs;
        LOAD_STATE();
        DISPATCH();
    }
}
}
