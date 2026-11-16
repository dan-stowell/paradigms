#!/usr/bin/env python3
"""Five rotating interleaved rounds; all files remain inside this clone."""
import array, json, os, pathlib, re, statistics, subprocess, time
ROOT = pathlib.Path(__file__).resolve().parents[1]
DATA = pathlib.Path('/home/exedev/paradigms/data/minio')
def col(name, typ):
    a = array.array(typ); a.frombytes((DATA/name).read_bytes()); return a

def expected_image():
    oracle = (DATA/'oracle/p3_image.bin').read_bytes()
    expected = bytearray(len(oracle))
    # imageBase is recorded in the flat metadata; no produced bytes come from oracle.
    meta = dict(line.split()[:2] for line in (DATA/'meta.txt').read_text().splitlines() if line.strip() and not line.startswith('#'))
    base = int(meta['imageBase'])
    covered = 0
    for a, n, b in zip(col('oracle/p3_addr.u32','I'),col('p3_size.u32','I'),col('p3_bucket.u8','B')):
        if b < 13:
            p = a-base; expected[p:p+n] = oracle[p:p+n]; covered += n
    return expected, covered, sum(x!=y for x,y in zip(expected,oracle))

def check_file(path, expected):
    got = path.read_bytes()
    assert got == expected, f'{path}: output differs from independently normalized oracle'

if __name__ == '__main__':
    os.chdir(ROOT)
    expected, covered, outside = expected_image()
    print(f'Independent oracle: image={len(expected)} bytes, placed={covered}, original nonplaced differing bytes={outside}', flush=True)
    # Prove that the independent comparator detects both a placed-byte error and a gap error.
    fault = ROOT/'build/review5-b-fault.img'
    for label, pos in [('placed', next(i for i,x in enumerate(expected) if x)), ('gap', 0)]:
        corrupted = bytearray(expected); corrupted[pos] ^= 1; fault.write_bytes(corrupted)
        try: check_file(fault, expected)
        except AssertionError: print(f'Independent comparator rejects {label} byte corruption at {pos}', flush=True)
        else: raise AssertionError(f'comparator accepted {label} fault')
    fault.unlink()
    variants = []
    for name, binary, workers in [('goose8','systems/goose/bin/p3par',['8']),('goose1','systems/goose/bin/p3',[]),('c1','build/review5-p3',[])]:
        for huge in [False,True]:
            variants.append((name+('-2m' if huge else '-4k')+'-file',binary,workers,huge,True))
            if name != 'goose8': variants.append((name+('-2m' if huge else '-4k')+'-kernel',binary,workers,huge,False))
    results=[]
    for rnd in range(5):
        for name,binary,workers,huge,file in variants[rnd:]+variants[:rnd]:
            print(f'RUN {rnd+1} {name} {time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())}',flush=True)
            subprocess.run(['uptime'],check=True)
            env=dict(os.environ, P3_REPS='64')
            for k in list(env):
                if k in ['P3_THP','P3_FILE','P3_PREFAULT','GLIBC_TUNABLES','FLAT_HUGE'] or k.startswith('OMP_'): env.pop(k,None)
            env['FLAT_HUGE']='0'
            if huge:env['P3_THP']='1'
            path=ROOT/'build/review5-goose.img'
            if file:env['P3_FILE']=str(path)
            affinity='0-7' if workers else '6'
            cmd=['systemd-run','--user','--scope','-q','-p','MemoryMax=8G','taskset','-c',affinity,str(ROOT/binary),str(DATA),*workers]
            out=subprocess.check_output(cmd,env=env,text=True,stderr=subprocess.STDOUT)
            print(out,flush=True)
            assert 'P3 OK' in out
            if file: check_file(path,expected); print('Independent full file check OK',flush=True)
            if name.startswith('c'):
                m=re.search(r'p3b reps 64 .*?: min ([\d.]+) median ([\d.]+) max ([\d.]+)',out)
            else:m=re.search(r'x64\) ([\d.]+) / ([\d.]+) / ([\d.]+)',out)
            assert m,out
            values=list(map(float,m.groups())); assert values[0]*64>=1000, (name,values)
            results.append(dict(round=rnd+1,name=name,min=values[0],median=values[1],max=values[2]))
            (ROOT/'results/review5-b-runs.json').write_text(json.dumps(results,indent=2)+'\n')
    print('SUMMARY: min/median/max of five per-process minima (ms)')
    for name,*_ in variants:
        v=[r['min'] for r in results if r['name']==name]
        print(name,min(v),statistics.median(v),max(v))
