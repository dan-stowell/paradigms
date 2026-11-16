#!/usr/bin/env python3
"""Five interleaved warm samples; run from repo root. Writes raw evidence."""
import os
from pathlib import Path
import subprocess
import tempfile

Path('build').mkdir(exist_ok=True)
flags = ['gcc', '-O2', '-march=native', '-fopenmp', '-Iref/flat']
subprocess.run(flags + ['ref/flat/p2pipe.c', '-o', 'build/p2pipe'], check=True)
# Instrument a temporary copy of p2par only to expose each sample, so the first
# iteration can be discarded identically for both programs. Algorithm unchanged.
s = Path('ref/flat/p2par.c').read_text()
s = s.replace('double t1 = now();', 'double t1 = now();\n printf("baseline sample=%.3f ms\\n", (t1-t0)*1000);')
with tempfile.TemporaryDirectory(prefix='p2pipe-') as tmp:
    source = Path(tmp) / 'p2par.c'
    source.write_text(s)
    baseline = str(Path(tmp) / 'p2par')
    subprocess.run(flags + [str(source), '-o', baseline], check=True)
    with Path('results/codex-d-runs.txt').open('w', buffering=1) as out:
        for r in range(1, 6):
            for dataset in ['minio', 'kube-apiserver']:
                for threads in [1, 8]:
                    load = subprocess.check_output(['cut', "-d ", '-f1', '/proc/loadavg'], text=True).strip()
                    stamp = subprocess.check_output(['date', '-u'], text=True).strip()
                    out.write(f'ROUND {r} dataset={dataset} threads={threads} load={load} date={stamp}\n')
                    programs = ['build/p2pipe', baseline]
                    if r % 2 == 0:
                        programs.reverse()
                    for program in programs:
                        env = dict(os.environ, OMP_NUM_THREADS=str(threads))
                        cmd = ['prlimit', '--as=8589934592', program,
                               f'/home/exedev/paradigms/data/{dataset}', '2']
                        p = subprocess.run(cmd, env=env, text=True, stdout=subprocess.PIPE,
                                           stderr=subprocess.STDOUT)
                        out.write('COMMAND ' + ' '.join(cmd) + '\n' + p.stdout)
                        if p.returncode:
                            raise SystemExit(p.returncode)
print('Five rounds complete; use only the second sample of each invocation.')
