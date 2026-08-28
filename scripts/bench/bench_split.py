# bench_split.py
line = "v 1.234567 -2.345678 3.456789"
for i in range(1000000):
    parts = line.split()
print("split done")