# Test augmented assignment inside functions
def aug():
    x = 10
    x += 5
    x -= 2
    x *= 3
    return x

assert aug() == 39, "augmented assign"
print("augmented assign OK")

# Test closure capturing implicit locals
def make_counter():
    count = 0
    def inc():
        nonlocal count
        count = count + 1
        return count
    return inc

c = make_counter()
assert c() == 1
assert c() == 2
assert c() == 3
print("closure local OK")

# Test for loop variable as implicit local
def for_test():
    total = 0
    for i in [1, 2, 3, 4, 5]:
        total += i
    return total

assert for_test() == 15, "for loop"
print("for loop local OK")

# Test nested functions with same-name locals
def outer():
    x = 1
    def inner():
        x = 2
        return x
    return x + inner()

assert outer() == 3
print("nested same-name OK")

# Test global keyword
counter = 0
def bump():
    global counter
    counter = counter + 1

bump()
bump()
bump()
assert counter == 3
print("global keyword OK")

# Test that without global, assignment doesn't affect outer
val = 100
def no_global():
    val = 999
    return val

assert no_global() == 999
assert val == 100
print("implicit local OK")

# Test multiple locals in a loop
def multi_local_loop():
    a = 0
    b = 1
    i = 0
    while i < 10:
        temp = a
        a = b
        b = temp + b
        i = i + 1
    return a

assert multi_local_loop() == 55, "fib loop"
print("multi local loop OK")

# Test augmented assign on global (no global keyword needed for +=)
g = 10
g += 5
assert g == 15
print("global augmented OK")

# Test local with function call result
def call_result():
    x = len([1,2,3])
    y = x + 1
    return y

assert call_result() == 4
print("call result local OK")

# Test assignment in if branch
def if_assign(n):
    if n > 0:
        sign = 1
    else:
        sign = -1
    return sign

assert if_assign(5) == 1
assert if_assign(-3) == -1
print("if branch assign OK")

# Test deeply nested scopes
def deep():
    a = 1
    if True:
        b = 2
        if True:
            c = 3
            if True:
                d = a + b + c
    return d

assert deep() == 6
print("deep nested OK")

print("\n=== ALL SCOPE TESTS PASSED ===")
