/*
** invoke_vec.inl — Vec2 / Vec3 method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is VAL_VEC2 or VAL_VEC3.
**
** Available variables: base, arg_count, receiver, mname, args, R, K, sel_slot
** (see invoke_string.inl; same dense-id scheme: vec_method(sel_slot)).
**
** A vector is a value, not an object: nothing here allocates except
** to_list(). Every method returns a new value in R[base]; none mutates the
** receiver (with_x / with_y / with_z return a modified copy).
**
** Scalars come back as float (computed in double from the float
** components), vectors as the same kind as the receiver. Arguments are
** checked: wrong count, wrong kind (a Vec2 where a Vec3 is needed, a
** non-number where a number is needed) is a runtime error naming the
** method. Vec2.cross(Vec2) is the 2D cross product, a scalar
** (x1*y2 - y1*x2); Vec3.cross(Vec3) is the vector product.
*/

#define VEC_NARGS(n)                                                              \
    if (arg_count != (n))                                                         \
    RT_ERROR("%s.%s() expects %d argument(s), got %d", zen_type_name_of(receiver), \
             mname, (n), arg_count)
#define VEC_SAME(k)                                                               \
    if (args[k].type != receiver.type)                                            \
    RT_ERROR("%s.%s() expects a %s argument, got %s", zen_type_name_of(receiver), \
             mname, zen_type_name_of(receiver), zen_type_name_of(args[k]))
#define VEC_NUM(k)                                                                \
    if (!is_numeric_like(args[k]))                                                \
    RT_ERROR("%s.%s() expects a number, got %s", zen_type_name_of(receiver),      \
             mname, zen_type_name_of(args[k]))
/* The Vec2 invariant is z == 0, so the 3-component formulas below are exact
** for both kinds; MK rebuilds a value of the receiver's kind. */
#define VEC_MK(x, y, z) (receiver.type == VAL_VEC3 ? val_vec3((x), (y), (z)) : val_vec2((x), (y)))

{
    const double rx = (double)receiver.vx, ry = (double)receiver.as.v.y, rz = (double)receiver.as.v.z;
    const uint8_t vm_method = vec_method(sel_slot);
    switch (vm_method)
    {
    case VEC_LENGTH:
    {
        VEC_NARGS(0);
        R[base] = val_float(sqrt(rx * rx + ry * ry + rz * rz));
        break;
    }
    case VEC_LENGTH_SQ:
    {
        VEC_NARGS(0);
        R[base] = val_float(rx * rx + ry * ry + rz * rz);
        break;
    }
    case VEC_NORMALIZED:
    {
        VEC_NARGS(0);
        double len = sqrt(rx * rx + ry * ry + rz * rz);
        if (len == 0.0)
            R[base] = VEC_MK(0.0f, 0.0f, 0.0f); /* zero vector stays zero */
        else
            R[base] = VEC_MK((float)(rx / len), (float)(ry / len), (float)(rz / len));
        break;
    }
    case VEC_DOT:
    {
        VEC_NARGS(1);
        VEC_SAME(0);
        const Value o = args[0];
        R[base] = val_float(rx * (double)o.vx + ry * (double)o.as.v.y + rz * (double)o.as.v.z);
        break;
    }
    case VEC_CROSS:
    {
        VEC_NARGS(1);
        VEC_SAME(0);
        const Value o = args[0];
        if (receiver.type == VAL_VEC3)
            R[base] = val_vec3((float)(ry * (double)o.as.v.z - rz * (double)o.as.v.y),
                               (float)(rz * (double)o.vx - rx * (double)o.as.v.z),
                               (float)(rx * (double)o.as.v.y - ry * (double)o.vx));
        else
            R[base] = val_float(rx * (double)o.as.v.y - ry * (double)o.vx); /* 2D cross: scalar */
        break;
    }
    case VEC_DISTANCE:
    case VEC_DISTANCE_SQ:
    {
        VEC_NARGS(1);
        VEC_SAME(0);
        const Value o = args[0];
        const double dx = rx - (double)o.vx, dy = ry - (double)o.as.v.y, dz = rz - (double)o.as.v.z;
        const double d2 = dx * dx + dy * dy + dz * dz;
        R[base] = val_float(vm_method == VEC_DISTANCE ? sqrt(d2) : d2);
        break;
    }
    case VEC_LERP:
    {
        VEC_NARGS(2);
        VEC_SAME(0);
        VEC_NUM(1);
        const Value o = args[0];
        const float t = (float)to_number(args[1]);
        R[base] = VEC_MK(receiver.vx + (o.vx - receiver.vx) * t,
                         receiver.as.v.y + (o.as.v.y - receiver.as.v.y) * t,
                         receiver.as.v.z + (o.as.v.z - receiver.as.v.z) * t);
        break;
    }
    case VEC_ABS:
    {
        VEC_NARGS(0);
        R[base] = VEC_MK(fabsf(receiver.vx), fabsf(receiver.as.v.y), fabsf(receiver.as.v.z));
        break;
    }
    case VEC_WITH_X:
    {
        VEC_NARGS(1);
        VEC_NUM(0);
        R[base] = VEC_MK((float)to_number(args[0]), receiver.as.v.y, receiver.as.v.z);
        break;
    }
    case VEC_WITH_Y:
    {
        VEC_NARGS(1);
        VEC_NUM(0);
        R[base] = VEC_MK(receiver.vx, (float)to_number(args[0]), receiver.as.v.z);
        break;
    }
    case VEC_WITH_Z:
    {
        VEC_NARGS(1);
        VEC_NUM(0);
        if (receiver.type != VAL_VEC3)
            RT_ERROR("Vec2 has no z component (with_z is Vec3 only)");
        R[base] = val_vec3(receiver.vx, receiver.as.v.y, (float)to_number(args[0]));
        break;
    }
    case VEC_TO_LIST:
    {
        VEC_NARGS(0);
        const Value self = receiver;
        ObjArray *arr = new_array(&gc_);
        R[base] = val_obj((Obj *)arr); /* root before push triggers GC */
        array_push(&gc_, as_array(R[base]), val_float((double)self.vx));
        array_push(&gc_, as_array(R[base]), val_float((double)self.as.v.y));
        if (self.type == VAL_VEC3)
            array_push(&gc_, as_array(R[base]), val_float((double)self.as.v.z));
        break;
    }
    default:
    {
        RT_ERROR("%s has no method '%s'", zen_type_name_of(receiver), mname);
    }
    }
}

#undef VEC_NARGS
#undef VEC_SAME
#undef VEC_NUM
#undef VEC_MK
