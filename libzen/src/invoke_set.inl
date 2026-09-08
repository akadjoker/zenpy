/*
** invoke_set.inl — Set method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_SET.
**
** Available variables: base, arg_count, receiver, mname, mlen, args, R, K,
** sel_slot (compile-time-resolved method selector — see BuiltinSelectors in
** vm.h and docs/plano-selector-dispatch-builtins.md).
*/

ObjSet *set = as_set(receiver);

do
{
if (sel_slot == bsel_.set_add)
{
    /* set.add(val) → returns true if newly added */
    if (arg_count != 1) { RT_ERROR("add() expects 1 argument"); }
    R[base] = val_bool(set_add(&gc_, set, args[0]));
    break;
}
if (sel_slot == bsel_.set_has)
{
    /* set.has(val) → bool */
    if (arg_count != 1) { RT_ERROR("has() expects 1 argument"); }
    R[base] = val_bool(set_contains(set, args[0]));
    break;
}
if (sel_slot == bsel_.set_delete)
{
    /* set.delete(val) → returns true if was present */
    if (arg_count != 1) { RT_ERROR("delete() expects 1 argument"); }
    R[base] = val_bool(set_remove(set, args[0]));
    break;
}
if (sel_slot == bsel_.set_size)
{
    /* set.size() → number of elements */
    R[base] = val_int(set->count);
    break;
}
if (sel_slot == bsel_.set_clear)
{
    /* set.clear() → remove all elements */
    set_clear(&gc_, set);
    R[base] = val_nil();
    break;
}
if (sel_slot == bsel_.set_values)
{
    /* set.values() → array of all values */
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result);
    for (int32_t si = 0; si < set->capacity; si++) {
        if (set->nodes[si].hash != 0xFFFFFFFFu) {
            array_push(&gc_, as_array(R[base]), set->nodes[si].key);
        }
    }
    break;
}
if (sel_slot == bsel_.set_dump)
{
    /* set.dump() → pretty-print contents recursively */
    dump_value_rec(receiver, 0);
    putchar('\n');
    R[base] = val_nil();
    break;
}
/* ---- Python set methods ---- */
if (sel_slot == bsel_.set_discard || sel_slot == bsel_.set_remove)
{
    if (arg_count != 1) RT_ERROR("%s() expects 1 argument", mname);
    bool was = set_remove(set, args[0]);
    if (!was && sel_slot == bsel_.set_remove) RT_ERROR("remove(): element not in set");
    R[base] = val_nil();
    break;
}
if (sel_slot == bsel_.set_issubset || sel_slot == bsel_.set_issuperset || sel_slot == bsel_.set_isdisjoint)
{
    if (arg_count != 1 || !is_set(args[0])) RT_ERROR("%s() expects a set", mname);
    ObjSet *other = as_set(args[0]);
    ObjSet *walk = sel_slot == bsel_.set_issuperset ? other : set;
    ObjSet *look = sel_slot == bsel_.set_issuperset ? set : other;
    bool ok = true;
    for (int32_t si = 0; si < walk->capacity && ok; si++)
    {
        if (walk->nodes[si].hash == 0xFFFFFFFFu) continue;
        bool in = set_contains(look, walk->nodes[si].key);
        ok = sel_slot == bsel_.set_isdisjoint ? !in : in;
    }
    R[base] = val_bool(ok);
    break;
}
if (sel_slot == bsel_.set_union || sel_slot == bsel_.set_intersection || sel_slot == bsel_.set_difference ||
    sel_slot == bsel_.set_symmetric_difference || sel_slot == bsel_.set_copy)
{
    int kind = sel_slot == bsel_.set_union ? 0 : sel_slot == bsel_.set_intersection ? 1 : sel_slot == bsel_.set_difference ? 2 : sel_slot == bsel_.set_symmetric_difference ? 3 : 4;
    gc_pause(&gc_);
    ObjSet *out = new_set(&gc_);
    for (int32_t si = 0; si < set->capacity; si++)
        if (set->nodes[si].hash != 0xFFFFFFFFu)
            set_add(&gc_, out, set->nodes[si].key);
    for (int ai = 0; ai < arg_count && kind != 4; ai++)
    {
        /* each argument: a set or a list */
        ObjSet *other = nullptr;
        ObjArray *arr = nullptr;
        if (is_set(args[ai])) other = as_set(args[ai]);
        else if (is_array(args[ai])) arr = as_array(args[ai]);
        else { gc_resume(&gc_); RT_ERROR("%s() expects sets or lists", mname); }
        ObjSet *tmp = new_set(&gc_);
        if (other) { for (int32_t si = 0; si < other->capacity; si++) if (other->nodes[si].hash != 0xFFFFFFFFu) set_add(&gc_, tmp, other->nodes[si].key); }
        else for (int32_t k = 0; k < arr_count(arr); k++) set_add(&gc_, tmp, arr->data[k]);
        if (kind == 0)
        {
            for (int32_t si = 0; si < tmp->capacity; si++) if (tmp->nodes[si].hash != 0xFFFFFFFFu) set_add(&gc_, out, tmp->nodes[si].key);
        }
        else if (kind == 1 || kind == 2)
        {
            ObjSet *next = new_set(&gc_);
            for (int32_t si = 0; si < out->capacity; si++)
            {
                if (out->nodes[si].hash == 0xFFFFFFFFu) continue;
                bool in = set_contains(tmp, out->nodes[si].key);
                if (kind == 1 ? in : !in) set_add(&gc_, next, out->nodes[si].key);
            }
            out = next;
        }
        else
        {
            ObjSet *next = new_set(&gc_);
            for (int32_t si = 0; si < out->capacity; si++)
                if (out->nodes[si].hash != 0xFFFFFFFFu && !set_contains(tmp, out->nodes[si].key)) set_add(&gc_, next, out->nodes[si].key);
            for (int32_t si = 0; si < tmp->capacity; si++)
                if (tmp->nodes[si].hash != 0xFFFFFFFFu && !set_contains(out, tmp->nodes[si].key)) set_add(&gc_, next, tmp->nodes[si].key);
            out = next;
        }
    }
    gc_resume(&gc_);
    R[base] = val_obj((Obj *)out);
    break;
}
{
    RT_ERROR("set has no method '%s'", mname);
}
} while (0);
