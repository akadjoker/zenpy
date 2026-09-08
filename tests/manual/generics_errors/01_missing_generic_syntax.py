def create<T>():
    return T()

# Bug #1 from the original report: calling a generic function WITHOUT
# generic syntax must be an error, not silently succeed by treating the
# first positional arg as T.
create(int)
