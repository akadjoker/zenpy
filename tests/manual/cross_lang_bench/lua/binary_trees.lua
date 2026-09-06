local Tree = {}
Tree.__index = Tree
function Tree.new(item, depth)
    local self = setmetatable({}, Tree)
    self.item = item
    self.left = nil
    self.right = nil
    if depth > 0 then
        local item2 = item + item
        depth = depth - 1
        self.left = Tree.new(item2 - 1, depth)
        self.right = Tree.new(item2, depth)
    end
    return self
end
function Tree:check()
    if self.left == nil then
        return self.item
    end
    return self.item + self.left:check() - self.right:check()
end

local minDepth = 4
local maxDepth = 12
local stretchDepth = maxDepth + 1

local start = os.clock()

local t = Tree.new(0, stretchDepth)
print("stretch tree of depth " .. stretchDepth .. " check: " .. t:check())

local longLivedTree = Tree.new(0, maxDepth)

local iterations = 1
for d = 0, maxDepth - 1 do
    iterations = iterations * 2
end

local depth = minDepth
while depth < stretchDepth do
    local check = 0
    for i = 1, iterations do
        check = check + Tree.new(i, depth):check() + Tree.new(-i, depth):check()
    end
    print(iterations * 2 .. " trees of depth " .. depth .. " check: " .. check)
    iterations = iterations // 4
    depth = depth + 2
end

print("long lived tree of depth " .. maxDepth .. " check: " .. longLivedTree:check())
print("elapsed:", os.clock() - start)
