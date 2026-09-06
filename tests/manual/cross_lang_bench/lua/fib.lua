local function fib(n)
    if n < 2 then return n end
    return fib(n - 1) + fib(n - 2)
end

local start = os.clock()
for i = 1, 5 do
    print(fib(28))
end
print("elapsed:", os.clock() - start)
