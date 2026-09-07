// Grid pathfinding: A* and Dijkstra. Port of py/pathfind.py.
var W = 128
var H = 128
var N = W * H
var BIG = 1000000000
var QUERIES = 40
var WALL_PCT = 30

var Seed = 42
var rnd = Fn.new {
  Seed = (Seed * 16807) % 2147483647
  return Seed
}
var iabs = Fn.new { |x|
  if (x < 0) return -x
  return x
}

class Heap {
  construct new() {
    _keys = []
    _vals = []
    _n = 0
  }
  n { _n }
  push(key, val) {
    var keys = _keys
    var vals = _vals
    var i = _n
    _n = i + 1
    keys.add(key)
    vals.add(val)
    while (i > 0) {
      var p = ((i - 1) / 2).floor
      if (keys[p] <= key) break
      keys[i] = keys[p]
      vals[i] = vals[p]
      i = p
    }
    keys[i] = key
    vals[i] = val
  }
  pop() {
    var keys = _keys
    var vals = _vals
    var top = vals[0]
    var lastKey = keys.removeAt(-1)
    var lastVal = vals.removeAt(-1)
    var n = _n - 1
    _n = n
    if (n > 0) {
      var i = 0
      while (true) {
        var l = 2 * i + 1
        if (l >= n) break
        var r = l + 1
        var m = l
        if (r < n && keys[r] < keys[l]) m = r
        if (keys[m] >= lastKey) break
        keys[i] = keys[m]
        vals[i] = vals[m]
        i = m
      }
      keys[i] = lastKey
      vals[i] = lastVal
    }
    return top
  }
}

var makeGrid = Fn.new {
  var blocked = List.filled(N, 0)
  for (i in 0...N) {
    if (rnd.call() % 100 < WALL_PCT) blocked[i] = 1
  }
  return blocked
}

var search = Fn.new { |blocked, start, goal, useHeur|
  var gx = goal % W
  var gy = (goal / W).floor
  var dist = List.filled(N, BIG)
  var closed = List.filled(N, 0)
  var heap = Heap.new()
  dist[start] = 0
  heap.push(0, start)
  while (heap.n > 0) {
    var cur = heap.pop()
    if (closed[cur] == 0) {
      closed[cur] = 1
      if (cur == goal) return dist[cur]
      var cx = cur % W
      var cy = (cur / W).floor
      var d = dist[cur] + 1
      if (cx > 0) {
        var nb = cur - 1
        if (blocked[nb] == 0 && d < dist[nb]) {
          dist[nb] = d
          if (useHeur) heap.push(d + iabs.call(cx - 1 - gx) + iabs.call(cy - gy), nb) else heap.push(d, nb)
        }
      }
      if (cx < W - 1) {
        var nb = cur + 1
        if (blocked[nb] == 0 && d < dist[nb]) {
          dist[nb] = d
          if (useHeur) heap.push(d + iabs.call(cx + 1 - gx) + iabs.call(cy - gy), nb) else heap.push(d, nb)
        }
      }
      if (cy > 0) {
        var nb = cur - W
        if (blocked[nb] == 0 && d < dist[nb]) {
          dist[nb] = d
          if (useHeur) heap.push(d + iabs.call(cx - gx) + iabs.call(cy - 1 - gy), nb) else heap.push(d, nb)
        }
      }
      if (cy < H - 1) {
        var nb = cur + W
        if (blocked[nb] == 0 && d < dist[nb]) {
          dist[nb] = d
          if (useHeur) heap.push(d + iabs.call(cx - gx) + iabs.call(cy + 1 - gy), nb) else heap.push(d, nb)
        }
      }
    }
  }
  return -1
}

var run = Fn.new { |useHeur|
  Seed = 42
  var blocked = makeGrid.call()
  var total = 0
  var found = 0
  for (q in 0...QUERIES) {
    var s = rnd.call() % N
    var g = rnd.call() % N
    while (blocked[s] == 1) s = rnd.call() % N
    while (blocked[g] == 1) g = rnd.call() % N
    var r = search.call(blocked, s, g, useHeur)
    if (r >= 0) {
      found = found + 1
      total = total + r
    }
  }
  return total * 1000 + found
}

var t0 = System.clock
var a = run.call(true)
var t1 = System.clock
var b = run.call(false)
var t2 = System.clock
System.print("astar checksum %(a)")
System.print("dijkstra checksum %(b)")
System.print("astar %(t1 - t0)")
System.print("dijkstra %(t2 - t1)")
