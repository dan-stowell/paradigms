#!/usr/bin/env python3
"""Fresh and repeated maintained reach, interleaved with destination recomputes.
Run: python3 ref/flat/bench-p2inc-followup.py [rounds=5]
All generated files stay in this clone. Data is read-only.
"""
import collections
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
DATA = Path('/home/exedev/paradigms/data')
BUILD = ROOT / 'build'
BUILD.mkdir(exist_ok=True)
ROUNDS = int(sys.argv[1]) if len(sys.argv) > 1 else 5
assert ROUNDS >= 5
CAP = ['prlimit', '--as=8589934592']
raw = (ROOT / 'results/c-incremental-followup-raw.txt').open('w')
rows = []

def run(cmd, env=None):
    fullenv = os.environ.copy()
    for k in ('FUZZ', 'CHAIN', 'CHAIN_EDIT', 'EVICT', 'FRESH_B'):
        fullenv.pop(k, None)
    fullenv.update(env or {})
    fullenv['TMPDIR'] = str(BUILD)
    result = subprocess.run([str(c) for c in cmd], env=fullenv, cwd=ROOT,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    raw.write('$ ' + ' '.join(f'{k}={v}' for k, v in (env or {}).items()) + ' ' + ' '.join(map(str, cmd)) + '\n')
    raw.write(result.stdout)
    raw.flush()
    result.check_returncode()
    return result.stdout

def stamp(label):
    raw.write(label + '\n')
    run(['date', '-u', '+%FT%TZ'])
    run(['uptime'])

for target, extra in [('p2inc', []), ('p2', []), ('p2par', ['-fopenmp'])]:
    run(['gcc', '-O2', '-march=native', *extra, '-o', BUILD / (target + '-followup'),
         ROOT / f'ref/flat/{target}.c'])

stamp('start')
pattern = re.compile(r'^update (A->B|B->A)\t([\d.]+) ms\trep (\d+)\tover-deleted (\d+) live,.*reverse entries (\d+),', re.M)
for round_ in range(ROUNDS):
    stamp(f'round {round_ + 1}')
    for edit in ('minio-edit', 'minio-edit2'):
        # Separate processes for every treatment; no oracle or initialization in update timer.
        for mode, env, reps in [('fresh A', {}, 1), ('fresh B', {'FRESH_B': '1'}, 1),
                                ('toggles', {}, 5), ('evicted toggles', {'EVICT': '1'}, 5)]:
            output = run(CAP + [BUILD / 'p2inc-followup', DATA / 'minio', DATA / edit, str(reps)], env)
            assert 'P2INC OK' in output
            matches = pattern.findall(output)
            assert matches, output
            for direction, time, rep, killed, reverse in matches:
                if mode.startswith('fresh'):
                    if direction != ('A->B' if mode == 'fresh A' else 'B->A') or rep != '0':
                        continue
                    category = mode
                else:
                    category = mode + (' first' if rep == '0' else ' repeat')
                rows.append((edit, direction, category, float(time), int(killed), int(reverse)))
        for direction, destination in [('A->B', edit), ('B->A', 'minio')]:
            output = run(CAP + [BUILD / 'p2-followup', DATA / destination])
            assert 'P2 OK' in output
            time = float(re.search(r'flood ([\d.]+) ms', output).group(1))
            rows.append((edit, direction, 'p2 destination', time, None, None))
            for threads in (1, 8):
                output = run(CAP + [BUILD / 'p2par-followup', DATA / destination, '5'],
                             {'OMP_NUM_THREADS': str(threads), 'OMP_WAIT_POLICY': 'passive'})
                assert 'P2 OK' in output
                time = float(re.search(r'flood best ([\d.]+) ms', output).group(1))
                rows.append((edit, direction, f'p2par {threads} threads best of 5', time, None, None))
stamp('end')
raw.close()
groups = collections.defaultdict(list)
for edit, direction, mode, *values in rows:
    groups[edit, direction, mode].append(values)
assert len(rows) == ROUNDS * 56
assert len(groups) == 32
for (_, _, mode), values in groups.items():
    assert len(values) == ROUNDS * (4 if mode.endswith('repeat') else 1)
with (ROOT / 'results/c-incremental-followup-summary.tsv').open('w') as summary:
    summary.write('edit\tdirection\tmode\tn\tms min/median/max\tretractions min/median/max\treverse reads min/median/max\n')
    for key, vals in sorted(groups.items()):
        columns = []
        for col in zip(*vals):
            columns.append('-' if col[0] is None else f'{min(col):.3f}/{statistics.median(col):.3f}/{max(col):.3f}')
        summary.write('\t'.join((*key, str(len(vals)), *columns)) + '\n')
print('P2INC follow-up OK:', len(rows), 'samples;', ROUNDS, 'interleaved rounds')
