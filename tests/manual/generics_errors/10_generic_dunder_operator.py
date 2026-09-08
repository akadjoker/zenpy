# Regression: a generic dunder (__add__<T>) reachable via plain infix
# syntax (a + b) used to silently bind `other` into T's register instead
# of erroring — there is no <...> opt-in possible at an operator call site.
class Vec:
    def __init__(self, v):
        self.v = v
    def __add__<T>(self, other):
        return (T, self.v, other)

a = Vec(10)
b = Vec(20)
a + b
