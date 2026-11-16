#!/usr/bin/env python3
"""Five interleaved >=1-second layout kernels, including a malloc hugepage variant."""
import os, subprocess, json, pathlib
out=pathlib.Path('results/codex-i-runs.json')
rows=[]
def uptime(): return subprocess.check_output(['uptime'],text=True).strip()
for r in range(5):
 for dataset in ['minio','kube-apiserver']:
  for kind,threads,huge in [('p3',1,False),('p3a_par',1,False),('p3a_par',8,False),('p3a_par',1,True),('p3a_par',8,True)]:
   env=dict(os.environ,OMP_NUM_THREADS=str(threads),OMP_WAIT_POLICY='passive')
   env.pop('GLIBC_TUNABLES',None)
   if huge: env['GLIBC_TUNABLES']='glibc.malloc.hugetlb=2'
   cmd=['prlimit','--as=8589934592','build-i/'+kind,'data/'+dataset]
   cmd+=['--layout'] if kind=='p3' else ['1']
   before=uptime()
   p=subprocess.run(cmd,env=env,text=True,capture_output=True,check=True)
   row=dict(round=r,dataset=dataset,kind=kind,threads=threads,huge=huge,before=before,after=uptime(),output=p.stdout)
   rows.append(row);out.write_text(json.dumps(rows,indent=2)+'\n')
   print(dataset,kind,threads,huge,p.stdout.splitlines()[0],flush=True)
