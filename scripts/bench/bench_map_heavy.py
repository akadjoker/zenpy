# bench_map_heavy.py
m = {}
for i in range(200000):
    m[i] = i * 3
total = 0
for i in range(200000):
    total = total + m[i]
print("map done, total:", total)