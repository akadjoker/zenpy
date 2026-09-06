import time

class Toggle:
    def __init__(self, start_state):
        self.state = start_state
    def value(self):
        return self.state
    def activate(self):
        self.state = not self.state
        return self

class NthToggle(Toggle):
    def __init__(self, start_state, max_counter):
        super().__init__(start_state)
        self.count_max = max_counter
        self.count = 0
    def activate(self):
        self.count = self.count + 1
        if self.count >= self.count_max:
            super().activate()
            self.count = 0
        return self

start = time.perf_counter()
n = 100000
val = True
toggle = Toggle(val)

for i in range(n):
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()
    val = toggle.activate().value()

print(toggle.value())

val = True
ntoggle = NthToggle(val, 3)

for i in range(n):
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()
    val = ntoggle.activate().value()

print(ntoggle.value())
print("elapsed:", time.perf_counter() - start)
