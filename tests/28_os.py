# Test os module
import os
import io

# A directory beside the test run — Windows has no /tmp.
TMP = "_zen_os_tmp"
if not os.isdir(TMP):
    os.mkdir(TMP)

# getcwd
cwd = os.getcwd()
assert len(cwd) > 0

# mkdir + isdir
subdir = TMP + "/testdir"
os.mkdir(subdir)
d1 = os.isdir(subdir)
assert d1
d2 = os.isdir(TMP + "/no_such_dir_xyz")
assert not d2
f1 = os.isfile(subdir)
assert not f1

# listdir
files = os.listdir(TMP)
assert len(files) > 0

# getenv
path_var = os.getenv("PATH")
assert len(path_var) > 0

# isfile
testfile = TMP + "/testfile.txt"
io.write(testfile, "hello")
f2 = os.isfile(testfile)
assert f2
f3 = os.isfile(TMP + "/no_such_file_xyz")
assert not f3

# rename
testfile2 = TMP + "/testfile2.txt"
os.rename(testfile, testfile2)
e1 = io.exists(testfile)
assert not e1
e2 = io.exists(testfile2)
assert e2

# remove
os.remove(testfile2)
e3 = io.exists(testfile2)
assert not e3
os.remove(subdir)
d3 = os.isdir(subdir)
assert not d3
os.remove(TMP)

print("All os tests passed.")
