#!/usr/bin/env python3
"""Normalize displacement encodings using Capstone 5.0.6 (pip --target build/cpdeps).
Outputs a deduplicated table, node IDs, and decoded ModRM offsets; input is read-only.
Run PYTHONPATH=build/cpdeps python3 ref/p4/gen_cp1.py DATA build/cp1.
"""
import array
import collections
import pathlib
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

D, OUT = map(pathlib.Path, sys.argv[1:3])
OUT.mkdir(parents=True, exist_ok=True)
def rd(name, ty):
    a = array.array(ty); a.frombytes((D / name).read_bytes()); return a
def wr(name, ty, values):
    (OUT / name).write_bytes(array.array(ty, values).tobytes())
O = rd('tmpl.off.u64', 'Q'); B = (D / 'tmpl.bin').read_bytes()
S = rd('tmpl_seg_off.u32', 'I'); E = rd('tseg_end.u32', 'I')
H = rd('tmpl_hole_off.u32', 'I'); A = rd('hole_at.u32', 'I')
W = rd('hole_w.u8', 'B'); K = rd('hole_kind.u8', 'B')
ENC = rd('hole_enc.u8', 'B'); R = rd('hole_rtype.u16', 'H')
SH = rd('tmpl_shape.u32', 'I'); M = rd('tmpl_mode.u8', 'B')
NT = rd('node_tmpl.u32', 'I')
md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
keys = {}; templates = []; ids = []; counts = collections.Counter(); allends=[]; allso=[0]
for t in range(len(O)-1):
    raw = B[O[t]:O[t+1]]; segs = E[S[t]:S[t+1]]
    disps = {}; start = 0
    # Decode at actual segment/instruction boundaries, never guess backward from a hole.
    for end in segs:
        cursor = start
        for ins in md.disasm(raw[start:end], start):
            assert ins.address == cursor
            cursor += ins.size
            if ins.disp_size:
                # Capstone 5 reports disp_size=2 for 66-prefixed disp32 stores.
                # In 64-bit addressing the ModRM selects 1 or 4, never 2.
                mode = ins.modrm >> 6
                width = 1 if mode == 1 else 4
                assert ins.disp_offset + width <= ins.size
                disps[ins.address + ins.disp_offset] = (ins.address + ins.modrm_offset, width)
        assert cursor == end, (t, start, end, cursor, raw[cursor:end].hex())
        start = end
    mods = {}; expanded = []
    for h in range(H[t], H[t+1]):
        if K[h] not in (1, 2, 3): continue
        at = A[h]; mod, width = disps[at]
        assert width == W[h] and width in (1, 4), (t,h,width,W[h])
        mode = raw[mod] >> 6
        # mod=00/rm=101 is RIP relative; mod=00 SIB/base=101 has no base.
        # Neither can be shrunk by changing mod bits. They must remain fixed width.
        assert mode in (1, 2), ('non-relaxable displacement kind', t,h,raw[mod])
        assert mode == (1 if width == 1 else 2)
        mods[h] = mod
        if width == 1: expanded.append(at)
        counts['sib' if raw[mod] & 7 == 4 else 'non_sib'] += 1
    def shifted(at): return at + 3 * sum(x < at for x in expanded)
    new = bytearray(raw)
    for h, mod in mods.items(): new[mod] = (new[mod] & 63) | 128
    for at in reversed(expanded): new[at:at+1] = bytes(4)
    holes = tuple((shifted(A[h]), 4 if h in mods else W[h], ENC[h], K[h], R[h]) for h in range(H[t], H[t+1]))
    modlist = tuple(shifted(mods[h]) if h in mods else 0xffffffff for h in range(H[t], H[t+1]))
    ends = tuple(shifted(end) for end in segs)
    key = (SH[t], M[t], bytes(new), holes)
    if key not in keys:
        keys[key] = len(templates); templates.append((key, ends, modlist))
    ix = keys[key]
    assert templates[ix][2] == modlist
    allends.extend(ends); allso.append(len(allends))
    ids.append(ix)
nodecount = collections.Counter(); bytecount = collections.Counter()
for t in NT:
    nodecount[ids[t]] += 1; bytecount[ids[t]] += O[t+1]-O[t]
rank = sorted(nodecount, key=lambda x: (-nodecount[x], x))
print('original',len(O)-1,'relaxed',len(templates),'nodes',len(NT),'node_bytes',sum(bytecount.values()))
for k in (256,1024,4096):
    n = sum(nodecount[x] for x in rank[:k]); b = sum(bytecount[x] for x in rank[:k])
    print('top',k,'nodes',n, f'{100*n/len(NT):.6f}%', 'bytes',b,f'{100*b/sum(bytecount.values()):.6f}%')
print('decoded displacement fields in original templates',dict(counts))
bo=[0]; blob=bytearray(); so=[0]; ends=[]; ho=[0]; holes=[]; mods=[]
for key, se, mo in templates:
    blob.extend(key[2]); bo.append(len(blob)); ends.extend(se); so.append(len(ends))
    holes.extend(key[3]); ho.append(len(holes)); mods.extend(mo)
(OUT/'tmpl.bin').write_bytes(blob)
for name,ty,values in [('tmpl.off.u64','Q',bo),('tmpl_seg_off.u32','I',allso),('tseg_end.u32','I',allends),('node_seg_tmpl.u32','I',NT),('tmpl_hole_off.u32','I',ho),('hole_mod.u32','I',mods),('node_tmpl.u32','I',(ids[t] for t in NT))]: wr(name,ty,values)
for col,(name,ty) in enumerate([('hole_at.u32','I'),('hole_w.u8','B'),('hole_enc.u8','B'),('hole_kind.u8','B'),('hole_rtype.u16','H')]): wr(name,ty,(h[col] for h in holes))
print('table_bytes', len(blob),'table_holes',len(holes))
