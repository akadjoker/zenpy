local C = {}
C.__index = C
function C.new() return setmetatable({v = 0}, C) end
function C:m() return self.v end
local function loop(n)
    local o = C.new()
    local i = 0
    while i < n do
        o:m()
        i = i + 1
    end
end
local start = os.clock()
loop(5000000)
print("elapsed:", os.clock() - start)
