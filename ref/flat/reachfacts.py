#!/usr/bin/env python3
# How much of the eagerly decoded P2 CSR does the flood use?  python3 ref/flat/reachfacts.py data/minio
import sys
from array import array
d = sys.argv[1] if len(sys.argv) > 1 else 'data/minio'
def L(f, t):
    a = array(t); a.frombytes(open(f'{d}/{f}', 'rb').read()); return a
off, dst, live = L('p2_edge_off.u32', 'I'), L('p2_edge_dst.u32', 'I'), L('oracle/p2_live.u32', 'I')
used = sum(off[g + 1] - off[g] for g in live)
dup = sum((off[g + 1] - off[g]) - len(set(dst[off[g]:off[g + 1]])) for g in live)
print(f'{d}: CSR edges {off[-1]}, edges of live sources {used} ({100*used/off[-1]:.1f}%), '
      f'duplicate (same source, same target) among those {dup} ({100*dup/used:.1f}%), live {len(live)}')
