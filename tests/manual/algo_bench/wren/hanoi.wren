// Towers of Hanoi and flood fill. Port of py/hanoi.py.
var DISKS = 20
var W = 256
var H = 256
var N = W * H
var WALL_PCT = 35
var SEEDS = 60

var Seed = 42
var rnd = Fn.new {
  Seed = (Seed * 16807) % 2147483647
  return Seed
}

class Pegs {
  construct new() {
    _moves = 0
    _a = 0
    _b = 0
    _c = 0
  }
  moves { _moves }
  move(n, src, dst, tmp) {
    if (n == 1) {
      _moves = _moves + 1
      return
    }
    move(n - 1, src, tmp, dst)
    _moves = _moves + 1
    move(n - 1, tmp, dst, src)
  }
}

class Hanoi {
  static plain(n, src, dst, tmp) {
    if (n == 1) return 1
    return plain(n - 1, src, tmp, dst) + 1 + plain(n - 1, tmp, dst, src)
  }
}

var runHanoi = Fn.new {
  var p = Pegs.new()
  p.move(DISKS, 0, 2, 1)
  return p.moves + Hanoi.plain(DISKS - 2, 0, 2, 1)
}

var makeGrid = Fn.new {
  var cells = List.filled(N, 0)
  for (i in 0...N) {
    if (rnd.call() % 100 < WALL_PCT) cells[i] = 1
  }
  return cells
}

var flood = Fn.new { |cells, start, color|
  if (cells[start] != 0) return 0
  var stack = [start]
  cells[start] = color
  var painted = 0
  while (stack.count > 0) {
    var cur = stack.removeAt(-1)
    painted = painted + 1
    var x = cur % W
    if (x > 0 && cells[cur - 1] == 0) {
      cells[cur - 1] = color
      stack.add(cur - 1)
    }
    if (x < W - 1 && cells[cur + 1] == 0) {
      cells[cur + 1] = color
      stack.add(cur + 1)
    }
    if (cur >= W && cells[cur - W] == 0) {
      cells[cur - W] = color
      stack.add(cur - W)
    }
    if (cur < N - W && cells[cur + W] == 0) {
      cells[cur + W] = color
      stack.add(cur + W)
    }
  }
  return painted
}

var runFlood = Fn.new {
  Seed = 42
  var cells = makeGrid.call()
  var total = 0
  var regions = 0
  for (s in 0...SEEDS) {
    var n = flood.call(cells, rnd.call() % N, 2 + s)
    if (n > 0) {
      regions = regions + 1
      total = total + n
    }
  }
  return total * 100 + regions
}

var t0 = System.clock
var a = runHanoi.call()
var t1 = System.clock
var b = runFlood.call()
var t2 = System.clock
System.print("hanoi checksum %(a)")
System.print("floodfill checksum %(b)")
System.print("hanoi %(t1 - t0)")
System.print("floodfill %(t2 - t1)")
