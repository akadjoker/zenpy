# bench_float_parse.py
s = "3.14159265"
x = 0.0
for i in range(1000000):
    x = x + float(s)
print("float parse done, x:", x)