-- Lua 5.4 quadtree demo (same shape as quadtree.zen).
local CAPACITY = 8
local MAX_DEPTH = 8
local RADIUS = 6
local MOUSE_W = 160
local MOUSE_H = 120

local Node = {}
Node.__index = Node

local function new_point(x, y, vx, vy)
    return { x = x, y = y, vx = vx, vy = vy, color = 0 }
end

function Node.new(x, y, w, h, depth)
    return setmetatable({ x = x, y = y, w = w, h = h, depth = depth, pts = {}, count = 0,
                          split = false, c0 = nil, c1 = nil, c2 = nil, c3 = nil }, Node)
end

function Node:child_for(px, py)
    if py < self.y + self.h * 0.5 then
        if px < self.x + self.w * 0.5 then return self.c0 end
        return self.c1
    end
    if px < self.x + self.w * 0.5 then return self.c2 end
    return self.c3
end

function Node:insert(p)
    local node = self
    while node.split do node = node:child_for(p.x, p.y) end
    local c = node.count + 1
    node.pts[c] = p
    node.count = c
    if c > CAPACITY and node.depth < MAX_DEPTH then node:subdivide() end
end

function Node:subdivide()
    local hw = self.w * 0.5
    local hh = self.h * 0.5
    local d = self.depth + 1
    self.c0 = Node.new(self.x, self.y, hw, hh, d)
    self.c1 = Node.new(self.x + hw, self.y, hw, hh, d)
    self.c2 = Node.new(self.x, self.y + hh, hw, hh, d)
    self.c3 = Node.new(self.x + hw, self.y + hh, hw, hh, d)
    self.split = true
    local old = self.pts
    for i = 1, self.count do
        local p = old[i]
        self:child_for(p.x, p.y):insert(p)
    end
    self.pts = {}
    self.count = 0
end

function Node:count_in(x0, y0, x1, y1)
    if x1 <= self.x or y1 <= self.y or x0 >= self.x + self.w or y0 >= self.y + self.h then return 0 end
    if self.split then
        local n = self.c0:count_in(x0, y0, x1, y1) + self.c1:count_in(x0, y0, x1, y1)
        n = n + self.c2:count_in(x0, y0, x1, y1) + self.c3:count_in(x0, y0, x1, y1)
        return n
    end
    local n = 0
    local pts = self.pts
    for i = 1, self.count do
        local p = pts[i]
        if p.x >= x0 and p.x < x1 and p.y >= y0 and p.y < y1 then n = n + 1 end
    end
    return n
end

function Node:mark_in(x0, y0, x1, y1, color)
    if x1 <= self.x or y1 <= self.y or x0 >= self.x + self.w or y0 >= self.y + self.h then return 0 end
    if self.split then
        local n = self.c0:mark_in(x0, y0, x1, y1, color) + self.c1:mark_in(x0, y0, x1, y1, color)
        n = n + self.c2:mark_in(x0, y0, x1, y1, color) + self.c3:mark_in(x0, y0, x1, y1, color)
        return n
    end
    local n = 0
    local pts = self.pts
    for i = 1, self.count do
        local p = pts[i]
        if p.x >= x0 and p.x < x1 and p.y >= y0 and p.y < y1 then
            p.color = color
            n = n + 1
        end
    end
    return n
end

function Node:draw()
    draw_rect(self.x, self.y, self.w, self.h, 3)
    if not self.split then return 1 end
    return 1 + self.c0:draw() + self.c1:draw() + self.c2:draw() + self.c3:draw()
end

points = {}
local W = screen_width()
local H = screen_height()

function add_points(n, x, y)
    local list = points
    for i = 1, n do
        if x < 0 then
            list[#list + 1] = new_point(rand(0, W), rand(0, H), rand(-60, 60), rand(-60, 60))
        else
            list[#list + 1] = new_point(x, y, rand(-60, 60), rand(-60, 60))
        end
    end
end

function update_all(dt)
    local pts = points
    local n = #pts
    local root = Node.new(0, 0, W, H, 0)

    for i = 1, n do
        local p = pts[i]
        p.x = p.x + p.vx * dt
        p.y = p.y + p.vy * dt
        if p.x < 0 then p.x = 0; p.vx = -p.vx
        elseif p.x >= W then p.x = W - 1; p.vx = -p.vx end
        if p.y < 0 then p.y = 0; p.vy = -p.vy
        elseif p.y >= H then p.y = H - 1; p.vy = -p.vy end
        p.color = 0
        root:insert(p)
    end

    local neighbours = 0
    for i = 1, n do
        local p = pts[i]
        if root:count_in(p.x - RADIUS, p.y - RADIUS, p.x + RADIUS, p.y + RADIUS) > 1 then
            p.color = 1
            neighbours = neighbours + 1
        end
    end

    local mx = mouse_x() - MOUSE_W * 0.5
    local my = mouse_y() - MOUSE_H * 0.5
    local hits = root:mark_in(mx, my, mx + MOUSE_W, my + MOUSE_H, 2)

    local nodes = root:draw()
    draw_rect(mx, my, MOUSE_W, MOUSE_H, 2)
    for i = 1, n do
        local p = pts[i]
        draw_point(p.x, p.y, p.color)
    end
    set_stats(nodes, neighbours, hits)
end
