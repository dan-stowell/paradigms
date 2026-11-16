#!/usr/bin/env python3
"""Run unchanged P4 cells sequentially; all generated output stays in this clone."""
import os, pathlib, subprocess, json, datetime, re, sys
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'results/codex-kube-p4-raw.json'
RECORDS = json.loads(OUT.read_text()) if OUT.exists() else {}
TMP = ROOT / 'build/kube-p4-tmp'
TMP.mkdir(exist_ok=True)
DATA = pathlib.Path('/home/exedev/paradigms/data')
env = os.environ.copy()
for key in ['P4_TOPK','P4_K','P4_JOIN','P4_NOCHECK','PARTS','CHUNK','TUNING','FUT_TUNING','FUT_PROFILE','BENCH_CPU','YIELD']:
    env.pop(key, None)
env.update(TMPDIR=str(TMP), P4_OUT=str(TMP), OMP_WAIT_POLICY='passive', P4_NOPATCH='0',
           GLIBC_TUNABLES='glibc.malloc.hugetlb=2', P4_HUGE='1', PREP_REPS='8')
def run(tag, cmd, threads=1, reps=1):
    e=env | dict(OMP_NUM_THREADS=str(threads), HL_NUM_THREADS=str(threads), P4_REPS=str(reps))
    start=datetime.datetime.now(datetime.timezone.utc).isoformat()
    up=subprocess.check_output(['uptime'],text=True).strip()
    p=subprocess.run(['timeout','300','prlimit','--as=8589934592','--',*map(str,cmd)],cwd=ROOT,env=e,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    record=dict(tag=tag,start=start,uptime=up,command=list(map(str,cmd)),threads=threads,reps=reps,returncode=p.returncode,output=p.stdout)
    RECORDS[tag] = record
    OUT.write_text(json.dumps(RECORDS,indent=2)+'\n')
    print(tag, 'exit',p.returncode, up, flush=True)
    if p.returncode or ('4lay' not in tag and not re.search(r'P4(?:bf|D)? OK',p.stdout)):
        print(p.stdout,flush=True)
        if 'goose' not in tag:
            raise RuntimeError(tag+' failed')
    return record

def fcmd(d,t,cell,reps):
    return [ROOT/'systems/futhark/build/p4_multicore.exe',DATA/d,str(t),str(reps),cell]
hal=ROOT/'systems/halide/cells/build/p4'
goose=ROOT/'systems/goose/bin/p4'
if sys.argv[1]=='check':
    for d in ['kube-p4','minio-p4']:
        run('check-'+d+'-p4',[ROOT/'build/p4', DATA/d])
        for t in [1,8]:
            run(f'check-{d}-{t}-p4par',[ROOT/'build/p4par',DATA/d],t)
            for cell in ['4bf','4D']:
                run(f'check-{d}-{t}-{cell}',fcmd(d,t,cell,1),t)
            run(f'check-{d}-{t}-halide',[hal,DATA/d,'e2e','lay_t_lf','emit_t_i2_v32_p1'],t)
            run(f'check-{d}-{t}-goose',[goose,DATA/d,str(t)],t)
else:
    for r in range(1,6):
        ds=['minio-p4','kube-p4'] if r%2 else ['kube-p4','minio-p4']
        for t in ([1,8] if r%2 else [8,1]):
            for system in ['p4','4bf','p4par','halide','4D','goose','4lay']:
                if system=='p4' and t==8: continue
                for d in ds:
                    tag=f'r{r}-{d}-{t}-{system}'
                    if system in ['p4','p4par']:
                        run(tag,[ROOT/'build'/system,DATA/d],t,16 if t==1 else 64)
                    elif system in ['4bf','4D','4lay']:
                        n={'4bf':16,'4D':64,'4lay':256}[system]
                        run(tag,fcmd(d,t,system,n),t,n)
                    elif system=='halide':
                        run(tag+'-layout',[hal,DATA/d,'lay_t_lf'],t)
                        run(tag+'-emit',[hal,DATA/d,'emit_t_i2_v32_p1'],t)
                    else:
                        run(tag,[goose,DATA/d,str(t)],t,64)
