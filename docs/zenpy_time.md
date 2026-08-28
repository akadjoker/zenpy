# Zen Time Library Reference

**Complete reference for time measurement and delays with 4 functions.**

Import with: `import time`

---

## Overview

Wall clock and monotonic time measurement, plus sleep/delay functionality. Microsecond precision.

| Function | Arity | Description |
|----------|-------|-------------|
| `time` | 0 | Get seconds since Unix epoch (1970-01-01 00:00:00 UTC). |
| `monotonic` | 0 | Get monotonic time (never goes backwards). |
| `perf_counter` | 0 | Alias for `monotonic()` (performance counter). |
| `sleep` | 1 | Sleep for N seconds (float ok). |

---

## time() → float

Get current wall clock time as seconds since Unix epoch (January 1, 1970, 00:00:00 UTC).

**Signature:** `time() → float`

**Returns:** Seconds since Unix epoch as float with microsecond precision.

**Use for:**
- Absolute timestamps (database records, logs)
- Converting to human-readable dates
- Session timestamps
- **NOT for measuring elapsed time** (use `monotonic()` instead)

**Why NOT for timing:** System clock can jump forward/backward due to NTP sync, DST, clock adjustments.

**Example:**
```python
import time

# Current Unix timestamp
ts = time.time()
print(ts)  # → 1747738400.123456 (May 18, 2026, ~14:00 UTC)

# Store when user logged in
login_time = time.time()

# Later: how long since login?
elapsed = time.time() - login_time  # ✗ WRONG: clock adjustments break this
```

---

## monotonic() → float

Get current monotonic time in seconds. **Always increases, never jumps backwards.**

**Signature:** `monotonic() → float`

**Returns:** Seconds as float with microsecond precision.

**Guarantees:**
- Always increases (immune to clock adjustments)
- Immune to NTP syncs
- Safe for measuring elapsed time
- Immune to manual clock changes

**Use for:**
- Timing game loops
- Benchmarking code
- Measuring frame times
- Animation durations
- **ANY measurement of elapsed time**

**Example:**
```python
import time

# Measure frame time
frame_start = time.monotonic()
# ... do frame work ...
frame_time = time.monotonic() - frame_start
print("Frame took", frame_time * 1000, "ms")

# Benchmark operation
start = time.monotonic()
for i in range(1000000):
    x = i * i
end = time.monotonic()
print("Million squareRoot took", end - start, "seconds")

# Timer that survives clock adjustments
timer_start = time.monotonic()
def elapsed():
    return time.monotonic() - timer_start

# Timer immune to NTP, DST, user changing system clock
print(elapsed())  # Safe!
```

---

## perf_counter() → float

Alias for `monotonic()`. Provided for Python compatibility.

**Signature:** `perf_counter() → float`

**Returns:** Same as `monotonic()` — monotonic time in seconds.

**Example:**
```python
import time

# Same as monotonic()
start = time.perf_counter()
# ... work ...
elapsed = time.perf_counter() - start
```

---

## sleep(seconds) → nil

Sleep for N seconds. Accepts float (supports fractional seconds).

**Signature:** `sleep(duration: float | int) → nil`

**Parameters:**
- `duration`: Seconds to sleep. Can be int or float. Sub-second sleeps ok (0.1 → 100ms).

**Precision:** Microsecond level (may sleep slightly longer due to OS scheduling).

**Example:**
```python
import time

# Sleep 1 second
time.sleep(1)

# Sleep 100 milliseconds
time.sleep(0.1)

# Sleep 50 milliseconds
time.sleep(0.05)

# Sleep 1.5 seconds
time.sleep(1.5)

# Zero or negative = return immediately
time.sleep(0)

# Loop with fixed framerate (30 FPS)
frame_duration = 1.0 / 30.0  # ~33ms
while true:
    frame_start = time.monotonic()
    # ... render frame ...
    frame_elapsed = time.monotonic() - frame_start
    remaining = frame_duration - frame_elapsed
    if remaining > 0:
        time.sleep(remaining)
```

---

## Zen time() vs monotonic() Comparison

| Feature | `time()` | `monotonic()` |
|---------|----------|---------------|
| What it measures | Seconds since 1970 | Arbitrary reference point |
| Can jump backwards | Yes (NTP, DST, user changes) | Never |
| Can jump forwards | Yes | No (steady increase) |
| Precision | Microseconds | Microseconds |
| Use for absolute time | ✓ Yes | ✗ No |
| Use for elapsed time | ✗ No (unsafe) | ✓ Yes (safe) |
| Database timestamps | ✓ Yes | ✗ No |
| Benchmarking | ✗ No | ✓ Yes |
| Frame timing | ✗ No | ✓ Yes |
| Animations | ✗ No | ✓ Yes |

**Quick rule:** "Need to measure elapsed time? Use `monotonic()`. Need absolute timestamp? Use `time()`."

---

## Common Patterns

### Fixed-Timestep Game Loop

```python
import time

class GameLoop:
    def __init__(self, target_fps = 60):
        self.target_fps = target_fps
        self.frame_time = 1.0 / target_fps
        self.last_frame = time.monotonic()
        self.dt = 0.0
    
    def tick(self):
        now = time.monotonic()
        self.dt = now - self.last_frame
        self.last_frame = now
        
        # Cap dt to prevent spiral of death
        if self.dt > 0.1:
            self.dt = 0.1
        
        return self.dt
    
    def sync(self):
        # Sleep to maintain target FPS
        elapsed = time.monotonic() - self.last_frame
        remaining = self.frame_time - elapsed
        if remaining > 0.001:  # Only sleep if > 1ms remaining
            time.sleep(remaining)

# Usage
loop = GameLoop(60)
while running:
    dt = loop.tick()
    update(dt)
    render()
    loop.sync()  # Maintain 60 FPS
```

### Timeout Handler

```python
import time

class Timeout:
    def __init__(self, duration):
        self.duration = duration
        self.start = time.monotonic()
    
    def is_expired(self):
        return (time.monotonic() - self.start) >= self.duration
    
    def remaining(self):
        left = self.duration - (time.monotonic() - self.start)
        return max(0.0, left)

# Usage
timeout = Timeout(5.0)  # 5 second timeout
while not timeout.is_expired():
    print("Remaining:", timeout.remaining())
    time.sleep(0.1)
print("Timeout!")
```

### Profiler/Benchmark

```python
import time

class Timer:
    def __init__(self, name=""):
        self.name = name
        self.start = time.monotonic()
        self.samples = []
    
    def lap(self):
        now = time.monotonic()
        elapsed = now - self.start
        self.samples.push(elapsed)
        self.start = now
        return elapsed
    
    def average(self):
        if len(self.samples) == 0:
            return 0.0
        total = 0.0
        for s in self.samples:
            total = total + s
        return total / len(self.samples)

# Usage
timer = Timer("render")
for frame in range(100):
    # ... render ...
    timer.lap()

print(timer.name, "average:", timer.average() * 1000, "ms")
```

### Delay-Based Animation

```python
import time

class DelayedAction:
    def __init__(self, delay):
        self.delay = delay
        self.start = time.monotonic()
        self.executed = false
    
    def update(self):
        if not self.executed:
            elapsed = time.monotonic() - self.start
            if elapsed >= self.delay:
                self.executed = true
                return true
        return false

# Usage
explosion = DelayedAction(1.5)  # Explode after 1.5 seconds

def update(dt):
    if explosion.update():
        trigger_explosion()
```

### Session Tracking

```python
import time
import json
import io

class Session:
    def __init__(self, user_id):
        self.user_id = user_id
        self.login_time = time.time()  # ✓ Absolute timestamp
        self.session_start = time.monotonic()  # ✓ For duration
        self.activity_count = 0
    
    def get_duration(self):
        # How long has session been active?
        return time.monotonic() - self.session_start
    
    def save(self):
        return {
            user_id: self.user_id,
            login_timestamp: self.login_time,
            duration_seconds: self.get_duration(),
            activity_count: self.activity_count
        }

# Usage
session = Session("alice@example.com")
session.activity_count = session.activity_count + 1
session.activity_count = session.activity_count + 1

# Later: save session data
data = session.save()
f = io.open("session.json", "w")
f.write(json.stringify(data, 2))
f.close()
```

### Interval-Based Tasks

```python
import time

class IntervalTask:
    def __init__(self, interval):
        self.interval = interval
        self.last_run = time.monotonic()
    
    def should_run(self):
        elapsed = time.monotonic() - self.last_run
        if elapsed >= self.interval:
            self.last_run = time.monotonic()
            return true
        return false

# Usage
checkpoint_timer = IntervalTask(5.0)  # Every 5 seconds
enemy_spawn_timer = IntervalTask(2.0)  # Every 2 seconds

def update(dt):
    if checkpoint_timer.should_run():
        save_checkpoint()
    
    if enemy_spawn_timer.should_run():
        spawn_enemy()
```

### Adaptive Frame Limit

```python
import time

target_fps = 60
frame_time_target = 1.0 / target_fps
frame_times = []
max_history = 60

last_frame = time.monotonic()

def update():
    global last_frame
    
    now = time.monotonic()
    frame_time = now - last_frame
    last_frame = now
    
    # Track frame time
    frame_times.push(frame_time)
    if len(frame_times) > max_history:
        frame_times = frame_times[1:]  # Keep last 60 frames
    
    # Calculate average
    avg = 0.0
    for ft in frame_times:
        avg = avg + ft
    avg = avg / len(frame_times)
    
    # Report if framerate drops
    actual_fps = 1.0 / avg if avg > 0 else 0.0
    if actual_fps < target_fps * 0.95:
        print("FPS drop:", actual_fps)
    
    # Sleep if ahead of schedule
    elapsed = time.monotonic() - last_frame
    remaining = frame_time_target - elapsed
    if remaining > 0.001:
        time.sleep(remaining)
```

---

## Precision & Accuracy

| Feature | Value |
|---------|-------|
| Precision | ±1 microsecond (1e-6 seconds) |
| Sleep accuracy | ±10 milliseconds (OS scheduling) |
| Minimum sleep | ~1 millisecond (OS dependent) |
| CLOCK_REALTIME | Wall clock, may jump |
| CLOCK_MONOTONIC | Never backwards, unaffected by adjtime() |

---

## Platform Notes

- **POSIX systems** (Linux, macOS): Uses `clock_gettime()` with nanosecond resolution
- **Windows**: Implemented via `QueryPerformanceCounter()` (similar behavior)
- **Sleep accuracy:** Can vary by OS scheduler. May sleep longer than requested.
- **Precision:** Microseconds on most systems. Check `time.monotonic()` twice rapidly to see actual resolution.

---

## Performance

- `time()`: ~100 nanoseconds
- `monotonic()`: ~100 nanoseconds
- `sleep()`: Blocks thread (use async alternatives for non-blocking in production)

---

**Generated from:** `libzen/src/builtin_time.cpp`  
**Last Updated:** 2026-05-18  
**Coverage:** 4 functions (time, monotonic, perf_counter, sleep)
