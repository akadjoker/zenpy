# OP_INVOKE_R / OP_INVOKE_VT_R: a method call whose receiver is a local
# variable copies the receiver into the call base inside the VM (no MOVE).
# Every shape below must behave exactly as the two-instruction form did.

class A:
    def __init__(self, v):
        self.v = v
    def m(self):
        return self.v
    def add(self, x, y=10):
        return self.v + x + y
    def pair(self):
        return self.v, self.v * 2
    def chain(self):
        return self
    def var(self, *xs):
        return len(xs) + self.v

def plain(a: A, b):
    # typed receiver (INVOKE_VT_R) and dynamic receiver (INVOKE_R)
    return a.m() + b.m()
assert plain(A(1), A(2)) == 3

def nested(a: A):
    # arguments that are themselves method calls on the same local
    return a.add(a.m(), a.m())
assert nested(A(2)) == 6

def defaults_kw(a: A):
    return a.add(1) + a.add(1, y=2) + a.add(x=3)
assert defaults_kw(A(0)) == 11 + 3 + 13

def spread(a: A):
    xs = [1, 2, 3]
    return a.var(*xs) + a.var() + a.var(0, *xs)
assert spread(A(5)) == 8 + 5 + 9

class SP:
    def m(self, *xs):
        return len(xs)
class SC(SP):
    def m(self, *xs):
        # super() with *args: packing and spread (both were missing)
        return super().m(1, 2, 3) * 100 + super().m(0, *xs)
assert SC().m(1, 2) == 303

def multi(a: A):
    # multi-assign keeps the patchable form: both results must arrive
    p, q = a.pair()
    return p + q
assert multi(A(4)) == 12

def chain(a: A):
    return a.chain().chain().m()
assert chain(A(7)) == 7

def self_calls():
    class B:
        def __init__(self):
            self.n = 0
        def inc(self):
            self.n = self.n + 1
            return self
        def twice(self):
            self.inc()
            return self.inc().n
    return B().twice()
assert self_calls() == 2

def captured():
    # a closure reassigns the receiver while the arguments are evaluated:
    # the receiver seen by the call is the one before the arguments ran
    a = A(1)
    def swap():
        nonlocal a
        a = A(100)
        return 0
    r = a.add(swap(), 0)
    return r, a.m()
cr, cs = captured()
assert cr == 1 and cs == 100

def builtin_receivers():
    s = "abc"
    xs = [3, 1]
    xs.append(2)
    return s.upper() + str(len(xs))
assert builtin_receivers() == "ABC3"

def in_loop(xs):
    total = 0
    for b in xs:
        total = total + b.m()
    return total
assert in_loop([A(1), A(2), A(3)]) == 6

def cond_expr(a: A, flag):
    return a.m() if flag else a.add(0, 0)
assert cond_expr(A(3), True) == 3 and cond_expr(A(3), False) == 3

def many_regs(a: A):
    # receiver in a high register: still an 8-bit operand
    r0 = 0; r1 = 1; r2 = 2; r3 = 3; r4 = 4; r5 = 5; r6 = 6; r7 = 7
    r8 = 8; r9 = 9; r10 = 10; r11 = 11; r12 = 12; r13 = 13; r14 = 14
    b = a
    return b.add(r14, r0) + r1 + r13
assert many_regs(A(1)) == 15 + 14

print("ok")
