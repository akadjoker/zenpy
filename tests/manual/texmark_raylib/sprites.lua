-- Lua 5.4 texmark script (same shape as sprites.zen).
local Sprite = {}
Sprite.__index = Sprite

function Sprite.new(tex, x, y, vx, vy)
    return setmetatable({ tex = tex, x = x, y = y, vx = vx, vy = vy }, Sprite)
end

function Sprite:update(dt)
    self.x = self.x + self.vx * dt
    self.y = self.y + self.vy * dt
    if self.x > max_x or self.x < 0 then
        self.vx = -self.vx
    end
    if self.y > max_y or self.y < 0 then
        self.vy = -self.vy
    end
    self.tex:draw(self.x, self.y)
end

wabbit = Texture("wabbit_alpha.png")
sprites = {}
max_x = screen_width() - wabbit:width()
max_y = screen_height() - wabbit:height()

function add_sprites(n, x, y)
    local list = sprites
    for i = 1, n do
        if x < 0 then
            list[#list + 1] = Sprite.new(wabbit, rand(0, max_x), rand(0, max_y), rand(-250, 250), rand(-250, 250))
        else
            list[#list + 1] = Sprite.new(wabbit, x, y, rand(-250, 250), rand(-250, 250))
        end
    end
end

function update_all(dt)
    local list = sprites
    for i = 1, #list do
        list[i]:update(dt)
    end
end
