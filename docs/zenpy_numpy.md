# Zen NumPy Library Reference

**Complete reference for numerical arrays with 40 functions. Mini NumPy for typed arrays.**

Import with: `import numpy` or `import numpy as np`

---

## Overview

Numerical operations on typed arrays (Float64Array by default). Provides array creation, element-wise operations, reductions, and utility functions.

### Quick Start

```python
import numpy as np

# Create arrays
a = np.zeros(10)         # [0, 0, 0, ...]
b = np.ones(5)           # [1, 1, 1, 1, 1]
c = np.arange(10)        # [0, 1, 2, 3, ..., 9]
d = np.linspace(0, 1, 5) # [0, 0.25, 0.5, 0.75, 1]

# Element-wise operations
e = np.add(a, b)
f = np.multiply(a, 2)

# Reductions
total = np.sum(a)
average = np.mean(a)
```

---

## Array Creation

### zeros(n) → Float64Array

Create array of n zeros.

**Signature:** `zeros(size: int) → Float64Array`

**Example:**
```python
import numpy as np

a = np.zeros(5)
print(len(a))  # → 5
print(a[0])    # → 0.0
```

---

### ones(n) → Float64Array

Create array of n ones.

**Signature:** `ones(size: int) → Float64Array`

**Example:**
```python
import numpy as np

a = np.ones(3)
print(a[0])    # → 1.0
print(a[2])    # → 1.0
```

---

### full(n, value) → Float64Array

Create array of n copies of value.

**Signature:** `full(size: int, value: number) → Float64Array`

**Example:**
```python
import numpy as np

a = np.full(4, 3.14)
print(a[0])    # → 3.14
print(len(a))  # → 4
```

---

### arange([start,] stop[, step]) → Float64Array

Create array with evenly spaced values.

**Signature:** 
- `arange(stop)` → [0, 1, ..., stop-1]
- `arange(start, stop)` → [start, start+1, ..., stop-1]
- `arange(start, stop, step)` → evenly spaced by step

**Example:**
```python
import numpy as np

a = np.arange(5)              # [0, 1, 2, 3, 4]
b = np.arange(2, 7)           # [2, 3, 4, 5, 6]
c = np.arange(0, 10, 2)       # [0, 2, 4, 6, 8]
d = np.arange(0, 1, 0.25)     # [0, 0.25, 0.5, 0.75]
e = np.arange(10, 0, -1)      # [10, 9, 8, ..., 1]
```

---

### linspace(start, stop, n) → Float64Array

Create n evenly spaced values from start to stop (inclusive).

**Signature:** `linspace(start: number, stop: number, num: int) → Float64Array`

**Returns:** Array with exactly `num` elements

**Example:**
```python
import numpy as np

a = np.linspace(0, 1, 5)      # [0, 0.25, 0.5, 0.75, 1]
b = np.linspace(0, 10, 11)    # [0, 1, 2, 3, ..., 10]
c = np.linspace(-1, 1, 3)     # [-1, 0, 1]
```

---

### array(list) → Float64Array

Convert array/list to Float64Array.

**Signature:** `array(obj: array | list) → Float64Array`

**Example:**
```python
import numpy as np

a = np.array([1, 2, 3, 4, 5])
print(a[0])    # → 1.0
print(len(a))  # → 5
```

---

### zeros_like(array) → Float64Array

Create zeros array with same shape as input.

**Signature:** `zeros_like(arr: array) → Float64Array`

**Example:**
```python
import numpy as np

a = np.array([1, 2, 3])
b = np.zeros_like(a)
print(len(b))  # → 3
print(b[0])    # → 0.0
```

---

### ones_like(array) → Float64Array

Create ones array with same shape as input.

**Signature:** `ones_like(arr: array) → Float64Array`

**Example:**
```python
import numpy as np

a = np.array([1, 2, 3])
b = np.ones_like(a)
print(len(b))  # → 3
print(b[0])    # → 1.0
```

---

## Binary Element-Wise Operations

All binary operations work element-by-element and require same-length arrays.

### add(a, b) → Float64Array

Element-wise addition.

**Signature:** `add(arr1: array, arr2: array) → Float64Array`

**Example:**
```python
import numpy as np

a = np.array([1, 2, 3])
b = np.array([4, 5, 6])
c = np.add(a, b)  # [5, 7, 9]
```

---

### subtract(a, b) → Float64Array

Element-wise subtraction.

**Example:**
```python
a = np.array([10, 20, 30])
b = np.array([1, 2, 3])
c = np.subtract(a, b)  # [9, 18, 27]
```

---

### multiply(a, b) → Float64Array

Element-wise multiplication.

**Example:**
```python
a = np.array([1, 2, 3])
b = np.array([2, 3, 4])
c = np.multiply(a, b)  # [2, 6, 12]
```

---

### divide(a, b) → Float64Array

Element-wise division.

**Example:**
```python
a = np.array([10, 20, 30])
b = np.array([2, 4, 5])
c = np.divide(a, b)  # [5, 5, 6]
```

---

### mod(a, b) → Float64Array

Element-wise modulo.

**Example:**
```python
a = np.array([10, 20, 30])
b = np.array([3, 7, 4])
c = np.mod(a, b)  # [1, 6, 2]
```

---

### power(a, b) → Float64Array

Element-wise power (a raised to b).

**Example:**
```python
a = np.array([2, 3, 4])
b = np.array([2, 2, 2])
c = np.power(a, b)  # [4, 9, 16]
```

---

### minimum(a, b) → Float64Array

Element-wise minimum.

**Example:**
```python
a = np.array([5, 2, 8])
b = np.array([3, 6, 1])
c = np.minimum(a, b)  # [3, 2, 1]
```

---

### maximum(a, b) → Float64Array

Element-wise maximum.

**Example:**
```python
a = np.array([5, 2, 8])
b = np.array([3, 6, 1])
c = np.maximum(a, b)  # [5, 6, 8]
```

---

## Unary Element-Wise Operations

### negative(a) → Float64Array

Negate elements.

**Example:**
```python
a = np.array([1, -2, 3])
b = np.negative(a)  # [-1, 2, -3]
```

---

### abs(a) → Float64Array

Absolute value.

**Example:**
```python
a = np.array([-1, -2, 3])
b = np.abs(a)  # [1, 2, 3]
```

---

### sqrt(a) → Float64Array

Square root.

**Example:**
```python
a = np.array([1, 4, 9, 16])
b = np.sqrt(a)  # [1, 2, 3, 4]
```

---

### sin(a) → Float64Array

Sine (angle in radians).

**Example:**
```python
import math
a = np.array([0, math.pi/2, math.pi])
b = np.sin(a)  # [0, 1, ~0]
```

---

### cos(a) → Float64Array

Cosine (angle in radians).

---

### exp(a) → Float64Array

Exponential (e^x).

---

### log(a) → Float64Array

Natural logarithm.

---

### floor(a) → Float64Array

Floor (round down).

---

### ceil(a) → Float64Array

Ceiling (round up).

---

## Reduction Operations

Reduce array to single value.

### sum(a) → float

Sum of all elements.

**Signature:** `sum(arr: array) → float`

**Example:**
```python
a = np.array([1, 2, 3, 4, 5])
total = np.sum(a)  # 15.0
```

---

### prod(a) → float

Product of all elements.

**Signature:** `prod(arr: array) → float`

**Example:**
```python
a = np.array([1, 2, 3, 4])
product = np.prod(a)  # 24.0
```

---

### min(a) → float

Minimum element.

**Signature:** `min(arr: array) → float`

**Example:**
```python
a = np.array([5, 2, 8, 1, 9])
minimum = np.min(a)  # 1.0
```

---

### max(a) → float

Maximum element.

**Signature:** `max(arr: array) → float`

**Example:**
```python
a = np.array([5, 2, 8, 1, 9])
maximum = np.max(a)  # 9.0
```

---

### mean(a) → float

Mean (average) of elements.

**Signature:** `mean(arr: array) → float`

**Example:**
```python
a = np.array([1, 2, 3, 4, 5])
average = np.mean(a)  # 3.0
```

---

### std(a) → float

Standard deviation.

**Signature:** `std(arr: array) → float`

**Example:**
```python
a = np.array([1, 2, 3, 4, 5])
deviation = np.std(a)  # ~1.414
```

---

### dot(a, b) → float

Dot product (sum of element-wise multiplication).

**Signature:** `dot(arr1: array, arr2: array) → float`

**Example:**
```python
a = np.array([1, 2, 3])
b = np.array([4, 5, 6])
result = np.dot(a, b)  # 1*4 + 2*5 + 3*6 = 32.0
```

---

### argmin(a) → int

Index of minimum element.

**Signature:** `argmin(arr: array) → int`

**Example:**
```python
a = np.array([5, 2, 8, 1, 9])
idx = np.argmin(a)  # 3 (index of 1)
```

---

### argmax(a) → int

Index of maximum element.

**Signature:** `argmax(arr: array) → int`

**Example:**
```python
a = np.array([5, 2, 8, 1, 9])
idx = np.argmax(a)  # 4 (index of 9)
```

---

## Utility Functions

### tolist(a) → array

Convert Float64Array to Zen array.

**Signature:** `tolist(arr: array) → array`

**Example:**
```python
a = np.array([1, 2, 3])
lst = np.tolist(a)
print(lst)  # [1, 2, 3]
```

---

### clip(a, min, max) → Float64Array

Clip values to range [min, max].

**Signature:** `clip(arr: array, min: number, max: number) → Float64Array`

**Example:**
```python
a = np.array([1, 5, 10, 15, 20])
b = np.clip(a, 5, 15)  # [5, 5, 10, 15, 15]
```

---

### cumsum(a) → Float64Array

Cumulative sum.

**Signature:** `cumsum(arr: array) → Float64Array`

**Example:**
```python
a = np.array([1, 2, 3, 4, 5])
b = np.cumsum(a)  # [1, 3, 6, 10, 15]
```

---

### diff(a) → Float64Array

Differences between consecutive elements.

**Signature:** `diff(arr: array) → Float64Array`

**Returns:** Array with length-1 elements

**Example:**
```python
a = np.array([1, 3, 6, 10, 15])
b = np.diff(a)  # [2, 3, 4, 5]  (differences)
```

---

### where(condition, a, b) → Float64Array

Choose elements based on condition.

**Signature:** `where(cond: array, arr1: array, arr2: array) → Float64Array`

**Behavior:** For each position, if cond[i] != 0, take a[i], else take b[i]

**Example:**
```python
cond = np.array([1, 0, 1, 0, 1])
a = np.array([1, 2, 3, 4, 5])
b = np.array([10, 20, 30, 40, 50])
c = np.where(cond, a, b)  # [1, 20, 3, 40, 5]
```

---

### concatenate(a, b) → Float64Array

Concatenate two arrays.

**Signature:** `concatenate(arr1: array, arr2: array) → Float64Array`

**Example:**
```python
a = np.array([1, 2, 3])
b = np.array([4, 5, 6])
c = np.concatenate(a, b)  # [1, 2, 3, 4, 5, 6]
```

---

## Common Patterns

### Data Normalization

```python
import numpy as np

function normalize_minmax(data):
    min_val = np.min(data)
    max_val = np.max(data)
    range_val = max_val - min_val
    
    if range_val == 0:
        return np.zeros_like(data)
    
    shifted = np.subtract(data, np.full(len(data), min_val))
    return np.divide(shifted, np.full(len(data), range_val))

# Usage
scores = np.array([40, 50, 60, 70, 80])
normalized = normalize_minmax(scores)
# → [0, 0.25, 0.5, 0.75, 1]
```

### Moving Average

```python
import numpy as np

function moving_average(data, window_size):
    if window_size <= 0 or len(data) < window_size:
        return data
    
    result = []
    for i in range(len(data) - window_size + 1):
        window = []
        for j in range(window_size):
            window.push(data[i + j])
        avg = np.mean(np.array(window))
        result.push(avg)
    
    return np.array(result)

# Usage
prices = np.array([100, 102, 101, 103, 105, 104])
ma = moving_average(prices, 3)
```

### Statistical Analysis

```python
import numpy as np

class Stats:
    def __init__(self, data):
        self.data = data
    
    def summary(self):
        return {
            min: np.min(self.data),
            max: np.max(self.data),
            mean: np.mean(self.data),
            std: np.std(self.data),
            sum: np.sum(self.data)
        }

# Usage
data = np.array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
stats = Stats(data)
print(stats.summary())
```

### Matrix-like Operations

```python
import numpy as np

function matrix_multiply_vector(matrix_flat, n_cols, vector):
    // Assumes matrix is stored flat (row-major)
    n_rows = len(matrix_flat) / n_cols
    result = np.zeros(int(n_rows))
    
    for i in range(int(n_rows)):
        sum_val = 0.0
        for j in range(n_cols):
            idx = i * n_cols + j
            sum_val = sum_val + matrix_flat[idx] * vector[j]
        result[i] = sum_val
    
    return result

// Matrix is [[1, 2], [3, 4]], vector is [5, 6]
matrix = np.array([1, 2, 3, 4])
vec = np.array([5, 6])
result = matrix_multiply_vector(matrix, 2, vec)
// [1*5 + 2*6, 3*5 + 4*6] = [17, 39]
```

### Grade Scaling

```python
import numpy as np

function scale_grades(raw_scores, curve_amount = 0):
    scaled = np.add(raw_scores, np.full(len(raw_scores), curve_amount))
    clamped = np.clip(scaled, 0, 100)
    return clamped

# Usage
scores = np.array([72, 85, 68, 91, 78])
curved = scale_grades(scores, 5)  # Add 5 points
```

### Outlier Detection

```python
import numpy as np

function detect_outliers(data, std_threshold = 2):
    mean = np.mean(data)
    deviation = np.std(data)
    
    threshold = deviation * std_threshold
    lower = mean - threshold
    upper = mean + threshold
    
    outliers = []
    for i in range(len(data)):
        if data[i] < lower or data[i] > upper:
            outliers.push(i)
    
    return outliers

# Usage
measurements = np.array([100, 102, 101, 103, 150, 105])
outlier_indices = detect_outliers(measurements, 2)
```

---

## Performance Notes

- Operations are on Float64Array by default
- Element-wise operations: O(n)
- Reductions: O(n)
- No lazy evaluation (all computed immediately)
- Good for small to medium arrays (< 1M elements)

---

## Limits

- Array size: limited by available memory
- No multi-dimensional arrays (1D only)
- No sparse array support
- All arrays use Float64 (double precision)

---

**Generated from:** `libzen/src/builtin_numpy.cpp`  
**Last Updated:** 2026-05-18  
**Coverage:** 40 functions (creation, binary ops, unary ops, reductions, utilities)
