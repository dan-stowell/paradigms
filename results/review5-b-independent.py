#!/usr/bin/env python3
"""Independent range/gap check of a produced image; never fills expected output."""
import array, mmap, pathlib, sys
D=pathlib.Path('/home/exedev/paradigms/data/minio')
def column(path,code):
 a=array.array(code); a.frombytes((D/path).read_bytes()); return a
meta=dict(line.split() for line in (D/'meta.txt').read_text().splitlines())
addr=column('oracle/p3_addr.u32','I'); size=column('p3_size.u32','I'); bucket=column('p3_bucket.u8','B')
path=pathlib.Path(sys.argv[1])
assert path.stat().st_size==int(meta['image_bytes'])
with path.open('rb') as g, (D/'oracle/p3_image.bin').open('rb') as w:
 got=mmap.mmap(g.fileno(),0,access=mmap.ACCESS_READ); want=mmap.mmap(w.fileno(),0,access=mmap.ACCESS_READ)
 spans=[]
 for a,s,b in zip(addr,size,bucket):
  if b>=13:continue
  lo=a-int(meta['imageBase']); hi=lo+s
  assert 0<=lo<=hi<=len(got)
  assert got[lo:hi]==want[lo:hi],f'placed mismatch at {lo}:{hi}'
  spans.append((lo,hi))
 cursor=0; gaps=0
 for lo,hi in sorted(spans):
  if lo>cursor:
   assert not any(got[cursor:lo]),f'nonzero gap at {cursor}:{lo}'
   gaps+=lo-cursor
  cursor=max(cursor,hi)
 assert not any(got[cursor:]),f'nonzero tail at {cursor}'
 gaps+=len(got)-cursor
 print(f'Independent range/gap check OK: {len(got)} bytes, {len(spans)} placed spans, {gaps} gap bytes all zero')
