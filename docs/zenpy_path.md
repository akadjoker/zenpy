# Zen Path Library Reference

**Complete reference for file path manipulation with 7 functions. Pure string operations (no I/O).**

Import with: `import path`

---

## Overview

Cross-platform file path handling. Works with both Unix-style (`/`) and Windows-style (`\`) separators. All operations are pure string manipulation—no filesystem access.

| Function | Arity | Description |
|----------|-------|-------------|
| `join` | -1 | Join path components. |
| `dirname` | 1 | Get directory part of path. |
| `basename` | 1 | Get filename part of path. |
| `ext` | 1 | Get file extension (with dot). |
| `stem` | 1 | Get filename without extension. |
| `isabs` | 1 | Check if path is absolute. |
| `norm` | 1 | Normalize path (collapse `.`, `..`, double slashes). |

---

## join(...parts) → string

Join path components into a single path.

**Signature:** `join(part1: string[, part2: string, ...]) → string`

**Arity:** Variadic (-1), accepts 1+ arguments

**Behavior:**
- Uses `/` as separator (converts `\` to `/`)
- Absolute paths reset (if a part starts with `/`, previous parts are discarded)
- Removes empty parts
- Handles multiple separators

**Example:**
```python
import path

# Simple join
result = path.join("a", "b", "c")
print(result)  # → "a/b/c"

# With directory
result = path.join("/home", "user", "documents")
print(result)  # → "/home/user/documents"

# Absolute path resets
result = path.join("a", "b", "/c", "d")
print(result)  # → "/c/d"  (a/b discarded)

# Empty parts skipped
result = path.join("a", "", "b")
print(result)  # → "a/b"

# Mixed separators normalized
result = path.join("a\\b", "c/d")
print(result)  # → "a/b/c/d"

# Build paths dynamically
def build_asset_path(category, name, ext):
    return path.join("assets", category, name + "." + ext)

sprite = build_asset_path("sprites", "player", "png")
print(sprite)  # → "assets/sprites/player.png"
```

---

## dirname(path) → string

Extract the directory portion of a path.

**Signature:** `dirname(path: string) → string`

**Returns:**
- Directory part (without filename)
- `"."` for relative paths with no directory
- `"/"` for root directory

**Example:**
```python
import path

# Full path
result = path.dirname("/home/user/file.txt")
print(result)  # → "/home/user"

# Nested
result = path.dirname("a/b/c/file.txt")
print(result)  # → "a/b/c"

# No directory
result = path.dirname("file.txt")
print(result)  # → "."

# Root directory
result = path.dirname("/")
print(result)  # → "/"

# Trailing slashes stripped
result = path.dirname("/home/user/")
print(result)  # → "/home/user"

# Get parent directory
current = "a/b/c/file.txt"
parent_dir = path.dirname(current)
print(parent_dir)  # → "a/b/c"
```

---

## basename(path) → string

Extract the filename portion of a path.

**Signature:** `basename(path: string) → string`

**Returns:**
- Filename only (everything after last `/` or `\`)
- Trailing slashes ignored

**Example:**
```python
import path

# Full path
result = path.basename("/home/user/file.txt")
print(result)  # → "file.txt"

# Just filename
result = path.basename("file.txt")
print(result)  # → "file.txt"

# Nested path
result = path.basename("a/b/c/d/e/filename.md")
print(result)  # → "filename.md"

# Directory (trailing slash stripped)
result = path.basename("/home/user/")
print(result)  # → "user"

# No extension
result = path.basename("README")
print(result)  # → "README"

# Extract just filename from full path
full_path = "/assets/textures/ground.png"
filename = path.basename(full_path)
print(filename)  # → "ground.png"
```

---

## ext(path) → string

Get file extension (including the dot).

**Signature:** `ext(path: string) → string`

**Returns:**
- Extension with leading dot (e.g., `".txt"`)
- Empty string if no extension
- Only looks at filename, not directory names

**Example:**
```python
import path

# Standard extension
result = path.ext("file.txt")
print(result)  # → ".txt"

# Full path
result = path.ext("/home/user/documents/file.pdf")
print(result)  # → ".pdf"

# No extension
result = path.ext("README")
print(result)  # → ""

# Double extension
result = path.ext("archive.tar.gz")
print(result)  # → ".gz"  (only last)

# Hidden file (no extension)
result = path.ext(".gitignore")
print(result)  # → ""

# Extension with numbers
result = path.ext("backup.2024.zip")
print(result)  # → ".zip"

# Check file type
filename = "image.png"
ext = path.ext(filename)
if ext == ".png":
    load_png_image(filename)
elif ext == ".jpg":
    load_jpg_image(filename)

# Filter by extension
function is_image(filepath):
    ext_lower = path.ext(filepath).lower()
    return ext_lower == ".png" or ext_lower == ".jpg" or ext_lower == ".gif"

files = ["photo.PNG", "document.pdf", "sprite.jpg"]
for f in files:
    if is_image(f):
        print("Image:", f)
```

---

## stem(path) → string

Get filename without extension.

**Signature:** `stem(path: string) → string`

**Returns:**
- Filename minus the extension
- Extension dot is removed
- Same as `basename` minus `ext`

**Example:**
```python
import path

# Standard file
result = path.stem("document.txt")
print(result)  # → "document"

# Full path
result = path.stem("/home/user/image.png")
print(result)  # → "image"

# No extension
result = path.stem("README")
print(result)  # → "README"

# Double extension
result = path.stem("archive.tar.gz")
print(result)  # → "archive.tar"  (removes only last .gz)

# Hidden file
result = path.stem(".gitignore")
print(result)  # → ".gitignore"

# Save with new extension
import io
def convert_file(source_file, new_ext):
    name = path.stem(source_file)
    output = name + new_ext
    # Read, process, write
    return output

result = convert_file("data.csv", ".json")
print(result)  # → "data.json"

# Build versioned filename
def backup_file(original):
    stem = path.stem(original)
    ext = path.ext(original)
    timestamp = str(time.time())
    return stem + "_backup_" + timestamp + ext

backup = backup_file("config.ini")
print(backup)  # → "config_backup_1747738400.123_ini"
```

---

## isabs(path) → bool

Check if path is absolute (starts with `/`).

**Signature:** `isabs(path: string) → bool`

**Returns:**
- `true` if path starts with `/`
- `false` for relative paths

**Note:** On Windows, `C:\` paths are not recognized as absolute (Zen treats them as relative). For portability, use `/` separators.

**Example:**
```python
import path

# Absolute paths
print(path.isabs("/home/user"))     # → true
print(path.isabs("/usr/bin/zen"))   # → true
print(path.isabs("/"))              # → true

# Relative paths
print(path.isabs("file.txt"))       # → false
print(path.isabs("./data"))         # → false
print(path.isabs("../parent"))      # → false
print(path.isabs("a/b/c"))          # → false

# Decision logic
function get_full_path(filepath, base_dir):
    if path.isabs(filepath):
        return filepath  # Already absolute
    else:
        return path.join(base_dir, filepath)  # Relative to base

rel = "data/config.json"
abs1 = get_full_path(rel, "/etc/myapp")
print(abs1)  # → "/etc/myapp/data/config.json"

abs2 = get_full_path("/etc/override.json", "/etc/myapp")
print(abs2)  # → "/etc/override.json"
```

---

## norm(path) → string

Normalize path by collapsing `.`, `..`, and duplicate separators.

**Signature:** `norm(path: string) → string`

**Normalizations:**
- Removes `.` (current directory)
- Collapses `..` (parent directory)
- Removes duplicate separators
- Converts `\` to `/`

**Returns:**
- Normalized path
- `"."` for empty result
- Handles mixed relative/absolute

**Example:**
```python
import path

# Double slashes
result = path.norm("a//b///c")
print(result)  # → "a/b/c"

# Current directory (.)
result = path.norm("a/./b/./c")
print(result)  # → "a/b/c"

# Parent directory (..)
result = path.norm("a/b/../c")
print(result)  # → "a/c"

# Mixed
result = path.norm("a//./b/../c/./d")
print(result)  # → "a/c/d"

# Absolute with ..
result = path.norm("/home/user/../admin")
print(result)  # → "/home/admin"

# Relative with ..
result = path.norm("a/b/../../c")
print(result)  # → "c"

# Too many ..
result = path.norm("a/../../../b")
print(result)  # → "../../b" or "b" (depends on platform)

# Complex real-world case
messy = "data//./config/../config/game.cfg"
clean = path.norm(messy)
print(clean)  # → "data/config/game.cfg"

# Resolve script paths
script_dir = "/opt/app/scripts"
relative_path = "../../lib/helper.zen"
resolved = path.norm(path.join(script_dir, relative_path))
print(resolved)  # → "/opt/lib/helper.zen"
```

---

## Common Patterns

### File Type Router

```python
import path
import io

function process_file(filepath):
    ext = path.ext(filepath).lower()
    
    if ext == ".json":
        return parse_json_file(filepath)
    elif ext == ".csv":
        return parse_csv_file(filepath)
    elif ext == ".txt":
        return read_text_file(filepath)
    elif ext == ".png" or ext == ".jpg":
        return load_image(filepath)
    else:
        print("Unknown format:", ext)
        return nil

# Usage
data = process_file("config.json")
```

### Backup Creator

```python
import path
import io
import time

function create_backup(original_path):
    if not io.exists(original_path):
        return nil
    
    dir = path.dirname(original_path)
    stem = path.stem(original_path)
    ext = path.ext(original_path)
    timestamp = int(time.time())
    
    backup_name = stem + "_" + str(timestamp) + ".bak" + ext
    backup_path = path.join(dir, backup_name)
    
    content = io.read(original_path)
    io.write(backup_path, content)
    
    return backup_path

# Usage
saved = create_backup("config.json")
if saved:
    print("Backup created:", saved)
```

### Relative Path Resolver

```python
import path

function resolve_relative_path(base_dir, relative_path):
    if path.isabs(relative_path):
        return relative_path
    
    full_path = path.join(base_dir, relative_path)
    return path.norm(full_path)

# Usage
project_root = "/home/user/myproject"
asset_ref = "assets/sprites/player.png"
resolved = resolve_relative_path(project_root, asset_ref)
print(resolved)  # → "/home/user/myproject/assets/sprites/player.png"
```

### Path Component Extractor

```python
import path

class PathInfo:
    def __init__(self, filepath):
        self.path = filepath
        self.dir = path.dirname(filepath)
        self.name = path.basename(filepath)
        self.ext = path.ext(filepath)
        self.stem = path.stem(filepath)
        self.is_absolute = path.isabs(filepath)

# Usage
info = PathInfo("/home/user/projects/game.cpp")
print("Directory:", info.dir)      # → "/home/user/projects"
print("Filename:", info.name)       # → "game.cpp"
print("Stem:", info.stem)           # → "game"
print("Extension:", info.ext)       # → ".cpp"
print("Is absolute:", info.is_absolute)  # → true
```

### Asset Path Builder

```python
import path

class AssetManager:
    def __init__(self, asset_root):
        self.root = asset_root
    
    def get_sprite_path(self, name):
        return path.norm(path.join(self.root, "sprites", name + ".png"))
    
    def get_sound_path(self, name):
        return path.norm(path.join(self.root, "sounds", name + ".wav"))
    
    def get_config_path(self, name):
        return path.norm(path.join(self.root, "config", name + ".json"))

# Usage
assets = AssetManager("./assets")
sprite = assets.get_sprite_path("player")      # → "assets/sprites/player.png"
sound = assets.get_sound_path("jump")           # → "assets/sounds/jump.wav"
config = assets.get_config_path("game")         # → "assets/config/game.json"
```

### Project-Relative Paths

```python
import path
import os

class Project:
    def __init__(self, root):
        self.root = path.norm(root)
    
    def rel(self, file):
        return path.norm(path.join(self.root, file))
    
    def assets(self, file):
        return self.rel(path.join("assets", file))
    
    def scripts(self, file):
        return self.rel(path.join("scripts", file))
    
    def data(self, file):
        return self.rel(path.join("data", file))

# Usage
project = Project("/home/user/game")
sprite = project.assets("sprites/hero.png")
script = project.scripts("player.zen")
config = project.data("settings.json")

print(sprite)  # → "/home/user/game/assets/sprites/hero.png"
print(script)  # → "/home/user/game/scripts/player.zen"
print(config)  # → "/home/user/game/data/settings.json"
```

### Nested Directory Path

```python
import path

function create_nested_path(base, parts):
    // parts = ["dir1", "dir2", "dir3", "file.txt"]
    return path.norm(path.join.apply(nil, [base] + parts))

# Or simpler:
def build_path(base, ...dirs):
    all = [base]
    for d in dirs:
        all.push(d)
    return path.norm(path.join(all))

# Usage
p = build_path("/root", "a", "b", "c", "file.txt")
print(p)  # → "/root/a/b/c/file.txt"
```

### Path Validation

```python
import path

function is_valid_path(filepath):
    // No empty
    if len(filepath) == 0:
        return false
    
    // No invalid characters (basic check)
    invalid_chars = "<>:\"|?*"
    for ch in invalid_chars:
        if filepath.contains(ch):
            return false
    
    return true

// Usage
print(is_valid_path("/home/user/file.txt"))    # → true
print(is_valid_path("file<>.txt"))             # → false
```

---

## Separator Handling

| Separator | Treated As | Example |
|-----------|-----------|---------|
| `/` | Path separator | `/home/user/file.txt` |
| `\` | Path separator (auto-converted) | `C:\Users\file.txt` → `C:/Users/file.txt` |
| `//` | Double slash (collapsed) | `a//b` → `a/b` |
| `./` | Current dir (removed) | `a/./b` → `a/b` |
| `../` | Parent dir (resolved) | `a/b/../c` → `a/c` |

---

## Cross-Platform Notes

- **Separators:** This implementation uses `/` as the universal separator (POSIX style)
- **Windows paths:** `C:\Users\file.txt` is treated as relative (not absolute in Zen)
- **Portability:** Always use `/` for cross-platform code
- **No I/O:** These functions don't check if paths exist—they're pure string operations

---

## Limitations

- Buffer limit: 4096 characters (path too long = error)
- Max path components: 256 (for normalization)
- No symlink following (string-based only)
- Windows drive letters (`C:`) not recognized

---

**Generated from:** `libzen/src/builtin_path.cpp`  
**Last Updated:** 2026-05-18  
**Coverage:** 7 functions (join, dirname, basename, ext, stem, isabs, norm)
