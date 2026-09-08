# Regression: a generic __init__<T> constructed via plain ClassName(x)
# (no <...>) used to silently bind x into T's register instead of erroring.
class Foo:
    def __init__<T>(self, x):
        self.t = T
        self.x = x

Foo(42)
