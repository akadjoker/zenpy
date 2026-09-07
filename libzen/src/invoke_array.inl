/*
** invoke_array.inl — Array method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_ARRAY.
**
** Available variables:
**   base      — register index of receiver (R[base] = array)
**   arg_count — number of arguments
**   receiver  — Value of the array
**   mname     — method name (const char*)
**   mlen      — method name length
**   args      — pointer to first argument (R[base+1])
**   R         — register file
**   K         — constant pool
*/

ObjArray *arr = as_array(receiver);

#define ARRAY_METHOD(lit) (method->length == (int)(sizeof(lit) - 1) && memcmp(mname, lit, sizeof(lit) - 1) == 0)

do
{
if (ARRAY_METHOD("push") || ARRAY_METHOD("append"))
{
    /* arr.push(val) → append, returns new length */
    if (arg_count < 1)
    {
        RT_ERROR("push() expects at least 1 argument");
    }
    for (int ai = 0; ai < arg_count; ai++)
        array_push(&gc_, arr, args[ai]);
    R[base] = val_int(arr_count(arr));
    break;
}
if (ARRAY_METHOD("pop"))
{
    /* arr.pop() → remove+return last element; arr.pop(i) → at index i */
    if (arr_count(arr) == 0)
    {
        RT_ERROR("pop() on empty array");
    }
    if (arg_count >= 1 && is_int(args[0]))
    {
        int64_t pi = args[0].as.integer;
        if (pi < 0) pi += arr_count(arr);
        if (pi < 0 || pi >= arr_count(arr))
            RT_ERROR("pop index out of range");
        R[base] = arr->data[pi];
        array_remove(arr, (int32_t)pi);
        break;
    }
    R[base] = *--arr->end;
    break;
}
if (ARRAY_METHOD("len"))
{
    /* arr.len() → length */
    R[base] = val_int(arr_count(arr));
    break;
}
if (ARRAY_METHOD("remove"))
{
    /* arr.remove(idx) → remove at index, return removed value */
    if (arg_count != 1 || !is_int(args[0]))
    {
        RT_ERROR("remove() expects 1 integer argument");
    }
    int32_t idx = args[0].as.integer;
    int32_t count = arr_count(arr);
    if (idx < 0 || idx >= count)
    {
        RT_ERROR("remove() index out of bounds");
    }
    Value removed = arr->data[idx];
    memmove(&arr->data[idx], &arr->data[idx + 1], (size_t)(count - idx - 1) * sizeof(Value));
    arr->end--;
    R[base] = removed;
    break;
}
if (ARRAY_METHOD("insert"))
{
    /* arr.insert(idx, val) → insert at position */
    if (arg_count != 2 || !is_int(args[0]))
    {
        RT_ERROR("insert() expects (int, value)");
    }
    int32_t idx = args[0].as.integer;
    int32_t count = arr_count(arr);
    if (idx < 0 || idx > count)
    {
        RT_ERROR("insert() index out of bounds");
    }
    array_push(&gc_, arr, val_nil()); /* ensure capacity, end++ */
    /* shift right */
    memmove(&arr->data[idx + 1], &arr->data[idx], (size_t)(count - idx) * sizeof(Value));
    arr->data[idx] = args[1];
    R[base] = val_int(arr_count(arr));
    break;
}
if (ARRAY_METHOD("slice"))
{
    /* arr.slice(start, end?) → new array [start, end) */
    int32_t count = arr_count(arr);
    int32_t start = 0, end_idx = count;
    if (arg_count >= 1 && is_int(args[0]))
        start = args[0].as.integer;
    if (arg_count >= 2 && is_int(args[1]))
        end_idx = args[1].as.integer;
    if (start < 0)
        start += count;
    if (end_idx < 0)
        end_idx += count;
    if (start < 0)
        start = 0;
    if (end_idx > count)
        end_idx = count;
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result); /* root before reserve triggers GC */
    if (start < end_idx)
    {
        int32_t new_count = end_idx - start;
        array_reserve(&gc_, as_array(R[base]), new_count);
        result = as_array(R[base]);
        memcpy(result->data, &arr->data[start], (size_t)new_count * sizeof(Value));
        result->end = result->data + new_count;
    }
    break;
}
if (ARRAY_METHOD("reverse"))
{
    /* arr.reverse() → in-place reverse, returns arr */
    array_reverse(arr);
    R[base] = receiver;
    break;
}
if (ARRAY_METHOD("clear"))
{
    /* arr.clear() → empty the array */
    array_clear(arr);
    R[base] = val_nil();
    break;
}
if (ARRAY_METHOD("contains"))
{
    /* arr.contains(val) → bool */
    if (arg_count != 1)
    {
        RT_ERROR("contains() expects 1 argument");
    }
    R[base] = val_bool(array_contains(arr, args[0]));
    break;
}
if (ARRAY_METHOD("join"))
{
    /* arr.join(sep?) → string */
    const char *sep = "";
    int sep_len = 0;
    if (arg_count >= 1 && is_string(args[0]))
    {
        sep = as_cstring(args[0]);
        sep_len = as_string(args[0])->length;
    }
    int32_t count = arr_count(arr);
    char num_buf[64];
    /* First pass: compute length */
    int total_len = 0;
    for (int32_t ji = 0; ji < count; ji++)
    {
        if (ji > 0)
            total_len += sep_len;
        Value v = arr->data[ji];
        if (is_string(v))
            total_len += as_string(v)->length;
        else if (is_int(v))
            total_len += int_to_cstr(v.as.integer, num_buf);
        else if (is_float(v))
            total_len += snprintf(num_buf, sizeof(num_buf), "%g", v.as.number);
        else if (is_nil(v))
            total_len += 3;
        else if (is_bool(v))
            total_len += v.as.boolean ? 4 : 5;
        else
            total_len += 3;
    }
    /* Second pass: build string */
    char *buf = (char *)malloc(total_len + 1);
    char *p = buf;
    for (int32_t ji = 0; ji < count; ji++)
    {
        if (ji > 0)
        {
            memcpy(p, sep, sep_len);
            p += sep_len;
        }
        Value v = arr->data[ji];
        if (is_string(v))
        {
            memcpy(p, as_cstring(v), as_string(v)->length);
            p += as_string(v)->length;
        }
        else if (is_int(v))
        {
            int n = int_to_cstr(v.as.integer, num_buf);
            memcpy(p, num_buf, n);
            p += n;
        }
        else if (is_float(v))
        {
            int n = snprintf(num_buf, sizeof(num_buf), "%g", v.as.number);
            memcpy(p, num_buf, n);
            p += n;
        }
        else if (is_nil(v))
        {
            memcpy(p, "nil", 3);
            p += 3;
        }
        else if (is_bool(v))
        {
            const char *s = v.as.boolean ? "true" : "false";
            int n = v.as.boolean ? 4 : 5;
            memcpy(p, s, n);
            p += n;
        }
        else
        {
            memcpy(p, "???", 3);
            p += 3;
        }
    }
    R[base] = val_obj((Obj *)create_string(&gc_, buf, total_len));
    free(buf);
    break;
}
if (ARRAY_METHOD("sort"))
{
    /* arr.sort() / arr.sort("desc") / arr.sort(key=f, reverse=True): stable,
    ** in place, __lt__ on instances. */
    bool descending = false;
    if (arg_count >= 1 && is_string(args[0]) && as_string(args[0])->length == 4 && memcmp(as_string(args[0])->chars, "desc", 4) == 0)
        descending = true;
    Value keyfn = val_nil();
    if (kwmap)
    {
        bool found;
        Value v = map_get(kwmap, val_obj((Obj *)make_string("reverse")), &found);
        if (found && is_truthy_full(v)) descending = true;
        v = map_get(kwmap, val_obj((Obj *)make_string("key")), &found);
        if (found) keyfn = v;
    }
    int32_t count = arr_count(arr);
    if (count > 1)
    {
        SAVE_IP();
        std::vector<std::pair<Value, Value>> keyed((size_t)count);
        for (int32_t k = 0; k < count; k++)
        {
            keyed[k].second = arr->data[k];
            if (is_nil(keyfn))
                keyed[k].first = arr->data[k];
            else
            {
                Value arg = arr->data[k];
                keyed[k].first = call_fn(keyfn, &arg, 1);
                if (had_error_) return;
            }
        }
        VM *self_vm = this;
        std::stable_sort(keyed.begin(), keyed.end(), [self_vm](const std::pair<Value, Value> &x, const std::pair<Value, Value> &y) { return zen_compare_vm(self_vm, x.first, y.first) < 0; });
        if (had_error_) return;
        for (int32_t k = 0; k < count; k++)
            arr->data[descending ? count - 1 - k : k] = keyed[k].second;
        LOAD_STATE();
    }
    R[base] = val_nil();
    break;
}
if (ARRAY_METHOD("index_of") || ARRAY_METHOD("index"))
{
    /* arr.index_of(val) → index or -1 */
    if (arg_count != 1)
    {
        RT_ERROR("index_of() expects 1 argument");
    }
    R[base] = val_int(array_find(arr, args[0]));
    break;
}
if (ARRAY_METHOD("dump"))
{
    /* arr.dump() → pretty-print contents recursively */
    dump_value_rec(receiver, 0);
    putchar('\n');
    R[base] = val_nil();
    break;
}
/* ---- Python list methods ---- */
if (ARRAY_METHOD("count"))
{
    if (arg_count != 1) RT_ERROR("count() expects 1 argument");
    int32_t n = 0;
    for (int32_t k = 0; k < arr_count(arr); k++)
        if (values_deep_equal(arr->data[k], args[0])) n++;
    R[base] = val_int(n);
    break;
}
if (ARRAY_METHOD("extend"))
{
    if (arg_count != 1) RT_ERROR("extend() expects 1 argument");
    if (is_array(args[0]))
    {
        ObjArray *src = as_array(args[0]);
        int32_t n = arr_count(src);
        if (n > 0)
        {
            gc_pause(&gc_);
            ObjArray *copy = new_array(&gc_); /* src may be arr itself */
            array_push_n(&gc_, copy, src->data, n);
            array_push_n(&gc_, arr, copy->data, n);
            gc_resume(&gc_);
        }
    }
    else if (is_string(args[0]))
    {
        ObjString *s = as_string(args[0]);
        gc_pause(&gc_);
        for (int k = 0; k < s->length; k++)
            array_push(&gc_, arr, val_obj((Obj *)create_string(&gc_, s->chars + k, 1)));
        gc_resume(&gc_);
    }
    else
        RT_ERROR("extend() expects a list or a string");
    R[base] = val_nil();
    break;
}
if (ARRAY_METHOD("copy"))
{
    gc_pause(&gc_);
    ObjArray *copy = new_array(&gc_);
    if (arr_count(arr) > 0)
        array_push_n(&gc_, copy, arr->data, arr_count(arr));
    gc_resume(&gc_);
    R[base] = val_obj((Obj *)copy);
    break;
}
{
    RT_ERROR("array has no method '%s'", mname);
}
} while (0);

#undef ARRAY_METHOD
