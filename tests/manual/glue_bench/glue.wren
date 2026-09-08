// Wren 0.4: quadtree workload, script tree vs C++ QuadTree foreign class.
foreign class QuadTree {
  construct new(w, h) {}
  foreign clear()
  foreign insert(x, y)
  foreign count(x0, y0, x1, y1)
}

var W = 1280.0
var H = 720.0
var DT = 1.0 / 60.0
var R = 6.0
var CAPACITY = 8
var MAX_DEPTH = 8

var Seed = 42
var Rnd = Fn.new {
  Seed = (Seed * 16807) % 2147483647
  return Seed
}
var RndRange = Fn.new { |lo, hi| lo + (hi - lo) * (Rnd.call() / 2147483647.0) }

class Point {
  construct new(x, y, vx, vy) {
    _x = x
    _y = y
    _vx = vx
    _vy = vy
  }
  x { _x }
  y { _y }
  x=(v) { _x = v }
  y=(v) { _y = v }
  vx { _vx }
  vy { _vy }
  vx=(v) { _vx = v }
  vy=(v) { _vy = v }
  move() {
    _x = _x + _vx * DT
    _y = _y + _vy * DT
    if (_x < 0) {
      _x = 0
      _vx = -_vx
    } else if (_x >= W) {
      _x = W - 1
      _vx = -_vx
    }
    if (_y < 0) {
      _y = 0
      _vy = -_vy
    } else if (_y >= H) {
      _y = H - 1
      _vy = -_vy
    }
  }
}

class Node {
  construct new(x, y, w, h, depth) {
    _x = x
    _y = y
    _w = w
    _h = h
    _depth = depth
    _pts = []
    _count = 0
    _split = false
    _c0 = null
    _c1 = null
    _c2 = null
    _c3 = null
  }
  split { _split }
  childFor(px, py) {
    if (py < _y + _h * 0.5) {
      if (px < _x + _w * 0.5) return _c0
      return _c1
    }
    if (px < _x + _w * 0.5) return _c2
    return _c3
  }
  add(p) {
    _pts.add(p)
    _count = _count + 1
    if (_count > CAPACITY && _depth < MAX_DEPTH) subdivide()
  }
  insert(p) {
    var node = this
    while (node.split) node = node.childFor(p.x, p.y)
    node.add(p)
  }
  subdivide() {
    var hw = _w * 0.5
    var hh = _h * 0.5
    var d = _depth + 1
    _c0 = Node.new(_x, _y, hw, hh, d)
    _c1 = Node.new(_x + hw, _y, hw, hh, d)
    _c2 = Node.new(_x, _y + hh, hw, hh, d)
    _c3 = Node.new(_x + hw, _y + hh, hw, hh, d)
    _split = true
    var old = _pts
    for (i in 0..._count) {
      var p = old[i]
      childFor(p.x, p.y).insert(p)
    }
    _pts = []
    _count = 0
  }
  countIn(x0, y0, x1, y1) {
    if (x1 <= _x || y1 <= _y || x0 >= _x + _w || y0 >= _y + _h) return 0
    if (_split) {
      var n = _c0.countIn(x0, y0, x1, y1) + _c1.countIn(x0, y0, x1, y1)
      return n + _c2.countIn(x0, y0, x1, y1) + _c3.countIn(x0, y0, x1, y1)
    }
    var n = 0
    var pts = _pts
    for (i in 0..._count) {
      var p = pts[i]
      if (p.x >= x0 && p.x < x1 && p.y >= y0 && p.y < y1) n = n + 1
    }
    return n
  }
}

class Game {
  static makePoints(n) {
    Seed = 42
    var pts = []
    for (i in 0...n) {
      pts.add(Point.new(RndRange.call(0, W), RndRange.call(0, H), RndRange.call(-60, 60), RndRange.call(-60, 60)))
    }
    return pts
  }

  static runScript(n, frames) {
    var pts = makePoints(n)
    var checksum = 0
    for (f in 0...frames) {
      var root = Node.new(0.0, 0.0, W, H, 0)
      for (i in 0...n) {
        var p = pts[i]
        p.move()
        root.insert(p)
      }
      for (i in 0...n) {
        var p = pts[i]
        if (root.countIn(p.x - R, p.y - R, p.x + R, p.y + R) > 1) checksum = checksum + 1
      }
    }
    return checksum
  }

  static runNative(n, frames) {
    var pts = makePoints(n)
    var tree = QuadTree.new(W, H)
    var checksum = 0
    for (f in 0...frames) {
      tree.clear()
      for (i in 0...n) {
        var p = pts[i]
        p.move()
        tree.insert(p.x, p.y)
      }
      for (i in 0...n) {
        var p = pts[i]
        if (tree.count(p.x - R, p.y - R, p.x + R, p.y + R) > 1) checksum = checksum + 1
      }
    }
    return checksum
  }
}
