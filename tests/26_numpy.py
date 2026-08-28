# Test numpy module

import numpy

# --- constructors ---
a = numpy.zeros(5)
assert a.len() == 5
assert a[0] == 0.0
assert a[4] == 0.0
print("zeros OK")

b = numpy.ones(5)
assert b[0] == 1.0
assert b[4] == 1.0
print("ones OK")

c = numpy.full(4, 3.14)
assert c[0] > 3.13
assert c[0] < 3.15
assert c.len() == 4
print("full OK")

d = numpy.arange(5)
assert d.len() == 5
assert d[0] == 0.0
assert d[4] == 4.0
print("arange(5) OK")

d2 = numpy.arange(2, 6)
assert d2.len() == 4
assert d2[0] == 2.0
assert d2[3] == 5.0
print("arange(2,6) OK")

d3 = numpy.arange(0, 10, 2)
assert d3.len() == 5
assert d3[0] == 0.0
assert d3[4] == 8.0
print("arange(0,10,2) OK")

e = numpy.linspace(0, 1, 5)
assert e.len() == 5
assert e[0] == 0.0
assert e[4] == 1.0
assert e[2] == 0.5
print("linspace OK")

f = numpy.array([1, 2, 3, 4, 5])
assert f.len() == 5
assert f[0] == 1.0
assert f[4] == 5.0
print("array OK")

# --- element-wise binary ---
a = numpy.array([1, 2, 3])
b = numpy.array([4, 5, 6])

c = numpy.add(a, b)
assert c[0] == 5.0
assert c[1] == 7.0
assert c[2] == 9.0
print("add OK")

c = numpy.subtract(a, b)
assert c[0] == -3.0
assert c[1] == -3.0
print("subtract OK")

c = numpy.multiply(a, b)
assert c[0] == 4.0
assert c[1] == 10.0
assert c[2] == 18.0
print("multiply OK")

c = numpy.divide(b, a)
assert c[0] == 4.0
assert c[1] == 2.5
assert c[2] == 2.0
print("divide OK")

c = numpy.mod(b, a)
assert c[0] == 0.0
assert c[1] == 1.0
assert c[2] == 0.0
print("mod OK")

c = numpy.power(a, numpy.array([2, 2, 2]))
assert c[0] == 1.0
assert c[1] == 4.0
assert c[2] == 9.0
print("power OK")

c = numpy.minimum(a, b)
assert c[0] == 1.0
assert c[2] == 3.0
print("minimum OK")

c = numpy.maximum(a, b)
assert c[0] == 4.0
assert c[2] == 6.0
print("maximum OK")

# --- scalar broadcast ---
c = numpy.add(a, 10)
assert c[0] == 11.0
assert c[2] == 13.0
print("add scalar OK")

c = numpy.multiply(a, 3)
assert c[0] == 3.0
assert c[2] == 9.0
print("multiply scalar OK")

c = numpy.add(100, a)
assert c[0] == 101.0
print("scalar + array OK")

# --- element-wise unary ---
a = numpy.array([1, -2, 3])
c = numpy.negative(a)
assert c[0] == -1.0
assert c[1] == 2.0
print("negative OK")

c = numpy.abs(a)
assert c[0] == 1.0
assert c[1] == 2.0
print("abs OK")

a = numpy.array([0, 1, 4, 9])
c = numpy.sqrt(a)
assert c[0] == 0.0
assert c[1] == 1.0
assert c[2] == 2.0
assert c[3] == 3.0
print("sqrt OK")

a = numpy.array([0, 1, 2])
c = numpy.exp(a)
assert c[0] == 1.0
assert c[1] > 2.71
assert c[1] < 2.72
print("exp OK")

c = numpy.floor(numpy.array([1.5, 2.9, -0.1]))
assert c[0] == 1.0
assert c[1] == 2.0
assert c[2] == -1.0
print("floor OK")

c = numpy.ceil(numpy.array([1.1, 2.0, -0.1]))
assert c[0] == 2.0
assert c[1] == 2.0
assert c[2] == 0.0
print("ceil OK")

# --- reductions ---
a = numpy.array([1, 2, 3, 4, 5])

assert numpy.sum(a) == 15.0
print("sum OK")

assert numpy.prod(a) == 120.0
print("prod OK")

assert numpy.min(a) == 1.0
print("min OK")

assert numpy.max(a) == 5.0
print("max OK")

assert numpy.mean(a) == 3.0
print("mean OK")

v = numpy.std(numpy.array([2, 4, 4, 4, 5, 5, 7, 9]))
assert v > 1.99
assert v < 2.01
print("std OK")

assert numpy.dot(numpy.array([1,2,3]), numpy.array([4,5,6])) == 32.0
print("dot OK")

assert numpy.argmin(numpy.array([3, 1, 2])) == 1
print("argmin OK")

assert numpy.argmax(numpy.array([3, 1, 5, 2])) == 2
print("argmax OK")

# --- utility ---
z = numpy.zeros_like(numpy.array([1,2,3]))
assert z.len() == 3
assert z[0] == 0.0
print("zeros_like OK")

o = numpy.ones_like(numpy.array([1,2,3]))
assert o[0] == 1.0
print("ones_like OK")

lst = numpy.tolist(numpy.array([10, 20, 30]))
assert lst[0] == 10.0
assert lst[2] == 30.0
print("tolist OK")

c = numpy.clip(numpy.array([1, 5, 10, 15, 20]), 5, 15)
assert c[0] == 5.0
assert c[1] == 5.0
assert c[2] == 10.0
assert c[3] == 15.0
assert c[4] == 15.0
print("clip OK")

c = numpy.cumsum(numpy.array([1, 2, 3, 4]))
assert c[0] == 1.0
assert c[1] == 3.0
assert c[2] == 6.0
assert c[3] == 10.0
print("cumsum OK")

c = numpy.diff(numpy.array([1, 3, 6, 10]))
assert c.len() == 3
assert c[0] == 2.0
assert c[1] == 3.0
assert c[2] == 4.0
print("diff OK")

cond = numpy.array([1, 0, 1, 0])
x = numpy.array([10, 20, 30, 40])
y = numpy.array([100, 200, 300, 400])
w = numpy.where(cond, x, y)
assert w[0] == 10.0
assert w[1] == 200.0
assert w[2] == 30.0
assert w[3] == 400.0
print("where OK")

c = numpy.concatenate(numpy.array([1,2,3]), numpy.array([4,5]))
assert c.len() == 5
assert c[0] == 1.0
assert c[4] == 5.0
print("concatenate OK")

print("all numpy tests passed")
