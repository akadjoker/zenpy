# INT64_MIN // -1 and INT64_MIN % -1 are C++ UB (the CPU raises SIGFPE).
# They must not crash. CPython promotes -(2**63) // -1 to 2**63, which does
# not fit an int64: ZenPy wraps it to INT64_MIN (ints are 64-bit wrapping,
# see bugs.md "Overflow de int silencioso"). The remainder is exactly 0.
#
# The literal -9223372036854775808 still parses as ...807 (bugs.md), so the
# minimum is built as -9223372036854775807 - 1.
m = -9223372036854775807 - 1
mx = 9223372036854775807
one = -1

assert m // -1 == m          # wraps; CPython would give 2**63
assert m % -1 == 0           # CPython: 0
assert m // one == m         # same, divisor not a constant
assert m % one == 0

q = m
q //= one                    # augmented forms take the same opcode
assert q == m
r = m
r %= one
assert r == 0

d = divmod(m, -1)            # CPython: (2**63, 0)
assert d[0] == m
assert d[1] == 0
d2 = divmod(m, one)
assert d2[0] == m and d2[1] == 0

# The neighbours of the trap stay exact.
assert mx // -1 == -mx
assert mx % -1 == 0
assert (m + 1) // -1 == mx
assert (m + 1) % -1 == 0
assert m // 1 == m
assert m % 1 == 0
assert m // 2 == -4611686018427387904
assert m % 2 == 0
assert m // mx == -2          # floor of -1.00000000000000000011
assert m % mx == mx - 1

# Ordinary floor semantics are unchanged by the -1 guard.
assert 7 // -1 == -7
assert -7 // -1 == 7
assert 0 // -1 == 0
assert 7 % -1 == 0
assert -7 % -1 == 0
assert -7 // 2 == -4
assert -7 % 2 == 1
assert 7 // -2 == -4
assert 7 % -2 == -1
assert divmod(-7, 2) == [-4, 1] or divmod(-7, 2) == (-4, 1)
assert divmod(7, -2)[0] == -4 and divmod(7, -2)[1] == -1
assert divmod(7, -1)[0] == -7 and divmod(7, -1)[1] == 0

# 2**63 does not fit an int, so ** falls back to a float rather than wrapping.
assert -2**63 == -9223372036854775808.0

print("int64 edges OK")
