# Zen JSON Library Reference

**Complete reference for JSON parsing and serialization with 2 functions.**

Import with: `import json`

---

## Overview

Full JSON parser and serializer with no external dependencies.

| Function | Arity | Description |
|----------|-------|-------------|
| `parse` | 1 | Parse JSON string to Zen value. |
| `stringify` | -1 | Serialize Zen value to JSON string: `stringify(value[, indent])`. |

---

## parse(string) → value

Parse a JSON string and return the corresponding Zen value.

**Signature:** `parse(json_string: string) → any`

**Supported Types:**

| JSON Type | Maps to Zen |
|-----------|------------|
| `null` | `nil` |
| `true` / `false` | `bool` |
| `42`, `-3`, `0` | `int` |
| `3.14`, `1e10`, `1.5e-2` | `float` |
| `"string"` | `string` |
| `[1, 2, 3]` | `array` |
| `{"key": value}` | `map` |

**Raises Error On:**
- Invalid JSON syntax
- Unterminated strings
- Invalid escape sequences
- Trailing characters after root value
- Invalid numbers

**Example:**
```python
import json

# Parse objects
obj = json.parse('{"name": "Alice", "age": 30}')
print(obj["name"])  # → "Alice"
print(obj["age"])   # → 30

# Parse arrays
arr = json.parse('[1, 2, 3, 4, 5]')
for val in arr:
    print(val)

# Parse nested structures
data = json.parse('''
{
    "player": {"name": "Bob", "level": 5},
    "items": ["sword", "shield"],
    "health": 100.5
}
''')
print(data["player"]["name"])  # → "Bob"
print(data["items"][0])         # → "sword"

# Parse primitives
json.parse('"hello"')   # → "hello"
json.parse('42')        # → 42
json.parse('3.14')      # → 3.14
json.parse('true')      # → true
json.parse('null')      # → nil
json.parse('[]')        # → []
json.parse('{}')        # → {}

# Error handling
try:
    result = json.parse('invalid json {')
except:
    print("Parse error!")
```

---

## stringify(value, [indent]) → string

Serialize a Zen value to JSON string.

**Signature:** `stringify(value: any[, indent: bool | int]) → string`

**Indent Parameter:**

| Value | Behavior |
|-------|----------|
| omitted | Compact JSON (no whitespace) |
| `true` | Pretty-print with 2 spaces per indent level |
| `false` | Compact (same as omitted) |
| `int` (0-16) | Pretty-print with N spaces per indent level |

**Supported Types:**

| Zen Type | Serializes to JSON |
|----------|------------------|
| `nil` | `null` |
| `bool` | `true` / `false` |
| `int` | number |
| `float` | number (no trailing `.0` if integral) |
| `string` | quoted string with escapes |
| `array` | `[...]` array |
| `map` | `{...}` object |

**String Escaping:**

- `"` → `\"`
- `\` → `\\`
- `/` → `\/`
- `\b` → `\b`
- `\f` → `\f`
- `\n` → `\n`
- `\r` → `\r`
- `\t` → `\t`
- Non-ASCII UTF-8 → preserved as-is

**Example:**
```python
import json

# Compact
data = {name: "Alice", age: 30, active: true}
compact = json.stringify(data)
print(compact)  # {"name":"Alice","age":30,"active":true}

# Pretty (default 2 spaces)
pretty = json.stringify(data, true)
print(pretty)
# {
#   "name": "Alice",
#   "age": 30,
#   "active": true
# }

# Pretty (4 spaces)
pretty4 = json.stringify(data, 4)
print(pretty4)
# {
#     "name": "Alice",
#     "age": 30,
#     "active": true
# }

# Nested structures
obj = {
    player: {name: "Bob", level: 5},
    items: ["sword", "shield"],
    health: 100.5,
    alive: true
}
print(json.stringify(obj, 2))

# Arrays
arr = [1, "two", 3.0, true, nil, {nested: true}]
print(json.stringify(arr, true))

# Strings with escapes
text = "He said \"hello\"\nI replied \"hi\""
print(json.stringify(text))
# "He said \"hello\"\nI replied \"hi\""
```

---

## Zen ↔ JSON Type Mapping

### From JSON to Zen (parse)

```
JSON Input           → Zen Value
null                 → nil
true                 → true
false                → false
42                   → 42 (int)
3.14                 → 3.14 (float)
"text"               → "text" (string)
[]                   → [] (array)
{}                   → {} (map)
[1, "two", true]     → [1, "two", true] (mixed array)
{"a": 1, "b": "x"}   → {a: 1, b: "x"} (map with string keys)
```

### From Zen to JSON (stringify)

```
Zen Value            → JSON Output
nil                  → null
true                 → true
false                → false
42 (int)             → 42
3.14 (float)         → 3.14
3 (int, prints as)   → 3
"text" (string)      → "text"
[] (array)           → []
{} (map)             → {}
[1, "x", true]       → [1,"x",true]
{a: 1, b: "x"}       → {"a":1,"b":"x"}
function             → ERROR (not serializable)
Node instance        → ERROR (not serializable)
```

---

## Error Handling

**Common parse errors:**

```python
import json

# Syntax error
try:
    json.parse('{invalid}')  # Missing quotes on keys
except:
    print("Syntax error")

# Unterminated string
try:
    json.parse('{"name": "Alice}')  # Missing closing quote
except:
    print("Unterminated string")

# Trailing junk
try:
    json.parse('{"a": 1} extra stuff')
except:
    print("Trailing characters")

# Invalid escape
try:
    json.parse('"bad \\x escape"')
except:
    print("Invalid escape")
```

**Stringify errors:**

```python
import json

# Cannot serialize functions
def my_func():
    return 42

try:
    json.stringify(my_func)  # Function not serializable
except:
    print("Function cannot be serialized")

# Cannot serialize Node instances (game engine types)
# They must be converted to data structures first
data = {
    name: "Item",
    position: [10, 20]  # Convert from Vector to array
}
json.stringify(data)  # ✓ OK
```

---

## Common Patterns

### Config Files

```python
import json

# Save config
config = {
    version: "1.0",
    video: {
        width: 1920,
        height: 1080,
        fullscreen: false
    },
    audio: {
        master_volume: 80,
        music_volume: 60,
        sfx_volume: 100
    },
    controls: {
        forward: "W",
        back: "S",
        left: "A",
        right: "D"
    }
}

# Write to file
import io
f = io.open("config.json", "w")
f.write(json.stringify(config, 2))
f.close()

# Read back
f = io.open("config.json")
loaded_config = json.parse(f.read())
f.close()

print(loaded_config["video"]["width"])  # → 1920
```

### Save/Load Game State

```python
import json
import io

class GameState:
    def save(self, filename):
        data = {
            level: self.current_level,
            player: {
                health: self.player_health,
                position: [self.player_x, self.player_y],
                inventory: self.inventory
            },
            timestamp: self.time_played
        }
        
        f = io.open(filename, "w")
        f.write(json.stringify(data, 2))
        f.close()
    
    def load(self, filename):
        f = io.open(filename)
        data = json.parse(f.read())
        f.close()
        
        self.current_level = data["level"]
        self.player_health = data["player"]["health"]
        self.player_x = data["player"]["position"][0]
        self.player_y = data["player"]["position"][1]
        self.inventory = data["player"]["inventory"]
        self.time_played = data["timestamp"]
```

### API Responses

```python
import json
import http

# Fetch data from API
response = http.get("https://api.example.com/users/123")
if response.status == 200:
    user = json.parse(response.body)
    print("User:", user["name"])
    print("Email:", user["email"])
    for item in user["items"]:
        print("  -", item["name"])
else:
    print("API error:", response.status)
```

### Data Transformation

```python
import json

# Parse raw data
raw = json.parse('''
[
    {"id": 1, "name": "Alice", "active": true},
    {"id": 2, "name": "Bob", "active": false},
    {"id": 3, "name": "Charlie", "active": true}
]
''')

# Transform
active_users = []
for user in raw:
    if user["active"]:
        active_users.push({
            id: user["id"],
            name: user["name"].upper()
        })

# Re-serialize
output = json.stringify({
    count: len(active_users),
    users: active_users
}, 2)

print(output)
```

### Deep Copy Objects

```python
import json

def deep_copy(obj):
    # Convert to JSON and back to create deep copy
    return json.parse(json.stringify(obj))

original = {a: 1, nested: {b: 2}}
copy = deep_copy(original)
copy["nested"]["b"] = 999

print(original["nested"]["b"])  # → 2 (unchanged)
print(copy["nested"]["b"])      # → 999 (changed)
```

---

## Performance Notes

- **Parse:** ~1-5 MB/s depending on size and complexity. No external dependencies.
- **Stringify:** Fast, streaming writer. O(n) where n = output size.
- **Memory:** Parser creates intermediate objects; no streaming mode yet.
- **Recursion Limit:** No explicit limit, but deep nesting (>1000 levels) may cause issues.

---

## Limitations

- Map keys must be strings in JSON (Zen maps can have any key type). Non-string keys are converted to strings during stringify.
- Functions, closures, and custom classes cannot be serialized.
- NaN and Infinity are serialized as `null` (JSON spec).
- Binary data (typed arrays) must be encoded as arrays of bytes or base64 strings.
- Comments are NOT supported (pure JSON).

---

## Spec Compliance

- Follows JSON RFC 7159
- UTF-8 encoding
- Full escape sequence support
- Number format: decimal, scientific notation (1e10, 1.5e-2)
- No trailing commas allowed

---

**Generated from:** `libzen/src/builtin_json.cpp`  
**Last Updated:** 2026-05-18  
**Coverage:** 2 functions (parse, stringify)
