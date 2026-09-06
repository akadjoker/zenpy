-- Lua 5.4 bunnymark script (same shape as bunny.zen).
local Bunny = {}
Bunny.__index = Bunny

function Bunny.new(x, y, vx, vy)
    return setmetatable({ x = x, y = y, vx = vx, vy = vy }, Bunny)
end

function Bunny:update(dt)
    self.x = self.x + self.vx * dt
    self.y = self.y + self.vy * dt
    if self.x > max_x or self.x < 0 then
        self.vx = -self.vx
    end
    if self.y > max_y or self.y < 0 then
        self.vy = -self.vy
    end
    draw_bunny(self.x, self.y)
end

bunnies = {}
max_x = screen_width() - 26
max_y = screen_height() - 37

function add_bunnies(n)
    local list = bunnies
    for i = 1, n do
        list[#list + 1] = Bunny.new(rand(0, max_x), rand(0, max_y), rand(-250, 250), rand(-250, 250))
    end
end

function update_all(dt)
    local list = bunnies
    for i = 1, #list do
        list[i]:update(dt)
    end
end
