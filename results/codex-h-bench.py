#!/usr/bin/env python3
"""Run from repo root after building all four variants and both phase copies in build-h/."""
import datetime, json, math, os, pathlib, re, subprocess, time
out=pathlib.Path('results/codex-h-raw'); out.mkdir(exist_ok=True)
def uptime(): return subprocess.check_output(['prlimit','--as=8589934592','uptime'],text=True).strip()
summary={'start':datetime.datetime.now(datetime.timezone.utc).isoformat(),'uptime_before':uptime(),'runs':[]}
print(summary,flush=True)
for data in ['minio','kube-apiserver']:
 for threads in [1,8]:
  env=dict(os.environ,OMP_NUM_THREADS=str(threads),OMP_WAIT_POLICY="passive")
  reps=64
  for iteration in range(5):
   for name in (['p1par','p1onlyA','p1onlyB','p1compact'][iteration%4:]+['p1par','p1onlyA','p1onlyB','p1compact'][:iteration%4]):
    while True:
     cmd=['prlimit','--as=8589934592',f'build-h/{name}',f'data/{data}',str(reps)]
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
# Separate one-thread phase measurements, also five interleaved runs.
for data in ['minio','kube-apiserver']:
 for iteration in range(5):
  for name in (['p1par','p1compact'] if iteration%2==0 else ['p1compact','p1par']):
   cmd=['prlimit','--as=8589934592',f'build-h/{name}-phases',f'data/{data}','64']
   p=subprocess.run(cmd,env=dict(os.environ,OMP_NUM_THREADS='1',OMP_WAIT_POLICY='passive'),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
   assert 'P1 OK' in p.stdout
   vals=list(map(float,re.search(r'phases ms: (.*)',p.stdout).group(1).split()))
   (out/f'{data}-phases-{iteration}-{name}.txt').write_text(p.stdout)
   summary.setdefault('phases',[]).append(dict(dataset=data,version=name,iteration=iteration,ms=vals))
   (out/'measurements.json').write_text(json.dumps(summary,indent=2)+'\n')
   print(data,name,'phases',vals,flush=True)
summary.update(end=datetime.datetime.now(datetime.timezone.utc).isoformat(),uptime_after=uptime())
(out/'measurements.json').write_text(json.dumps(summary,indent=2)+'\n'); print(summary['uptime_after'],flush=True)
