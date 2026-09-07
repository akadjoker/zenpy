// Wren 0.4 texmark script (same shape as sprites.zen).
foreign class Texture {
  construct new(path) {}
  foreign draw(x, y)
  foreign width
  foreign height
}

class Native {
  foreign static drawTexture(tex, x, y)
  foreign static rand(lo, hi)
  foreign static screenWidth
  foreign static screenHeight
}

class Sprite {
  construct new(tex, x, y, vx, vy) {
    _tex = tex
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
    _tex.draw(_x, _y)
  }
}

class Game {
  static init() {
    __wabbit = Texture.new("wabbit_alpha.png")
    __sprites = []
    __maxX = Native.screenWidth - __wabbit.width
    __maxY = Native.screenHeight - __wabbit.height
  }
  static maxX { __maxX }
  static maxY { __maxY }

  static addSprites(n) {
    for (i in 0...n) {
      __sprites.add(Sprite.new(__wabbit, Native.rand(0, __maxX), Native.rand(0, __maxY),
                               Native.rand(-250, 250), Native.rand(-250, 250)))
    }
  }

  static updateAll(dt) {
    for (s in __sprites) s.update(dt)
  }
}

Game.init()
