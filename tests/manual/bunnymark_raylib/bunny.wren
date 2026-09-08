// Wren 0.4 bunnymark script (same shape as bunny.zen).
class Native {
  foreign static drawBunny(x, y)
  foreign static rand(lo, hi)
  foreign static screenWidth
  foreign static screenHeight
}

class Bunny {
  construct new(x, y, vx, vy) {
    _x = x
    _y = y
    _vx = vx
    _vy = vy
  }

  update(dt) {
    _x = _x + _vx * dt
    _y = _y + _vy * dt
    if (_x > Game.maxX || _x < 0) _vx = -_vx
    if (_y > Game.maxY || _y < 0) _vy = -_vy
    Native.drawBunny(_x, _y)
  }
}

class Game {
  static init() {
    __bunnies = []
    __maxX = Native.screenWidth - 26
    __maxY = Native.screenHeight - 37
  }
  static maxX { __maxX }
  static maxY { __maxY }

  static addBunnies(n) {
    for (i in 0...n) {
      __bunnies.add(Bunny.new(Native.rand(0, __maxX), Native.rand(0, __maxY),
                              Native.rand(-250, 250), Native.rand(-250, 250)))
    }
  }

  static updateAll(dt) {
    for (b in __bunnies) b.update(dt)
  }
}

Game.init()
