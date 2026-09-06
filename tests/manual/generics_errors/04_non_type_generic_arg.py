def create<T>():
    return T()

x = 5
# Bug #4 variant: a generic argument that resolves to a non-class value
# must be a runtime error ("not a type"), not silently accepted.
create<x>()
