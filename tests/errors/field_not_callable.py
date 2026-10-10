# expect: is not callable
class Box:
    def __init__(self):
        self.cb = 5

b = Box()
b.cb(1)
