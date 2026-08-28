# Zen OS Library Reference

**Complete reference for operating system interaction with 10 functions.**

Import with: `import os`

---

## Overview

File system and process control operations. Provides directory navigation, file operations, environment variables, and process management.

| Function | Arity | Description |
|----------|-------|-------------|
| `getcwd` | 0 | Get current working directory. |
| `chdir` | 1 | Change current working directory. |
| `listdir` | 1 | List files in directory. |
| `mkdir` | 1 | Create directory. |
| `remove` | 1 | Delete file or empty directory. |
| `rename` | 2 | Rename or move file. |
| `getenv` | 1 | Get environment variable. |
| `exit` | -1 | Terminate process with code. |
| `isdir` | 1 | Check if path is directory. |
| `isfile` | 1 | Check if path is regular file. |

---

## Directory Operations

### getcwd() → string

Get the current working directory.

**Signature:** `getcwd() → string`

**Returns:** Absolute path to current directory

**Raises error on:**
- Cannot read directory (permission denied, etc.)

**Example:**
```python
import os

# Get current working directory
cwd = os.getcwd()
print("Current directory:", cwd)

# Save and restore
original_cwd = os.getcwd()
try:
    os.chdir("subdir")
    # ... do work in subdir ...
finally:
    os.chdir(original_cwd)
```

---

### chdir(path) → nil

Change the current working directory.

**Signature:** `chdir(path: string) → nil`

**Behavior:**
- Changes working directory for the process
- All relative paths are now relative to this new directory
- Affects subsequent `os.listdir()`, file opens, etc.

**Raises error on:**
- Directory not found
- Permission denied

**Example:**
```python
import os
import io

# Change to project directory
os.chdir("/home/user/myproject")

# Now relative paths work from here
config = io.read("config.json")

# Go back up
os.chdir("..")

# Change to subdirectory
os.chdir("./data")

# Temporary directory change
def work_in_dir(path):
    original = os.getcwd()
    os.chdir(path)
    try:
        # ... work happens here ...
        return do_work()
    finally:
        os.chdir(original)
```

---

### listdir(path) → [string]

List all files and directories in a directory.

**Signature:** `listdir(path: string) → array of strings`

**Returns:**
- Array of filenames (strings)
- `.` and `..` are excluded
- No specific order

**Raises error on:**
- Directory not found
- Permission denied

**Example:**
```python
import os

# List current directory
files = os.listdir(".")
for f in files:
    print(f)

# List specific directory
contents = os.listdir("/tmp")
print("Files in /tmp:", len(contents))

# List and filter
all_files = os.listdir(".")
zen_files = []
for f in all_files:
    if f.ends_with(".zen"):
        zen_files.push(f)

print("Zen files:", zen_files)

# Recursive listing
function list_all(path, prefix = ""):
    try:
        items = os.listdir(path)
        for item in items:
            full_path = path + "/" + item
            print(prefix + item)
            if os.isdir(full_path):
                list_all(full_path, prefix + "  ")
    except:
        return

list_all(".")
```

---

### mkdir(path) → nil

Create a new directory.

**Signature:** `mkdir(path: string) → nil`

**Permissions:** Created with mode `0755` (rwxr-xr-x)

**Raises error on:**
- Directory already exists
- Parent directory doesn't exist
- Permission denied
- Path is invalid

**Note:** Only creates single directory. Parent must exist. Use recursive approach for nested creation.

**Example:**
```python
import os

# Create directory
os.mkdir("new_folder")

# Create with error handling
function safe_mkdir(path):
    try:
        os.mkdir(path)
        print("Created:", path)
    except:
        print("Already exists or error:", path)

# Recursive directory creation
function mkdir_recursive(path):
    if os.isdir(path):
        return  # Already exists
    
    parts = path.split("/")
    current = ""
    
    for part in parts:
        if len(part) == 0:
            continue
        current = current + "/" + part
        if not os.isdir(current):
            try:
                os.mkdir(current)
            except:
                return  # Continue if exists
    
    print("Created:", path)

mkdir_recursive("a/b/c/d/e")
```

---

## File Operations

### remove(path) → nil

Delete a file or empty directory.

**Signature:** `remove(path: string) → nil`

**Behavior:**
- Deletes regular files
- Can delete empty directories
- Cannot delete non-empty directories

**Raises error on:**
- File/directory not found
- Permission denied
- Directory not empty
- Other I/O error

**Example:**
```python
import os

# Delete file
os.remove("temp.txt")

# Delete empty directory
os.remove("empty_folder")

# Safe delete
function safe_remove(path):
    try:
        os.remove(path)
        print("Deleted:", path)
    except:
        print("Cannot delete:", path)

# Remove files matching pattern
import io

function cleanup_backups(directory):
    files = os.listdir(directory)
    for f in files:
        if f.contains(".bak"):
            path = directory + "/" + f
            os.remove(path)
            print("Removed:", f)

cleanup_backups(".")
```

---

### rename(old_path, new_path) → nil

Rename or move a file/directory.

**Signature:** `rename(old: string, new: string) → nil`

**Behavior:**
- Renames file/directory
- Can move to different directory (if target directory exists)
- Atomic operation

**Raises error on:**
- Source doesn't exist
- Destination already exists
- Permission denied

**Example:**
```python
import os

# Simple rename
os.rename("old_name.txt", "new_name.txt")

# Move to different directory
os.rename("file.txt", "archive/file.txt")

# Rename with backup
function backup_and_rename(old_name, new_name):
    backup_name = new_name + ".backup"
    
    if os.exists(new_name):
        os.rename(new_name, backup_name)
    
    os.rename(old_name, new_name)
    print("Renamed to:", new_name)

# Versioned backups
function rotate_logs(base_name):
    for i in range(10, 0, -1):
        old = base_name + "." + str(i)
        new = base_name + "." + str(i + 1)
        if os.exists(old):
            os.rename(old, new)
    
    if os.exists(base_name):
        os.rename(base_name, base_name + ".1")
```

---

## Environment & Process

### getenv(name) → string | nil

Get an environment variable value.

**Signature:** `getenv(name: string) → string | nil`

**Returns:**
- String value if variable exists
- `nil` if not set

**Common variables:**
- `HOME` — user's home directory
- `USER` — username
- `PATH` — executable search path
- `PWD` — current working directory
- `LANG` — locale/language
- `TEMP` / `TMP` — temporary directory

**Example:**
```python
import os

# Get home directory
home = os.getenv("HOME")
if home != nil:
    print("Home:", home)

# Get and use environment variable
temp_dir = os.getenv("TEMP")
if temp_dir == nil:
    temp_dir = "/tmp"

print("Using temp:", temp_dir)

# Check if running in specific environment
debug_mode = os.getenv("DEBUG_MODE") != nil
if debug_mode:
    print("Debug mode enabled")

# Read configuration from env
function get_config(name, default_value):
    env_var = "APP_" + name.upper()
    val = os.getenv(env_var)
    return val if val != nil else default_value

database_host = get_config("db_host", "localhost")
database_port = get_config("db_port", "5432")
```

---

### exit([code]) → (does not return)

Terminate the process immediately.

**Signature:** `exit([code: int]) → (terminates)`

**Parameters:**
- `code` (optional): Exit code (default 0)
- Convention: 0 = success, non-zero = error

**Note:** This call doesn't return. The process terminates.

**Example:**
```python
import os

# Successful exit
if all_checks_passed:
    print("All systems go!")
    os.exit(0)

# Error exit
if critical_error:
    print("Fatal error detected")
    os.exit(1)

# Exit with specific codes
function check_config(config_file):
    if not io.exists(config_file):
        print("Config missing")
        os.exit(2)  # Missing file
    
    if not is_valid_config(config_file):
        print("Config invalid")
        os.exit(3)  # Invalid config
    
    print("Config OK")
    os.exit(0)  # Success

# Wrapper for scripts
function main():
    try:
        run_application()
        os.exit(0)
    except:
        print("Fatal exception")
        os.exit(1)

main()
```

---

## Path Testing

### isdir(path) → bool

Check if path exists and is a directory.

**Signature:** `isdir(path: string) → bool`

**Returns:**
- `true` if path is an existing directory
- `false` if path doesn't exist, is a file, or is inaccessible

**Example:**
```python
import os

# Check if directory exists
if os.isdir("/home/user"):
    print("User directory exists")

# Conditional logic
function safe_listdir(path):
    if not os.isdir(path):
        print("Not a directory:", path)
        return []
    
    return os.listdir(path)

# Find all directories
files = os.listdir(".")
for f in files:
    if os.isdir(f):
        print("Directory:", f)

# Recursive traversal
function count_directories(path):
    count = 1
    if os.isdir(path):
        items = os.listdir(path)
        for item in items:
            full_path = path + "/" + item
            if os.isdir(full_path):
                count = count + count_directories(full_path)
    return count
```

---

### isfile(path) → bool

Check if path exists and is a regular file.

**Signature:** `isfile(path: string) → bool`

**Returns:**
- `true` if path is an existing regular file
- `false` if path doesn't exist, is a directory, or is inaccessible

**Example:**
```python
import os

# Check if file exists
if os.isfile("config.json"):
    print("Config found")

# Read only existing files
function safe_read(path):
    if not os.isfile(path):
        print("File not found:", path)
        return nil
    
    return io.read(path)

# Process all files in directory
import io

function process_files(directory):
    items = os.listdir(directory)
    for item in items:
        full_path = directory + "/" + item
        if os.isfile(full_path):
            print("Processing:", item)
            content = io.read(full_path)
            # ... process content ...

# Find all files of type
function find_files_with_ext(directory, ext):
    results = []
    items = os.listdir(directory)
    for item in items:
        full_path = directory + "/" + item
        if os.isfile(full_path) and item.ends_with(ext):
            results.push(full_path)
    return results

zen_files = find_files_with_ext("scripts", ".zen")
```

---

## Common Patterns

### Directory Cleaner

```python
import os
import io

function clean_temp_files(directory, min_age_hours = 24):
    import time
    cutoff_time = time.time() - (min_age_hours * 3600)
    
    items = os.listdir(directory)
    for item in items:
        full_path = directory + "/" + item
        if os.isfile(full_path):
            try:
                os.remove(full_path)
            except:
                pass

# Usage
clean_temp_files("/tmp/myapp")
```

### Project File Finder

```python
import os

function find_all_files(root_dir, extension = nil):
    results = []
    
    function traverse(path):
        items = os.listdir(path)
        for item in items:
            full_path = path + "/" + item
            if os.isfile(full_path):
                if extension == nil or item.ends_with(extension):
                    results.push(full_path)
            elif os.isdir(full_path):
                traverse(full_path)
    
    traverse(root_dir)
    return results

# Usage
all_zen_files = find_all_files(".", ".zen")
print("Found", len(all_zen_files), ".zen files")
```

### Safe Directory Operations

```python
import os

class SafeFS:
    def exists(self, path):
        return os.isfile(path) or os.isdir(path)
    
    def ensure_dir(self, path):
        if not os.isdir(path):
            try:
                os.mkdir(path)
            except:
                pass
    
    def remove_safe(self, path):
        try:
            os.remove(path)
            return true
        except:
            return false
    
    def listdir_files(self, path):
        if not os.isdir(path):
            return []
        
        results = []
        items = os.listdir(path)
        for item in items:
            if os.isfile(path + "/" + item):
                results.push(item)
        return results

# Usage
fs = SafeFS()
fs.ensure_dir("logs")
log_files = fs.listdir_files("logs")
```

### Directory Tree Display

```python
import os

function print_tree(path, prefix = "", is_last = true):
    filename = path.split("/")[-1]
    
    connector = "└── " if is_last else "├── "
    print(prefix + connector + filename)
    
    if os.isdir(path):
        try:
            items = os.listdir(path)
            for i in range(len(items)):
                item = items[i]
                full_path = path + "/" + item
                extension = "    " if is_last else "│   "
                is_last_item = (i == len(items) - 1)
                print_tree(full_path, prefix + extension, is_last_item)
        except:
            pass

# Usage
print_tree(".")
```

---

## Error Codes

Exit codes are conventional (not enforced):

| Code | Meaning |
|------|---------|
| `0` | Success |
| `1` | General error |
| `2` | Misuse of shell command |
| `127` | Command not found |
| `128+` | Signal (128 + signal number) |

---

## Platform Notes

- **POSIX systems** (Linux, macOS): Full support
- **Directory mode:** Always `0755` (rwxr-xr-x)
- **Symlinks:** Treated as their target type by `isdir`/`isfile`
- **Path separators:** Uses `/` (POSIX)

---

**Generated from:** `libzen/src/builtin_os.cpp`  
**Last Updated:** 2026-05-18  
**Coverage:** 10 functions (getcwd, chdir, listdir, mkdir, remove, rename, getenv, exit, isdir, isfile)
