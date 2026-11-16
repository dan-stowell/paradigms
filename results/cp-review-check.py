#!/usr/bin/env python3
"""Independent synthetic CP1 decoder/normalizer regression; writes only /tmp."""
import array, os, pathlib, subprocess, sys, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
d=pathlib.Path(tempfile.mkdtemp(prefix='cp1-review-',dir='/tmp')); out=d/'normalized'
def write(name,ty,values): (d/name).write_bytes(array.array(ty,values).tobytes())
def read(name,ty):
    a=array.array(ty); a.frombytes((out/name).read_bytes()); return a
# BP and SIB both widths; RIP and no-base SIB relocation fields must stay fixed.
raw=[bytes.fromhex(x) for x in ['488b4500','488b8500000000','488b442400','488b842400000000','488b0500000000','488b042500000000','66898500000000']]
ends=[0]
for b in raw: ends.append(ends[-1]+len(b))
(d/'tmpl.bin').write_bytes(b''.join(raw)); write('tmpl.off.u64','Q',ends)
write('tmpl_seg_off.u32','I',range(8)); write('tseg_end.u32','I',map(len,raw))
write('tmpl_hole_off.u32','I',range(8)); write('hole_at.u32','I',[3,3,4,4,3,4,3])
write('hole_w.u8','B',[1,4,1,4,4,4,4]); write('hole_kind.u8','B',[1,1,2,2,7,7,1])
write('hole_enc.u8','B',[1,1,1,1,3,3,1]); write('hole_rtype.u16','H',[0,0,0,0,1,1,0])
write('tmpl_shape.u32','I',[0]*7); write('tmpl_mode.u8','B',[0]*7); write('node_tmpl.u32','I',range(7))
env=dict(os.environ,PYTHONPATH=str(root/'build/cpdeps'))
subprocess.run([sys.executable,str(root/'ref/p4/gen_cp1.py'),str(d),str(out)],env=env,check=True)
ids=read('node_tmpl.u32','I'); assert list(ids)==[0,0,1,1,2,3,4],list(ids)
assert list(read('hole_mod.u32','I'))==[2,2,0xffffffff,0xffffffff,2]
o=read('tmpl.off.u64','Q'); b=(out/'tmpl.bin').read_bytes()
assert [b[o[i]:o[i+1]] for i in range(5)]==[raw[1],raw[3],raw[4],raw[5],raw[6]]
print('PASS: BP, SIB, RIP, no-base SIB, 66-prefix width normalization; fixture',d)
