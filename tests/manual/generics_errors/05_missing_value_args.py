def create_with<T>(a, b, c):
    return (a, b, c)

class Transform:
    pass

# Generic arity satisfied, but value arity is short — must still enforce
# normal arity checking on the value parameters.
create_with<Transform>(1, 2)
