import re,pathlib
p=pathlib.Path('results/codex-kube-goose-bench.log')
s=p.read_text(); rows={}
for cell,t,a,b,c in re.findall(r'^(\S+)\s+([18]) warm\s+([\d.]+) /\s+([\d.]+) /\s+([\d.]+)',s,re.M):rows[cell,int(t)]=(a,b,c)
base={'p1a':('79–115','37–57',97,47),'p1b':('39–57','42–67',48,54.5),'p1c':('97–118','39–47',107.5,43),'p2-csr':('41–49','22–32',45,27),'p2-lazy':('74–79','37–49',76.5,43),'p3a':('15–26','17–31',20.5,24),'p3b-4k':('87–99','33–40',93,36.5),'p3b-2m':('41–42','17–24',41.5,20.5),'p3b-gather':('N/A','142–171',None,156.5)}
out=['## Goose','','Oracle: **OK** for all cells, including the serial P1→P3 pipeline. Checks compare every u32 output and placed symbol bytes; raw commands and results: [check log](codex-kube-goose-check.log). Serial p1/p2/p3 ignore a numeric trailing argument (p3 writes an optional image there); only `*par` commands select workers. The lazy serial numeric argument selects repetitions.','','Build: `goose build p.goose -o bin/p` for p1, p1par, p2, p2lazy, p2par, p3, p3par. Benchmark: `OMP_WAIT_POLICY=passive systemd-run --user --scope -p MemoryMax=10G bash systems/goose/bench.sh 5 /home/exedev/paradigms/data/kube-apiserver`. Five interleaved rounds; each round uses the bench’s best in-process repetition, except layout and gather (one sample per round). 8-worker relocate excludes gathering the worker segments; gather is separate.','','| Cell | Oracle | kube 1 thread min/med/max | kube 8 workers min/med/max | GRID MinIO 1 / 8 | kube/MinIO median 1 / 8 |','|---|---|---|---|---|---|']
for cell,b in base.items():
 vals=[]; ratios=[]
 for t,den in [(1,b[2]),(8,b[3])]:
  v=rows.get((cell,t)); vals.append(' / '.join(v) if v else 'N/A'); ratios.append(f'{float(v[1])/den:.2f}×' if v and den else 'N/A')
 out.append(f'| {cell} | OK | {vals[0]} | {vals[1]} | {b[0]} / {b[1]} | {ratios[0]} / {ratios[1]} |')
out+=['','Load and UTC from [bench log](codex-kube-goose-bench.log):','```',* [x for x in s.splitlines() if x.startswith(('goose bench:','round ','done ')) or 'load average:' in x],'```','','Goose overlapped DuckDB checks/bench and external builders. Historical ratios therefore include load differences as well as dataset differences. No oracle failures. Explicit huge-page checks also pass at 1/8 workers: [log](codex-kube-goose-huge-check.log), with nonzero huge-page byte counts.']
pathlib.Path('results/codex-kube-goose.md').write_text('\n'.join(out)+'\n')
