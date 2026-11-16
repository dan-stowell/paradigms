#!/usr/bin/env python3
"""Five interleaved rounds of witness/count/BFS ablations and destination recomputes.
All writes are in this clone. Raw log includes commands, oracles, counts, UTC and load.
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
ROUNDS = int(sys.argv[1]) if len(sys.argv)>1 else 5
assert ROUNDS>=5
CAP=['prlimit','--as=12884901888']
BASE='587b3514085808aacd8365164258d199acb0316e'
raw=(ROOT/'results/c-incremental-witness-raw.txt').open('w')
rows=[]
def run(cmd,env=None):
    full=os.environ.copy()
    for k in ('FUZZ','CHAIN','CHAIN_EDIT','EVICT','FRESH_B','LIFO','COUNTS','BFS','NO_WITNESS'):
        full.pop(k,None)
    full.update(env or {})
    full['TMPDIR']=str(BUILD)
    p=subprocess.run(list(map(str,cmd)),env=full,cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    raw.write('$ '+' '.join(f'{k}={v}' for k,v in (env or {}).items())+' '+' '.join(map(str,cmd))+'\n'+p.stdout)
    raw.flush();p.check_returncode();return p.stdout
def stamp(label):
    raw.write(label+'\n');run(['date','-u','+%FT%TZ']);run(['uptime'])
(BUILD/'p2inc-baseline.c').write_text(run(['git','show',BASE+':ref/flat/p2inc.c']))
for name,source,extra in [('p2inc-baseline',BUILD/'p2inc-baseline.c',[]),('p2inc-witness',ROOT/'ref/flat/p2inc.c',[]),
                          ('p2-witness',ROOT/'ref/flat/p2.c',[]),('p2par-witness',ROOT/'ref/flat/p2par.c',['-fopenmp'])]:
    run(['gcc','-O2','-march=native','-I',ROOT/'ref/flat',*extra,'-o',BUILD/name,source])
pat=re.compile(r'^update (A->B|B->A)\t([\d.]+) ms\trep (\d+)\tover-deleted (\d+) live,.*edges read (\d+), reverse entries (\d+), support checks (\d+)',re.M)
stages=[('baseline','p2inc-baseline',{}),('witness','p2inc-witness',{'LIFO':'1'}),
        ('counts','p2inc-witness',{'LIFO':'1','COUNTS':'1'}),('BFS witness','p2inc-witness',{})]
stamp('start')
for r in range(ROUNDS):
    stamp(f'round {r+1}')
    for edit in ('minio-edit','minio-edit2'):
        for stage,binary,extra in stages:
            modes=[('fresh A',{},1),('fresh B',{'FRESH_B':'1'},1),('toggles',{},5)]
            if stage=='BFS witness':modes.append(('evicted toggles',{'EVICT':'1'},5))
            for mode,env,reps in modes:
                out=run(CAP+[BUILD/binary,DATA/'minio',DATA/edit,str(reps)],{**extra,**env})
                assert 'P2INC OK' in out
                matches=pat.findall(out);assert len(matches)==2*reps
                for direction,time,rep,killed,edges,reverse,checks in matches:
                    if mode.startswith('fresh'):
                        if direction!=('A->B' if mode=='fresh A' else 'B->A') or rep!='0':continue
                        cat='fresh'
                    else:cat=mode+(' first' if rep=='0' else ' repeat')
                    rows.append((edit,direction,stage,cat,float(time),int(killed),int(edges),int(reverse),int(checks)))
        for direction,dest in [('A->B',edit),('B->A','minio')]:
            out=run(CAP+[BUILD/'p2-witness',DATA/dest]);assert 'P2 OK' in out
            rows.append((edit,direction,'p2','destination',float(re.search(r'flood ([\d.]+) ms',out)[1]),None,None,None,None))
            out=run(CAP+[BUILD/'p2par-witness',DATA/dest,'5'],{'OMP_NUM_THREADS':'8','OMP_WAIT_POLICY':'passive'})
            assert 'P2 OK' in out
            rows.append((edit,direction,'p2par 8','destination best of 5',float(re.search(r'flood best ([\d.]+) ms',out)[1]),None,None,None,None))
stamp('end');raw.close()
groups=collections.defaultdict(list)
for *key,time,killed,edges,reverse,checks in rows:groups[tuple(key)].append((time,killed,edges,reverse,checks))
with (ROOT/'results/c-incremental-witness-summary.tsv').open('w') as f:
    f.write('edit\tdirection\tstage\ttreatment\tn\tms min/median/max\tretractions min/median/max\tout edges min/median/max\treverse min/median/max\tchecks min/median/max\n')
    for key,vals in sorted(groups.items()):
        assert len(vals)==ROUNDS*(4 if key[-1].endswith('repeat') else 1)
        cols=['-' if c[0] is None else f'{min(c):.3f}/{statistics.median(c):.3f}/{max(c):.3f}' for c in zip(*vals)]
        f.write('\t'.join((*key,str(len(vals)),*cols))+'\n')
print('witness benchmark OK',len(rows),'samples',ROUNDS,'rounds')
