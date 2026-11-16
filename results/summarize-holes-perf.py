#!/usr/bin/env python3
"""Whole-process perf counters divided by warm repetitions (not kernel-only)."""
import re
from pathlib import Path
s=Path('results/codex-holes-perf.log').read_text()
print('| Variant | M cycles / rep | M instructions / rep | M branches / rep | M branch misses / rep | M cache misses / rep |')
print('|---|---:|---:|---:|---:|---:|')
for block in s.split('PERF ')[1:]:
    v=block.splitlines()[0]; assert 'P4 OK' in block
    reps=int(re.search(r'reps (\d+)',block)[1])
    counts=[]
    for counter in ('cycles','instructions','branches','branch-misses','cache-misses'):
        n=int(re.search(r'([\d,]+)\s+'+counter+r':u\b',block)[1].replace(',',''))
        counts.append(n/reps/1e6)
    print('| '+v+' | '+' | '.join(f'{n:.3f}' for n in counts)+' |')
