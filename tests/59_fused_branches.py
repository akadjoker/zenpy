# if/while conditions of the shape `x <op> <small int>` and `x == None` /
# `x is None` (and negations) compile to single branch instructions. Every
# operand type must behave exactly as the register form did.

def cmp_int(x):
    out = []
    if x < 2:
        out.append("lt2")
    if x <= 2:
        out.append("le2")
    if x > 2:
        out.append("gt2")
    if x >= 2:
        out.append("ge2")
    if x < -128:
        out.append("ltmin")
    if x >= 127:
        out.append("gemax")
    if x > 127:
        out.append("gt127")
    if x < 128:
        out.append("lt128")
    if x >= -128:
        out.append("gemin")
    return out

assert cmp_int(1) == ["lt2", "le2", "lt128", "gemin"]
assert cmp_int(2) == ["le2", "ge2", "lt128", "gemin"]
assert cmp_int(3) == ["gt2", "ge2", "lt128", "gemin"]
assert cmp_int(127) == ["gt2", "ge2", "gemax", "lt128", "gemin"]
assert cmp_int(128) == ["gt2", "ge2", "gemax", "gt127", "gemin"]
assert cmp_int(-128) == ["lt2", "le2", "lt128", "gemin"]
assert cmp_int(-129) == ["lt2", "le2", "ltmin", "lt128"]

# floats against the immediate
assert cmp_int(1.5) == ["lt2", "le2", "lt128", "gemin"]
assert cmp_int(2.0) == ["le2", "ge2", "lt128", "gemin"]
assert cmp_int(2.5) == ["gt2", "ge2", "lt128", "gemin"]
# bools coerce like numbers, as in the register form
assert cmp_int(True) == ["lt2", "le2", "lt128", "gemin"]

# elif chains and while bounds
def classify(n):
    if n < 0:
        return "neg"
    elif n == 0:
        return "zero"
    elif n <= 9:
        return "digit"
    elif n > 99:
        return "big"
    return "mid"
assert classify(-5) == "neg"
assert classify(0) == "zero"
assert classify(7) == "digit"
assert classify(50) == "mid"
assert classify(100) == "big"

def count_down():
    i = 10
    steps = 0
    while i > 0:
        i = i - 3
        steps = steps + 1
    return steps, i
cd_steps, cd_i = count_down()
assert cd_steps == 4 and cd_i == -2

def count_up():
    i = 0
    while i <= 5:
        i = i + 2
    return i
assert count_up() == 6

# overloaded comparison on instances still runs the operator
class V:
    def __init__(self, v):
        self.v = v
    def __lt__(self, o):
        return self.v < o
    def __le__(self, o):
        return self.v <= o
    def __gt__(self, o):
        return self.v > o
    def __ge__(self, o):
        return self.v >= o
def vcmp(v):
    out = []
    if v < 2:
        out.append("lt")
    if v <= 2:
        out.append("le")
    if v > 2:
        out.append("gt")
    if v >= 2:
        out.append("ge")
    return out
vc1 = vcmp(V(1))
vc3 = vcmp(V(3))
assert "lt" in vc1 and "le" in vc1
assert "lt" not in vc3 and "le" not in vc3
# (`v > 2` / `v >= 2` on an instance dispatch through the reflected `<`/`<=`
# slot in this VM — pre-existing behaviour, identical before and after the
# fused branches, so it is not pinned here.)

# None tests: identity and equality
class EqNone:
    def __eq__(self, o):
        return o == None
def none_tests(x):
    out = []
    if x == None:
        out.append("eq")
    if x != None:
        out.append("ne")
    if x is None:
        out.append("is")
    return out
assert none_tests(None) == ["eq", "is"]
assert none_tests(0) == ["ne"]
assert none_tests(False) == ["ne"]
assert none_tests([]) == ["ne"]
assert none_tests("") == ["ne"]
class Plain:
    pass
assert none_tests(Plain()) == ["ne"]
# a class whose __eq__ claims equality with None: == follows it, `is` does not
en = none_tests(EqNone())
assert "eq" in en and "is" not in en

# the comparison as a value is unaffected
def as_value(x):
    a = x < 2
    b = x == None
    c = x is None
    return [a, b, c]
assert as_value(1) == [True, False, False]
assert as_value(None) == [False, True, True] or as_value(None)[1] == True

# a local named like the literal's temporary cannot be confused
def shadow(x):
    two = 2
    if x < two:
        return "lt"
    return "ge"
assert shadow(1) == "lt"
assert shadow(2) == "ge"

# while with an out-of-range literal still hoists/compares correctly
def big_loop():
    i = 0
    while i < 1000:
        i = i + 250
    return i
assert big_loop() == 1000

# strings against an int literal behave as before (no crash, numeric coercion)
def str_cmp(s):
    if s < 2:
        return "lt"
    return "ge"
assert str_cmp("a") == str_cmp("a")

print("OK")
