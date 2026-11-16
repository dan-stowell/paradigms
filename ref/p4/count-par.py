#!/usr/bin/env python3
"""Untimed logical traffic counts; no third-party modules, input files read-only."""
from array import array
from pathlib import Path
import sys
p = Path(sys.argv[1] if len(sys.argv) > 1 else '/home/exedev/paradigms/data/minio-p4')
def col(name, kind):
    a = array(kind)
    with (p / name).open('rb') as f:
        a.frombytes(f.read())
    if sys.byteorder != 'little':
        a.byteswap()
    return a
meta = dict(zip(*(iter((p / 'meta.txt').read_text().split()),) * 2))
meta = {k: int(v) for k, v in meta.items()}
node = col('node_tmpl.u32', 'I')
ends = col('tmpl_hole_off.u32', 'I')
enc = col('hole_enc.u8', 'B')
width = col('hole_w.u8', 'B')
freq = [0] * meta['ntmpl']
for t in node:
    freq[t] += 1
counts, writes = [0]*4, [0]*4
for t, n in enumerate(freq):
    for h in range(ends[t], ends[t+1]):
        counts[enc[h]] += n
        if enc[h] != 3:
            writes[enc[h]] += n * width[h]
glueoff = col('glue.off.u64', 'Q')
kind, ref = col('item_kind.u8', 'B'), col('item_ref.u32', 'I')
glue = sum(glueoff[r+1] - glueoff[r] for k, r in zip(kind, ref) if k == 0)
print('items', meta['nitem'], 'glue items', kind.count(0), 'node segments', kind.count(1))
print('copy bytes', meta['code_bytes'], 'glue', glue, 'templates', meta['code_bytes']-glue)
print('hole counts integer/rel32/relocation', counts[1:], 'total', sum(counts))
print('patched bytes integer/rel32', writes[1:3], 'total', sum(writes))
print('relocations', meta['nrel'], 'node', counts[3], 'glue', meta['ngrel'], 'markers', meta['nmark'])
# Narrow, reproducible logical traffic model, deliberately excludes metadata/index reads.
# Every visited operand reads val (8); jumps/symbols read aux (8); jumps read at[target] (8).
# Rel is 24 bytes here: private write, concatenation read, final write.
copy = meta['code_bytes']
operand = 8*sum(counts) + 8*(counts[2]+counts[3]) + 8*counts[2]
rel_input = 19*meta['ngrel'] + 18*meta['nmark']
rel = 24*meta['nrel']
read = copy + operand + rel_input + rel
write = copy + sum(writes) + 2*rel
print('model read bytes', read, 'write bytes', write, 'operand/target reads', operand, 'glue/marker reads', rel_input)
print('model read ms', read/36e6, 'write ms', write/27e6, 'overlapped bound ms', max(read/36e6, write/27e6), 'serial service ms', read/36e6+write/27e6)
print('copy-only read ms', copy/36e6, 'write ms', copy/27e6, 'overlapped bound ms', copy/27e6)
