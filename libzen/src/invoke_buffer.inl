/*
** invoke_buffer.inl — Buffer method dispatch for OP_INVOKE.
** Minimal API — raw speed via buf[i] / buf[i]=x is the primary interface.
**
** sel_slot — compile-time-resolved method selector (see BuiltinSelectors in
** vm.h and docs/plano-selector-dispatch-builtins.md).
*/

ObjBuffer *buf = as_buffer(receiver);

do
{
if (sel_slot == bsel_.buf_len)
{
    R[base] = val_int(buf->count);
    break;
}
if (sel_slot == bsel_.buf_fill)
{
    if (arg_count != 1)
    {
        RT_ERROR("fill() expects 1 argument");
    }
    double v = 0;
    if (is_int(args[0]))
        v = (double)args[0].as.integer;
    else if (is_float(args[0]))
        v = args[0].as.number;
    else
    {
        RT_ERROR("fill() expects a number");
    }
    buffer_fill(buf, v);
    R[base] = receiver;
    break;
}
if (sel_slot == bsel_.buf_byte_len)
{
    R[base] = val_int(buf->count * buffer_elem_size[buf->btype]);
    break;
}
if (sel_slot == bsel_.buf_tolist)
{
    ObjArray *arr = new_array(&gc_);
    R[base] = val_obj((Obj *)arr); /* root before push triggers GC */
    for (int32_t idx = 0; idx < buf->count; idx++) {
        double v = buffer_get(buf, idx);
        if (buf->btype < BUF_FLOAT32)
            array_push(&gc_, as_array(R[base]), val_int((int32_t)v));
        else
            array_push(&gc_, as_array(R[base]), val_float(v));
    }
    break;
}
if (sel_slot == bsel_.buf_copy)
{
    ObjBuffer *dst = new_buffer(&gc_, buf->btype, buf->count);
    int32_t byte_count = buf->count * buffer_elem_size[buf->btype];
    memcpy(dst->data, buf->data, (size_t)byte_count);
    R[base] = val_obj((Obj *)dst);
    break;
}
if (sel_slot == bsel_.buf_slice)
{
    if (arg_count < 1 || arg_count > 2) { RT_ERROR("slice() expects 1-2 arguments"); }
    if (!is_int(args[0])) { RT_ERROR("slice() start must be integer"); }
    int32_t start = args[0].as.integer;
    int32_t end = buf->count;
    if (arg_count == 2) {
        if (!is_int(args[1])) { RT_ERROR("slice() end must be integer"); }
        end = args[1].as.integer;
    }
    if (start < 0) start += buf->count;
    if (end < 0) end += buf->count;
    if (start < 0) start = 0;
    if (end > buf->count) end = buf->count;
    int32_t len = end > start ? end - start : 0;
    ObjBuffer *dst = new_buffer(&gc_, buf->btype, len);
    if (len > 0)
        memcpy(dst->data, buf->data + start * buffer_elem_size[buf->btype],
               (size_t)(len * buffer_elem_size[buf->btype]));
    R[base] = val_obj((Obj *)dst);
    break;
}
if (sel_slot == bsel_.buf_type_name)
{
    static const char *names[] = {
        "Int8Array", "Int16Array", "Int32Array",
        "Uint8Array", "Uint16Array", "Uint32Array",
        "Float32Array", "Float64Array"
    };
    const char *n = names[buf->btype];
    ObjString *s = intern_string(&gc_, n, (int)strlen(n), hash_string(n, (int)strlen(n)));
    R[base] = val_obj((Obj *)s);
    break;
}
{
    RT_ERROR("buffer has no method '%s'", mname);
}
} while (0);
