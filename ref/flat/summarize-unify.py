#!/usr/bin/env python3
# Summarize ref/flat/bench-unify.sh output: min / median / max (ms) per (edit, mode), warm and end-to-end.
import re, statistics, sys
cur, rows, loads = None, {}, []
for line in open(sys.argv[1]):
    m = re.match(r'== round \d+ (\S+) (\S+) \|.*load average: ([\d.]+)', line)
    if m:
        cur = (m.group(1), m.group(2)); loads.append(float(m.group(3))); continue
    if cur is None: continue
    r = rows.setdefault(cur, {})
    m = re.match(r'end-to-end: map ([\d.]+) ms .* \+ unify ([\d.]+) ms = ([\d.]+) ms.*; (\d+) minor faults', line)
    if m:
        r.setdefault('map', []).append(float(m.group(1))); r.setdefault('e2e', []).append(float(m.group(3)))
        r.setdefault('flt', []).append(int(m.group(4)))
    m = re.match(r'warm: \d+ reps in [\d.]+ s, ([\d.]+) ms per unify \(fast path ([\d.]+), slow-path classes ([\d.]+), myers ([\d.]+)\)', line)
    if m:
        for k, v in zip(('warm', 'fast', 'classes', 'myers'), m.groups()): r.setdefault(k, []).append(float(v))
f = lambda xs: f"{min(xs):.2f} / {statistics.median(xs):.2f} / {max(xs):.2f}"
print(f"load (1-min, before each run) {min(loads)}-{max(loads)}, {len(next(iter(rows.values()))['warm'])} interleaved rounds; ms min / median / max")
print()
print("| edit | mode | warm per unify | fast path | slow-path classes | myers | end-to-end (map + 1 unify) | of which map | minor faults (median) |")
print("|---|---|---|---|---|---|---|---|---|")
for (e, mode), r in rows.items():
    print(f"| {e} | {mode} | {f(r['warm'])} | {f(r['fast'])} | {f(r['classes'])} | {f(r['myers'])} | {f(r['e2e'])} | {f(r['map'])} | {int(statistics.median(r['flt']))} |")
