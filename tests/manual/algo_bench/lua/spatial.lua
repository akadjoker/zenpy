-- Quadtree and octree. Port of py/spatial.py.
local NPOINTS = 20000
local NQUERIES = 2000
local SIZE = 1024
local CAPACITY = 8
local MAX_DEPTH = 8

local seed = 42
local function rnd()
  seed = (seed * 16807) % 2147483647
  return seed
end

local Quad = {}
Quad.__index = Quad
function Quad.new(x, y, w, depth)
  return setmetatable({x = x, y = y, w = w, depth = depth, px = {}, py = {},
    count = 0, split = false, c0 = nil, c1 = nil, c2 = nil, c3 = nil}, Quad)
end
function Quad:child_for(x, y)
  local h = self.w // 2
  if y < self.y + h then
    if x < self.x + h then return self.c0 end
    return self.c1
  end
  if x < self.x + h then return self.c2 end
  return self.c3
end
function Quad:insert(x, y)
  local node = self
  while node.split do node = node:child_for(x, y) end
  local c = node.count + 1
  node.px[c] = x
  node.py[c] = y
  node.count = c
  if c > CAPACITY and node.depth < MAX_DEPTH then node:subdivide() end
end
function Quad:subdivide()
  local h = self.w // 2
  local d = self.depth + 1
  self.c0 = Quad.new(self.x, self.y, h, d)
  self.c1 = Quad.new(self.x + h, self.y, h, d)
  self.c2 = Quad.new(self.x, self.y + h, h, d)
  self.c3 = Quad.new(self.x + h, self.y + h, h, d)
  self.split = true
  local px, py = self.px, self.py
  for i = 1, self.count do
    self:child_for(px[i], py[i]):insert(px[i], py[i])
  end
  self.px = {}
  self.py = {}
  self.count = 0
end
function Quad:query(x0, y0, x1, y1)
  if x1 <= self.x or y1 <= self.y or x0 >= self.x + self.w or y0 >= self.y + self.w then return 0 end
  if self.split then
    local n = self.c0:query(x0, y0, x1, y1) + self.c1:query(x0, y0, x1, y1)
    n = n + self.c2:query(x0, y0, x1, y1) + self.c3:query(x0, y0, x1, y1)
    return n
  end
  local n = 0
  local px, py = self.px, self.py
  for i = 1, self.count do
    if px[i] >= x0 and px[i] < x1 and py[i] >= y0 and py[i] < y1 then n = n + 1 end
  end
  return n
end

local Oct = {}
Oct.__index = Oct
function Oct.new(x, y, z, w, depth)
  return setmetatable({x = x, y = y, z = z, w = w, depth = depth, px = {}, py = {}, pz = {},
    count = 0, split = false, kids = nil}, Oct)
end
function Oct:child_for(x, y, z)
  local h = self.w // 2
  local i = 0
  if x >= self.x + h then i = i + 1 end
  if y >= self.y + h then i = i + 2 end
  if z >= self.z + h then i = i + 4 end
  return self.kids[i + 1]
end
function Oct:insert(x, y, z)
  local node = self
  while node.split do node = node:child_for(x, y, z) end
  local c = node.count + 1
  node.px[c] = x
  node.py[c] = y
  node.pz[c] = z
  node.count = c
  if c > CAPACITY and node.depth < MAX_DEPTH then node:subdivide() end
end
function Oct:subdivide()
  local h = self.w // 2
  local d = self.depth + 1
  local kids = {}
  for i = 0, 7 do
    local ox, oy, oz = self.x, self.y, self.z
    if i % 2 == 1 then ox = ox + h end
    if (i // 2) % 2 == 1 then oy = oy + h end
    if i >= 4 then oz = oz + h end
    kids[i + 1] = Oct.new(ox, oy, oz, h, d)
  end
  self.kids = kids
  self.split = true
  local px, py, pz = self.px, self.py, self.pz
  for i = 1, self.count do
    self:child_for(px[i], py[i], pz[i]):insert(px[i], py[i], pz[i])
  end
  self.px = {}
  self.py = {}
  self.pz = {}
  self.count = 0
end
function Oct:query(x0, y0, z0, x1, y1, z1)
  if x1 <= self.x or y1 <= self.y or z1 <= self.z then return 0 end
  if x0 >= self.x + self.w or y0 >= self.y + self.w or z0 >= self.z + self.w then return 0 end
  if self.split then
    local n = 0
    local kids = self.kids
    for i = 1, 8 do n = n + kids[i]:query(x0, y0, z0, x1, y1, z1) end
    return n
  end
  local n = 0
  local px, py, pz = self.px, self.py, self.pz
  for i = 1, self.count do
    if px[i] >= x0 and px[i] < x1 and py[i] >= y0 and py[i] < y1 and pz[i] >= z0 and pz[i] < z1 then
      n = n + 1
    end
  end
  return n
end

local function run_quad()
  seed = 42
  local q = Quad.new(0, 0, SIZE, 0)
  for i = 1, NPOINTS do q:insert(rnd() % SIZE, rnd() % SIZE) end
  local total = 0
  for i = 1, NQUERIES do
    local x = rnd() % SIZE
    local y = rnd() % SIZE
    local w = 32 + rnd() % 64
    total = total + q:query(x, y, x + w, y + w)
  end
  return total
end

local function run_oct()
  seed = 42
  local o = Oct.new(0, 0, 0, SIZE, 0)
  for i = 1, NPOINTS do o:insert(rnd() % SIZE, rnd() % SIZE, rnd() % SIZE) end
  local total = 0
  for i = 1, NQUERIES do
    local x = rnd() % SIZE
    local y = rnd() % SIZE
    local z = rnd() % SIZE
    local w = 64 + rnd() % 128
    total = total + o:query(x, y, z, x + w, y + w, z + w)
  end
  return total
end

local t0 = os.clock()
local a = run_quad()
local t1 = os.clock()
local b = run_oct()
local t2 = os.clock()
print("quadtree checksum", a)
print("octree checksum", b)
print("quadtree", t1 - t0)
print("octree", t2 - t1)
