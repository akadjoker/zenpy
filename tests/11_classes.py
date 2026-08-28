# Test classes: __init__, methods, self, fields

class Counter:
    def __init__(self, start):
        self.count = start

    def inc(self):
        self.count = self.count + 1

    def get(self):
        return self.count

c = Counter(0)
c.inc()
c.inc()
c.inc()
print(c.get())
# expected: 3

# Class without __init__
class Empty:
    def hello(self):
        return "hi"

e = Empty()
print(e.hello())
# expected: hi

# Multiple instances
a = Counter(10)
b = Counter(20)
a.inc()
b.inc()
b.inc()
print(a.get())
print(b.get())
# expected: 11, 22

# Method calling other method
class Calc:
    def __init__(self, val):
        self.val = val

    def double(self):
        self.val = self.val * 2

    def add(self, n):
        self.val = self.val + n

    def double_and_add(self, n):
        self.double()
        self.add(n)
        return self.val

r = Calc(5)
print(r.double_and_add(3))
# expected: 5*2 + 3 = 13

# Class with conditional logic in methods
class Abs:
    def __init__(self, x):
        if x < 0:
            self.val = 0 - x
        else:
            self.val = x

    def get(self):
        return self.val

print(Abs(-7).get())
print(Abs(3).get())
# expected: 7, 3
