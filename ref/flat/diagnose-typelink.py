#!/usr/bin/env python3
"""Read-only bucket-6 diagnosis; optional old p3inc export addresses attribute movement."""
from array import array
from collections import Counter
from pathlib import Path
import sys

def read(p, kind='I'):
    out = array(kind)
    out.frombytes(p.read_bytes())
    return out

def addresses(p):
    x = read(p)
    return dict(zip(x[::2], x[1::2]))

root = Path(sys.argv[1] if len(sys.argv)>1 else '/home/exedev/paradigms/data')
for suffix in ('', '-edit', '-edit2'):
    d = root / ('minio'+suffix)
    ids, sizes, starts, targets = [read(d/f) for f in ('p3_sym.u32','p3_size.u32','p3_rel_start.u32','p3_rel_tgt.u32')]
    buckets, kinds, widths = [read(d/f,'B') for f in ('p3_bucket.u8','p3_rel_kind.u8','p3_rel_siz.u8')]
    canon = read(d/'oracle/p1c_canon.u32')
    fixed = read(d/'p3_fixed_id.u32')
    present = set(ids) | set(fixed)
    rows = [i for i,b in enumerate(buckets) if b==6]
    count=Counter(); stores=0; storebytes=0; skipped=0
    for i in rows:
        for r in range(starts[i], starts[i+1]):
            count[kinds[r]] += 1
            target=targets[r]
            target=canon[target] if target<len(canon) else target
            if kinds[r]==3 and target not in present:
                skipped+=1
            else:
                stores+=1; storebytes+=widths[r]
    print(f'{d.name}: descriptors={len(rows)} descriptor_bytes={sum(sizes[i] for i in rows)} relocations={sum(count.values())} kinds={dict(sorted(count.items()))} actual_stores={stores} store_bytes={storebytes} skipped_missing_ADDROFF={skipped} offset_table_bytes={4*len(rows)}')
    if suffix=='-edit2' and len(sys.argv)>2:
        exp=Path(sys.argv[2]); aa=addresses(exp/'a.addr'); bb=addresses(exp/'b.addr'); mp=read(d/'diff/idmap_b.u32')
        moved=[i for i in rows if mp[ids[i]] in aa and aa[mp[ids[i]]]!=bb[mp[ids[i]]]]
        ns=sk=0
        for i in moved:
            for r in range(starts[i],starts[i+1]):
                t=targets[r];t=canon[t] if t<len(canon) else t
                if kinds[r]==3 and t not in present: sk+=1
                else: ns+=1
        print(f'old export: moved_descriptors={len(moved)} relocation_records_in_moved={sum(starts[i+1]-starts[i] for i in moved)} actual_stores_in_moved={ns} skipped_missing_ADDROFF_in_moved={sk}')
        rest=0; detail=[]
        for i in rows:
            u=mp[ids[i]]
            if u not in aa or aa[u]!=bb[u]: continue
            for r in range(starts[i],starts[i+1]):
                t=targets[r];t=canon[t] if t<len(canon) else t
                if t not in present: continue
                tu=mp[t]
                if aa.get(tu,0)!=bb.get(tu,0):
                    rest+=1;detail.append((u,kinds[r],tu))
        print(f'old export: reverse_sites_in_unmoved_descriptors={rest} total_stores_explained={ns+rest} details={detail}')
