local function f() return 0 end
local function loop(n)
    local i = 0
    while i < n do
        f()
        i = i + 1
    end
end
local start = os.clock()
loop(5000000)
print("elapsed:", os.clock() - start)
