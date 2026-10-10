-- Game-shaped workloads. Port of py/gamebench.py.
local NBODIES = 1000
local NFRAMES = 60
local NSPRITES = 4000
local WORLD = 2048

local seed = 42
local function rnd()
  seed = (seed * 16807) % 2147483647
  return seed
end

local function make_poly(n, radius)
  local pts = {}
  for i = 0, n - 1 do
    local a = 6.28318530718 * i / n
    local c, s = 1.0, a
    local a2 = a * a
    local term_c, term_s = 1.0, a
    for k = 1, 5 do
      term_c = -term_c * a2 / ((2 * k - 1) * (2 * k))
      c = c + term_c
      term_s = -term_s * a2 / ((2 * k) * (2 * k + 1))
      s = s + term_s
    end
    pts[i + 1] = {c * radius, s * radius}
  end
  return pts
end

local Body = {}
Body.__index = Body
function Body.new(px, py, angle, vx, vy, spin, poly)
  local b = setmetatable({px = px, py = py, angle = angle, vx = vx, vy = vy,
    spin = spin, poly = poly, wx = {}, wy = {},
    minx = 0.0, miny = 0.0, maxx = 0.0, maxy = 0.0}, Body)
  for i = 1, #poly do
    b.wx[i] = 0.0
    b.wy[i] = 0.0
  end
  return b
end

function Body:integrate(dt)
  self.px = self.px + self.vx * dt
  self.py = self.py + self.vy * dt
  self.angle = self.angle + self.spin * dt
  if self.px < 0.0 then self.px = self.px + WORLD end
  if self.px > WORLD then self.px = self.px - WORLD end
  if self.py < 0.0 then self.py = self.py + WORLD end
  if self.py > WORLD then self.py = self.py - WORLD end
end

function Body:transform()
  local a = self.angle
  while a > 3.14159265359 do a = a - 6.28318530718 end
  while a < -3.14159265359 do a = a + 6.28318530718 end
  local a2 = a * a
  local c, s = 1.0, a
  local term_c, term_s = 1.0, a
  for k = 1, 5 do
    term_c = -term_c * a2 / ((2 * k - 1) * (2 * k))
    c = c + term_c
    term_s = -term_s * a2 / ((2 * k) * (2 * k + 1))
    s = s + term_s
  end
  local n = #self.poly
  local minx, miny = 1000000.0, 1000000.0
  local maxx, maxy = -1000000.0, -1000000.0
  for i = 1, n do
    local p = self.poly[i]
    local x = p[1] * c - p[2] * s + self.px
    local y = p[1] * s + p[2] * c + self.py
    self.wx[i] = x
    self.wy[i] = y
    if x < minx then minx = x end
    if x > maxx then maxx = x end
    if y < miny then miny = y end
    if y > maxy then maxy = y end
  end
  self.minx, self.miny, self.maxx, self.maxy = minx, miny, maxx, maxy
end

local function project(wx, wy, n, ax, ay)
  local lo = wx[1] * ax + wy[1] * ay
  local hi = lo
  for i = 2, n do
    local d = wx[i] * ax + wy[i] * ay
    if d < lo then lo = d end
    if d > hi then hi = d end
  end
  return lo, hi
end

local function sat_overlap(a, b)
  local na, nb = #a.wx, #b.wx
  for i = 1, na do
    local j = i + 1
    if j > na then j = 1 end
    local ex = a.wx[j] - a.wx[i]
    local ey = a.wy[j] - a.wy[i]
    local ax, ay = -ey, ex
    local alo, ahi = project(a.wx, a.wy, na, ax, ay)
    local blo, bhi = project(b.wx, b.wy, nb, ax, ay)
    if ahi < blo or bhi < alo then return false end
  end
  for i = 1, nb do
    local j = i + 1
    if j > nb then j = 1 end
    local ex = b.wx[j] - b.wx[i]
    local ey = b.wy[j] - b.wy[i]
    local ax, ay = -ey, ex
    local alo, ahi = project(a.wx, a.wy, na, ax, ay)
    local blo, bhi = project(b.wx, b.wy, nb, ax, ay)
    if ahi < blo or bhi < alo then return false end
  end
  return true
end

local function run_physics()
  local shapes = {make_poly(3, 12.0), make_poly(4, 14.0), make_poly(5, 11.0),
                  make_poly(6, 13.0), make_poly(8, 10.0)}
  local bodies = {}
  for i = 1, NBODIES do
    local px = (rnd() % WORLD) * 1.0
    local py = (rnd() % WORLD) * 1.0
    local ang = (rnd() % 628) / 100.0
    local vx = (rnd() % 200 - 100) / 2.0
    local vy = (rnd() % 200 - 100) / 2.0
    local spin = (rnd() % 200 - 100) / 100.0
    bodies[i] = Body.new(px, py, ang, vx, vy, spin, shapes[rnd() % 5 + 1])
  end

  local hits = 0
  local dt = 0.016
  for _ = 1, NFRAMES do
    for i = 1, NBODIES do
      local b = bodies[i]
      b:integrate(dt)
      b:transform()
    end

    local order = {}
    for i = 1, NBODIES do order[i] = bodies[i] end
    table.sort(order, function(p, q) return p.minx < q.minx end)

    for i = 1, NBODIES do
      local a = order[i]
      local amaxx = a.maxx
      local j = i + 1
      while j <= NBODIES do
        local b = order[j]
        if b.minx > amaxx then
          j = NBODIES + 1
        else
          if a.miny <= b.maxy and b.miny <= a.maxy then
            if sat_overlap(a, b) then hits = hits + 1 end
          end
          j = j + 1
        end
      end
    end
  end
  return hits
end

local World = {}
World.__index = World
function World.new()
  return setmetatable({px = {}, py = {}, vx = {}, vy = {}, life = {},
    frame = {}, alive = {}, count = 0}, World)
end

function World:spawn(px, py, vx, vy, life)
  local n = self.count + 1
  self.px[n] = px
  self.py[n] = py
  self.vx[n] = vx
  self.vy[n] = vy
  self.life[n] = life
  self.frame[n] = 0
  self.alive[n] = 1
  self.count = n
end

function World:system_move(dt)
  for i = 1, self.count do
    if self.alive[i] == 1 then
      self.px[i] = self.px[i] + self.vx[i] * dt
      self.py[i] = self.py[i] + self.vy[i] * dt
      if self.px[i] < 0.0 or self.px[i] > WORLD then self.vx[i] = -self.vx[i] end
      if self.py[i] < 0.0 or self.py[i] > WORLD then self.vy[i] = -self.vy[i] end
    end
  end
end

function World:system_animate()
  local acc = 0
  for i = 1, self.count do
    if self.alive[i] == 1 then
      local f = self.frame[i] + 1
      if f >= 8 then f = 0 end
      self.frame[i] = f
      acc = acc + f
    end
  end
  return acc
end

function World:system_lifetime()
  local killed = 0
  for i = 1, self.count do
    if self.alive[i] == 1 then
      local l = self.life[i] - 1
      self.life[i] = l
      if l <= 0 then
        self.alive[i] = 0
        killed = killed + 1
      end
    end
  end
  return killed
end

local function run_ecs()
  local w = World.new()
  for _ = 1, NSPRITES do
    w:spawn((rnd() % WORLD) * 1.0, (rnd() % WORLD) * 1.0,
            (rnd() % 100 - 50) * 1.0, (rnd() % 100 - 50) * 1.0,
            20 + rnd() % 40)
  end

  local total = 0
  for frame = 0, NFRAMES - 1 do
    w:system_move(0.016)
    total = total + w:system_animate()
    total = total + w:system_lifetime()
    if frame % 10 == 0 then
      for _ = 1, 200 do
        w:spawn((rnd() % WORLD) * 1.0, (rnd() % WORLD) * 1.0,
                (rnd() % 100 - 50) * 1.0, (rnd() % 100 - 50) * 1.0,
                20 + rnd() % 40)
      end
    end
  end
  return total
end

local t0 = os.clock()
local a = run_physics()
local t1 = os.clock()
local b = run_ecs()
local t2 = os.clock()
print("physics checksum", a)
print("ecs checksum", b)
print("physics", t1 - t0)
print("ecs", t2 - t1)
