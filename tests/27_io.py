# Test io module
import io
import os

# A directory beside the test run — Windows has no /tmp.
TMP = "_zen_io_tmp"
if not os.isdir(TMP):
    os.mkdir(TMP)

f1 = TMP + "/test.txt"
f2 = TMP + "/test2.txt"
f3 = TMP + "/file.txt"

# write + read
io.write(f1, "hello zen")
text = io.read(f1)
assert text == "hello zen"

# append
io.append(f1, "\nline2")
text = io.read(f1)
assert text == "hello zen\nline2"

# exists
ex1 = io.exists(f1)
assert ex1
ex2 = io.exists(TMP + "/no_such_file_xyz.txt")
assert not ex2

# size
sz = io.size(f1)
assert sz == 15

# readlines
lines = io.readlines(f1)
assert len(lines) == 2
assert lines[0] == "hello zen"
assert lines[1] == "line2"

# readbytes / writebytes
buf = io.readbytes(f1)
assert len(buf) == 15
io.writebytes(f2, buf)
text2 = io.read(f2)
assert text2 == "hello zen\nline2"

# File class
f = File(f3, "wb")
f.write("abc123")
f.close()

f = File(f3, "rb")
data = f.read()
assert data == "abc123"
f.close()

# File seek/tell
f = File(f3, "rb")
f.seek(3)
rest = f.read()
assert rest == "123"
f.seek(0)
pos = f.tell()
assert pos == 0
f.close()

# cleanup
os.remove(f1)
os.remove(f2)
os.remove(f3)
os.remove(TMP)

print("All io tests passed.")
