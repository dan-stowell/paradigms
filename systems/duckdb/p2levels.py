# P2 levels: BFS level sizes of the plain closure from the roots (round 1), from the flat files. python3 -I p2levels.py
import array
D='/home/exedev/paradigms/data/minio/'
def rd(f,t):
    a=array.array(t); a.frombytes(open(D+f,'rb').read()); return a
off=rd('p2_edge_off.u32','I'); dst=rd('p2_edge_dst.u32','I'); k=rd('p2_edge_kind.u8','B'); roots=rd('p2_roots.u32','I')
seen=set(r for r in roots if r); fr=list(seen); lv=[]
while fr:
    lv.append(len(fr)); nx=[]
    for s in fr:
        if s+1 < len(off):
            for e in range(off[s],off[s+1]):
                if k[e]==0:
                    t=dst[e]
                    if t and t not in seen: seen.add(t); nx.append(t)
    fr=nx
print(len(lv), lv)
