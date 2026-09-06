def one_type<T>(x):
    return x

# Bug #3: def f<T>(x) called as f<A, B>() must be an error (old sugar only
# checked TOTAL arg count, so this used to silently compile).
one_type<int, float>(5)
