# Zen Struct Module Reference

**Complete reference for binary packing/unpacking with 5 functions.**

Import with: `import struct`

---

## Overview

Python-compatible binary packing/unpacking over typed arrays. Convert between Zen values and binary byte buffers.

### Quick Start

```python
import struct

data = struct.pack("iif", 42, 100, 3.14)   # → Uint8Array (12 bytes)
vals = struct.unpack("iif", data)           # → [42, 100, 3.14]
n    = struct.calcsize("iif")               # → 12
```

---

## Format Characters

| Char | Type | Size | Description |
|------|------|------|-------------|
| `b` | int8 | 1 | Signed 8-bit integer |
| `B` | uint8 | 1 | Unsigned 8-bit integer |
| `h` | int16 | 2 | Signed 16-bit integer |
| `H` | uint16 | 2 | Unsigned 16-bit integer |
| `i` | int32 | 4 | Signed 32-bit integer |
| `I` | uint32 | 4 | Unsigned 32-bit integer |
| `q` | int64 | 8 | Signed 64-bit integer |
| `Q` | uint64 | 8 | Unsigned 64-bit integer |
| `f` | float32 | 4 | 32-bit float |
| `d` | float64 | 8 | 64-bit float |
| `x` | pad | 1 | Pad byte (pack: writes 0, unpack: skips) |

### Byte Order Prefixes

| Prefix | Endianness |
|--------|------------|
| `<` | Little-endian |
| `>` or `!` | Big-endian |
| `=` or `@` | Native (default) |

### Repeat Counts

Format chars can be prefixed with a repeat count: `"3i"` = `"iii"`.

---

## Functions

| Function | Arity | Description |
|----------|-------|-------------|
| `pack` | -1 | Pack values to binary: `pack(fmt, v1, v2, ...) → Uint8Array` |
| `unpack` | 2 | Unpack binary to values: `unpack(fmt, buffer) → array` |
| `calcsize` | 1 | Calculate byte size: `calcsize(fmt) → int` |
| `pack_into` | -1 | Pack into existing buffer: `pack_into(fmt, buffer, offset, v1, v2, ...) → nil` |
| `unpack_from` | -1 | Unpack from offset: `unpack_from(fmt, buffer[, offset]) → array` |

---

### pack(fmt, ...) → Uint8Array

Pack values into a new binary buffer.

**Signature:** `pack(fmt: string, v1, v2, ...) → Uint8Array`

**Example:**
```python
import struct

# Pack an int32, a uint16, and a float
data = struct.pack("ihf", 42, 1000, 3.14)
print(len(data))  # 10 bytes (4 + 2 + 4)

# Big-endian network packet
header = struct.pack(">IHH", seq_num, msg_type, payload_len)
```

---

### unpack(fmt, buffer) → array

Unpack binary buffer into an array of values.

**Signature:** `unpack(fmt: string, buffer: Uint8Array) → array`

**Example:**
```python
import struct

data = struct.pack("iif", 10, 20, 3.14)
vals = struct.unpack("iif", data)
print(vals[0])  # 10
print(vals[1])  # 20
print(vals[2])  # 3.14
```

---

### calcsize(fmt) → int

Calculate the byte size of a format string.

**Signature:** `calcsize(fmt: string) → int`

**Example:**
```python
import struct

struct.calcsize("i")     # 4
struct.calcsize("iif")   # 12 (4 + 4 + 4)
struct.calcsize("3i")    # 12 (3 × 4)
struct.calcsize("BHI")   # 7 (1 + 2 + 4)
```

---

### pack_into(fmt, buffer, offset, ...) → nil

Pack values into an existing buffer at a given offset.

**Signature:** `pack_into(fmt: string, buffer: Uint8Array, offset: int, v1, v2, ...) → nil`

**Example:**
```python
import struct

buf = Uint8Array(20)
struct.pack_into("ii", buf, 0, 100, 200)   # Write at offset 0
struct.pack_into("f",  buf, 8, 3.14)       # Write at offset 8
```

---

### unpack_from(fmt, buffer[, offset]) → array

Unpack values from a buffer starting at an offset.

**Signature:** `unpack_from(fmt: string, buffer: Uint8Array[, offset: int]) → array`

**Example:**
```python
import struct

buf = struct.pack("iif", 1, 2, 3.0)
vals = struct.unpack_from("i", buf, 4)   # Read second int
print(vals[0])  # 2
```
