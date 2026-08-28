#!/usr/bin/env python3
"""
Benchmark Zenpy vs Python3 - measures real performance gains from SSO infrastructure
Tests focus on small string operations where SSO should provide benefits
"""
import subprocess
import time
import sys

TESTS = {
    "small_strings": '''
def test():
    for i in range(10000):
        a = "x"
        b = "ab"
        c = "abc"
        d = "test"
        e = "hello"
        f = "zenpy!"
''',
    
    "string_concat": '''
def test():
    result = ""
    for i in range(1000):
        result = result + "a"
''',
    
    "dict_small_keys": '''
def test():
    d = {}
    for i in range(1000):
        d["x"] = i
        d["ab"] = i
        d["abc"] = i
        d["test"] = i
''',
    
    "string_methods": '''
def test():
    for i in range(1000):
        s = "hello"
        s1 = s.upper()
        s2 = s.lower()
        s3 = s[0:2]
''',
    
    "array_strings": '''
def test():
    arr = []
    for i in range(1000):
        arr.push("x")
        arr.push("ab")
        arr.push("abc")
''',
    
    "string_interning": '''
def test():
    for i in range(10000):
        s1 = "test"
        s2 = "test"
        if s1 == s2:
            pass
''',
    
    "gc_pressure_strings": '''
def test():
    for i in range(5000):
        a = "x" + "y"
        b = "ab" + "cd"
        c = "test" + "123"
        d = [a, b, c]
''',
}

def run_python(code):
    """Run code in Python 3 and measure time"""
    script = code + "\nstart = time.time()\ntest()\nprint(time.time() - start)"
    full_code = "import time\n" + script
    result = subprocess.run([sys.executable, "-c", full_code], 
                          capture_output=True, text=True, timeout=30)
    if result.returncode != 0:
        print(f"Python error: {result.stderr}")
        return None
    try:
        return float(result.stdout.strip())
    except:
        return None

def run_zenpy(code):
    """Run code in Zenpy and measure time"""
    # Convert Python to Zenpy-compatible syntax
    zenpy_code = code.replace("import time", "").replace(".push(", ".append(").replace("time.time()", "0")
    script = zenpy_code + "\nimport time\nstart = time.time()\ntest()\nprint(time.time() - start)"
    
    with open("/tmp/zenpy_bench.py", "w") as f:
        f.write(script)
    
    result = subprocess.run(["./bin/zen", "/tmp/zenpy_bench.py"],
                          capture_output=True, text=True, timeout=30)
    if result.returncode != 0:
        print(f"Zenpy error: {result.stderr}")
        return None
    try:
        return float(result.stdout.strip().split('\n')[-1])
    except:
        return None

def benchmark():
    print("=" * 70)
    print("BENCHMARK: Zenpy vs Python3 — SSO Infrastructure Impact")
    print("=" * 70)
    print()
    
    py_total = 0
    zen_total = 0
    
    for name, code in TESTS.items():
        print(f"Test: {name:25s} ... ", end="", flush=True)
        
        # Run Python
        py_time = run_python(code)
        if py_time is None:
            print("PYTHON ERROR")
            continue
        
        # Run Zenpy
        zen_time = run_zenpy(code)
        if zen_time is None:
            print("ZENPY ERROR")
            continue
        
        py_total += py_time
        zen_total += zen_time
        
        ratio = zen_time / py_time if py_time > 0 else 0
        status = "✓ FASTER" if ratio < 1.0 else "✗ SLOWER"
        
        print(f"Python: {py_time:7.4f}s | Zenpy: {zen_time:7.4f}s | Ratio: {ratio:.2f}x {status}")
    
    print()
    print("=" * 70)
    print(f"TOTAL: Python: {py_total:.4f}s | Zenpy: {zen_total:.4f}s | Ratio: {zen_total/py_total:.2f}x")
    print("=" * 70)

if __name__ == "__main__":
    benchmark()
