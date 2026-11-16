#!/usr/bin/env python3
import pathlib, re, json, statistics, csv
root=pathlib.Path(__file__).resolve().parent/'review5-c-raw'
rows=[]
for p in sorted(root.glob('*.json')):
 meta=json.loads(p.read_text()); log=p.with_suffix('.log').read_text()
 assert meta['exit']==0 and 'P1 OK' in log
 counts={}
 for row in csv.reader(p.with_suffix('.csv').open()):
  if len(row)>=5:
   assert float(row[4])==100
   counts[row[2]]=int(row[0])
 assert counts['dTLB-load-misses']==counts['ls_l1_d_tlb_miss.all_l2_miss:u']
 minor=int(re.search(r'join minor faults: (\d+)',log)[1]); assert counts['page-faults:u']==minor
 mean=float(re.search(r'mean: join ([\d.]+)',log)[1]); assert mean*64>=1000
 first,first_faults=re.search(r'first join: ([\d.]+) ms, (\d+)',log).groups()
 huge=int(re.search(r'Private_Hugetlb=(\d+)',log)[1]); assert huge==(192512 if 'huge' in p.name else 0)
 rows.append(dict(name=p.stem[2:],mean=mean,first=float(first),first_faults=int(first_faults),faults=minor,walks=counts['dTLB-load-misses']/64,l1=counts['ls_l1_d_tlb_miss.all:u']/64,load=float(re.search(r'load average: ([\d.]+)',meta['uptime'])[1]),utc=meta['utc']))
def triple(v):return ' / '.join(f'{x:.3f}' for x in [min(v),statistics.median(v),max(v)])
print('| Mode | Mean join ms | First join ms | First faults | L1 dTLB misses/join | Walk requests/join | Total faults/64 joins |')
print('|---|---|---|---|---|---|---|')
for name in ['base-cold','huge-cold','base-warm','huge-warm']:
 group=[r for r in rows if r['name']==name]; assert len(group)==5
 print('| '+name+' | '+' | '.join(triple([r[k] for r in group]) for k in ['mean','first','first_faults','l1','walks','faults'])+' |')
print('Window:',min(r['utc'] for r in rows),'to last start',max(r['utc'] for r in rows))
print('Load:',min(r['load'] for r in rows),max(r['load'] for r in rows))
print('Min accumulated join seconds:',min(r['mean']*64/1000 for r in rows))
