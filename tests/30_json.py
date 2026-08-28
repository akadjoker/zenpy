# Test json module
import json

# parse primitives
assert(json.parse("null") == nil, "json null")
assert(json.parse("true") == true, "json true")
assert(json.parse("false") == false, "json false")
assert(json.parse("42") == 42, "json int")
assert(json.parse("3.14") == 3.14, "json float")
assert(json.parse('"hello"') == "hello", "json string")

# parse array
arr = json.parse("[1, 2, 3]")
assert(len(arr) == 3, "json array len")
assert(arr[0] == 1, "json arr[0]")
assert(arr[2] == 3, "json arr[2]")

# parse object
obj = json.parse('{"name": "zen", "version": 1}')
assert(obj["name"] == "zen", "json obj name")
assert(obj["version"] == 1, "json obj version")

# parse nested
nested = json.parse('{"a": [1, {"b": true}]}')
assert(nested["a"][0] == 1, "json nested array")
assert(nested["a"][1]["b"] == true, "json nested obj")

# parse escapes
esc = json.parse('"hello\\nworld"')
assert(len(esc) == 11, "json escape len")

# stringify compact
s = json.stringify(42)
assert(s == "42", "stringify int")
s = json.stringify("hello")
assert(s == '"hello"', "stringify string")
s = json.stringify(true)
assert(s == "true", "stringify bool")
s = json.stringify(nil)
assert(s == "null", "stringify nil")

# stringify array
s = json.stringify([1, 2, 3])
assert(s == "[1,2,3]", "stringify array")

# stringify object
m = {}
m["a"] = 1
s = json.stringify(m)
# should contain "a":1
assert(len(s) > 0, "stringify map not empty")

# roundtrip
original = '{"x":10,"y":[1,2,3],"z":true}'
parsed = json.parse(original)
assert(parsed["x"] == 10, "roundtrip x")
assert(parsed["z"] == true, "roundtrip z")
assert(len(parsed["y"]) == 3, "roundtrip y len")

# pretty print
s = json.stringify(42, 2)
assert(s == "42", "pretty int unchanged")

# stringify with bool indent
s = json.stringify([1], true)
assert(len(s) > 3, "pretty array has newlines")

print("All json tests passed.")
