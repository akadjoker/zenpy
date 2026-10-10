# Helper module for 76_module_closure_call.py (name must not start with a digit).
def greet(name):
    return "hello " + name + "!"

def with_default(x, y=7):
    return x * 10 + y

def gather(first, *rest):
    total = first
    for r in rest:
        total = total + r
    return total * 100 + len(rest)

def twice(name):
    return greet(name) + greet(name)

def make_adder(n):
    def add(v):
        return v + n
    return add

def label():
    return "module-label"
