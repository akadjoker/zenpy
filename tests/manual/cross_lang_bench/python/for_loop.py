import time

start = time.perf_counter()
sum = 0
i = 0
while i < 5000000:
    sum = sum + i
    i = i + 1
print(sum)
print("elapsed:", time.perf_counter() - start)
