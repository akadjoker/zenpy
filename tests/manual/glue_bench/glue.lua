-- Lua 5.4: quadtree workload, script tree vs C++ QuadTree userdata.
local W = 1280.0
local H = 720.0
local DT = 1.0 / 60.0
local R = 6.0
local CAPACITY = 8
local MAX_DEPTH = 8

local seed = 42
local function rnd()
    seed = (seed * 16807) % 2147483647
    return seed
end
local function rnd_range(lo, hi) return lo + (hi - lo) * (rnd() / 2147483647.0) end

local Node = {}
Node.__index = Node
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
    local hw, hh, d = self.w * 0.5, self.h * 0.5, self.depth + 1
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
        return n + self.c2:count_in(x0, y0, x1, y1) + self.c3:count_in(x0, y0, x1, y1)
    end
    local n = 0
    local pts = self.pts
    for i = 1, self.count do
        local p = pts[i]
        if p.x >= x0 and p.x < x1 and p.y >= y0 and p.y < y1 then n = n + 1 end
    end
    return n
end

local function make_points(n)
    seed = 42
    local pts = {}
    for i = 1, n do
        pts[i] = { x = rnd_range(0, W), y = rnd_range(0, H), vx = rnd_range(-60, 60), vy = rnd_range(-60, 60) }
    end
    return pts
end

local function move(p)
    p.x = p.x + p.vx * DT
    p.y = p.y + p.vy * DT
    if p.x < 0 then p.x = 0; p.vx = -p.vx
    elseif p.x >= W then p.x = W - 1; p.vx = -p.vx end
    if p.y < 0 then p.y = 0; p.vy = -p.vy
    elseif p.y >= H then p.y = H - 1; p.vy = -p.vy end
end

function run_script(n, frames)
    local pts = make_points(n)
    local checksum = 0
    for f = 1, frames do
        local root = Node.new(0.0, 0.0, W, H, 0)
        for i = 1, n do
            local p = pts[i]
            move(p)
            root:insert(p)
        end
        for i = 1, n do
            local p = pts[i]
            if root:count_in(p.x - R, p.y - R, p.x + R, p.y + R) > 1 then checksum = checksum + 1 end
        end
    end
    return checksum
end

function run_native(n, frames)
    local pts = make_points(n)
    local tree = QuadTree(W, H)
    local checksum = 0
    for f = 1, frames do
        tree:clear()
        for i = 1, n do
            local p = pts[i]
            move(p)
            tree:insert(p.x, p.y)
        end
        for i = 1, n do
            local p = pts[i]
            if tree:count(p.x - R, p.y - R, p.x + R, p.y + R) > 1 then checksum = checksum + 1 end
        end
    end
    return checksum
end
