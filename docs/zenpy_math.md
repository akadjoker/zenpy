# Zen Math Library Reference

**Complete reference for the math module with 31 functions and 5 constants.**

Import with: `import math` or `from math import sin, cos, pi`

---

## Trigonometric Functions

All angles are in **degrees** (not radians).

| Function | Arity | Description |
|----------|-------|-------------|
| `sin` | 1 | Sine of angle (degrees). Returns float [-1, 1]. |
| `cos` | 1 | Cosine of angle (degrees). Returns float [-1, 1]. |
| `tan` | 1 | Tangent of angle (degrees). |
| `asin` | 1 | Inverse sine. Returns angle in degrees [-90, 90]. |
| `acos` | 1 | Inverse cosine. Returns angle in degrees [0, 180]. |
| `atan` | 1 | Inverse tangent. Returns angle in degrees [-90, 90]. |
| `atan2` | 2 | Two-argument inverse tangent: `atan2(y, x)`. Returns angle in degrees [-180, 180]. |

**Example:**
```python
import math

# Basic trig
sin_45 = math.sin(45)        # → 0.707...
cos_0 = math.cos(0)          # → 1.0
angle = math.asin(0.707)     # → 45

# Angle from coordinates
angle = math.atan2(3, 4)     # → ~36.87° (arctan of 3/4)

# Normalize to [-180, 180]
angle = math.atan2(y, x)
```

---

## Exponential & Logarithmic

| Function | Arity | Description |
|----------|-------|-------------|
| `exp` | 1 | e^x (exponential). |
| `log` | 1 | Natural logarithm (base e). |
| `log2` | 1 | Base-2 logarithm. |
| `log10` | 1 | Base-10 logarithm. |

**Example:**
```python
import math

math.exp(1)       # → e ≈ 2.718
math.log(2.718)   # → 1.0
math.log2(8)      # → 3 (2^3 = 8)
math.log10(100)   # → 2 (10^2 = 100)
```

---

## Power & Roots

| Function | Arity | Description |
|----------|-------|-------------|
| `sqrt` | 1 | Square root. |
| `cbrt` | 1 | Cube root. |
| `pow` | 2 | Power: `pow(base, exponent)`. |
| `abs` | 1 | Absolute value (works on all numbers). |

**Example:**
```python
import math

math.sqrt(9)      # → 3.0
math.cbrt(27)     # → 3.0
math.pow(2, 3)    # → 8.0 (2^3)
math.pow(16, 0.5) # → 4.0 (square root via pow)
math.abs(-42)     # → 42
math.abs(3.14)    # → 3.14
```

---

## Rounding

| Function | Arity | Description |
|----------|-------|-------------|
| `ceil` | 1 | Round up to nearest integer. |
| `floor` | 1 | Round down to nearest integer. |
| `round` | 1 | Round to nearest integer. |

**Example:**
```python
import math

math.ceil(3.2)    # → 4.0
math.floor(3.8)   # → 3.0
math.round(3.5)   # → 4.0
math.round(3.4)   # → 3.0
```

---

## Min, Max, Clamp

| Function | Arity | Description |
|----------|-------|-------------|
| `min` | 2 | Return smaller of two values. |
| `max` | 2 | Return larger of two values. |
| `clamp` | 3 | Constrain value to range: `clamp(value, min, max)`. |

**Example:**
```python
import math

math.min(3, 5)           # → 3
math.max(3, 5)           # → 5
math.clamp(7, 0, 5)      # → 5 (clamped to max)
math.clamp(-2, 0, 5)     # → 0 (clamped to min)
math.clamp(3, 0, 5)      # → 3 (within range)
```

---

## Interpolation

### Linear Interpolation

| Function | Arity | Description |
|----------|-------|-------------|
| `lerp` | 3 | Linear interpolation: `lerp(a, b, t)` where t ∈ [0, 1]. |

**Example:**
```python
import math

start = 0
end = 100
progress = 0.5

current = math.lerp(start, end, progress)  # → 50 (halfway)

# Animation over time
duration = 2.0  # seconds
elapsed = 0.5
t = elapsed / duration
pos = math.lerp(0, 200, t)  # Animate from 0 to 200 over 2s
```

### Smooth Interpolation

| Function | Arity | Description |
|----------|-------|-------------|
| `smooth_step` | 3 | Smooth interpolation with easing: `smooth_step(a, b, t)`. Same signature as lerp but with smooth acceleration/deceleration. |
| `hermite` | 5 | Hermite spline interpolation: `hermite(v1, tang1, v2, tang2, t)`. More advanced smooth curve with tangent control. |

**Example:**
```python
import math

# Smooth easing (vs linear)
t = 0.5
linear = math.lerp(0, 100, t)           # → 50 (sharp)
smooth = math.smooth_step(0, 100, t)    # → 50 (smooth S-curve)

# Hermite for path following
v1 = 0       # start value
tang1 = 10   # start tangent (velocity)
v2 = 100     # end value
tang2 = 5    # end tangent
t = 0.5
curve = math.hermite(v1, tang1, v2, tang2, t)
```

---

## Angle Functions

| Function | Arity | Description |
|----------|-------|-------------|
| `angle_delta` | 2 | Shortest angle difference: `angle_delta(from, to)`. Returns [-180, 180]. |
| `lerp_angle` | 3 | Linear interpolation for angles: `lerp_angle(current, target, t)`. Handles angle wrapping. |
| `ping_pong` | 2 | Oscillate between 0 and length: `ping_pong(t, length)`. |

**Example:**
```python
import math

# Shortest angle between two directions
delta = math.angle_delta(350, 10)  # → 20 (not 340, but shortest path)
delta = math.angle_delta(10, 350)  # → -20

# Smooth rotation toward target
current_rotation = 0
target_rotation = 90
t = 0.5

new_rotation = math.lerp_angle(current_rotation, target_rotation, t)  # → 45

# Oscillating value (for animations)
time = 2.0
length = 1.0
value = math.ping_pong(time, length)  # Oscillates 0 → 1 → 0 → 1 → ...
```

---

## Utilities

| Function | Arity | Description |
|----------|-------|-------------|
| `hypot` | 2 | Hypotenuse: √(a² + b²). For distance calculations. |
| `random` | 0 | Random float in [0, 1). |
| `isnan` | 1 | Check if value is NaN. |
| `isinf` | 1 | Check if value is infinite. |

**Example:**
```python
import math

# Distance from origin
dx = 3
dy = 4
distance = math.hypot(dx, dy)  # → 5.0

# Random number
rand = math.random()           # → 0.0 to 0.999...
rand_0_to_100 = math.random() * 100

# Check special floats
if math.isnan(result):
    print("Invalid calculation")
if math.isinf(result):
    print("Division by zero or overflow")
```

---

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `pi` | 3.14159... | π (ratio of circle circumference to diameter). |
| `e` | 2.71828... | Euler's number (base of natural logarithm). |
| `tau` | 6.28318... | 2π (full circle in radians, though Zen uses degrees). |
| `inf` | ∞ | Positive infinity. |
| `nan` | NaN | Not a Number. |

**Example:**
```python
import math

# Geometry
radius = 5
circumference = 2 * math.pi * radius
area = math.pi * radius * radius

# Exponential growth
growth_factor = math.e
time = 2
result = 100 * math.pow(growth_factor, time)

# Full circle (for reference, Zen uses degrees)
full_circle = math.tau  # 360 degrees
half_circle = math.pi   # 180 degrees

# Special values
epsilon = 1e-10
if abs(result) > math.inf:  # Unreachable, but demonstrates
    pass
```

---

## Common Patterns

### Distance Calculation

```python
import math

def distance(x1, y1, x2, y2):
    dx = x2 - x1
    dy = y2 - y1
    return math.hypot(dx, dy)

# Usage
dist = distance(0, 0, 3, 4)  # → 5.0
```

### Normalize Angle

```python
import math

def normalize_angle(angle):
    # Keep in range [0, 360)
    normalized = angle % 360
    if normalized < 0:
        normalized += 360
    return normalized

# Or use angle_delta for [-180, 180] range
def shortest_angle(from_angle, to_angle):
    delta = math.angle_delta(from_angle, to_angle)
    return from_angle + delta
```

### Random Range

```python
import math

def random_int(min_val, max_val):
    # Random integer in [min, max]
    return int(min_val + math.random() * (max_val - min_val + 1))

def random_float(min_val, max_val):
    # Random float in [min, max]
    return min_val + math.random() * (max_val - min_val)

# Usage
dice_roll = random_int(1, 6)
random_x = random_float(-100, 100)
```

### Smooth Animation

```python
import math

class SmoothAnimator:
    def __init__(self, duration):
        self.duration = duration
        self.elapsed = 0
        self.is_finished = false
    
    def update(self, dt):
        self.elapsed += dt
        if self.elapsed >= self.duration:
            self.elapsed = self.duration
            self.is_finished = true
    
    def progress(self):
        # Returns [0, 1] with smooth easing
        t = self.elapsed / self.duration
        return math.smooth_step(0, 1, t)
    
    def evaluate(self, start, end):
        # Smoothly animate from start to end
        return math.lerp(start, end, self.progress())

# Usage
anim = SmoothAnimator(2.0)  # 2-second animation
anim.update(1.0)  # 1 second elapsed
pos_x = anim.evaluate(0, 100)  # Smoothly move toward 100
```

### Easing Functions

```python
import math

def ease_in_quad(t):
    # t ∈ [0, 1]
    return t * t

def ease_out_quad(t):
    return 1 - (1 - t) * (1 - t)

def ease_in_out_cubic(t):
    if t < 0.5:
        return 4 * t * t * t
    else:
        return 1 - math.pow(-2 * t + 2, 3) / 2

# Usage
t = 0.5
eased = ease_in_out_cubic(t)
value = math.lerp(0, 100, eased)
```

---

## Notes

- **Angles:** All functions use degrees, not radians. Convert: `radians = degrees * π / 180`
- **Random:** Not seeded by default. Add seed support via external module if needed.
- **Performance:** All math functions are O(1) and very fast.
- **Special Values:** `inf` and `nan` propagate through calculations (IEEE 754 rules).
- **Integer/Float:** All functions return floats. Cast to `int()` if needed.

---

**Generated from:** `libzen/src/builtin_math.cpp`  
**Last Updated:** 2026-05-18  
**Coverage:** 31 functions, 5 constants
