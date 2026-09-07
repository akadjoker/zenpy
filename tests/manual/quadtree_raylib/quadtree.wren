// Wren 0.4 quadtree demo (same shape as quadtree.zen).
class Native {
  foreign static drawPoint(x, y, color)
  foreign static drawRect(x, y, w, h, color)
  foreign static setStats(nodes, neighbours, hits)
  foreign static mouseX
  foreign static mouseY
  foreign static rand(lo, hi)
  foreign static screenWidth
  foreign static screenHeight
}

var CAPACITY = 8
var MAX_DEPTH = 8
var RADIUS = 6
var MOUSE_W = 160
var MOUSE_H = 120

class Point {
  construct new(x, y, vx, vy) {
    _x = x
    _y = y
    _vx = vx
    _vy = vy
    _color = 0
  }
  x { _x }
  y { _y }
  x=(v) { _x = v }
  y=(v) { _y = v }
  vx { _vx }
  vy { _vy }
  vx=(v) { _vx = v }
  vy=(v) { _vy = v }
  color { _color }
  color=(v) { _color = v }
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
      n = n + _c2.countIn(x0, y0, x1, y1) + _c3.countIn(x0, y0, x1, y1)
      return n
    }
    var n = 0
    var pts = _pts
    for (i in 0..._count) {
      var p = pts[i]
      if (p.x >= x0 && p.x < x1 && p.y >= y0 && p.y < y1) n = n + 1
    }
    return n
  }

  markIn(x0, y0, x1, y1, color) {
    if (x1 <= _x || y1 <= _y || x0 >= _x + _w || y0 >= _y + _h) return 0
    if (_split) {
      var n = _c0.markIn(x0, y0, x1, y1, color) + _c1.markIn(x0, y0, x1, y1, color)
      n = n + _c2.markIn(x0, y0, x1, y1, color) + _c3.markIn(x0, y0, x1, y1, color)
      return n
    }
    var n = 0
    var pts = _pts
    for (i in 0..._count) {
      var p = pts[i]
      if (p.x >= x0 && p.x < x1 && p.y >= y0 && p.y < y1) {
        p.color = color
        n = n + 1
      }
    }
    return n
  }

  draw() {
    Native.drawRect(_x, _y, _w, _h, 3)
    if (!_split) return 1
    return 1 + _c0.draw() + _c1.draw() + _c2.draw() + _c3.draw()
  }
}

class Game {
  static init() {
    __points = []
    __w = Native.screenWidth
    __h = Native.screenHeight
  }

  static addPoints(n, x, y) {
    for (i in 0...n) {
      if (x < 0) {
        __points.add(Point.new(Native.rand(0, __w), Native.rand(0, __h), Native.rand(-60, 60), Native.rand(-60, 60)))
      } else {
        __points.add(Point.new(x, y, Native.rand(-60, 60), Native.rand(-60, 60)))
      }
    }
  }

  static updateAll(dt) {
    var pts = __points
    var n = pts.count
    var w = __w
    var h = __h
    var root = Node.new(0, 0, w, h, 0)

    for (i in 0...n) {
      var p = pts[i]
      p.x = p.x + p.vx * dt
      p.y = p.y + p.vy * dt
      if (p.x < 0) {
        p.x = 0
        p.vx = -p.vx
      } else if (p.x >= w) {
        p.x = w - 1
        p.vx = -p.vx
      }
      if (p.y < 0) {
        p.y = 0
        p.vy = -p.vy
      } else if (p.y >= h) {
        p.y = h - 1
        p.vy = -p.vy
      }
      p.color = 0
      root.insert(p)
    }

    var neighbours = 0
    for (i in 0...n) {
      var p = pts[i]
      if (root.countIn(p.x - RADIUS, p.y - RADIUS, p.x + RADIUS, p.y + RADIUS) > 1) {
        p.color = 1
        neighbours = neighbours + 1
      }
    }

    var mx = Native.mouseX - MOUSE_W * 0.5
    var my = Native.mouseY - MOUSE_H * 0.5
    var hits = root.markIn(mx, my, mx + MOUSE_W, my + MOUSE_H, 2)

    var nodes = root.draw()
    Native.drawRect(mx, my, MOUSE_W, MOUSE_H, 2)
    for (i in 0...n) {
      var p = pts[i]
      Native.drawPoint(p.x, p.y, p.color)
    }
    Native.setStats(nodes, neighbours, hits)
  }
}

Game.init()
