# Test SSO (Small String Optimization)

# Basic short strings
s1 = "a"
s2 = "hi"
s3 = "test"
s4 = "hello5"
s5 = "1234567"

assert len(s1) == 1
assert len(s2) == 2
assert len(s3) == 4
assert len(s4) == 6
assert len(s5) == 7

print("basic_short OK")

# String concatenation
result = "x" + "y"
assert result == "xy"
assert len(result) == 2

result2 = "a" + "b" + "c"
assert result2 == "abc"
assert len(result2) == 3

print("concat OK")

# String in collections
arr = ["a", "b", "c"]
assert arr[0] == "a"
assert arr[2] == "c"
assert len(arr) == 3

m = {"x": "y", "z": "w"}
assert m["x"] == "y"
assert m["z"] == "w"

print("collections OK")

# String operations
s = "test"
assert "e" in s
assert "x" not in s
assert s.upper() == "TEST"
assert s.lower() == "test"

print("string_ops OK")

# Mixed short and long
short = "x"
long = "this_is_a_long_string_that_exceeds_7_bytes_and_goes_to_heap"
combined = short + long
assert len(combined) == len(short) + len(long)

print("mixed OK")

# Loop with strings
for i in range(5):
    s = "a"
    assert len(s) == 1

print("loop OK")

print("=== ALL SSO TESTS PASSED ===")


