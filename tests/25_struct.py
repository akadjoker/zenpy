# Test struct module — binary packing/unpacking

import struct

# --- calcsize ---
assert struct.calcsize('b') == 1
assert struct.calcsize('h') == 2
assert struct.calcsize('i') == 4
assert struct.calcsize('q') == 8
assert struct.calcsize('f') == 4
assert struct.calcsize('d') == 8
assert struct.calcsize('iif') == 12
assert struct.calcsize('3i') == 12
assert struct.calcsize('BHI') == 7
print("calcsize OK")

# --- pack + unpack basic types ---
data = struct.pack('i', 42)
assert data.len() == 4
vals = struct.unpack('i', data)
assert vals[0] == 42
print("int32 OK")

data = struct.pack('f', 3.14)
vals = struct.unpack('f', data)
# float32 loses precision
assert vals[0] > 3.13
assert vals[0] < 3.15
print("float32 OK")

data = struct.pack('d', 3.141592653589793)
vals = struct.unpack('d', data)
assert vals[0] == 3.141592653589793
print("float64 OK")

# --- multiple values ---
data = struct.pack('iif', 42, 100, 2.5)
vals = struct.unpack('iif', data)
assert vals[0] == 42
assert vals[1] == 100
assert vals[2] > 2.49
assert vals[2] < 2.51
print("multi-value OK")

# --- byte types ---
data = struct.pack('bB', -128, 255)
vals = struct.unpack('bB', data)
assert vals[0] == -128
assert vals[1] == 255
print("byte types OK")

# --- short types ---
data = struct.pack('hH', -32768, 65535)
vals = struct.unpack('hH', data)
assert vals[0] == -32768
assert vals[1] == 65535
print("short types OK")

# --- repeat count ---
data = struct.pack('3i', 10, 20, 30)
vals = struct.unpack('3i', data)
assert vals[0] == 10
assert vals[1] == 20
assert vals[2] == 30
print("repeat count OK")

# --- pad byte 'x' ---
assert struct.calcsize('ixf') == 9  # 4 + 1 + 4
data = struct.pack('ixf', 42, 2.5)
vals = struct.unpack('ixf', data)
assert vals[0] == 42
assert vals[1] > 2.49
print("pad byte OK")

# --- pack_into / unpack_from ---
buf = Uint8Array(20)
struct.pack_into('iif', buf, 0, 1, 2, 3.0)
vals = struct.unpack_from('iif', buf, 0)
assert vals[0] == 1
assert vals[1] == 2
assert vals[2] > 2.99
print("pack_into/unpack_from OK")

# --- pack_into at offset ---
struct.pack_into('i', buf, 12, 999)
vals2 = struct.unpack_from('i', buf, 12)
assert vals2[0] == 999
# original data untouched
vals3 = struct.unpack_from('i', buf, 0)
assert vals3[0] == 1
print("offset pack_into OK")

# --- mixed int and float ---
data = struct.pack('BHId', 255, 1000, 100000, 1.23456789)
vals = struct.unpack('BHId', data)
assert vals[0] == 255
assert vals[1] == 1000
assert vals[2] == 100000
assert vals[3] > 1.234
assert vals[3] < 1.235
print("mixed types OK")

print("all struct tests passed")
