def sum_to(n):
    total = 0
    i = 1
    while i <= n:
        total = total + i
        i = i + 1
    return total

print(sum_to(10))
print(sum_to(100))
