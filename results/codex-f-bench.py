#!/usr/bin/env python3
"""Run from repo root after building build-f/{p1par,p1compact}."""
import datetime, json, math, os, pathlib, re, subprocess, time
out=pathlib.Path('results/codex-f-raw'); out.mkdir(exist_ok=True)
def uptime(): return subprocess.check_output(['uptime'],text=True).strip()
summary={'start':datetime.datetime.now(datetime.timezone.utc).isoformat(),'uptime_before':uptime(),'runs':[]}
print(summary,flush=True)
for data in ['minio','kube-apiserver']:
 for threads in [1,8]:
  env=dict(os.environ,OMP_NUM_THREADS=str(threads))
  reps=100
  for iteration in range(5):
   for name in ['p1par','p1compact']:
    while True:
     cmd=['prlimit','--as=8589934592',f'build-f/{name}',f'data/{data}',str(reps)]
     t=time.monotonic(); p=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True); wall=time.monotonic()-t
     assert 'P1 OK' in p.stdout
     m=re.search(r'mean: join ([\d.]+) ms total ([\d.]+) ms \((\d+) reps, ([\d.]+) s kernel\)',p.stdout)
     join,total,n,kernel=map(float,m.groups())
     if kernel>=1: break
     reps=math.ceil(reps*1.3/kernel)
    tag=f'{data}-{threads}-{iteration}-{name}'
    (out/(tag+'.txt')).write_text(p.stdout)
    row=dict(dataset=data,threads=threads,iteration=iteration,version=name,reps=reps,join_ms=join,total_ms=total,kernel_s=kernel,wall_s=wall)
    summary['runs'].append(row); print(row,flush=True)
    (out/'measurements.json').write_text(json.dumps(summary,indent=2)+'\n')
summary.update(end=datetime.datetime.now(datetime.timezone.utc).isoformat(),uptime_after=uptime())
(out/'measurements.json').write_text(json.dumps(summary,indent=2)+'\n'); print(summary['uptime_after'],flush=True)
