# Bound methods: `f = obj.method` yields a callable that remembers obj
# (Python semantics). Methods used to be invisible as values ("no field").

class Counter:
    def __init__(self, start):
        self.n = start

    def add(self, k):
        self.n = self.n + k
        return self.n

    def get(self):
        return self.n

    def scaled(self, a, b=10):
        return (a + b) * self.n

    def total(self, *xs):
        t = self.n
        for x in xs:
            t = t + x
        return t

class Base:
    def __init__(self):
        self.tag = "base"

    def who(self):
        return self.tag

    def twice(self, v):
        return v * 2

class Derived(Base):
    def __init__(self):
        self.tag = "derived"

    def extra(self):
        return self.tag + "!"

# --- basic: store, call, state is shared with the instance
c = Counter(5)
f = c.add
assert f(1) == 6
assert f(4) == 10
assert c.n == 10
g = c.get
assert g() == 10
c.n = 100
assert g() == 100          # bound to the instance, not a snapshot

# --- defaults, *args, multiple arguments
s = c.scaled
assert s(1, 2) == 300
assert s(1) == 1100
t = c.total
assert t() == 100
assert t(1, 2, 3) == 106

# --- stored in a list and in a dict, called later
objs = [Counter(1), Counter(2), Counter(3)]
fs = [o.add for o in objs]
for fn in fs:
    fn(10)
assert [o.n for o in objs] == [11, 12, 13]
assert fs[1](0) == 12

d = {"inc": objs[0].add, "get": objs[2].get}
assert d["inc"](5) == 16
assert d["get"]() == 13

# --- passed as an argument to a function that calls it
def apply(fn, v):
    return fn(v)

def apply_twice(fn, v):
    return fn(fn(v))

c2 = Counter(0)
assert apply(c2.add, 3) == 3
assert apply_twice(c2.add, 3) == 12    # n: 3 -> 6, then add(6) -> 12
assert c2.n == 12

# --- inherited method bound through a subclass instance
dv = Derived()
w = dv.who
assert w() == "derived"
tw = dv.twice
assert tw(21) == 42
assert dv.extra() == "derived!"
e = dv.extra
assert e() == "derived!"

# --- original variable reassigned: the bound method keeps its receiver
a = Counter(7)
bg = a.get
a = Counter(8)
assert bg() == 7
assert a.get() == 8

# --- a closure capturing a bound method
def make_caller(o):
    m = o.add
    def call(k):
        return m(k)
    return call

cc = make_caller(Counter(1))
assert cc(1) == 2
assert cc(1) == 3

# --- callable / typeof / equality / str
assert callable(f)
assert callable(w)
assert typeof(f) == "function"
f2 = f
assert f == f2
assert f == f
h1 = c.get
h2 = c.get
# Each read of `obj.method` builds a fresh bound method; == is identity.
assert h1 != h2
assert h1 == h1
assert str(f) == "<bound method Counter.add>"
assert str(w) == "<bound method Base.who>"   # defining class, as in Python
assert str(Signal().count) == "<bound method Signal.count>"

# --- bound method of a native class (ClassBuilder method)
sig = Signal()
cnt = sig.count
assert cnt() == 0
hits = []
def listener(x):
    hits.append(x)
sig.connect(listener)
assert cnt() == 1
em = sig.emit
em(5)
em(6)
assert hits == [5, 6]

# --- a bound script method as a listener (called from native via call_fn)
class Recorder:
    def __init__(self):
        self.seen = []

    def on_event(self, v):
        self.seen.append(v)

r = Recorder()
sig2 = Signal()
sig2.connect(r.on_event)
sig2.emit(1)
sig2.emit(2)
assert r.seen == [1, 2]
assert sig2.disconnect(r.on_event)     # matches by receiver + method
assert sig2.count() == 0

# --- sorted(key=...) with a bound method goes through call_fn too
class Keyer:
    def __init__(self, m):
        self.m = m

    def key(self, v):
        return -v * self.m

k = Keyer(2)
assert sorted([3, 1, 2], key=k.key) == [3, 2, 1]

# --- self.method taken as a value inside the class (the compiler must not
# turn it into a phantom nil field), including after self.method(...) calls
class Widget:
    def __init__(self):
        self.k = 3
        self.cb = self.helper          # stored callback
        self.r = self.step(1)          # a call first: no phantom "step" field

    def helper(self, a):
        return a * self.k

    def step(self, a):
        return a + 1

    def run(self):
        f = self.helper
        g = self.cb                    # (calling self.cb(1) directly: INVOKE looks up methods only)
        return f(2) + g(1)

    def grab(self):
        return self.step

wd = Widget()
assert wd.r == 2
assert wd.run() == 9
cbv = wd.cb
assert cbv(4) == 12
st = wd.step                   # outside, after self.step(...) calls inside
assert st(10) == 11
assert wd.grab()(20) == 21
assert wd.grab() != wd.grab()

class SubWidget(Widget):
    def own(self):
        return self.helper     # inherited method, read as a value

assert SubWidget().own()(5) == 15

# --- GC pressure: many bound methods created and called
acc = Counter(0)
total = 0
i = 0
while i < 100000:
    m = acc.add
    total = total + m(1)
    i = i + 1
assert acc.n == 100000
assert total == 100000 * 100001 // 2

# garbage-heavy loop while holding some bound methods
keep = [Counter(i).get for i in range(50)]
junk = []
for j in range(20000):
    junk = [j, str(j), {"k": j}]
assert [kf() for kf in keep] == list(range(50))

print("bound methods ok")
