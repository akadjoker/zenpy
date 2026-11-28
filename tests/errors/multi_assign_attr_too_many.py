# expect: too many values to unpack (expected 2, got 3)
class Point:
    pass

p = Point()
p.x, p.y = 1, 2, 3
