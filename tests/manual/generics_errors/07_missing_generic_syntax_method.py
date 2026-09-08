# Same bug as 06, for a generic METHOD with value parameters called through
# plain OP_INVOKE (no <...>).
class Box:
    def get<T>(self, x):
        return (T, x)

b = Box()
b.get(5)
