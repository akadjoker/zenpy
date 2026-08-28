name = "Alice"
age = 30
pi = 3.14

# Basic interpolation
print(f"Hello, {name}!")
print(f"Age: {age}")
print(f"Pi: {pi}")

# Expression in braces
print(f"Double: {age * 2}")
print(f"Sum: {1 + 2 + 3}")

# Multiple interpolations
print(f"{name} is {age} years old")

# No interpolation (plain string)
print(f"no braces here")

# Escaped braces
print(f"{{not interpolated}}")

# Nested expressions
def greet(x):
    return f"Hi {x}!"

print(greet("Bob"))
