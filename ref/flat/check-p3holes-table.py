#!/usr/bin/env python3
"""Independent sorted-offset sidecar check, using only raw strings and exported addresses."""
from array import array
from pathlib import Path
import subprocess
import sys

def read(path, kind='I'):
    out=array(kind);out.frombytes(path.read_bytes());return out

def check(a,b,export):
    sect=[int(line.split()[1]) for line in (export/'anchors').read_text().splitlines() if line.startswith('sect ')]
    types=sect[3]
    counts=[]
    for d in (a,b): counts.append(sum(x==6 for x in read(d/'p3_bucket.u8','B')))
    capacity=max(counts)
    for k,d in enumerate((a,b)):
        label='ab'[k]
        adr=read(export/f'{label}.addr');addresses=dict(zip(adr[::2],adr[1::2]))
        ids=read(d/'p3_sym.u32');buckets=read(d/'p3_bucket.u8','B')
        mp=read(d/'diff/idmap_b.u32') if k else None
        strings=read(d/'p3_tlstr.u32');off=read(d/'p3_str.off.u64','Q');raw=(d/'p3_str.bin').read_bytes()
        keys=[]
        for i,bucket in enumerate(buckets):
            if bucket!=6:continue
            u=mp[ids[i]] if mp is not None else ids[i]
            t=strings[i]
            keys.append((raw[off[t]:off[t+1]],u))
        keys.sort()
        expected=[addresses[u]-types for _,u in keys]+[0]*(capacity-len(keys))
        actual=read(export/f'{label}.table')
        assert len(actual)==capacity,(label,'capacity',len(actual),capacity)
        assert list(actual)==expected,(label,'offset array differs')
        print(f'TYPELINK INDEPENDENT OK {label} count={len(keys)} bytes={4*capacity}')

a,b,export=map(Path,sys.argv[1:4])
check(a,b,export)
if '--negative' in sys.argv[4:]:
    p=export/'b.table';saved=p.read_bytes();bad=bytearray(saved);bad[0]^=1
    try:
        p.write_bytes(bad)
        result=subprocess.run([sys.executable,__file__,str(a),str(b),str(export)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        assert result.returncode!=0,'corrupt table was accepted'
    finally:p.write_bytes(saved)
    check(a,b,export)
    print('TYPELINK NEGATIVE OK corrupted table rejected; restored table passes')
