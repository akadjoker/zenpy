# bench_map_insert
m = {}
for i in range(100000):
    m[i] = i * 2
total = 0
for k in m:
    total = total + m[k]
print("map done, total:", total)
