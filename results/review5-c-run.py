#!/usr/bin/env python3
"""Five rotated rounds; output and FIFO files stay inside this clone."""
import os, pathlib, subprocess, datetime, json
root=pathlib.Path(__file__).resolve().parents[1]
os.chdir(root)
out=root/'results/review5-c-raw'; out.mkdir(exist_ok=True)
fifos=root/'build/review5-perf'; fifos.mkdir(exist_ok=True)
for n in ['ctl','ack']:
 p=fifos/n
 if not p.exists(): os.mkfifo(p)
subprocess.run(['gcc','-O2','-march=native','-fopenmp','ref/flat/p1compact.c','-o','build/review5-p1compact'],check=True)
perf=root/'build/perf-tools/usr/lib/linux-tools-6.8.0-149/perf'
with open(out/'events.txt','w') as f: subprocess.run([str(perf),'list'],stdout=f,stderr=f)
variants=[('base-cold',0,0),('huge-cold',1,0),('base-warm',0,1),('huge-warm',1,1)]
for r in range(5):
 for name,huge,warm in variants[r%4:]+variants[:r%4]:
  prefix=out/f'{r+1}-{name}'
  env=os.environ.copy(); env.update(OMP_NUM_THREADS='1',OMP_WAIT_POLICY='passive',FLAT_HUGE='0',P1_PROFILE='1',P1_WARMUP=str(warm),GLIBC_TUNABLES='glibc.malloc.hugetlb=2' if huge else '',P1_PERF_CTL=str(fifos/'ctl'),P1_PERF_ACK=str(fifos/'ack'))
  meta={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'uptime':subprocess.check_output(['uptime'],text=True).strip(),'meminfo_before':pathlib.Path('/proc/meminfo').read_text(),'reps':64}
  cmd=[str(perf),'stat','-D','-1','--control=fifo:'+str(fifos/'ctl')+','+str(fifos/'ack'),'-x',',','-o',str(prefix)+'.csv','-e','dTLB-load-misses:u,ls_l1_d_tlb_miss.all:u,ls_l1_d_tlb_miss.all_l2_miss:u,page-faults','--','taskset','-c','6',str(root/'build/review5-p1compact'),'/home/exedev/paradigms/data/minio','64']
  with open(str(prefix)+'.log','w') as f: p=subprocess.run(cmd,env=env,stdout=f,stderr=f)
  meta['exit']=p.returncode; meta['meminfo_after']=pathlib.Path('/proc/meminfo').read_text()
  pathlib.Path(str(prefix)+'.json').write_text(json.dumps(meta,indent=2)+'\n')
  assert p.returncode==0 and 'P1 OK' in pathlib.Path(str(prefix)+'.log').read_text()
  print(r+1,name,meta['uptime'],flush=True)
