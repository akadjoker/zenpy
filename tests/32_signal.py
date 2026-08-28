# Test Signal class

s = Signal()
acc = []

def on_event(v):
    acc.append(v)

def on_event2(v):
    acc.append(v * 10)

assert s.count() == 0

s.connect(on_event)
assert s.count() == 1

s.emit(3)
assert len(acc) == 1
assert acc[0] == 3

s.connect(on_event2)
assert s.count() == 2

s.emit(2)
assert len(acc) == 3
assert acc[1] == 2
assert acc[2] == 20

s.disconnect(on_event)
assert s.count() == 1

s.clear()
assert s.count() == 0

print("All Signal tests passed.")
