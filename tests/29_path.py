# Test path module
import path

# join
j1 = path.join("a", "b", "c")
assert j1 == "a/b/c"
j2 = path.join("a", "/b", "c")
assert j2 == "/b/c"
j3 = path.join("a")
assert j3 == "a"

# dirname
d1 = path.dirname("/a/b/c.txt")
assert d1 == "/a/b"
d2 = path.dirname("/a")
assert d2 == "/"
d3 = path.dirname("file.txt")
assert d3 == "."

# basename
b1 = path.basename("/a/b/c.txt")
assert b1 == "c.txt"
b2 = path.basename("/a/b/")
assert b2 == "b"

# ext
e1 = path.ext("file.txt")
assert e1 == ".txt"
e2 = path.ext("file.tar.gz")
assert e2 == ".gz"
e3 = path.ext("noext")
assert e3 == ""

# stem
s1 = path.stem("/a/b/c.txt")
assert s1 == "c"
s2 = path.stem("noext")
assert s2 == "noext"

# isabs
a1 = path.isabs("/foo")
assert a1
a2 = path.isabs("foo")
assert not a2

# norm
n1 = path.norm("a//b/../c")
assert n1 == "a/c"
n2 = path.norm("/a/./b")
assert n2 == "/a/b"
n3 = path.norm("")
assert n3 == "."

print("All path tests passed.")
