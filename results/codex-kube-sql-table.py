import re,pathlib,statistics
P=pathlib.Path(__file__).parent

def triple(x):return ' / '.join(f'{v:.1f}' for v in x)
def ref(x):return '—' if x is None else str(x) if isinstance(x,(int,float)) else '–'.join(map(str,x))
def ratio(x,b):
 if b is None:return '—'
 return f'{x[1]/(statistics.mean(b) if isinstance(b,tuple) else b):.2f}×'
refsD={'1a':(None,(103,113)),'1b':(None,(272,375)),'1c':(None,(128,190)),'2prep':(None,103),'2loop':(None,(1965,2354)),'3a':(None,(217,263)),'3b':(None,(339,368)),'3b-hugepages':(None,(309,332))}
refsS={'1a':((1928,2034),636),'1b':((5220,5426),1080),'1c':((1625,1865),587),'1c-slot':(None,394),'2':((1227,1467),431),'2-lazy':((2900,3200),960),'3a':((7222,7589),1339),'3b':(None,320)}
for sys,refs in [('duckdb',refsD),('souffle',refsS)]:
 f=P/f'codex-kube-{sys}-bench.txt'
 if not f.exists():continue
 rows={}
 for l in f.read_text().splitlines():
  pat=r'(\S+)\s+(\d+) threads (warm|e2e)\s+min\s+([\d.]+) median\s+([\d.]+) max\s+([\d.]+)' if sys=='duckdb' else r'(\S+)\s+-j(\d+)\s+(warm|e2e|assemble)\s+min\s+([\d.]+)\s+median\s+([\d.]+)\s+max\s+([\d.]+)'
  m=re.match(pat,l)
  if m:
   c,t,k,*v=m.groups();rows.setdefault((c,k),{})[int(t)]=tuple(map(float,v))
 if not rows:continue
 out=[f'## {dict(duckdb="DuckDB", souffle="Soufflé")[sys]}','', '| Cell | Oracle 1 / 8 | kube 1 thread min / median / max ms | kube 8 threads min / median / max ms | GRID MinIO 1 / 8 ms | kube/MinIO 1 / 8 |','|---|---|---|---|---|---|']
 for (c,k),ts in rows.items():
  if k!='warm':continue
  a,b=refs.get(c,(None,None));x=ts.get(1);y=ts.get(8)
  out.append(f'| {c} | OK / OK | {triple(x) if x else "missing"} | {triple(y) if y else "missing"} | {ref(a)} / {ref(b)} | {ratio(x,a) if x else "—"} / {ratio(y,b) if y else "—"} |')
 out+=['','E2e and separate assembly (same five rounds):','','| Cell / metric | 1 thread min / median / max ms | 8 threads min / median / max ms |','|---|---|---|']
 for (c,k),ts in rows.items():
  if k=='warm':continue
  out.append(f'| {c} / {k} | {triple(ts[1]) if 1 in ts else "missing"} | {triple(ts[8]) if 8 in ts else "missing"} |')
 (P/f'codex-kube-{sys}-table.md').write_text('\n'.join(out)+'\n')
