#!/usr/bin/env python3
"""Recompute CP7 tables from the committed, interleaved raw log."""
import re, statistics as st
from pathlib import Path
s=Path('results/codex-cp7-runs.log').read_text()
rows={}; prep={}
for line in s.splitlines():
    m=re.match(r'p4(?:par)? \((\d+) (?:core|threads)\): lay out ([\d.]+) ms, emit ([\d.]+) ms.*reps (\d+) total ([\d.]+) s',line)
    if m:
        t,layout,emit,reps,total=m.groups(); key=('p4par' if line.startswith('p4par') else 'p4')+'/'+t
        rows.setdefault(key,[]).append((float(emit),float(total),float(layout)))
    m=re.match(r'p4runs ([ABCD]) \((\d+) threads\): emit ([\d.]+) ms;.*reps (\d+) total ([\d.]+) s',line)
    if m:
        v,t,emit,reps,total=m.groups();rows.setdefault(v+'/'+t,[]).append((float(emit),float(total),0))
    m=re.match(r'prep ([ABCD]): construct ([\d.]+) ms, dedupe ([\d.]+) ms, total ([\d.]+) ms;',line)
    if m:
        v,*times=m.groups();prep.setdefault(v,[]).append(tuple(map(float,times)))
def triple(xs):return f'{min(xs):.3f} / {st.median(xs):.3f} / {max(xs):.3f}'
print('| Variant | Emit min / median / max ms | GB/s at median | Measured seconds min–max |')
print('|---|---:|---:|---:|')
for k,vs in rows.items():
    assert len(vs)==5,(k,len(vs))
    es=[v[0] for v in vs];ts=[v[1] for v in vs]
    assert min(ts)>=1,(k,ts)
    print(f'| {k} | {triple(es)} | {60.733909/st.median(es):.3f} | {min(ts):.3f}–{max(ts):.3f} |')
print('\n| Variant | Preparation total min / median / max ms | Construction incl. input mapping | Dedupe ms |')
print('|---|---:|---:|---:|')
for k,vs in prep.items():
    assert len(vs)==10
    print(f'| {k} | {triple([v[2] for v in vs])} | {triple([v[0] for v in vs])} | {triple([v[1] for v in vs])} |')
loads=list(map(float,re.findall(r'load average: ([\d.]+)',s)))
print('\nLoad range:',min(loads),max(loads))
print('Oracle passes:',s.count('P4 OK'))
print('Huge-page outputs:',s.count('output: MAP_HUGETLB 2 MB'))
print('Window:',re.findall(r'^\d{4}-.*Z$',s,re.M)[::len(re.findall(r'^\d{4}-.*Z$',s,re.M))-1])
print('\nPer-process emit minima, rounds 1–5:')
for k,vs in rows.items(): print(k,', '.join(f'{v[0]:.3f}' for v in vs))
