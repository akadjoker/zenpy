-- Towers of Hanoi and flood fill. Port of py/hanoi.py.
local DISKS = 20
local W = 256
local H = 256
local N = W * H
local WALL_PCT = 35
local SEEDS = 60

local seed = 42
local function rnd()
  seed = (seed * 16807) % 2147483647
  return seed
end

local Pegs = {}
Pegs.__index = Pegs
function Pegs.new() return setmetatable({moves = 0, a = 0, b = 0, c = 0}, Pegs) end
function Pegs:move(n, src, dst, tmp)
  if n == 1 then
    self.moves = self.moves + 1
    return
  end
  self:move(n - 1, src, tmp, dst)
  self.moves = self.moves + 1
  self:move(n - 1, tmp, dst, src)
end

local function hanoi_plain(n, src, dst, tmp)
  if n == 1 then return 1 end
  return hanoi_plain(n - 1, src, tmp, dst) + 1 + hanoi_plain(n - 1, tmp, dst, src)
end

local function run_hanoi()
  local p = Pegs.new()
  p:move(DISKS, 0, 2, 1)
  return p.moves + hanoi_plain(DISKS - 2, 0, 2, 1)
end

local function make_grid()
  local cells = {}
  for i = 1, N do cells[i] = 0 end
  for i = 1, N do
    if rnd() % 100 < WALL_PCT then cells[i] = 1 end
  end
  return cells
end

local function flood(cells, start, color)
  if cells[start + 1] ~= 0 then return 0 end
  local stack = {start}
  local sp = 1
  cells[start + 1] = color
  local painted = 0
  while sp > 0 do
    local cur = stack[sp]
    stack[sp] = nil
    sp = sp - 1
    painted = painted + 1
    local x = cur % W
    if x > 0 and cells[cur] == 0 then
      cells[cur] = color
      sp = sp + 1; stack[sp] = cur - 1
    end
    if x < W - 1 and cells[cur + 2] == 0 then
      cells[cur + 2] = color
      sp = sp + 1; stack[sp] = cur + 1
    end
    if cur >= W and cells[cur - W + 1] == 0 then
      cells[cur - W + 1] = color
      sp = sp + 1; stack[sp] = cur - W
    end
    if cur < N - W and cells[cur + W + 1] == 0 then
      cells[cur + W + 1] = color
      sp = sp + 1; stack[sp] = cur + W
    end
  end
  return painted
end

local function run_flood()
  seed = 42
  local cells = make_grid()
  local total = 0
  local regions = 0
  for s = 0, SEEDS - 1 do
    local n = flood(cells, rnd() % N, 2 + s)
    if n > 0 then
      regions = regions + 1
      total = total + n
    end
  end
  return total * 100 + regions
end

local t0 = os.clock()
local a = run_hanoi()
local t1 = os.clock()
local b = run_flood()
local t2 = os.clock()
print("hanoi checksum", a)
print("floodfill checksum", b)
print("hanoi", t1 - t0)
print("floodfill", t2 - t1)
