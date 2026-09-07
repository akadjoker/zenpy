-- Grid pathfinding: A* and Dijkstra. Port of py/pathfind.py (0-based ids kept;
-- arrays are 1-based so index = id + 1).
local W = 128
local H = 128
local N = W * H
local BIG = 1000000000
local QUERIES = 40
local WALL_PCT = 30

local seed = 42
local function rnd()
  seed = (seed * 16807) % 2147483647
  return seed
end

local function iabs(x)
  if x < 0 then return -x end
  return x
end

local Heap = {}
Heap.__index = Heap
function Heap.new()
  return setmetatable({keys = {}, vals = {}, n = 0}, Heap)
end
function Heap:push(key, val)
  local keys, vals = self.keys, self.vals
  local i = self.n
  self.n = i + 1
  while i > 0 do
    local p = (i - 1) // 2
    if keys[p + 1] <= key then break end
    keys[i + 1] = keys[p + 1]
    vals[i + 1] = vals[p + 1]
    i = p
  end
  keys[i + 1] = key
  vals[i + 1] = val
end
function Heap:pop()
  local keys, vals = self.keys, self.vals
  local top = vals[1]
  local n = self.n - 1
  local last_key = keys[n + 1]
  local last_val = vals[n + 1]
  keys[n + 1] = nil
  vals[n + 1] = nil
  self.n = n
  if n > 0 then
    local i = 0
    while true do
      local l = 2 * i + 1
      if l >= n then break end
      local r = l + 1
      local m = l
      if r < n and keys[r + 1] < keys[l + 1] then m = r end
      if keys[m + 1] >= last_key then break end
      keys[i + 1] = keys[m + 1]
      vals[i + 1] = vals[m + 1]
      i = m
    end
    keys[i + 1] = last_key
    vals[i + 1] = last_val
  end
  return top
end

local function make_grid()
  local blocked = {}
  for i = 1, N do blocked[i] = 0 end
  for i = 1, N do
    if rnd() % 100 < WALL_PCT then blocked[i] = 1 end
  end
  return blocked
end

local function search(blocked, start, goal, use_heur)
  local gx = goal % W
  local gy = goal // W
  local dist = {}
  local closed = {}
  for i = 1, N do dist[i] = BIG; closed[i] = 0 end
  local heap = Heap.new()
  dist[start + 1] = 0
  heap:push(0, start)
  while heap.n > 0 do
    local cur = heap:pop()
    if closed[cur + 1] == 0 then
      closed[cur + 1] = 1
      if cur == goal then return dist[cur + 1] end
      local cx = cur % W
      local cy = cur // W
      local d = dist[cur + 1] + 1
      if cx > 0 then
        local nb = cur - 1
        if blocked[nb + 1] == 0 and d < dist[nb + 1] then
          dist[nb + 1] = d
          if use_heur then heap:push(d + iabs(cx - 1 - gx) + iabs(cy - gy), nb) else heap:push(d, nb) end
        end
      end
      if cx < W - 1 then
        local nb = cur + 1
        if blocked[nb + 1] == 0 and d < dist[nb + 1] then
          dist[nb + 1] = d
          if use_heur then heap:push(d + iabs(cx + 1 - gx) + iabs(cy - gy), nb) else heap:push(d, nb) end
        end
      end
      if cy > 0 then
        local nb = cur - W
        if blocked[nb + 1] == 0 and d < dist[nb + 1] then
          dist[nb + 1] = d
          if use_heur then heap:push(d + iabs(cx - gx) + iabs(cy - 1 - gy), nb) else heap:push(d, nb) end
        end
      end
      if cy < H - 1 then
        local nb = cur + W
        if blocked[nb + 1] == 0 and d < dist[nb + 1] then
          dist[nb + 1] = d
          if use_heur then heap:push(d + iabs(cx - gx) + iabs(cy + 1 - gy), nb) else heap:push(d, nb) end
        end
      end
    end
  end
  return -1
end

local function run(use_heur)
  seed = 42
  local blocked = make_grid()
  local total = 0
  local found = 0
  for q = 1, QUERIES do
    local s = rnd() % N
    local g = rnd() % N
    while blocked[s + 1] == 1 do s = rnd() % N end
    while blocked[g + 1] == 1 do g = rnd() % N end
    local r = search(blocked, s, g, use_heur)
    if r >= 0 then
      found = found + 1
      total = total + r
    end
  end
  return total * 1000 + found
end

local t0 = os.clock()
local a = run(true)
local t1 = os.clock()
local b = run(false)
local t2 = os.clock()
print("astar checksum", a)
print("dijkstra checksum", b)
print("astar", t1 - t0)
print("dijkstra", t2 - t1)
