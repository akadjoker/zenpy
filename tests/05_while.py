def countdown(n):
    while n > 0:
        print(n)
        n = n - 1
    print(0)

countdown(5)

# A comparison in a while condition is compiled as the fused branch form.
# It must still use a class' ordinary __lt__ implementation.
class LoopCounter:
    def __init__(self, value):
        self.value = value

    def __lt__(self, limit):
        return self.value < limit

    def advance(self):
        self.value += 1

counter = LoopCounter(0)
while counter < 4:
    counter.advance()
assert counter.value == 4
