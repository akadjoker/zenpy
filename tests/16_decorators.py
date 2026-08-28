# Test 16: decorators

def logged(fn):
    def wrapper():
        print("before")
        fn()
        print("after")
    return wrapper

@logged
def greet():
    print("hello")

greet()
# before
# hello
# after

# decorator with argument (factory)
def repeat(n):
    def decorator(fn):
        def wrapper():
            i = 0
            while i < n:
                fn()
                i = i + 1
        return wrapper
    return decorator

@repeat(3)
def hi():
    print("hi")

hi()
# hi
# hi
# hi

# stacked decorators
def bold(fn):
    def wrapper():
        print("<b>")
        fn()
        print("</b>")
    return wrapper

def italic(fn):
    def wrapper():
        print("<i>")
        fn()
        print("</i>")
    return wrapper

@bold
@italic
def text():
    print("zen")

text()
# <b>
# <i>
# zen
# </i>
# </b>
