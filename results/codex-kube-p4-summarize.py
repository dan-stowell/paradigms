#!/usr/bin/env python3
"""Validate and summarize the five outer rounds (each sample is a process minimum)."""
import json, pathlib, re, statistics
R=pathlib.Path(__file__).resolve().parent
raw=R/'codex-kube-p4-raw'
archive=R/'codex-kube-p4-raw.json'
all_records=json.loads(archive.read_text()) if archive.exists() else {p.stem:json.loads(p.read_text()) for p in raw.glob('*.json')}
records={k:v for k,v in all_records.items() if k.startswith('r')}
def val(tag,pattern):
    x=records[tag]
    assert x['returncode']==0,tag
    m=re.search(pattern,x['output'],re.M)
    assert m,(tag,pattern)
    return float(m[1])
def series(system,d,t,phase):
    a=[]
    for r in range(1,6):
        tag=f'r{r}-{d}-p4-{t}-{system}'
        if system in ('p4','p4par'):
            p=r'lay out ([\d.]+)' if phase=='layout' else r'emit ([\d.]+)'
        elif system in ('4bf','4D'):
            if phase=='layout':tag=f'r{r}-{d}-p4-{t}-4lay';cell='4lay'
            else:cell='prepare' if phase=='prep' else system
            p=rf'^{cell}\s+.*?\| min ([\d.]+)'
        elif system=='halide':
            tag+='-layout' if phase=='layout' else '-emit'
            name={'layout':'lay_t_lf','emit':'emit_t_i2_v32_p1','prep':'templates'}[phase]
            p=rf'^4 {name}\s+min\s+([\d.]+)'
        else:raise ValueError(system)
        a.append(val(tag,p))
    return a

def fmt(a):return ' / '.join(f'{x:.2f}' for x in (min(a),statistics.median(a),max(a)))
lines=['| System | Dataset | Threads | Emit ms min / med / max | Layout ms min / med / max | Preparation ms min / med / max | Kube/MinIO median: emit; layout; prep |',
       '|---|---|---:|---:|---:|---:|---|']
for system,label in [('p4','C p4'),('p4par','C p4par'),('4bf','Futhark 4bf'),('4D','Futhark 4D'),('halide','Halide scatter'),('goose','Goose')]:
    for t in ([1] if system=='p4' else [1,8]):
        phases=['emit','layout'] if system in ['p4','p4par','4bf'] else ['emit','prep'] if system=='4D' else ['emit','layout','prep']
        for d in ['minio','kube']:
            if system=='goose':
                for r in range(1,6):
                    x=records[f'r{r}-{d}-p4-{t}-goose']; assert x['returncode']==1 and 'cannot reserve data stack address space' in x['output']
                fields=['failed before load']+['—']*3
            else:
                vals={p:series(system,d,t,p) for p in phases}
                ratios='; '.join(f'{statistics.median(vals[p])/statistics.median(series(system,"minio",t,p)):.2f}×' if p in vals else '—' for p in ['emit','layout','prep']) if d=='kube' else '—'
                fields=[fmt(vals[p]) if p in vals else '—' for p in ['emit','layout','prep']]+[ratios]
            lines.append('| '+' | '.join([label,d,str(t),*fields])+' |')
print('\n'.join(lines))
loads=[float(re.search(r'load average: ([\d.]+)',x['uptime'])[1]) for x in records.values()]
print('\nWindow:',min(x['start'] for x in records.values()),'to last start',max(x['start'] for x in records.values()))
print('Load min / median / max:',fmt(loads))
print('Records:',len(records))
