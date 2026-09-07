#!/usr/bin/env bash
# Regression gate on the speed report.
#   tools/speed_gate.sh tests/manual/baseline/baseline-1aaa13b.md [threshold-percent]
# Runs tools/speed_report.sh into a temporary label and fails (exit 1) if any
# cross-language, microbench or algo_bench number is slower than the baseline
# by more than the threshold (default 15%). Build times and sizes are shown
# but not gated.
set -u
cd "$(dirname "$0")/.."
BASE=$1; TH=${2:-15}
LABEL=gate-$(date +%s)
tools/speed_report.sh $LABEL >/dev/null
NEW=tests/manual/baseline/$LABEL.md
python3 - "$BASE" "$NEW" "$TH" <<'PY'
import re, sys
base, new, th = sys.argv[1], sys.argv[2], float(sys.argv[3])
def nums(p):
    d = {}
    sec = ""
    for line in open(p):
        if line.startswith("## "): sec = line[3:].strip()
        m = re.match(r"- (\S+)\s+([\d,\.]+)$", line.strip())
        if m and ("benchmarks" in sec.lower() or "microbench" in sec.lower()):
            d[m.group(1)] = float(m.group(2).replace(",", "."))
        m = re.match(r"zen\s+(.*)$", line.strip())
        if m and "algo_bench" in sec:
            vals = m.group(1).split()
            for i, v in enumerate(vals):
                d["algo_%d" % i] = float(v.replace(",", "."))
    return d
b, n = nums(base), nums(new)
bad = []
for k in sorted(b):
    if k in n and b[k] > 0:
        pct = (n[k] - b[k]) / b[k] * 100
        flag = "  <-- SLOWER" if pct > th else ""
        print("%-16s %8.3f -> %8.3f  %+6.1f%%%s" % (k, b[k], n[k], pct, flag))
        if pct > th: bad.append(k)
print("\nreport:", new)
sys.exit(1 if bad else 0)
PY
