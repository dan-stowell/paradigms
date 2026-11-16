#!/usr/bin/env python3
"""Independently compare all six method columns after the supplied ID mappings.
Run: prlimit --as=8589934592 python3 results/c-incremental-method-check.py [p2inc]
Optional p2inc also checks its unsupported-method rejection on a local fixture.
"""
from array import array
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

DATA = Path('/home/exedev/paradigms/data')
HERE = Path(__file__).resolve().parents[1]


def u32(path):
    a = array('I')
    a.frombytes(path.read_bytes())
    if sys.byteorder != 'little':
        a.byteswap()
    return a


def rows(path, mapped=False):
    off = u32(path / 'p2_meth_off.u32')
    columns = [u32(path / f'p2_meth_{c}.u32') for c in ('name', 'mtyp', 'ifn', 'tfn')]
    exported = (path / 'p2_meth_exported.u8').read_bytes()
    assert all(len(c) == len(exported) for c in columns)
    assert off[-1] == len(exported)
    if mapped:
        ids = u32(path / 'diff/idmap_b.u32')
        strings = u32(path / 'diff/strmap_b.u32')
        ident, string = ids.__getitem__, strings.__getitem__
    else:
        ident = string = lambda x: x
    result = set()
    for owner in range(len(off) - 1):
        for m in range(off[owner], off[owner + 1]):
            result.add((ident(owner), string(columns[0][m]),
                        *(ident(c[m]) for c in columns[1:]), exported[m]))
    return result, len(exported)


a, na = rows(DATA / 'minio')
print(f'A: {na} raw rows, {len(a)} distinct rows')
for edit in ('minio-edit', 'minio-edit2'):
    b, nb = rows(DATA / edit, True)
    print(f'{edit}: {nb} raw rows, {len(b)} distinct mapped rows; actual minus {len(a-b)}, plus {len(b-a)}')
    assert a == b
    for side in ('minus', 'plus'):
        cols = list((DATA / edit / 'diff').glob(f'meth_{side}_*'))
        assert len(cols) == 6
        assert all(p.stat().st_size == 0 for p in cols)
        print(f'  meth_{side}: all six columns empty (0 bytes)')

if len(sys.argv) > 1:
    binary = str(Path(sys.argv[1]).resolve())
    (HERE / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='method-reject-', dir=HERE / 'build') as tmp:
        diff = Path(tmp) / 'diff'
        diff.mkdir()
        (diff / 'meta.txt').write_text('n_unified 4\nnstr_unified 2\n')
        for side in ('minus', 'plus'):
            for suffix in ('edge_%s_src.u32', 'edge_%s_kind.u8', 'edge_%s_val.u64',
                           'flag_%s_id.u32', 'flag_%s_val.u8', 'root_%s.u32'):
                (diff / (suffix % side)).write_bytes(b'')
            for c in ('type', 'name', 'mtyp', 'ifn', 'tfn'):
                (diff / f'meth_{side}_{c}.u32').write_bytes(struct.pack('<I', 1) if side == 'minus' else b'')
            (diff / f'meth_{side}_exported.u8').write_bytes(b'\x01' if side == 'minus' else b'')
        result = subprocess.run([binary, str(DATA / 'minio'), tmp, '1'], capture_output=True, text=True)
        assert result.returncode == 3, result
        assert 'method-table diffs are not supported' in result.stderr
        print('Nonempty method-row fixture: exit 3, explicit unsupported diagnostic; no result silently returned')
print('METHOD CHECK OK')
