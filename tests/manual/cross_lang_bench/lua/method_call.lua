local Toggle = {}
Toggle.__index = Toggle
function Toggle.new(startState)
    local self = setmetatable({}, Toggle)
    self.state = startState
    return self
end
function Toggle:value() return self.state end
function Toggle:activate()
    self.state = not self.state
    return self
end

local NthToggle = setmetatable({}, { __index = Toggle })
NthToggle.__index = NthToggle
function NthToggle.new(startState, maxCounter)
    local self = setmetatable(Toggle.new(startState), NthToggle)
    self.countMax = maxCounter
    self.count = 0
    return self
end
function NthToggle:activate()
    self.count = self.count + 1
    if self.count >= self.countMax then
        Toggle.activate(self)
        self.count = 0
    end
    return self
end

local start = os.clock()
local n = 100000
local val = true
local toggle = Toggle.new(val)

for i = 1, n do
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
end

print(toggle:value())

val = true
local ntoggle = NthToggle.new(val, 3)

for i = 1, n do
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
end

print(ntoggle:value())
print("elapsed:", os.clock() - start)
