# Test typed arrays / buffers

# --- Int32Array ---
buf = Int32Array(5)
assert typeof(buf) == "buffer"
assert buf.len() == 5
assert buf.type_name() == "Int32Array"
assert buf.byte_len() == 20  # 5 * 4 bytes

buf[0] = 10
buf[1] = 20
buf[2] = 30
buf[3] = 40
buf[4] = 50
assert buf[0] == 10
assert buf[4] == 50
assert buf[-1] == 50
print("Int32Array basic OK")

# --- fill ---
buf.fill(99)
assert buf[0] == 99
assert buf[4] == 99
print("fill OK")

# --- tolist ---
lst = buf.tolist()
assert lst == [99, 99, 99, 99, 99]
print("tolist OK")

# --- copy (deep) ---
b2 = buf.copy()
b2[0] = 1
assert buf[0] == 99  # original unchanged
assert b2[0] == 1
print("copy OK")

# --- slice ---
buf[0] = 10
buf[1] = 20
buf[2] = 30
buf[3] = 40
buf[4] = 50
s = buf.slice(1, 4)
assert s.len() == 3
assert s.tolist() == [20, 30, 40]

s2 = buf.slice(3)
assert s2.tolist() == [40, 50]

s3 = buf.slice(-2)
assert s3.tolist() == [40, 50]
print("slice OK")

# --- Float64Array ---
f = Float64Array([1.5, 2.5, 3.5])
assert f.len() == 3
assert f[1] == 2.5
assert f.type_name() == "Float64Array"
assert f.byte_len() == 24  # 3 * 8 bytes
print("Float64Array OK")

# --- Float32Array ---
f32 = Float32Array([1.0, 2.0, 3.0])
assert f32.len() == 3
assert f32.type_name() == "Float32Array"
assert f32.byte_len() == 12  # 3 * 4 bytes
print("Float32Array OK")

# --- Uint8Array (byte buffer) ---
u8 = Uint8Array(256)
assert u8.len() == 256
assert u8.byte_len() == 256
for i in range(256):
    u8[i] = i
assert u8[0] == 0
assert u8[255] == 255
print("Uint8Array OK")

# --- All types exist ---
assert Int8Array(1).type_name() == "Int8Array"
assert Int16Array(1).type_name() == "Int16Array"
assert Int32Array(1).type_name() == "Int32Array"
assert Uint8Array(1).type_name() == "Uint8Array"
assert Uint16Array(1).type_name() == "Uint16Array"
assert Uint32Array(1).type_name() == "Uint32Array"
assert Float32Array(1).type_name() == "Float32Array"
assert Float64Array(1).type_name() == "Float64Array"
print("all types OK")

# --- From array with mixed types ---
mixed = Float64Array([1, 2.5, 3])
assert mixed[0] == 1.0
assert mixed[1] == 2.5
assert mixed[2] == 3.0
print("mixed init OK")

print("all buffer tests passed")
