import re, statistics, pathlib
R=pathlib.Path('results')
f=(R/'codex-kube-futhark-bench.log')
if f.exists():
 data={}
 for line in f.read_text().splitlines():
  m=re.match(r'(\S+)\s+(\S+)\s+warm\s+min\s+([\d.]+)\s+med\s+([\d.]+)\s+max\s+([\d.]+)',line)
  if m: data.setdefault((m[1],m[2]),[]).append(float(m[4]))
 print('## Futhark\n')
 print('Warm ms; min / median / max of five process medians. All configurations request huge pages.\n')
 print('| cell | config | oracle | kube warm | rounds | GRID MinIO | kube / MinIO |')
 print('|---|---|---|---|---|---|---|')
 base={'1a':(79,24),'1b_scan':(47,18),'1c':(111,38),'2':(97,58),'2lazy':(165,90),'3a':(22,10),'3a_loop':(14,7.3),'3b':(107,41)}
 for (cell,cfg),v in data.items():
  t=0 if cfg.startswith('1') else 1
  b=base.get(cell,(None,None))[t]
  print(f'| {cell} | {cfg} | OK | {min(v):.2f} / {statistics.median(v):.2f} / {max(v):.2f} | {len(v)} | {b if b else "—"} | {statistics.median(v)/b:.2f}x |' if b else f'| {cell} | {cfg} | OK | {min(v):.2f} / {statistics.median(v):.2f} / {max(v):.2f} | {len(v)} | — | — |')
h=R/'codex-kube-halide-bench.log'
if h.exists():
 print('\n## Halide\n')
 print('| cell / schedule / mapping | threads | oracle | kube warm min / median / max ms | GRID MinIO | kube / MinIO |')
 print('|---|---|---|---|---|---|')
 for line in h.read_text().splitlines():
  m=re.match(r'p[123] threads=(\d) warm (.*?)\s+min\s+([\d.]+)\s+med\s+([\d.]+)\s+max\s+([\d.]+)',line)
  if not m: continue
  t,cell,lo,med,hi=m.groups(); t=int(t); cell=" ".join(cell.split()); b=None
  ranges={'1a':((81,93),(19.8,27)), '1b':((42.7,46),(9.9,10.9)), '1c':((122,133),(33,36.5))}
  for prefix,bs in ranges.items():
   if cell.startswith(prefix): b=bs[t==8]
  if t==8:
   if cell.startswith('2 flood_t'): b=(79,90)
   elif cell.startswith('2 flood_f'): b=(70,127)
   elif cell.startswith('3a layout_c3_w1'): b=(20,23)
   elif cell.startswith('3b reloc_f0_pt_v32 warm'): b=(24.9,27)
   elif cell.startswith('3b reloc_f0_pt_v32 fresh 2MB'): b=(27,30.8)
  bs=f'{b[0]}–{b[1]}' if b else '—'; rat=f'{float(med)/(sum(b)/2):.2f}x' if b else '—'
  print(f'| {cell} | {t} | OK | {lo} / {med} / {hi} | {bs} | {rat} |')
