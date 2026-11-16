#!/usr/bin/env python3
"""Corrupt only scratch exports; require the independent checker to reject them.
Usage: check-p3inc-negative.py CHECKER A B EXPORT_DIR
"""
import array
import pathlib
import struct
import subprocess
import sys
checker, a, b, export = sys.argv[1:]
e = pathlib.Path(export)
def u32(path):
    x = array.array('I')
    x.frombytes(path.read_bytes())
    return x
pairs = u32(e / 'a.addr')
addresses = dict(zip(pairs[::2], pairs[1::2]))
ap = pathlib.Path(a)
syms = u32(ap / 'p3_sym.u32')
rs = u32(ap / 'p3_rel_start.u32')
offs = u32(ap / 'p3_rel_off.u32')
buckets = (ap / 'p3_bucket.u8').read_bytes()
base = int(dict(line.split() for line in (ap / 'meta.txt').read_text().splitlines())['imageBase'])
idx = next(i for i in range(len(syms)) if rs[i] != rs[i+1] and buckets[i] < 13)
byte_offset = addresses[syms[idx]] - base + offs[rs[idx]]

def corrupt(path, offset, replacement, label):
    with path.open('r+b') as f:
        f.seek(offset)
        original = f.read(len(replacement))
        f.seek(offset)
        f.write(replacement)
    try:
        r = subprocess.run(['prlimit', '--as=8589934592', checker, a, b, export], capture_output=True, text=True)
        if r.returncode not in (1, 2):
            raise RuntimeError(f'{label}: checker did not reject corruption: {r.returncode}\n{r.stdout}\n{r.stderr}')
        print(f'negative {label}: rejected ({r.returncode})')
    finally:
        with path.open('r+b') as f:
            f.seek(offset)
            f.write(original)
with (e / 'a.img').open('rb') as f:
    f.seek(byte_offset)
    original = f.read(1)
corrupt(e / 'a.img', byte_offset, bytes([original[0] ^ 1]), 'relocation byte')
corrupt(e / 'a.addr', 4, struct.pack('<I', pairs[1] + 1), 'misaligned address')
# First two rows are positive-size typelinks in this export; keep alignment but overlap them.
corrupt(e / 'a.addr', 12, struct.pack('<I', pairs[1]), 'overlapping address')
r = subprocess.run(['prlimit', '--as=8589934592', checker, a, b, export])
if r.returncode:
    raise RuntimeError('restored exports failed')
