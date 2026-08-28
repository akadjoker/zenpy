# Zen → Python Compatibility TODO

## High Priority (blocks common Python code)

- [ ] `try/except/finally` — `try: ... except ValueError as e: ...`
- [ ] `**kwargs` — `def f(**kw)`
- [x] `*args` unpack at call site — `f(*arr)`
- [x] `a, b = 1, 2` — multi-assign with tuple RHS (not just function call)
- [x] `not in` / `is not` — `if x not in arr:`
- [x] Chained comparisons — `0 < x < 10`
- [x] `None`/`True`/`False` as keywords
- [x] String methods — `split`, `join`, `strip`, `replace`, `startswith`, `find`, `upper`, `lower`
- [x] `in` operator (containment) — `if x in arr:` / `if c in "abc":`
- [x] Negative indexing — `a[-1]`

## Medium Priority

- [ ] `for/else`, `while/else` — else after loop without `break`
- [x] `enumerate()`, `zip()`, `map()`, `filter()` — common builtins
- [x] `isinstance()`, `type()` — introspection
- [ ] Inline `if x: y` — single-line blocks
- [x] `del x[i]` — element deletion (OP_DELINDEX)
- [ ] `with` statement — context managers
- [x] List methods — `push/append`, `pop`, `insert`, `remove`, `sort`, `reverse`, `index`
- [x] Dict methods — `keys`, `values`, `items`, `get`
- [x] `global`/`nonlocal` — scope control (parsed, Zen's scoping already resolves correctly)

## Low Priority (advanced)

- [x] Generators / `yield` — lazy iteration (OP_FOR_ITER + fiber)
- [ ] `*rest` unpack — `a, *b = [1,2,3,4]`
- [ ] Walrus `:=` — `if (n := len(x)) > 0:`
- [ ] Multiple inheritance — MRO
- [ ] Dunder methods — `__str__`, `__repr__`, `__add__`
- [ ] Decorators with args — `@deco(arg)`

## Already Done

- [x] f-strings `f"hello {name}"`
- [x] `eval()` as OP_EVAL
- [x] `*args` variadics
- [x] multi-assign `a, b = f()`
- [x] tuple return `return a, b`
- [x] decorators `@` (fixed upvalue/closure capture bug)
- [x] `enumerate()`, `zip()`, `map()`, `filter()` builtins
- [x] `//` floor division (OP_IDIV)
- [x] `**` power (OP_POW)
- [x] default params `def f(x=0)`
- [x] `import math` module system
- [x] decorators `@`
- [x] `\` line continuation
- [x] `//=` and `**=` augmented assign
- [x] type hints (silently ignored)
- [x] variable annotations `x: int = 1`
- [x] ternary `x if cond else y`
- [x] slices `a[1:3]`, `a[::2]`, `a[::-1]`
- [x] `assert expr [, msg]` (OP_ASSERT)
- [x] semicolons `;` between statements
- [x] tuples `(a, b)` as values
- [x] list comprehensions `[x for x in arr if cond]`
- [x] set literals `{1, 2, 3}`
- [x] dict comprehensions `{k: v for k in iter}`
- [x] set comprehensions `{x for x in iter}`
- [x] `len()` intrinsic (OP_LEN)
