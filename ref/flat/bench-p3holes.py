#!/usr/bin/env python3
"""Five interleaved rounds, >=1s per process/kernel; run check-p3holes.sh first."""
import os, re, statistics, subprocess, sys, time
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
os.chdir(ROOT)
DATA = Path(os.environ.get('DATA_ROOT', '/home/exedev/paradigms/data'))
CPU = int(os.environ.get('CPU', min(os.sched_getaffinity(0))))
rounds = int(sys.argv[1]) if len(sys.argv)>1 else 5
assert rounds >= 5
samples = {}
env = dict(os.environ, OMP_NUM_THREADS='1', OMP_WAIT_POLICY='passive')
for r in range(rounds):
    print(f'ROUND {r+1} UTC {time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())} CPU {CPU}', flush=True)
    subprocess.run(['uptime'], check=True)
    jobs = [(e, kind) for e in ('edit', 'edit2') for kind in ('inc', 'ref', 'par', 'refA', 'parA')] + [('chain', 'chain')]
    if r % 2: jobs.reverse()
    for e,kind in jobs:
        b = DATA / f'minio-{e}'
        if kind == 'chain': args = ['build-relayout2/p3holes',str(DATA/'minio'),str(DATA/'minio-edit2'),'1',str(DATA/'minio-edit')]
        elif kind == 'inc': args = ['build-relayout2/p3holes', str(DATA/'minio'), str(b), '1']
        else: args = [f'build-relayout2/p3{"par" if kind.startswith("par") else ""}bench',str(DATA/'minio' if kind.endswith('A') else b),'1']
        print('RUN', e, kind, flush=True)
        p = subprocess.run(['taskset','-c',str(CPU),'prlimit','--as=8589934592',*args],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
        print(p.stdout, end='',flush=True)
        if kind=='chain':
            matches=re.findall(r'chain_step=(\d+) mean_ms=([\d.]+)',p.stdout)
            assert len(matches)==4
            for step,value in matches: samples.setdefault((e,kind,step),[]).append(float(value))
        elif kind=='inc':
            m=re.search(r'forward_ms=([\d.]+) reverse_ms=([\d.]+)',p.stdout)
            assert m
            for direction,value in zip(('forward','reverse'),m.groups()): samples.setdefault((e,kind,direction),[]).append(float(value))
        else:
            m=re.search(r'mean_ms ([\d.]+) OK',p.stdout)
            assert m
            samples.setdefault((e,kind,'full'),[]).append(float(m[1]))
print('SUMMARY milliseconds min median max (per-process means)')
for key,values in sorted(samples.items()):
    print(*key, f'{min(values):.6f}', f'{statistics.median(values):.6f}',f'{max(values):.6f}')
subprocess.run(['uptime'],check=True)
