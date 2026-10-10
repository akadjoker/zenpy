# Integer // and % must keep all 64 bits (// used to truncate its result to
# 32 bits; % used to truncate its divisor). Expected values come from CPython.
import struct

big = 999999000000
two = 2
assert big // two == 499999500000
assert 999999000000 // 2 == 499999500000
assert (1 << 40) // 3 == 366503875925
assert -7 // 2 == -4
assert 7 // -2 == -4
neg = -999999000000
seven = 7
assert neg // seven == -142857000000
assert 999999000000 % 7 == 0
assert (1 << 40) % 3 == 1
assert -7 % 2 == 1
assert 7 % -2 == -1

# divisor wider than 32 bits (was truncated to 0 -> "modulo by zero")
w = 4294967296
assert 12884901893 // w == 3
assert 12884901893 % w == 5
assert (-(1 << 40)) // ((1 << 33) + 1) == -128
assert (-(1 << 40)) % ((1 << 33) + 1) == 128
assert (1 << 62) // 3 == 1537228672809129301
assert (1 << 62) % 3 == 1

# divmod
dm = divmod(999999000000, 2)
assert dm[0] == 499999500000 and dm[1] == 0
dm = divmod(1 << 40, 3)
assert dm[0] == 366503875925 and dm[1] == 1
dm = divmod(-7, 2)
assert dm[0] == -4 and dm[1] == 1
dm = divmod(7, -2)
assert dm[0] == -4 and dm[1] == -1
dm = divmod(12884901893, w)
assert dm[0] == 3 and dm[1] == 5

# float floor division is unaffected
assert 7.5 // 2 == 3.0
assert -7.5 // 2 == -4.0

# struct.unpack of 64-bit / unsigned 32-bit values
assert struct.unpack('q', struct.pack('q', -(1 << 40)))[0] == -(1 << 40)
assert struct.unpack('q', struct.pack('q', 1 << 40))[0] == 1 << 40
assert struct.unpack('I', struct.pack('I', 4000000000))[0] == 4000000000

# integer default arguments wider than 32 bits
def d(x=3000000000, y=-3000000000):
    return x + y * 2
assert d() == -3000000000
# Uint32Array.tolist() keeps values above 2**31
u = Uint32Array(2)
u[0] = 4000000000
u[1] = 5
ul = u.tolist()
assert ul[0] == 4000000000 and ul[1] == 5
print("ok")
