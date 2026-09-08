import time

class Tree:
    def __init__(self, item, depth):
        self.item = item
        self.left = None
        self.right = None
        if depth > 0:
            item2 = item + item
            depth = depth - 1
            self.left = Tree(item2 - 1, depth)
            self.right = Tree(item2, depth)
    def check(self):
        if self.left is None:
            return self.item
        return self.item + self.left.check() - self.right.check()

min_depth = 4
max_depth = 12
stretch_depth = max_depth + 1

start = time.perf_counter()

t = Tree(0, stretch_depth)
print("stretch tree of depth", stretch_depth, "check:", t.check())

long_lived_tree = Tree(0, max_depth)

iterations = 1
for d in range(max_depth):
    iterations = iterations * 2

depth = min_depth
while depth < stretch_depth:
    check = 0
    for i in range(1, iterations + 1):
        check = check + Tree(i, depth).check() + Tree(-i, depth).check()
    print(iterations * 2, "trees of depth", depth, "check:", check)
    iterations = iterations // 4
    depth = depth + 2

print("long lived tree of depth", max_depth, "check:", long_lived_tree.check())
print("elapsed:", time.perf_counter() - start)
