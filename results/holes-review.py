#!/usr/bin/env python3
"""Independent flat-column count, no prepared C records or oracle input."""
import mmap, pathlib, sys, collections, bisect
D=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else '/home/exedev/paradigms/data/minio-p4')
def col(name, code):
    with (D/name).open('rb') as f:
        return memoryview(mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)).cast(code)
fi=col('fn_item.u32','I'); ik=col('item_kind.u8','B'); ir=col('item_ref.u32','I'); seg=col('item_seg.u32','I')
nt=col('node_tmpl.u32','I'); no=col('node_op.u32','I'); ov=col('op_val.i64','q')
to=col('tmpl.off.u64','Q'); tb=col('tmpl.bin','B'); tso=col('tmpl_seg_off.u32','I'); te=col('tseg_end.u32','I')
tho=col('tmpl_hole_off.u32','I'); ha=col('hole_at.u32','I'); hw=col('hole_w.u8','B'); he=col('hole_enc.u8','B')
go=col('glue.off.u64','Q'); gro=col('glue_rel_off.u32','I'); grw=col('grel_siz.u8','B'); gra=col('grel_off.u32','I')
width=collections.Counter(); enc=collections.Counter(); previous_same=zero=union=0
pairs=touch=near=fit=ifit=0; groups=merged=0
for f in range(len(fi)-1):
    prior={}; holes=[]; pos=0
    for i in range(fi[f],fi[f+1]):
        r=ir[i]
        if not ik[i]:
            for h in range(gro[r],gro[r+1]):
                w=grw[h]; width[w]+=1; enc[3]+=1; holes.append((pos+gra[h],w,3))
            pos+=go[r+1]-go[r]
        else:
            t=nt[r]; b=tso[t]; j=seg[i]; lo=te[b+j-1] if j else 0; hi=te[b+j]
            h0=bisect.bisect_left(ha,lo,tho[t],tho[t+1]); h1=bisect.bisect_left(ha,hi,h0,tho[t+1])
            for h in range(h0,h1):
                w=hw[h]; e=he[h]; width[w]+=1; enc[e]+=1; holes.append((pos+ha[h]-lo,w,e))
                if e==1:
                    v=ov[no[r]+h-tho[t]]; eq=h in prior and prior[h]==v
                    z=bytes(tb[to[t]+ha[h]:to[t]+ha[h]+w])==(v&((1<<(8*w))-1)).to_bytes(w,'little')
                    previous_same+=eq; zero+=z; union+=eq or z; prior[h]=v
            pos+=hi-lo
    writable=sorted(x for x in holes if x[2]!=3)
    for a,b in zip(writable,writable[1:]):
        gap=b[0]-a[0]-a[1]
        if gap<0: continue
        pairs+=1; touch+=gap==0; near+=gap<=8
        ok=b[0]+b[1]-a[0]<=8 and a[0]+8<=pos
        fit+=ok; ifit+=ok and a[2]==b[2]==1
    j=0
    while j<len(holes):
        a=holes[j]; k=j+1
        if a[2]==1 and a[0]+8<=pos:
            while k<len(holes) and holes[k][2]==1 and holes[k][0]>=a[0] and holes[k][0]+holes[k][1]<=a[0]+8: k+=1
        if k>j+1: groups+=1; merged+=k-j
        j=k
print('records',sum(enc.values()),'encodings',dict(sorted(enc.items())))
print('widths',dict(sorted(width.items())))
print('previous_same_value',previous_same,'template_bytes_equal',zero,'union',union)
print('pairs',pairs,'touching',touch,'gap_le_8',near,'span_le_8_in_function',fit,'integer_pairs_fit',ifit)
print('H2 groups',groups,'merged_holes',merged,'remaining_records',sum(enc.values())-merged+groups)
