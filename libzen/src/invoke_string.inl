/*
** invoke_string.inl — String method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_STRING.
**
** Available variables: base, arg_count, receiver, mname, mlen, args, R, K
*/

ObjString *str = as_string(receiver);
#define STR_METHOD(lit) (method->length == (int)(sizeof(lit) - 1) && memcmp(mname, lit, sizeof(lit) - 1) == 0)

do
{
if (STR_METHOD("len"))
{
    R[base] = val_int(str->length);
    break;
}
if (STR_METHOD("sub"))
{
    /* str.sub(start, end?) → substring [start, end) */
    int32_t slen = str->length;
    int32_t start = 0, end = slen;
    if (arg_count >= 1 && is_int(args[0]))
        start = args[0].as.integer;
    if (arg_count >= 2 && is_int(args[1]))
        end = args[1].as.integer;
    if (start < 0)
        start += slen;
    if (end < 0)
        end += slen;
    if (start < 0)
        start = 0;
    if (end > slen)
        end = slen;
    if (start >= end)
        R[base] = val_obj((Obj *)create_string(&gc_, "", 0));
    else
        R[base] = val_obj((Obj *)create_string(&gc_, str->chars + start, end - start));
    break;
}
if (STR_METHOD("find"))
{
    /* str.find(needle[, start]) → index or -1 */
    if (arg_count < 1 || !is_string(args[0]))
    {
        RT_ERROR("find() expects a string argument");
    }
    ObjString *needle = as_string(args[0]);
    int start_at = (arg_count >= 2 && is_int(args[1])) ? (int)args[1].as.integer : 0;
    if (start_at < 0) start_at += str->length;
    if (start_at < 0) start_at = 0;
    if (start_at > str->length)
    {
        R[base] = val_int(-1);
    }
    else if (needle->length == 0)
    {
        R[base] = val_int(start_at);
    }
    else
    {
        const char *found = find_sep(str->chars + start_at, str->length - start_at, needle->chars, needle->length);
        R[base] = found ? val_int((int32_t)(found - str->chars)) : val_int(-1);
    }
    break;
}
if (STR_METHOD("upper"))
{
    /* str.upper() → new uppercase string */
    char *buf = (char *)malloc(str->length);
    for (int si = 0; si < str->length; si++)
        buf[si] = (str->chars[si] >= 'a' && str->chars[si] <= 'z') ? str->chars[si] - 32 : str->chars[si];
    R[base] = val_obj((Obj *)create_string(&gc_, buf, str->length));
    free(buf);
    break;
}
if (STR_METHOD("lower"))
{
    /* str.lower() → new lowercase string */
    char *buf = (char *)malloc(str->length);
    for (int si = 0; si < str->length; si++)
        buf[si] = (str->chars[si] >= 'A' && str->chars[si] <= 'Z') ? str->chars[si] + 32 : str->chars[si];
    R[base] = val_obj((Obj *)create_string(&gc_, buf, str->length));
    free(buf);
    break;
}
if (STR_METHOD("split"))
{
    if (arg_count > 1 || (arg_count == 1 && !is_string(args[0])))
        RT_ERROR("split() expects zero args or a string separator");

    gc_pause(&gc_);
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result);
    const uint8_t *ws = get_ws_table();

    if (arg_count == 0)
    {
        /* Lookup table — mais rápido que 4 comparisons por char */
       
        int i = 0;
        const char *chars = str->chars;
        const int len = str->length;

        while (i < len)
        {
            /* Skip whitespace */
            while (i < len && ws[(uint8_t)chars[i]]) i++;
            if (i >= len) break;

            /* Find end of token */
            int start = i;
            while (i < len && !ws[(uint8_t)chars[i]]) i++;

            array_push(&gc_, as_array(R[base]),
                val_obj((Obj *)create_string(&gc_, chars + start, i - start)));
        }
    }
    else
    {
        ObjString *sep = as_string(args[0]);
        if (sep->length == 0)
        {
            for (int si = 0; si < str->length; si++)
                array_push(&gc_, as_array(R[base]),
                    val_obj((Obj *)create_string(&gc_, &str->chars[si], 1)));
        }
        else
        {
            const char *start_ptr = str->chars;
            const char *end_ptr = str->chars + str->length;
            while (start_ptr <= end_ptr)
            {
                const char *found = find_sep(
                    start_ptr, end_ptr - start_ptr, sep->chars, sep->length);
                if (!found)
                {
                    array_push(&gc_, as_array(R[base]),
                        val_obj((Obj *)create_string(&gc_, start_ptr,
                            (int)(end_ptr - start_ptr))));
                    break;
                }
                array_push(&gc_, as_array(R[base]),
                    val_obj((Obj *)create_string(&gc_, start_ptr,
                        (int)(found - start_ptr))));
                start_ptr = found + sep->length;
            }
        }
    }
    gc_resume(&gc_);
    break;
}
if (STR_METHOD("trim") || STR_METHOD("strip"))
{
    /* str.strip() → whitespace; str.strip(chars) → any of those characters */
    const char *set = " \t\n\r\v\f";
    int set_len = 6;
    if (arg_count >= 1 && is_string(args[0]))
    {
        set = as_string(args[0])->chars;
        set_len = as_string(args[0])->length;
    }
    int start = 0, end = str->length;
    while (start < end && memchr(set, str->chars[start], (size_t)set_len))
        start++;
    while (end > start && memchr(set, str->chars[end - 1], (size_t)set_len))
        end--;
    R[base] = val_obj((Obj *)create_string(&gc_, str->chars + start, end - start));
    break;
}
if (STR_METHOD("replace"))
{
    /* str.replace(old, new[, count]) → new string with occurrences replaced */
    if (arg_count < 2 || !is_string(args[0]) || !is_string(args[1]))
    {
        RT_ERROR("replace() expects (string, string)");
    }
    int64_t replace_limit = (arg_count >= 3 && is_int(args[2])) ? args[2].as.integer : -1;
    ObjString *old_s = as_string(args[0]);
    ObjString *new_s = as_string(args[1]);
    if (old_s->length == 0)
    {
        R[base] = receiver;
    }
    else
    {
        /* Count occurrences first */
        int occurrences = 0;
        const char *sp = str->chars;
        const char *ep = str->chars + str->length;
        while (sp < ep)
        {
            const char *f = find_sep(sp, ep - sp, old_s->chars, old_s->length);
            if (!f)
                break;
            occurrences++;
            sp = f + old_s->length;
        }
        if (replace_limit >= 0 && occurrences > (int)replace_limit)
            occurrences = (int)replace_limit;
        if (occurrences == 0)
        {
            R[base] = receiver;
        }
        else
        {
            int replaced = 0;
            int new_len = str->length + occurrences * (new_s->length - old_s->length);
            char *buf = (char *)malloc(new_len);
            char *wp = buf;
            sp = str->chars;
            while (sp < ep)
            {
                const char *f = replaced < occurrences ? find_sep(sp, ep - sp, old_s->chars, old_s->length) : nullptr;
                if (!f)
                {
                    memcpy(wp, sp, ep - sp);
                    wp += (ep - sp);
                    break;
                }
                memcpy(wp, sp, f - sp);
                wp += (f - sp);
                memcpy(wp, new_s->chars, new_s->length);
                wp += new_s->length;
                sp = f + old_s->length;
                replaced++;
            }
            R[base] = val_obj((Obj *)create_string(&gc_, buf, new_len));
            free(buf);
        }
    }
    break;
}
if (STR_METHOD("starts_with") || STR_METHOD("startswith"))
{
    if (arg_count != 1 || !is_string(args[0]))
    {
        RT_ERROR("starts_with() expects a string");
    }
    ObjString *prefix = as_string(args[0]);
    bool match = (prefix->length <= str->length) &&
                 (memcmp(str->chars, prefix->chars, prefix->length) == 0);
    R[base] = val_bool(match);
    break;
}
if (STR_METHOD("ends_with") || STR_METHOD("endswith"))
{
    if (arg_count != 1 || !is_string(args[0]))
    {
        RT_ERROR("ends_with() expects a string");
    }
    ObjString *suffix = as_string(args[0]);
    bool match = (suffix->length <= str->length) &&
                 (memcmp(str->chars + str->length - suffix->length, suffix->chars, suffix->length) == 0);
    R[base] = val_bool(match);
    break;
}
if (STR_METHOD("char_at"))
{
    /* str.char_at(idx) → single-char string */
    if (arg_count != 1 || !is_int(args[0]))
    {
        RT_ERROR("char_at() expects an integer");
    }
    int32_t idx = args[0].as.integer;
    if (idx < 0 || idx >= str->length)
    {
        R[base] = val_nil();
    }
    else
    {
        R[base] = val_obj((Obj *)create_string(&gc_, &str->chars[idx], 1));
    }
    break;
}
if (STR_METHOD("byte_at"))
{
    /* str.byte_at(idx) → integer byte value */
    if (arg_count != 1 || !is_int(args[0]))
    {
        RT_ERROR("byte_at() expects an integer");
    }
    int32_t idx = args[0].as.integer;
    if (idx < 0 || idx >= str->length)
    {
        R[base] = val_int(0);
    }
    else
    {
        R[base] = val_int((uint8_t)str->chars[idx]);
    }
    break;
}
if (STR_METHOD("repeat"))
{
    /* str.repeat(n) → string repeated n times */
    if (arg_count != 1 || !is_int(args[0]))
    {
        RT_ERROR("repeat() expects an integer");
    }
    int32_t n = (int32_t)args[0].as.integer;
    if (n <= 0 || str->length == 0)
    {
        R[base] = val_obj((Obj *)create_string(&gc_, "", 0));
        break;
    }
    int32_t new_len = str->length * n;
    char *buf = (char *)malloc(new_len);
    for (int ri = 0; ri < n; ri++)
        memcpy(buf + ri * str->length, str->chars, str->length);
    R[base] = val_obj((Obj *)create_string(&gc_, buf, new_len));
    free(buf);
    break;
}
if (STR_METHOD("count"))
{
    /* str.count(needle) → number of non-overlapping occurrences */
    if (arg_count != 1 || !is_string(args[0]))
    {
        RT_ERROR("count() expects a string");
    }
    ObjString *needle = as_string(args[0]);
    if (needle->length == 0)
    {
        R[base] = val_int(str->length + 1);
        break;
    }
    int32_t cnt = 0;
    const char *sp = str->chars;
    const char *ep = str->chars + str->length;
    while (sp < ep)
    {
        const char *f = find_sep(sp, ep - sp, needle->chars, needle->length);
        if (!f)
            break;
        cnt++;
        sp = f + needle->length;
    }
    R[base] = val_int(cnt);
    break;
}
if (STR_METHOD("pad_left"))
{
    /* str.pad_left(width [, char=' ']) → right-justify string in field of width */
    if (arg_count < 1 || !is_int(args[0]))
    {
        RT_ERROR("pad_left() expects (width[, char])");
    }
    int32_t width = (int32_t)args[0].as.integer;
    char pad_ch = ' ';
    if (arg_count >= 2 && is_string(args[1]) && as_string(args[1])->length > 0)
        pad_ch = as_string(args[1])->chars[0];
    int32_t pad = width - str->length;
    if (pad <= 0)
    {
        R[base] = receiver;
        break;
    }
    char *buf = (char *)malloc(width);
    memset(buf, pad_ch, pad);
    memcpy(buf + pad, str->chars, str->length);
    R[base] = val_obj((Obj *)create_string(&gc_, buf, width));
    free(buf);
    break;
}
if (STR_METHOD("pad_right"))
{
    /* str.pad_right(width [, char=' ']) → left-justify string in field of width */
    if (arg_count < 1 || !is_int(args[0]))
    {
        RT_ERROR("pad_right() expects (width[, char])");
    }
    int32_t width = (int32_t)args[0].as.integer;
    char pad_ch = ' ';
    if (arg_count >= 2 && is_string(args[1]) && as_string(args[1])->length > 0)
        pad_ch = as_string(args[1])->chars[0];
    int32_t pad = width - str->length;
    if (pad <= 0)
    {
        R[base] = receiver;
        break;
    }
    char *buf = (char *)malloc(width);
    memcpy(buf, str->chars, str->length);
    memset(buf + str->length, pad_ch, pad);
    R[base] = val_obj((Obj *)create_string(&gc_, buf, width));
    free(buf);
    break;
}
if (STR_METHOD("contains"))
{
    /* str.contains(needle) → bool */
    if (arg_count != 1 || !is_string(args[0]))
    {
        RT_ERROR("contains() expects a string");
    }
    ObjString *needle = as_string(args[0]);
    if (needle->length == 0)
    {
        R[base] = val_bool(true);
        break;
    }
    const char *f = find_sep(str->chars, str->length, needle->chars, needle->length);
    R[base] = val_bool(f != nullptr);
    break;
}
if (STR_METHOD("reverse"))
{
    /* str.reverse() → reversed string (byte-level, not UTF-8 aware) */
    char *buf = (char *)malloc(str->length);
    for (int ri = 0; ri < str->length; ri++)
        buf[ri] = str->chars[str->length - 1 - ri];
    R[base] = val_obj((Obj *)create_string(&gc_, buf, str->length));
    free(buf);
    break;
}
if (STR_METHOD("join"))
{
    /* sep.join(array) → join elements with sep */
    if (arg_count != 1 || !is_array(args[0]))
    {
        RT_ERROR("join() expects an array argument");
    }
    ObjArray *arr = as_array(args[0]);
    int32_t arr_len = arr_count(arr);
    if (arr_len == 0)
    {
        R[base] = val_obj((Obj *)create_string(&gc_, "", 0));
        break;
    }
    /* Calculate total length */
    int32_t total = 0;
    for (int32_t ji = 0; ji < arr_len; ji++)
    {
        if (!is_string(arr->data[ji]))
        {
            RT_ERROR("join() array elements must be strings");
        }
        total += as_string(arr->data[ji])->length;
    }
    total += str->length * (arr_len - 1); /* separators */
    char *buf = (char *)malloc(total);
    char *wp = buf;
    for (int32_t ji = 0; ji < arr_len; ji++)
    {
        if (ji > 0)
        {
            memcpy(wp, str->chars, str->length);
            wp += str->length;
        }
        ObjString *elem = as_string(arr->data[ji]);
        memcpy(wp, elem->chars, elem->length);
        wp += elem->length;
    }
    R[base] = val_obj((Obj *)create_string(&gc_, buf, total));
    free(buf);
    break;
}
if (STR_METHOD("lstrip"))
{
    /* str.lstrip() → strip leading whitespace */
    int start = 0;
    while (start < str->length && (str->chars[start] == ' ' || str->chars[start] == '\t' ||
                                   str->chars[start] == '\n' || str->chars[start] == '\r'))
        start++;
    R[base] = val_obj((Obj *)create_string(&gc_, str->chars + start, str->length - start));
    break;
}
if (STR_METHOD("rstrip"))
{
    /* str.rstrip() → strip trailing whitespace */
    int end = str->length;
    while (end > 0 && (str->chars[end - 1] == ' ' || str->chars[end - 1] == '\t' ||
                       str->chars[end - 1] == '\n' || str->chars[end - 1] == '\r'))
        end--;
    R[base] = val_obj((Obj *)create_string(&gc_, str->chars, end));
    break;
}

/* ---- Python string methods (added by the CPython differential pass) ---- */
if (STR_METHOD("title") || STR_METHOD("capitalize") || STR_METHOD("swapcase"))
{
    bool title = STR_METHOD("title"), cap = STR_METHOD("capitalize");
    gc_pause(&gc_);
    ObjString *out = create_string(&gc_, str->chars, str->length);
    char *p = (char *)out->chars;
    bool start = true;
    for (int k = 0; k < out->length; k++)
    {
        unsigned char ch = (unsigned char)p[k];
        if (title)
            p[k] = (char)(isalpha(ch) ? (start ? toupper(ch) : tolower(ch)) : ch), start = !isalpha(ch);
        else if (cap)
            p[k] = (char)(k == 0 ? toupper(ch) : tolower(ch));
        else
            p[k] = (char)(isupper(ch) ? tolower(ch) : (islower(ch) ? toupper(ch) : ch));
    }
    gc_resume(&gc_);
    R[base] = val_obj((Obj *)out);
    break;
}
if (STR_METHOD("isalpha") || STR_METHOD("isdigit") || STR_METHOD("isalnum") || STR_METHOD("isspace") ||
    STR_METHOD("isupper") || STR_METHOD("islower"))
{
    int kind = STR_METHOD("isalpha") ? 0 : STR_METHOD("isdigit") ? 1 : STR_METHOD("isalnum") ? 2 : STR_METHOD("isspace") ? 3 : STR_METHOD("isupper") ? 4 : 5;
    bool ok = str->length > 0, cased = false;
    for (int k = 0; k < str->length && ok; k++)
    {
        unsigned char ch = (unsigned char)str->chars[k];
        switch (kind)
        {
        case 0: ok = isalpha(ch); break;
        case 1: ok = isdigit(ch); break;
        case 2: ok = isalnum(ch); break;
        case 3: ok = isspace(ch); break;
        case 4: ok = !islower(ch); if (isupper(ch)) cased = true; break;
        default: ok = !isupper(ch); if (islower(ch)) cased = true; break;
        }
    }
    if (kind >= 4) ok = ok && cased;
    R[base] = val_bool(ok);
    break;
}
if (STR_METHOD("index") || STR_METHOD("rfind") || STR_METHOD("rindex"))
{
    if (arg_count < 1 || !is_string(args[0]))
        RT_ERROR("%s() expects a string argument", mname);
    ObjString *needle = as_string(args[0]);
    bool from_right = mname[0] == 'r';
    int pos = -1;
    if (needle->length <= str->length)
    {
        if (from_right)
        {
            for (int k = str->length - needle->length; k >= 0; k--)
                if (memcmp(str->chars + k, needle->chars, (size_t)needle->length) == 0) { pos = k; break; }
        }
        else
        {
            const char *found = needle->length == 0 ? str->chars : find_sep(str->chars, str->length, needle->chars, needle->length);
            pos = found ? (int)(found - str->chars) : -1;
        }
    }
    if (pos < 0 && (STR_METHOD("index") || STR_METHOD("rindex")))
        RT_ERROR("substring not found");
    R[base] = val_int(pos);
    break;
}
if (STR_METHOD("center") || STR_METHOD("ljust") || STR_METHOD("rjust") || STR_METHOD("zfill"))
{
    if (arg_count < 1 || !is_int(args[0]))
        RT_ERROR("%s() expects a width", mname);
    int width = (int)args[0].as.integer;
    char fill = STR_METHOD("zfill") ? '0' : ' ';
    if (arg_count >= 2 && is_string(args[1]) && as_string(args[1])->length == 1)
        fill = as_string(args[1])->chars[0];
    int padn = width > str->length ? width - str->length : 0;
    int left = STR_METHOD("ljust") ? 0 : STR_METHOD("rjust") || STR_METHOD("zfill") ? padn : padn / 2;
    char *p = (char *)malloc((size_t)str->length + padn + 1);
    int k = 0;
    if (STR_METHOD("zfill") && str->length > 0 && (str->chars[0] == '-' || str->chars[0] == '+') && padn > 0)
    {
        p[k++] = str->chars[0];
        for (int z = 0; z < padn; z++) p[k++] = '0';
        memcpy(p + k, str->chars + 1, (size_t)str->length - 1);
    }
    else
    {
        for (int z = 0; z < left; z++) p[k++] = fill;
        memcpy(p + k, str->chars, (size_t)str->length);
        k += str->length;
        for (int z = left; z < padn; z++) p[k++] = fill;
    }
    R[base] = val_obj((Obj *)create_string(&gc_, p, str->length + padn));
    free(p);
    break;
}
if (STR_METHOD("splitlines"))
{
    gc_pause(&gc_);
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result);
    int startp = 0;
    for (int k = 0; k < str->length; k++)
    {
        if (str->chars[k] == '\n' || str->chars[k] == '\r')
        {
            array_push(&gc_, result, val_obj((Obj *)create_string(&gc_, str->chars + startp, k - startp)));
            if (str->chars[k] == '\r' && k + 1 < str->length && str->chars[k + 1] == '\n') k++;
            startp = k + 1;
        }
    }
    if (startp < str->length)
        array_push(&gc_, result, val_obj((Obj *)create_string(&gc_, str->chars + startp, str->length - startp)));
    gc_resume(&gc_);
    break;
}
if (STR_METHOD("rsplit") || STR_METHOD("partition") || STR_METHOD("rpartition"))
{
    if (arg_count < 1 || !is_string(args[0]) || as_string(args[0])->length == 0)
        RT_ERROR("%s() expects a non-empty separator", mname);
    ObjString *sep = as_string(args[0]);
    gc_pause(&gc_);
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result);
    if (STR_METHOD("rsplit"))
    {
        int maxsplit = (arg_count >= 2 && is_int(args[1])) ? (int)args[1].as.integer : -1;
        int endp = str->length, splits = 0;
        while (true)
        {
            int k = endp - sep->length;
            while (k >= 0 && memcmp(str->chars + k, sep->chars, (size_t)sep->length) != 0) k--;
            if (k < 0 || (maxsplit >= 0 && splits >= maxsplit))
            {
                array_insert(&gc_, result, 0, val_obj((Obj *)create_string(&gc_, str->chars, endp)));
                break;
            }
            array_insert(&gc_, result, 0, val_obj((Obj *)create_string(&gc_, str->chars + k + sep->length, endp - k - sep->length)));
            endp = k;
            splits++;
        }
    }
    else
    {
        bool right = STR_METHOD("rpartition");
        int k = -1;
        if (right) { for (int j = str->length - sep->length; j >= 0; j--) if (memcmp(str->chars + j, sep->chars, (size_t)sep->length) == 0) { k = j; break; } }
        else { const char *f = find_sep(str->chars, str->length, sep->chars, sep->length); k = f ? (int)(f - str->chars) : -1; }
        if (k < 0)
        {
            array_push(&gc_, result, right ? val_obj((Obj *)create_string(&gc_, "", 0)) : receiver);
            array_push(&gc_, result, val_obj((Obj *)create_string(&gc_, "", 0)));
            array_push(&gc_, result, right ? receiver : val_obj((Obj *)create_string(&gc_, "", 0)));
        }
        else
        {
            array_push(&gc_, result, val_obj((Obj *)create_string(&gc_, str->chars, k)));
            array_push(&gc_, result, val_obj((Obj *)sep));
            array_push(&gc_, result, val_obj((Obj *)create_string(&gc_, str->chars + k + sep->length, str->length - k - sep->length)));
        }
    }
    gc_resume(&gc_);
    break;
}
{
    RT_ERROR("string has no method '%s'", mname);
}
} while (0);

#undef STR_METHOD
