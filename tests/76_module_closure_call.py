# module.function(...) must run with the callee's own constants, upvalues,
# defaults and *args handling, regardless of which function ran before it.
import mod76_helper

def local_noise(s):
    # string constants that differ from every constant in the module
    return "LOCAL:" + s + ":"

# Pollute the frame slot with a different function's constants first.
assert local_noise("a") == "LOCAL:a:"
assert mod76_helper.greet("zen") == "hello zen!"

assert local_noise("b") == "LOCAL:b:"
assert mod76_helper.label() == "module-label"

# defaults
assert mod76_helper.with_default(1) == 17
assert mod76_helper.with_default(1, 2) == 12

# *args
assert mod76_helper.gather(1) == 100
assert mod76_helper.gather(1, 2, 3) == 602

# module function calling another module function
assert local_noise("c") == "LOCAL:c:"
assert mod76_helper.twice("x") == "hello x!hello x!"

# closure returned by a module function keeps its upvalue
add5 = mod76_helper.make_adder(5)
assert add5(10) == 15
assert mod76_helper.make_adder(100)(1) == 101
assert add5(1) == 6

# repeated calls in a loop, interleaved with local calls
acc = 0
for i in range(50):
    assert local_noise("l") == "LOCAL:l:"
    acc = acc + mod76_helper.with_default(i)
    assert mod76_helper.greet("n") == "hello n!"
assert acc == 10 * 1225 + 7 * 50

# result is usable as an argument / inside expressions
assert len(mod76_helper.greet("abc")) == 10
assert mod76_helper.with_default(mod76_helper.with_default(0)) == 77
print("ok")
