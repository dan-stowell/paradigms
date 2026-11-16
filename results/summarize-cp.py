#!/usr/bin/env python3
"""Summarize the committed interleaved benchmark log; no third-party dependencies."""
import collections,re,statistics,pathlib
runs=collections.defaultdict(list); covered=collections.defaultdict(list); loads=[]; label=''
for line in pathlib.Path('results/codex-cp-runs.log').read_text().splitlines():
 if line.startswith('RUN '): _,round,label=line.split()
 m=re.search(r'load average: ([\d.]+)',line)
 if m: loads.append(float(m[1]))
 m=re.search(r'lay out ([\d.]+) ms, emit ([\d.]+) ms.*reps (\d+) total ([\d.]+) s',line)
 if m: runs[label].append(tuple(map(float,(m[1],m[2],m[4]))))
 m=re.search(r'cp4 covered .*emit ([\d.]+) ms;.*total ([\d.]+) s',line)
 if m: covered[label].append(tuple(map(float,m.groups())))
def fmt(a): return ' / '.join(f'{x:.3f}' for x in (min(a),statistics.median(a),max(a)))
print('| Variant | Layout min / median / max ms | Emit min / median / max ms | Measured seconds min–max |')
print('|---|---:|---:|---:|')
for label,rows in runs.items():
 print('|',label,'|',fmt([r[0] for r in rows]),'|',fmt([r[1] for r in rows]),'|',f'{min(r[2] for r in rows):.3f}–{max(r[2] for r in rows):.3f}','|')
print('\nCovered-only:')
for label,rows in covered.items(): print(label,fmt([r[0] for r in rows]),'total sec',min(r[1] for r in rows),max(r[1] for r in rows))
print('load',min(loads),max(loads))
print('rounds',dict(runs))
