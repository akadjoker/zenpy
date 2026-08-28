# Zen Base Library Reference

**Complete reference for the base Zen library with 41 global functions and 2 built-in classes.**

---

## Type Conversion

Convert values between types.

| Function | Arity | Description |
|----------|-------|-------------|
| `str` | 1 | Convert to string. |
| `int` | 1 | Convert to integer (truncates floats). |
| `float` | 1 | Convert to floating-point. |
| `char` | 1 | Convert int codepoint to 1-char string. |
| `ord` | 1 | Get first character codepoint as int. |

**Example:**
```python
str(123)        # → "123"
int(3.14)       # → 3
float("2.5")    # → 2.5
char(65)        # → "A"
ord("A")        # → 65
```

---

## Type Checking

Check value types. Predicate helpers return `bool`; `typeof` and `type` return strings.

| Function | Arity | Description |
|----------|-------|-------------|
| `typeof` | 1 | Type name as string. |
| `type` | 1 | Python-style type name as string. |
| `isNil` | 1 | Check if value is nil. |
| `isBool` | 1 | Check if value is boolean. |
| `isInt` | 1 | Check if value is integer. |
| `isFloat` | 1 | Check if value is float. |
| `isNumber` | 1 | Check if value is int or float. |
| `isString` | 1 | Check if value is string. |
| `isArray` | 1 | Check if value is array. |
| `isMap` | 1 | Check if value is map/dictionary. |
| `isFunction` | 1 | Check if value is function or closure. |
| `isinstance` | 2 | Check if object is instance of class: `isinstance(obj, ClassName)`. |

**Example:**
```python
typeof(42)           # → "int"
typeof(3.14)         # → "float"
typeof([1,2,3])      # → "array"
typeof({a: 1})       # → "map"
type([1, 2, 3])      # → "list"
type({a: 1})         # → "dict"
isNumber(42)         # → true
isNumber("42")       # → false
isinstance(player, Player)  # → true if player is Player instance
```

---

## Utilities

General-purpose utilities.

| Function | Arity | Description |
|----------|-------|-------------|
| `input` | -1 | Read line from stdin: `input([prompt])`. |
| `assert` | -1 | Raise error if condition false: `assert(cond[, message])`. |
| `error` | 1 | Raise runtime error with message. |
| `range` | -1 | Create a range object: `range(start[, end[, step]])`. |
| `format` | -1 | C-style string formatting: `format(fmt, arg1, arg2, ...)`. |

**Example:**
```python
# Input
name = input("Enter name: ")  # Reads from stdin

# Assert & Error
assert(x > 0, "x must be positive")
if x < 0:
    error("Invalid x!")

# Range
for i in range(5):
    print(i)        # 0, 1, 2, 3, 4

for i in range(1, 5):
    print(i)        # 1, 2, 3, 4

# Format
format("Hello %s, you are %d years old", name, age)
```

---

## Garbage Collection & Memory

Control garbage collection and inspect memory usage.

| Function | Arity | Description |
|----------|-------|-------------|
| `collect` | 0 | Run garbage collection immediately. |
| `gc_pause` | 0 | Pause automatic GC (for batch operations). |
| `gc_resume` | 0 | Resume automatic GC. |
| `mem_used` | 0 | Get current memory usage in bytes. |
| `mem_info` | 0 | Get `(object_count, bytes_allocated, next_gc_threshold)`. |
| `clock` | 0 | Get a monotonic time value in seconds. |

**Example:**
```python
# Manual GC
gc_pause()
for i in range(1000):
    arr.push([1, 2, 3])  # Lots of allocations
gc_resume()
collect()  # Force collection

# Memory info
objects, used, next_gc = mem_info()
print("Objects:", objects)
print("Using", used, "bytes")

# Timing
start = clock()
expensive_operation()
elapsed = clock() - start
print("Took", elapsed, "seconds")
```

---

## Higher-Order Functions

Functional programming utilities.

| Function | Arity | Description |
|----------|-------|-------------|
| `enumerate` | 1 | Enumerate an array or string: `[[0, item0], [1, item1], ...]`. |
| `zip` | -1 | Zip multiple arrays to an array of tuples. |
| `map` | 2 | Transform an array: `map(func, array)`. |
| `filter` | 2 | Filter an array: `filter(predicate, array)`. |

**Example:**
```python
# Enumerate
for (i, val) in enumerate([10, 20, 30]):
    print(i, val)  # 0 10, 1 20, 2 30

# Zip
names = ["Alice", "Bob", "Charlie"]
ages = [25, 30, 35]
for (name, age) in zip(names, ages):
    print(name, age)

# Map & Filter
def double(x):
    return x * 2

def is_even(x):
    return x % 2 == 0

nums = [1, 2, 3, 4, 5]
doubled = map(double, nums)    # [2, 4, 6, 8, 10]
evens = filter(is_even, nums)  # [2, 4]
```

---

## I/O

File and stream operations.

| Function | Arity | Description |
|----------|-------|-------------|
| `open` | -1 | Open file: `open(path[, mode])`. Returns `File` and raises runtime error on failure. |

**File Modes:**
- `"r"` — Read (default)
- `"w"` — Write (overwrites)
- `"a"` — Append
- `"rb"` — Read binary
- `"wb"` — Write binary

### File Class

Opened via `open()`.

| Method | Signature | Description |
|--------|-----------|-------------|
| `read` | `read([size])` | Read remaining contents as string, or up to `size` bytes. |
| `readlines` | `readlines()` | Read all lines as array. |
| `write` | `write(text)` | Write string to file. Returns bytes written. |
| `close` | `close()` | Close file handle. |

**Example:**
```python
# Read file
f = open("config.txt", "r")
config = f.read()
f.close()

# Write file
f = open("output.txt", "w")
f.write("Hello, world!\n")
f.close()

# Read lines
f = open("lines.txt")
for line in f.readlines():
    print(line)
f.close()
```

---

## Typed Arrays

Fixed-size numeric buffers for binary data, performance, or C interop.

| Function | Arity | Description |
|----------|-------|-------------|
| `Int8Array` | 1 | Create an 8-bit signed integer buffer from a size or array. |
| `Int16Array` | 1 | Create a 16-bit signed integer buffer from a size or array. |
| `Int32Array` | 1 | Create a 32-bit signed integer buffer from a size or array. |
| `Uint8Array` | 1 | Create an 8-bit unsigned integer buffer from a size or array. |
| `Uint16Array` | 1 | Create a 16-bit unsigned integer buffer from a size or array. |
| `Uint32Array` | 1 | Create a 32-bit unsigned integer buffer from a size or array. |
| `Float32Array` | 1 | Create a 32-bit float buffer from a size or array. |
| `Float64Array` | 1 | Create a 64-bit float buffer from a size or array. |

**Example:**
```python
# Create typed array of 10 elements
data = Uint8Array(10)
data[0] = 255
data[1] = 128

# Float array for graphics
vertices = Float32Array(12)  # 4 vertices × 3 coords
vertices[0] = 0.0    # vertex 0, x
vertices[1] = 0.0    # vertex 0, y
vertices[2] = -1.0   # vertex 0, z
# ... etc

# Length
print(data.len())  # 10

# Iteration
for val in data:
    print(val)

# Index access
print(data[5])
data[5] = 42
```

**Performance:** Typed arrays are contiguous memory blocks — faster than dynamic arrays for numerical data, audio, images, etc.

---

## Signal Class

GDScript-style event system for callbacks and listeners.

| Method | Signature | Description |
|--------|-----------|-------------|
| `connect` | `connect(callback)` | Register listener function. |
| `disconnect` | `disconnect(callback)` | Unregister listener. Returns bool (found?). |
| `emit` | `emit(args...)` | Call all listeners with forwarded arguments. |
| `count` | `count() → int` | Number of connected listeners. |
| `clear` | `clear()` | Disconnect all listeners. |

**Example:**
```python
events = Signal()
acc = []

def on_event(value):
    acc.append(value)

events.connect(on_event)
events.emit(3)
print(acc[0])  # 3
```

**Signal Pattern:**
- **Sender** owns Signal, calls `emit()`
- **Listener** connects via `connect(callback)`
- **Decoupling** — sender doesn't know listeners, listeners don't know sender
- **Multiple subscribers** — all connected callbacks fire on emit

---

## Common Patterns

### Type Validation

```python
def process_data(data):
    assert(isArray(data), "Expected array")
    assert(len(data) > 0, "Array is empty")
    
    for item in data:
        if not isNumber(item):
            error("All items must be numbers")
```

### Safe Type Conversion

```python
def get_int(val, default = 0):
    if isInt(val):
        return val
    if isFloat(val):
        return int(val)
    return default

num = get_int(3.14)      # → 3
num = get_int("42")      # → 0 (this helper does not parse strings)
```

### File I/O with Error Handling

```python
def read_file_safe(path):
    f = open(path, "r")
    content = f.read()
    f.close()
    return content

def write_file_safe(path, content):
    f = open(path, "w")
    f.write(content)
    f.close()
```

### Custom Event System

```python
def on_player_died(level_name):
    print("player died in", level_name)

events = {}
events["player_died"] = Signal()
events["player_died"].connect(on_player_died)

# Usage
events["player_died"].emit("Level1")
```

---

## Notes

- **Range:** Start inclusive, end exclusive. It returns a range object, typically used directly in loops.
- **GC:** Automatic but pausable. Manual `collect()` rarely needed unless batch operations.
- **Signals:** Thread-unsafe. Use in single-threaded context only.
- **Typed Arrays:** Immutable size once created. For dynamic data, use regular arrays `[]`.

---

**Generated from:** `builtin_base.cpp`  
**Last Updated:** 2026-05-18  
**Coverage:** 41 global functions, 2 classes (File, Signal), typed array constructors
