#!/usr/bin/env python3
"""Streaming join traffic model; excludes data-dependent hash probes and metadata.
32-byte compact records charged at sizeof (includes four padding bytes).
Run under prlimit --as=8589934592 from repo root; no third-party modules.
"""
import collections,json,mmap,pathlib,struct

def mapped(path):
 with open(path,'rb') as f: return mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
for dataset in ['minio','kube-apiserver']:
 d=pathlib.Path('data')/dataset
 kind=mapped(d/'p1_kind.u8'); key=mapped(d/'p1_namekey.u64x2')
 keyed=collections.Counter(); D=kind[1:].count(1); N=len(kind)-1
 groups=set(); candidates=[]; partitions=collections.Counter()
 for g in range(1,N+1):
  k1,k2=struct.unpack_from('<QQ',key,16*g)
  if not k1: continue
  keyed[kind[g]]+=1
  if kind[g] in [4,5]: groups.add((k1,k2)); partitions[k1>>56]+=1
  elif kind[g]==1: candidates.append((k1,k2))
 want=(d/'p1_want.u64x2').read_bytes(); W=len(want)//16
 for k1,k2 in struct.iter_unpack('<QQ',want): groups.add((k1,k2)); partitions[k1>>56]+=1
 H=sum(k in groups for k in candidates)
 A=keyed[1]; M=keyed[4]+keyed[5]; K=A+M; B=M+W; L=K+W; nb=(d/'p1_builtin.u64x2').stat().st_size//16
 S=0
 for p in range(256):
  sz=64
  while sz<2*partitions[p]: sz*=2
  S+=sz
 common=4*(N+1)+4*(M+H)+4*nb+24*S
 # Streaming model: full name-key cache lines per pass, full record
 # extents per traversal; other input/output fields at their logical size.
 # Excludes hash probes/updates and hot count/prefix metadata.
 lines=set(g//4 for g in range(1,N+1) if kind[g]==1)
 Q=len(lines)
 parR=37*N+128*Q+4*(M+H)+28*B+24*M+24*W+16*nb
 compactR=17*N+5*K+96*L+16*W+16*nb
 row=dict(dataset=dataset,N=N,D=D,A=A,M=M,K=K,W=W,B=B,L=L,nb=nb,H=H,S=S,Q=Q,keyed=dict(keyed),p1par_read=parR,p1par_write=common+28*B,p1compact_read=compactR,p1compact_write=common+64*L)
 row['par_key_stream_coverage']=32*N+128*Q
 row['compact_key_stream_coverage']=16*N
 print(json.dumps(row),flush=True)
