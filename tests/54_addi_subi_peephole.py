# `x + <small int literal>` / `x - <small int literal>` compile to
# OP_ADDI / OP_SUBI (one instruction, no LOADI temporary). The peephole is
# purely an encoding change: every left-operand type must behave exactly as
# it would through OP_ADD / OP_SUB with the same literal in a register. This
# file pins that contract, contrasting the folded form with a form the
# peephole cannot fold (the literal held in a variable).

one = 1
two = 2
neg = -3

# --- ints, including the register-reuse shape `x = x + 1` (A == B) ---
i = 5
i = i + 1
assert i == 6
i = i - 1
assert i == 5
assert i + one == i + 1
assert i - one == i - 1
assert i + 127 == 132
assert i - 128 == -123
assert i + 128 == 133          # 128 doesn't fit int8: stays ADD, same result

# --- int wraparound matches OP_ADD's (uint64 wrap) ---
big = 9223372036854775807
assert big + 1 == big + one
assert (0 - big) - 2 == (0 - big) - two

# --- float left operand ---
f = 1.5
assert f + 1 == 2.5
assert f - 1 == 0.5
assert f + 1 == f + one

# --- bool / nil left operand: numeric coercion, same as OP_ADD ---
assert True + 1 == True + one
assert False - 1 == False - one

# --- string left operand: OP_ADD concatenates a formatted number ---
s = "n="
assert s + 1 == "n=1"
assert s + 1 == s + one
assert s + neg == "n=-3"     # neg isn't a literal: plain ADD; same output

# --- instance with __add__/__sub__/__radd__: the overload must still run ---
class Vec:
    def __init__(self, v):
        self.v = v
    def __add__(self, other):
        return Vec(self.v + other * 10)
    def __sub__(self, other):
        return Vec(self.v - other * 10)

vec = Vec(5)
assert (vec + 1).v == 15
assert (vec - 1).v == -5
assert (vec + 1).v == (vec + one).v
assert (vec - 1).v == (vec - one).v

# --- instance WITHOUT __add__: OP_ADD stringifies both sides and concatenates ---
class Plain:
    def __str__(self):
        return "plain"

p = Plain()
assert p + 1 == "plain1"
assert p + 1 == p + one

# --- nested / chained: only the innermost literal folds, result unchanged ---
x = 10
assert x + 1 + 2 == 13
assert x - 1 - 2 == 7
assert (x + 1) * 2 == 22
assert x * 2 + 1 == 21

# --- literal on the LEFT is not the peephole's shape; must still be right ---
assert 1 + x == 11
assert 1 - x == -9

print("ADDI/SUBI peephole OK")
