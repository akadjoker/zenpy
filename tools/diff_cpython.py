#!/usr/bin/env python3
"""Differential test: run scripts under CPython and Zen, compare stdout.

    tools/diff_cpython.py [--zen PATH] [--whole] [-v] [files or dirs...]

Default corpus: tests/diff/*.py, default mode: per chunk. Each file is split
into top-level statements; for every chunk the accumulated prefix is run in
both interpreters and only the chunk's own output is compared. A chunk Zen
cannot compile (or that fails at run time) is reported and left out of the
following prefixes, so one unsupported construct does not hide the rest.
--whole compares complete files instead.
"""
import argparse, os, subprocess, sys, tempfile

def run(cmd, path):
    try:
        p = subprocess.run(cmd + [path], capture_output=True, text=True, errors="replace", timeout=60)
        return p.returncode, p.stdout, (p.stderr or "").strip()
    except subprocess.TimeoutExpired:
        return -1, "", "timeout"

def run_src(cmd, src, suffix):
    with tempfile.NamedTemporaryFile("w", suffix=suffix, delete=False) as f:
        f.write(src)
        path = f.name
    try:
        return run(cmd, path)
    finally:
        os.unlink(path)

CONT = ("else", "elif", "except", "finally")

def chunks_of(src):
    lines = src.splitlines()
    out, cur, in_str = [], [], False
    for line in lines:
        starts_new = line and not line[0].isspace() and not in_str and \
                     not any(line.startswith(k) and (len(line) == len(k) or not line[len(k)].isalnum()) for k in CONT)
        if starts_new and cur:
            out.append("\n".join(cur)); cur = []
        cur.append(line)
        if line.count('"""') % 2 == 1 or line.count("'''") % 2 == 1:
            in_str = not in_str
    if cur:
        out.append("\n".join(cur))
    return [c for c in out if c.strip() and not all(l.strip().startswith("#") for l in c.splitlines())]

def first_err(err):
    for l in err.splitlines():
        if "Error" in l or "error" in l:
            return l.strip()
    return err.splitlines()[0].strip() if err else ""

import re
def defined_names(chunk):
    names = set()
    for line in chunk.splitlines():
        m = re.match(r"(?:def|class)\s+([A-Za-z_]\w*)", line)
        if m:
            names.add(m.group(1)); continue
        m = re.match(r"([A-Za-z_][\w, ]*?)\s*(?:\+|-|\*|/|%)?=[^=]", line)
        if m and not line.startswith((" ", "\t")):
            for n in re.split(r"\s*,\s*", m.group(1)):
                if re.fullmatch(r"[A-Za-z_]\w*", n):
                    names.add(n)
    return names

def uses_any(chunk, names):
    words = set(re.findall(r"[A-Za-z_]\w*", chunk))
    return bool(words & names)

def chunk_mode(zen, path, verbose):
    src = open(path).read()
    chunks = chunks_of(src)
    py_prefix, zen_prefix = [], []   # what each side has accepted so far
    py_prev = zen_prev = ""
    failed_names = set()              # names defined by chunks Zen rejected
    res = {"same": 0, "diff": 0, "zen-compile": 0, "zen-runtime": 0, "cascade": 0, "py-error": 0}
    for ch in chunks:
        head = ch.splitlines()[0][:70]
        prc, pout, perr = run_src([sys.executable], "\n".join(py_prefix + [ch]) + "\n", ".py")
        if prc != 0:
            res["py-error"] += 1
            print(f"  PY-ERROR   {head}   -> {first_err(perr)}")
            continue
        py_prefix.append(ch)
        pnew = pout[len(py_prev):]; py_prev = pout
        if uses_any(ch, failed_names):
            res["cascade"] += 1
            failed_names |= defined_names(ch)
            if verbose:
                print(f"  cascade    {head}")
            continue
        zrc, zout, zerr = run_src([zen], "\n".join(zen_prefix + [ch]) + "\n", ".py")
        if zrc != 0:
            kind = "zen-compile" if "compilation failed" in zerr or "Error at" in zerr else "zen-runtime"
            res[kind] += 1
            failed_names |= defined_names(ch)
            print(f"  {'ZEN-COMPILE' if kind == 'zen-compile' else 'ZEN-RUNTIME'} {head}   -> {first_err(zerr)}")
            continue
        zen_prefix.append(ch)
        znew = zout[len(zen_prev):]; zen_prev = zout
        if pnew == znew:
            res["same"] += 1
            if verbose:
                print(f"  same       {head}")
        else:
            res["diff"] += 1
            pl, zl = pnew.splitlines(), znew.splitlines()
            print(f"  DIFF       {head}")
            shown = 0
            for i in range(max(len(pl), len(zl))):
                p = pl[i] if i < len(pl) else "<missing>"
                z = zl[i] if i < len(zl) else "<missing>"
                if p != z:
                    print(f"               python: {p}\n               zen:    {z}")
                    shown += 1
                    if shown >= 2:
                        break
    return res

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--zen", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build_release", "bin", "zen"))
    ap.add_argument("--whole", action="store_true")
    ap.add_argument("--verbose", "-v", action="store_true")
    ap.add_argument("paths", nargs="*")
    a = ap.parse_args()
    paths = a.paths or [os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests", "diff")]
    files = []
    for p in paths:
        if os.path.isdir(p):
            files += sorted(os.path.join(p, f) for f in os.listdir(p) if f.endswith(".py"))
        else:
            files.append(p)
    total = {}
    bad = 0
    for f in files:
        name = os.path.relpath(f)
        if a.whole:
            prc, pout, perr = run([sys.executable], f)
            zrc, zout, zerr = run([a.zen], f)
            if prc != 0: print(f"PY-ERROR {name}: {first_err(perr)}")
            elif zrc != 0: print(f"ZEN-FAIL {name}: {first_err(zerr)}"); bad += 1
            elif pout == zout: print(f"same     {name}")
            else: print(f"DIFF     {name}"); bad += 1
            continue
        print(f"== {name}")
        r = chunk_mode(a.zen, f, a.verbose)
        for k, v in r.items():
            total[k] = total.get(k, 0) + v
        bad += r["diff"] + r["zen-compile"] + r["zen-runtime"]
    if total:
        print("\nTOTAL " + "   ".join(f"{k}: {v}" for k, v in total.items()))
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
