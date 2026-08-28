# Basic expression eval
print(eval("1 + 2"))         # 3
print(eval("10 * 5 - 3"))   # 47

# Globals visible inside eval
y = 10
print(eval("y * 3"))        # 30

# String expressions
print(eval('"hello" + " world"'))  # hello world

# Eval with side-effect statements (exec mode fallback)
eval("print('from eval')")  # from eval

# eval returns value of assignments (consistent with expression semantics)
r = eval("x = 99")
print(r)   # 99
