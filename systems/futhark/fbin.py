#!/usr/bin/env python3
# fbin.py: raw little-endian flat files -> one Futhark binary-format input stream on stdout, for
# `futhark bench` / the compiled executables:   fbin.py u8:FILE u64:FILE i64=VALUE ... > x.in
import sys, struct
out = sys.stdout.buffer
size = {"u8": 1, "u16": 2, "u32": 4, "u64": 8, "i32": 4, "i64": 8}
for a in sys.argv[1:]:
    if "=" in a:
        t, v = a.split("=")
        out.write(b"b\x02\x00" + (" " * (4 - len(t)) + t).encode() + struct.pack("<q", int(v)))
        continue
    t, f = a.split(":", 1)
    d = open(f, "rb").read()
    out.write(b"b\x02\x01" + (" " * (4 - len(t)) + t).encode() + struct.pack("<q", len(d) // size[t]) + d)
