# Test os module
import os

# getcwd
cwd = os.getcwd()
assert len(cwd) > 0

# mkdir + isdir
os.mkdir("/tmp/_zen_os_testdir")
d1 = os.isdir("/tmp/_zen_os_testdir")
assert d1
d2 = os.isdir("/tmp/_zen_no_such_dir_xyz")
assert not d2
f1 = os.isfile("/tmp/_zen_os_testdir")
assert not f1

# listdir
files = os.listdir("/tmp")
assert len(files) > 0

# getenv
home = os.getenv("HOME")
assert len(home) > 0

# isfile
import io
io.write("/tmp/_zen_os_testfile.txt", "hello")
f2 = os.isfile("/tmp/_zen_os_testfile.txt")
assert f2
f3 = os.isfile("/tmp/_zen_no_such_file_xyz")
assert not f3

# rename
os.rename("/tmp/_zen_os_testfile.txt", "/tmp/_zen_os_testfile2.txt")
e1 = io.exists("/tmp/_zen_os_testfile.txt")
assert not e1
e2 = io.exists("/tmp/_zen_os_testfile2.txt")
assert e2

# remove
os.remove("/tmp/_zen_os_testfile2.txt")
e3 = io.exists("/tmp/_zen_os_testfile2.txt")
assert not e3
os.remove("/tmp/_zen_os_testdir")
d3 = os.isdir("/tmp/_zen_os_testdir")
assert not d3

print("All os tests passed.")
