# Regression: super().method(x) on a generic parent method used to
# silently bind x into T's register instead of erroring (no
# super().method<T>(...) syntax exists yet).
class Base:
    def greet<T>(self, msg):
        return (T, msg)

class Child(Base):
    def greet(self, msg):
        return super().greet(msg)

Child().greet("hi")
