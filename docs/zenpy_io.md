# Zen I/O Library Reference

**Complete reference for file reading and writing with module-level functions and File class.**

Import with: `import io`

---

## Overview

File I/O operations with two interfaces: convenient module-level functions for simple operations, and File class for advanced control.

### Module-Level Functions

| Function | Arity | Description |
|----------|-------|-------------|
| `read` | 1 | Read entire file as string. |
| `write` | 2 | Write string to file (overwrites). |
| `append` | 2 | Append string to end of file. |
| `readbytes` | 1 | Read entire file as Uint8Array (binary). |
| `writebytes` | 2 | Write binary buffer to file. |
| `exists` | 1 | Check if file exists. |
| `size` | 1 | Get file size in bytes. |
| `readlines` | 1 | Read file as array of lines. |

### File Class

Constructor: `File(path[, mode])` → File instance

| Method | Arity | Description |
|--------|-------|-------------|
| `read` | -1 | Read N bytes (or rest of file). |
| `readlines` | 0 | Read remaining lines as array. |
| `readbytes` | -1 | Read N bytes as Uint8Array (or rest). |
| `write` | 1 | Write string, return bytes written. |
| `writebytes` | 1 | Write buffer, return bytes written. |
| `seek` | -1 | Move file pointer. |
| `tell` | 0 | Get current file position. |
| `close` | 0 | Close file (auto-closes on GC). |

---

## Module-Level Functions

### read(path) → string

Read entire file contents as a string.

**Signature:** `read(path: string) → string`

**Limits:**
- Maximum 100 MB
- Returns empty string if file is empty
- Entire file loaded to memory

**Raises error on:**
- File not found
- Cannot read permission
- File larger than 100 MB

**Example:**
```python
import io

# Read JSON config
config_text = io.read("config.json")
config = json.parse(config_text)

# Read text file
lines_text = io.read("data.txt")
lines = lines_text.split("\n")

# Read script
script = io.read("script.zen")
print("Script size:", len(script), "bytes")

# Error handling
try:
    text = io.read("missing.txt")
except:
    print("File not found")
```

---

### write(path, text) → nil

Write string to file, **overwriting if exists**.

**Signature:** `write(path: string, text: string) → nil`

**Behavior:**
- Truncates existing file
- Creates if doesn't exist
- Atomic write

**Example:**
```python
import io

# Save config
config = {version: "1.0", level: 5}
io.write("save.json", json.stringify(config, 2))

# Write log entry
log_text = "Session started: " + time.time()
io.write("session.log", log_text)

# Generate file
code = "def hello():\n  print(\"hi\")"
io.write("generated.zen", code)
```

---

### append(path, text) → nil

Append string to **end of file**.

**Signature:** `append(path: string, text: string) → nil`

**Behavior:**
- Preserves existing content
- Creates if doesn't exist
- Atomic append

**Example:**
```python
import io

# Append to log
io.append("debug.log", "Frame " + str(frame_count) + "\n")

# Accumulate data
for item in items:
    io.append("items.txt", item + "\n")

# Event log
io.append("events.log", json.stringify({
    event: "spawn_enemy",
    timestamp: time.time(),
    pos: [x, y]
}) + "\n")
```

---

### readbytes(path) → Uint8Array

Read entire file as binary data (Uint8Array).

**Signature:** `readbytes(path: string) → Uint8Array`

**Returns:** Uint8Array with file contents

**Limits:**
- Maximum 100 MB
- Each element is 0-255

**Use for:**
- Image data
- Compressed data
- Custom binary formats

**Example:**
```python
import io

# Read image file
image_bytes = io.readbytes("sprite.png")
print("Image size:", len(image_bytes), "bytes")

# Read compressed data
compressed = io.readbytes("archive.zip")
# ... decompress ...

# Binary format
binary_data = io.readbytes("data.bin")
# Parse manually or pass to C++ binary reader
```

---

### writebytes(path, buffer) → nil

Write binary buffer (typed array) to file.

**Signature:** `writebytes(path: string, buffer: TypedArray) → nil`

**Accepted buffers:**
- Uint8Array
- Int8Array
- Uint16Array
- Int16Array
- Uint32Array
- Int32Array
- Float32Array
- Float64Array

**Example:**
```python
import io

# Create binary data
pixels = Uint8Array(1024)
for i in range(1024):
    pixels[i] = i % 256

# Write to file
io.writebytes("pixels.bin", pixels)

# Write float array
data = Float32Array(100)
for i in range(100):
    data[i] = i * 3.14

io.writebytes("floats.bin", data)
```

---

### exists(path) → bool

Check if file exists.

**Signature:** `exists(path: string) → bool`

**Returns:**
- `true` if file exists
- `false` otherwise

**Example:**
```python
import io

if io.exists("save.json"):
    save = json.parse(io.read("save.json"))
else:
    save = {level: 1, score: 0}

# Conditional operations
if not io.exists("log.txt"):
    io.write("log.txt", "Log started\n")

# Check and create
if not io.exists("config.ini"):
    io.write("config.ini", default_config)
```

---

### size(path) → int

Get file size in bytes.

**Signature:** `size(path: string) → int`

**Returns:** File size in bytes

**Raises error on:**
- File not found
- Cannot read permission

**Example:**
```python
import io

sz = io.size("game.exe")
print("Executable:", sz, "bytes")

# Check if file is too large
if io.size("input.txt") > 10*1024*1024:
    print("File too large (>10MB)")
else:
    content = io.read("input.txt")

# Verify download
if io.size("download.zip") == expected_size:
    print("Download complete")
else:
    print("Incomplete download")
```

---

### readlines(path) → [string]

Read file as array of lines (line breaks removed).

**Signature:** `readlines(path: string) → array of strings`

**Line breaks:**
- Handles `\n` (Unix)
- Handles `\r\n` (Windows)
- Removes line breaks from result

**Limits:**
- Maximum 100 MB total
- Each line is a separate string

**Example:**
```python
import io

# Read config lines
lines = io.readlines("settings.txt")
for line in lines:
    if line.starts_with("#"):
        continue  # Skip comments
    parts = line.split("=")
    key = parts[0]
    value = parts[1]
    print(key, "→", value)

# Parse CSV-like format
rows = io.readlines("data.csv")
for row in rows:
    fields = row.split(",")
    process_record(fields)

# Count lines
count = len(io.readlines("input.txt"))
print("File has", count, "lines")
```

---

## File Class

### Constructor: File(path[, mode])

Create File instance for granular control.

**Signature:** `File(path: string[, mode: string]) → File`

**Modes:**

| Mode | Description | Usage |
|------|-------------|-------|
| `"r"` | Read (default) | Read existing file |
| `"w"` | Write (truncate) | Overwrite file |
| `"a"` | Append | Add to end |
| `"r+"` | Read/write | Read and modify |

**Raises error on:**
- Cannot open (missing, permission denied, etc.)
- Invalid mode

**Auto-close:** File automatically closes when garbage collected (even if not explicitly closed).

**Example:**
```python
import io

# Read mode (default)
f = File("data.txt")
content = f.read()
f.close()

# Write mode (truncate)
f = File("output.txt", "w")
f.write("Line 1\n")
f.write("Line 2\n")
f.close()

# Append mode
f = File("log.txt", "a")
f.write("New log entry\n")
f.close()

# Auto-close on scope exit (no explicit close needed)
def read_config(path):
    f = File(path)  # Opens
    data = f.read()
    return data
    # f auto-closes when function returns
```

---

### file.read([size]) → string

Read bytes from current position.

**Signature:** `read([size: int]) → string`

**Parameters:**
- `size` (optional): Max bytes to read. If omitted, reads to end of file.

**Returns:** String with data read

**Example:**
```python
import io

f = File("data.txt")

# Read all remaining
content = f.read()

# Read specific amount
f.seek(0)
first_1024 = f.read(1024)

# Read in chunks
chunk_size = 4096
f.seek(0)
while true:
    chunk = f.read(chunk_size)
    if len(chunk) == 0:
        break
    process_chunk(chunk)

f.close()
```

---

### file.readlines() → [string]

Read all remaining lines.

**Signature:** `readlines() → array of strings`

**Behavior:**
- Reads from current position to end
- Each line is separate array element
- Line breaks removed
- Handles `\n` and `\r\n`

**Example:**
```python
import io

f = File("items.txt")

# Read all lines
all_lines = f.readlines()
print("Total lines:", len(all_lines))

# Read part way through, then get rest
f.seek(100)  # Skip first 100 bytes
remaining_lines = f.readlines()

f.close()
```

---

### file.readbytes([size]) → Uint8Array

Read binary data.

**Signature:** `readbytes([size: int]) → Uint8Array`

**Parameters:**
- `size` (optional): Bytes to read. If omitted, reads to end.

**Returns:** Uint8Array with binary data

**Example:**
```python
import io

# Read binary file
f = File("image.png", "r")
png_data = f.readbytes()
f.close()

# Read in chunks
f = File("large.bin", "r")
chunk1 = f.readbytes(1024)
chunk2 = f.readbytes(1024)
chunk3 = f.readbytes()  # Rest
f.close()
```

---

### file.write(text) → int

Write string to file.

**Signature:** `write(text: string) → int`

**Returns:** Number of bytes written

**Example:**
```python
import io

f = File("output.txt", "w")

bytes_written = f.write("Hello, World!\n")
print("Wrote", bytes_written, "bytes")

# Multiple writes
f.write("Line 1\n")
f.write("Line 2\n")
f.write("Line 3\n")

f.close()
```

---

### file.writebytes(buffer) → int

Write binary data.

**Signature:** `writebytes(buffer: TypedArray) → int`

**Returns:** Number of bytes written

**Example:**
```python
import io

# Create binary data
data = Uint8Array(256)
for i in range(256):
    data[i] = i

# Write to file
f = File("binary.bin", "w")
n = f.writebytes(data)
print("Wrote", n, "bytes")
f.close()
```

---

### file.seek(offset[, whence]) → int

Move file pointer.

**Signature:** `seek(offset: int[, whence: int]) → int`

**Whence parameter:**
- `0` (SET): Absolute position from start
- `1` (CUR): Relative to current position (default if omitted)
- `2` (END): Relative to end of file

**Returns:** New file position

**Example:**
```python
import io

f = File("data.bin")

# Seek to start
f.seek(0, 0)
first_4 = f.read(4)

# Seek to end
f.seek(0, 2)  # Go to end
pos = f.tell()
print("File size:", pos)

# Seek backwards from end
f.seek(-10, 2)  # Last 10 bytes
last_10 = f.read(10)

# Seek from current position
f.seek(0)
f.seek(100, 1)  # Skip 100 bytes from current
next_100 = f.read(100)

f.close()
```

---

### file.tell() → int

Get current file position.

**Signature:** `tell() → int`

**Returns:** Current position in bytes

**Example:**
```python
import io

f = File("data.txt")

pos1 = f.tell()  # → 0

content = f.read(50)
pos2 = f.tell()  # → 50

f.seek(0)
pos3 = f.tell()  # → 0

f.close()
```

---

### file.close() → nil

Close file explicitly.

**Signature:** `close() → nil`

**Note:** File auto-closes on garbage collection. Explicit close is optional but recommended.

**Example:**
```python
import io

f = File("data.txt")
content = f.read()
f.close()

# After close, methods will error:
try:
    f.read()  # ✗ Error: file is closed
except:
    print("Can't read closed file")
```

---

## Common Patterns

### Load/Save Game State

```python
import io
import json

class GameSave:
    def __init__(self):
        self.level = 1
        self.score = 0
        self.player_pos = [100, 100]
        self.inventory = ["sword", "shield"]
    
    def save(self, filename):
        data = {
            level: self.level,
            score: self.score,
            player_pos: self.player_pos,
            inventory: self.inventory
        }
        io.write(filename, json.stringify(data, 2))
    
    def load(self, filename):
        if not io.exists(filename):
            return false
        
        data = json.parse(io.read(filename))
        self.level = data["level"]
        self.score = data["score"]
        self.player_pos = data["player_pos"]
        self.inventory = data["inventory"]
        return true

# Usage
game = GameSave()
game.level = 5
game.score = 1000
game.save("save.json")

# Later
game2 = GameSave()
game2.load("save.json")
print("Loaded level", game2.level)
```

### Process CSV File

```python
import io

class CSVReader:
    def __init__(self, path, delimiter = ","):
        self.path = path
        self.delimiter = delimiter
    
    def read(self):
        lines = io.readlines(self.path)
        rows = []
        for line in lines:
            if line.length == 0:
                continue
            fields = line.split(self.delimiter)
            rows.push(fields)
        return rows

# Usage
csv = CSVReader("data.csv")
rows = csv.read()
for row in rows:
    name = row[0]
    age = row[1]
    print(name, "is", age)
```

### Log Writer

```python
import io
import time

class Logger:
    def __init__(self, filename):
        self.filename = filename
    
    def log(self, level, message):
        timestamp = time.time()
        entry = str(timestamp) + " [" + level + "] " + message + "\n"
        io.append(self.filename, entry)

# Usage
log = Logger("debug.log")
log.log("INFO", "Game started")
log.log("ERROR", "Failed to load asset")
log.log("DEBUG", "Player position: (100, 200)")
```

### Binary File Pack/Unpack

```python
import io

def pack_binary(filename, data_map):
    # Write multiple typed arrays to file
    f = File(filename, "w")
    
    if "header" in data_map:
        f.writebytes(data_map["header"])
    if "pixels" in data_map:
        f.writebytes(data_map["pixels"])
    if "metadata" in data_map:
        f.writebytes(data_map["metadata"])
    
    f.close()

def unpack_binary(filename, sizes):
    # sizes = [header_size, pixels_size, metadata_size]
    f = File(filename)
    
    result = {}
    result["header"] = f.readbytes(sizes[0])
    result["pixels"] = f.readbytes(sizes[1])
    result["metadata"] = f.readbytes(sizes[2])
    
    f.close()
    return result

# Usage
data = {
    header: Uint8Array(16),
    pixels: Uint8Array(1024),
    metadata: Uint8Array(256)
}
pack_binary("output.bin", data)

# Read back
unpacked = unpack_binary("output.bin", [16, 1024, 256])
```

### Streaming Large Files

```python
import io

def process_large_file(filename, chunk_size = 65536):
    f = File(filename)
    
    while true:
        chunk = f.read(chunk_size)
        if len(chunk) == 0:
            break
        
        # Process chunk
        process_chunk(chunk)
    
    f.close()

def process_large_csv(filename, chunk_size = 65536):
    f = File(filename)
    buffer = ""
    
    while true:
        chunk = f.read(chunk_size)
        if len(chunk) == 0:
            if len(buffer) > 0:
                process_line(buffer)
            break
        
        buffer = buffer + chunk
        lines = buffer.split("\n")
        
        # Process all complete lines
        for i in range(len(lines) - 1):
            process_line(lines[i])
        
        # Keep incomplete line for next iteration
        buffer = lines[len(lines) - 1]
    
    f.close()
```

### Configuration File Parser

```python
import io

class ConfigParser:
    def __init__(self, filename):
        self.config = {}
        self.parse(filename)
    
    def parse(self, filename):
        lines = io.readlines(filename)
        
        for line in lines:
            # Skip empty and comment lines
            line = line.strip()
            if len(line) == 0 or line.starts_with("#"):
                continue
            
            # Parse key=value
            if not line.contains("="):
                continue
            
            parts = line.split("=")
            key = parts[0].strip()
            value = parts[1].strip()
            
            # Remove quotes if present
            if value.starts_with("\"") and value.ends_with("\""):
                value = value[1: len(value) - 1]
            
            self.config[key] = value
    
    def get(self, key, default = nil):
        if key in self.config:
            return self.config[key]
        return default

# Usage
cfg = ConfigParser("game.cfg")
width = int(cfg.get("width", "800"))
height = int(cfg.get("height", "600"))
fullscreen = cfg.get("fullscreen", "false") == "true"
```

---

## Limits & Constraints

- **Max file size:** 100 MB per operation
- **Max line length:** No explicit limit (memory limited)
- **Max lines:** Depends on available memory
- **Binary buffer size:** Limited by available memory
- **File modes:** "r", "w", "a", "r+" (or with "b" suffix, treated same)

---

## File Mode Reference

| Mode | Read | Write | Truncate | Create |
|------|------|-------|----------|--------|
| `r` | ✓ | ✗ | ✗ | ✗ |
| `w` | ✗ | ✓ | ✓ | ✓ |
| `a` | ✗ | ✓ | ✗ | ✓ |
| `r+` | ✓ | ✓ | ✗ | ✗ |

---

**Generated from:** `libzen/src/builtin_io.cpp`  
**Last Updated:** 2026-05-18  
**Coverage:** 8 module functions + File class with 8 methods
