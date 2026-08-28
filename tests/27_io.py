# Test io module
import io

# write + read
io.write("/tmp/_zen_io_test.txt", "hello zen")
text = io.read("/tmp/_zen_io_test.txt")
assert text == "hello zen"

# append
io.append("/tmp/_zen_io_test.txt", "\nline2")
text = io.read("/tmp/_zen_io_test.txt")
assert text == "hello zen\nline2"

# exists
ex1 = io.exists("/tmp/_zen_io_test.txt")
assert ex1
ex2 = io.exists("/tmp/_zen_no_such_file_xyz.txt")
assert not ex2

# size
sz = io.size("/tmp/_zen_io_test.txt")
assert sz == 15

# readlines
lines = io.readlines("/tmp/_zen_io_test.txt")
assert len(lines) == 2
assert lines[0] == "hello zen"
assert lines[1] == "line2"

# readbytes / writebytes
buf = io.readbytes("/tmp/_zen_io_test.txt")
assert len(buf) == 15
io.writebytes("/tmp/_zen_io_test2.txt", buf)
text2 = io.read("/tmp/_zen_io_test2.txt")
assert text2 == "hello zen\nline2"

# File class
f = File("/tmp/_zen_io_file.txt", "w")
f.write("abc123")
f.close()

f = File("/tmp/_zen_io_file.txt", "r")
data = f.read()
assert data == "abc123"
f.close()

# File seek/tell
f = File("/tmp/_zen_io_file.txt", "r")
f.seek(3)
rest = f.read()
assert rest == "123"
f.seek(0)
pos = f.tell()
assert pos == 0
f.close()

# cleanup
import os
os.remove("/tmp/_zen_io_test.txt")
os.remove("/tmp/_zen_io_test2.txt")
os.remove("/tmp/_zen_io_file.txt")

print("All io tests passed.")
