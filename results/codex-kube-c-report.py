#!/usr/bin/env python3
"""Summarize the reproducible C round log, retaining round-level samples."""
import re
import statistics
from pathlib import Path

root = Path(__file__).resolve().parent
raw = (root / 'codex-kube-c-raw.log').read_text()
samples = {}
t = None
cmd = ''
rounds = []
for line in raw.splitlines():
    if line.startswith('ROUND '): rounds.append(line)
    if line.startswith('THREADS '): t = int(line.split()[1])
    if line.startswith('COMMAND '): cmd = line
    vals = []
    if m := re.search(r'p1par \(.*best ([\d.]+) ms', line):
        samples.setdefault(('1a+1b', t), []).append(float(m[1]))
    if m := re.search(r'p1par \(.*join by name ([\d.]+) \+ relocation targets ([\d.]+)', line):
        vals = [('1a', float(m[1])), ('1b', float(m[2]))]
    if m := re.search(r'p1cpar .*content-hash join ([\d.]+)', line): vals = [('1c', float(m[1]))]
    if m := re.search(r'p2par .*flood best ([\d.]+)', line): vals = [('2 CSR', float(m[1]))]
    if m := re.search(r'p2lazy .*flood best ([\d.]+)', line): vals = [('2 lazy', float(m[1]))]
    if '--layout' in cmd and (m := re.search(r'p3a .*layout ([\d.]+)', line)): vals = [('3a serial layout', float(m[1]))]
    if m := re.search(r'p3b reps .*\((4 KB|2 MB) pages.*median ([\d.]+)', line): vals = [('3b '+m[1], float(m[2]))]
    for cell, v in vals: samples.setdefault((cell, t), []).append(v)

# Exact GRID ranges, with ratios against their arithmetic midpoint. Missing is intentional.
base = {
    '1a+1b': ((74,77),(13.9,16.8)),
    '1a': ((41.7,48.7), (9.5,12.9)), '1b': (None,(5.3,6.3)),
    '1c': ((36.9,36.9),(7.2,8.9)), '2 CSR': ((27,30),(13,17)),
    '2 lazy': ((52.6,52.6),(13.6,15.4)), '3a serial layout': (None,None),
    '3b 4 KB': (None,(13.4,29)), '3b 2 MB': (None,(5.4,12)),
}
lines = ['## Tuned C comparison', '',
    'Commands: `bash results/codex-kube-c-round.sh LABEL`; each process runs under a 10 GiB systemd scope with `OMP_WAIT_POLICY=passive`. '
    'P1/P2 values are each process’s best of 10/30/20 in-process repetitions respectively; 3a is the mean of iterations until >=1 s; '
    '3b is the median of 30 repetitions. Table entries are min / median / max across independent process rounds, in ms. '
    '3a in `p3par` is serial at both thread settings. Ratios use the midpoint of the exact MinIO range in GRID.md. '
    'The 1a denominator is GRID’s later p1par-versus-p1compact snapshot (16:34Z, load 3–5), while the 1a+1b total and 1b use its earlier snapshot. '
    'GRID has no matching separated 1-thread p1par 1b or p3par 3b number, and its tuned 3a row is a different program.', '',
    '| Cell | Oracle | Kube 1 thread | Kube 8 threads | MinIO GRID 1 / 8 | Kube/MinIO 1 / 8 | Rounds 1 / 8 |',
    '|---|---|---:|---:|---|---|---|']
for cell, bases in base.items():
    times, refs, ratios, counts = [], [], [], []
    for t, b in zip((1,8), bases):
        v = samples.get((cell,t), [])
        times.append(' / '.join(f'{x:.2f}' for x in (min(v),statistics.median(v),max(v))) if v else 'pending')
        counts.append(str(len(v)))
        refs.append(('–'.join(f'{x:g}' for x in b) if b[0]!=b[1] else f'{b[0]:g}') if b else '—')
        ratios.append(f'{statistics.median(v)/statistics.mean(b):.2f}x' if b and v else '—')
    lines.append(f'| {cell} | '+('OK' if ' FAIL' not in raw and all(int(x)==0 for x in re.findall(r'^STATUS (\d+)',raw,re.M)) else 'see raw log')+' | '+' | '.join(times)+' | '+' / '.join(refs)+' | '+' / '.join(ratios)+' | '+' / '.join(counts)+' |')
lines += ['', 'Raw data: [codex-kube-c-raw.log](codex-kube-c-raw.log). Every run performs built-in oracle checks; `--layout` checks all addresses, '
          'P3 compares all placed image symbols, P1 checks canon/builtins/targets/content canon, and P2 checks the full live list.', '',
          'Recorded window and uptime/load:', '', '```text']
lines += [l for l in raw.splitlines() if l.startswith(('ROUND ', 'ENDROUND ')) or 'load average:' in l]
lines += ['```', '', 'Failures: none.' if ' FAIL' not in raw and all(int(x)==0 for x in re.findall(r'^STATUS (\d+)',raw,re.M)) else 'Failures: inspect raw log.', '']
(root / 'codex-kube-c.md').write_text('\n'.join(lines))
