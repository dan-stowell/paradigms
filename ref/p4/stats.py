#!/usr/bin/env python3
"""Where the byte templates of problem 4 come from: how many per typed shape, and what splits them.
    python3 -I ref/p4/stats.py data/minio-p4
"""
import sys
from array import array
from collections import Counter, defaultdict

D = sys.argv[1]


def A(t, f):
    a = array(t)
    a.frombytes(open(D + "/" + f, "rb").read())
    return a


def B(f):
    return open(D + "/" + f, "rb").read()


def S(f):
    off = A('Q', f + ".off.u64")
    b = B(f + ".bin")
    return [b[off[i]:off[i + 1]].decode() for i in range(len(off) - 1)]


shape, mode = S("shape"), S("mode")
nt = A('I', "node_tmpl.u32")
ts, tm = A('I', "tmpl_shape.u32"), B("tmpl_mode.u8")
toff, tbin = A('Q', "tmpl.off.u64"), B("tmpl.bin")
tso, te = A('I', "tmpl_seg_off.u32"), A('I', "tseg_end.u32")
tho, hat, hw, hk = A('I', "tmpl_hole_off.u32"), A('I', "hole_at.u32"), B("hole_w.u8"), B("hole_k" + "ind.u8")
cnt = Counter(nt)
N = len(nt)


def curve(counts):
    l = sorted(counts, reverse=True)
    acc, out = 0, []
    for i, c in enumerate(l):
        acc += c
        if i + 1 in (256, 1024, 4096, 16384, 65536):
            out.append("top %d %.1f%%" % (i + 1, 100 * acc / N))
    return "%d distinct; %s" % (len(l), " ".join(out))


print("nodes by mode:", ", ".join("%s %.1f%%" % (m, 100 * c / N) for m, c in Counter(mode[tm[t]] for t in nt).most_common()))
by = defaultdict(list)
for t, c in cnt.items():
    by[(ts[t], tm[t])].append((c, t))
w = Counter()
for k, v in by.items():
    n = len(v)
    w[1 if n == 1 else 2 if n == 2 else 10 if n <= 10 else 100 if n <= 100 else 101] += sum(c for c, _ in v)
print("nodes whose (shape, mode) has 1 / 2 / 3-10 / 11-100 / >100 templates: " +
      " / ".join("%.1f%%" % (100 * w[k] / N) for k in (1, 2, 10, 100, 101)))


def length_and_holes(t):
    return (toff[t + 1] - toff[t], tso[t + 1] - tso[t], tuple((hk[h], hw[h]) for h in range(tho[t], tho[t + 1])))


def hole_kinds(t):
    return (tso[t + 1] - tso[t], tuple(hk[h] for h in range(tho[t], tho[t + 1])))


print("byte templates:", curve(cnt.values()))
for name, f in (("length + hole kinds and widths (bytes ignored)", length_and_holes),
                ("segment count + hole kinds (bytes and widths ignored)", hole_kinds)):
    c2 = Counter()
    for t, c in cnt.items():
        c2[(ts[t], tm[t], f(t))] += c
    print("keyed by shape + mode + %s: %s" % (name, curve(c2.values())))
big = sorted(by.items(), key=lambda kv: -sum(c for c, _ in kv[1]))[:15]
print("\nthe commonest (shape, mode) keys, with their three commonest templates (bytes, segment ends, holes as (at, kind, width)):")
for (s, m), v in big:
    v.sort(reverse=True)
    tot = sum(c for c, _ in v)
    print("%7d  %-48s @%-7s %5d templates, the commonest %.0f%%" % (tot, shape[s][:48], mode[m], len(v), 100 * v[0][0] / tot))
    for c, t in v[:3]:
        b = tbin[toff[t]:toff[t + 1]]
        print("         %6d  %s  segs %s holes %s" % (c, b.hex()[:72], list(te[tso[t]:tso[t + 1]])[:6],
                                                    [(hat[h], hk[h], hw[h]) for h in range(tho[t], tho[t + 1])][:5]))

# bytes: how much of the node bytes the commonest templates write, and nodes that write nothing themselves
tl = {t: toff[t + 1] - toff[t] for t in cnt}
nbytes = sum(tl[t] * c for t, c in cnt.items())
zero = sum(c for t, c in cnt.items() if tl[t] == 0)
l = sorted(cnt.items(), key=lambda kv: -kv[1])
acc, out = 0, []
for i, (t, c) in enumerate(l):
    acc += tl[t] * c
    if i + 1 in (256, 1024, 4096, 16384, 65536):
        out.append("top %d %.1f%%" % (i + 1, 100 * acc / nbytes))
print("\nnode bytes %d; templates by node count cover of those bytes: %s" % (nbytes, " ".join(out)))
print("nodes that write no bytes of their own (the value is a location the parent uses): %d (%.1f%%)" % (zero, 100 * zero / N))
