#!/usr/bin/env python3
"""Recompute five-process ranges; fail if any kernel run lasted under one second."""
import re, statistics
from pathlib import Path
s=Path('results/codex-holes-runs.log').read_text()
groups={}; preps={}; loads=[]; durations=[]; group_loads={}
for block in s.split('ROUND ')[1:]:
    header=block.splitlines()[0]
    _,t,v=re.match(r'(\d+) threads (\d+) variant (\S+)',header).groups()
    m=re.search(r'emit ([\d.]+) ms',block)
    if not m: raise ValueError('incomplete '+header)
    assert 'P4 OK' in block and 'P4 FAIL' not in block,header
    dur=float(re.search(r'reps \d+ total ([\d.]+) s',block)[1]); durations.append(dur)
    assert dur>=1,(header,dur)
    groups.setdefault((v,int(t)),[]).append(float(m[1]))
    load=float(re.search(r'load average: ([\d.]+)',block)[1]);loads.append(load); group_loads.setdefault((v,int(t)),[]).append(load)
    p=re.search(r'prep .*?total ([\d.]+) ms',block)
    if p:preps.setdefault(v,[]).append(float(p[1]))
def trip(a): return ' / '.join(f'{x:.3f}' for x in (min(a),statistics.median(a),max(a)))
print('| Variant | Threads | Emit min / median / max ms | One-minute load range |')
print('|---|---:|---:|---:|')
for t in (1,8):
    for v in ('p4par','B','H1','H2','H0'):
        a=groups[v,t];assert len(a)==5
        l=group_loads[v,t]
        print(f'| {v} | {t} | {trip(a)} | {min(l):.2f}–{max(l):.2f} |')
print('\n| Variant | Preparation min / median / max ms (10 processes) |\n|---|---:|')
for v in ('B','H1','H2','H0'):
    assert len(preps[v])==10
    print(f'| {v} | {trip(preps[v])} |')
print(f'\nLoad range {min(loads):.2f}–{max(loads):.2f}; kernel seconds {min(durations):.3f}–{max(durations):.3f}; {len(durations)} oracle passes.')
