# bench_list_ops
a = []
for i in range(500000):
    a.append(i)
s = 0
for x in a:
    s = s + x
print("list done, sum:", s)