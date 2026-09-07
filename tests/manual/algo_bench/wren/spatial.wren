// Quadtree and octree. Port of py/spatial.py.
var NPOINTS = 20000
var NQUERIES = 2000
var SIZE = 1024
var CAPACITY = 8
var MAX_DEPTH = 8

var Seed = 42
var rnd = Fn.new {
  Seed = (Seed * 16807) % 2147483647
  return Seed
}

class Quad {
  construct new(x, y, w, depth) {
    _x = x
    _y = y
    _w = w
    _depth = depth
    _px = []
    _py = []
    _count = 0
    _split = false
    _c0 = null
    _c1 = null
    _c2 = null
    _c3 = null
  }
  split { _split }
  childFor(x, y) {
    var h = (_w / 2).floor
    if (y < _y + h) {
      if (x < _x + h) return _c0
      return _c1
    }
    if (x < _x + h) return _c2
    return _c3
  }
  add(x, y) {
    _px.add(x)
    _py.add(y)
    _count = _count + 1
    if (_count > CAPACITY && _depth < MAX_DEPTH) subdivide()
  }
  insert(x, y) {
    var node = this
    while (node.split) node = node.childFor(x, y)
    node.add(x, y)
  }
  subdivide() {
    var h = (_w / 2).floor
    var d = _depth + 1
    _c0 = Quad.new(_x, _y, h, d)
    _c1 = Quad.new(_x + h, _y, h, d)
    _c2 = Quad.new(_x, _y + h, h, d)
    _c3 = Quad.new(_x + h, _y + h, h, d)
    _split = true
    var px = _px
    var py = _py
    for (i in 0..._count) {
      childFor(px[i], py[i]).insert(px[i], py[i])
    }
    _px = []
    _py = []
    _count = 0
  }
  query(x0, y0, x1, y1) {
    if (x1 <= _x || y1 <= _y || x0 >= _x + _w || y0 >= _y + _w) return 0
    if (_split) {
      var n = _c0.query(x0, y0, x1, y1) + _c1.query(x0, y0, x1, y1)
      n = n + _c2.query(x0, y0, x1, y1) + _c3.query(x0, y0, x1, y1)
      return n
    }
    var n = 0
    var px = _px
    var py = _py
    for (i in 0..._count) {
      if (px[i] >= x0 && px[i] < x1 && py[i] >= y0 && py[i] < y1) n = n + 1
    }
    return n
  }
}

class Oct {
  construct new(x, y, z, w, depth) {
    _x = x
    _y = y
    _z = z
    _w = w
    _depth = depth
    _px = []
    _py = []
    _pz = []
    _count = 0
    _split = false
    _kids = null
  }
  split { _split }
  childFor(x, y, z) {
    var h = (_w / 2).floor
    var i = 0
    if (x >= _x + h) i = i + 1
    if (y >= _y + h) i = i + 2
    if (z >= _z + h) i = i + 4
    return _kids[i]
  }
  add(x, y, z) {
    _px.add(x)
    _py.add(y)
    _pz.add(z)
    _count = _count + 1
    if (_count > CAPACITY && _depth < MAX_DEPTH) subdivide()
  }
  insert(x, y, z) {
    var node = this
    while (node.split) node = node.childFor(x, y, z)
    node.add(x, y, z)
  }
  subdivide() {
    var h = (_w / 2).floor
    var d = _depth + 1
    var kids = []
    for (i in 0...8) {
      var ox = _x
      var oy = _y
      var oz = _z
      if (i % 2 == 1) ox = ox + h
      if ((i / 2).floor % 2 == 1) oy = oy + h
      if (i >= 4) oz = oz + h
      kids.add(Oct.new(ox, oy, oz, h, d))
    }
    _kids = kids
    _split = true
    var px = _px
    var py = _py
    var pz = _pz
    for (i in 0..._count) {
      childFor(px[i], py[i], pz[i]).insert(px[i], py[i], pz[i])
    }
    _px = []
    _py = []
    _pz = []
    _count = 0
  }
  query(x0, y0, z0, x1, y1, z1) {
    if (x1 <= _x || y1 <= _y || z1 <= _z) return 0
    if (x0 >= _x + _w || y0 >= _y + _w || z0 >= _z + _w) return 0
    if (_split) {
      var n = 0
      var kids = _kids
      for (i in 0...8) n = n + kids[i].query(x0, y0, z0, x1, y1, z1)
      return n
    }
    var n = 0
    var px = _px
    var py = _py
    var pz = _pz
    for (i in 0..._count) {
      if (px[i] >= x0 && px[i] < x1 && py[i] >= y0 && py[i] < y1 && pz[i] >= z0 && pz[i] < z1) n = n + 1
    }
    return n
  }
}

var runQuad = Fn.new {
  Seed = 42
  var q = Quad.new(0, 0, SIZE, 0)
  for (i in 0...NPOINTS) q.insert(rnd.call() % SIZE, rnd.call() % SIZE)
  var total = 0
  for (i in 0...NQUERIES) {
    var x = rnd.call() % SIZE
    var y = rnd.call() % SIZE
    var w = 32 + rnd.call() % 64
    total = total + q.query(x, y, x + w, y + w)
  }
  return total
}

var runOct = Fn.new {
  Seed = 42
  var o = Oct.new(0, 0, 0, SIZE, 0)
  for (i in 0...NPOINTS) o.insert(rnd.call() % SIZE, rnd.call() % SIZE, rnd.call() % SIZE)
  var total = 0
  for (i in 0...NQUERIES) {
    var x = rnd.call() % SIZE
    var y = rnd.call() % SIZE
    var z = rnd.call() % SIZE
    var w = 64 + rnd.call() % 128
    total = total + o.query(x, y, z, x + w, y + w, z + w)
  }
  return total
}

var t0 = System.clock
var a = runQuad.call()
var t1 = System.clock
var b = runOct.call()
var t2 = System.clock
System.print("quadtree checksum %(a)")
System.print("octree checksum %(b)")
System.print("quadtree %(t1 - t0)")
System.print("octree %(t2 - t1)")
