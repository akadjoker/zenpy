# gc_tune(min_bytes, factor) / gc_stats(): runtime-configurable collector
# thresholds. A larger minimum heap and a larger grow factor must mean fewer
# collection cycles for the same allocation volume, memory must stay bounded
# under either setting, and out-of-range values are clamped, not errors.

import time

class P:
    def __init__(self, x, y):
        self.x = x
        self.y = y

# Under a stress-GC build every call and back edge collects, so the loops are
# scaled down there and the "fewer cycles" comparison (meaningless when the
# threshold is ignored) is skipped.
def probe():
    live = []
    for k in range(2000):
        live.append(P(k, k))
    t0 = time.perf_counter()
    k = 0
    while k < 300:
        P(k, k)
        k = k + 1
    return time.perf_counter() - t0

FULL = probe() < 0.05
CHUNK = 20000 if FULL else 100

def churn(n):
    # Short-lived instances: nothing survives an iteration.
    k = 0
    while k < n:
        p = P(k, k)
        k += 1

def cycles_for(min_bytes, factor, n):
    gc_tune(min_bytes, factor)
    collect()
    before = gc_stats()["collections"]
    peak = 0
    k = 0
    while k < n:
        churn(CHUNK)
        used = mem_used()
        if used > peak:
            peak = used
        k += CHUNK
    return gc_stats()["collections"] - before, peak

# --- gc_stats() shape ---
s = gc_stats()
for key in ["bytes", "next_gc", "objects", "collections", "min_threshold", "grow_factor"]:
    assert key in s, "gc_stats missing key " + key
assert isinstance(s["collections"], int)
assert s["collections"] >= 0
assert s["bytes"] > 0
assert s["objects"] > 0
assert s["next_gc"] > 0
assert s["min_threshold"] >= 64 * 1024
assert s["grow_factor"] >= 1.0

# --- tuning is reflected in gc_stats() ---
gc_tune(3 * 1024 * 1024, 2.5)
s = gc_stats()
assert s["min_threshold"] == 3 * 1024 * 1024
assert s["grow_factor"] == 2.5

# --- allocation volume (~20+ MB) triggers collections; small vs large ---
N = 400000 if FULL else 2000
small_cycles, small_peak = cycles_for(1024 * 1024, 2.0, N)
assert small_cycles > 0, "no collection ran under a 1 MB minimum"

large_cycles, large_peak = cycles_for(64 * 1024 * 1024, 4.0, N)
if FULL:
    assert large_cycles < small_cycles, "large threshold should collect less often"

# memory stays bounded under both settings (well below the volume allocated)
assert small_peak < 16 * 1024 * 1024
assert large_peak < 160 * 1024 * 1024

# --- clamping: nonsense values are accepted and clamped ---
gc_tune(1, 0.5)
s = gc_stats()
assert s["min_threshold"] == 64 * 1024
assert s["grow_factor"] == 1.0

# restore the defaults for whatever runs after us
gc_tune(8 * 1024 * 1024, 2.0)
s = gc_stats()
assert s["min_threshold"] == 8 * 1024 * 1024
assert s["grow_factor"] == 2.0
print("gc_tuning ok")
