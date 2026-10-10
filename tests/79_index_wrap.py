# Subscripts used to be narrowed to 32 bits before the bounds check, so
# xs[4294967296] (2**32) wrapped to xs[0] instead of being out of range.
# The out-of-range ERRORS cannot be asserted here (the language has no
# try/except); they are checked in tests/cpp/test_embedding.cpp
# (test_index_beyond_32_bits). This file pins the behaviour that must stay:
# valid indices, and slice bounds, which clamp (Python semantics) rather
# than wrap.
xs = [10, 20, 30]
big = 4294967296        # 2**32: the first value a 32-bit cast wraps to 0
s = "abc"

# Valid and negative indices, unchanged.
assert xs[0] == 10 and xs[2] == 30 and xs[-1] == 30 and xs[-3] == 10
assert s[0] == "a" and s[-1] == "c"
xs[1] = 99
assert xs[1] == 99
xs[-1] = 7
assert xs[2] == 7

# Slice bounds past either end clamp, they do not wrap to 0.
ys = [1, 2, 3]
assert ys[:big] == [1, 2, 3]
assert ys[big:] == []
assert ys[-big:] == [1, 2, 3]
assert ys[:-big] == []
assert ys[0:big] == [1, 2, 3]
assert ys[::big] == [1]
assert ys[::-big] == [3]
assert ys[big::-1] == [3, 2, 1]
assert ys[-big::-1] == []
assert s[:big] == "abc"
assert s[big:] == ""
assert s[-big:] == "abc"
assert s[:-big] == ""
assert s[::big] == "a"
assert s[big::-1] == "cba"
assert ys[:9223372036854775807] == [1, 2, 3]
assert ys[-9223372036854775807:] == [1, 2, 3]

# Array methods taking an index.
zs = [1, 2, 3]
assert zs.slice(big) == []
assert zs.slice(0, big) == [1, 2, 3]
assert zs.slice(-big) == [1, 2, 3]
assert zs.pop(-1) == 3
assert zs == [1, 2]
assert "abc".char_at(big) is None or "abc".char_at(big) == None
assert "abc".byte_at(big) == 0
assert "abc".char_at(1) == "b"
assert "abc".byte_at(1) == 98

# Typed buffers.
b = Uint32Array(4)
b[0] = 5
b[3] = 8
assert b[0] == 5 and b[3] == 8 and b[-1] == 8
sl = b.slice(big)
assert sl.len() == 0

print("index wrap OK")
