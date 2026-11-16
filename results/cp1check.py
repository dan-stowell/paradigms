# CP1 check (lead): is a disp hole's width (1 or 4) a function of its operand (fits int8)? python3 -I results/cp1check.py data/minio-p4
import sys, array, collections
D = sys.argv[1] + '/'
def rd(f, t):
    a = array.array(t); a.frombytes(open(D + f, 'rb').read()); return a
ntmpl = rd('tmpl_hole_off.u32', 'I'); hw = rd('hole_w.u8', 'B'); hk = rd('hole_kind.u8', 'B')
nt = rd('node_tmpl.u32', 'I'); nop = rd('node_op.u32', 'I'); ov = rd('op_val.i64', 'q')
c = collections.Counter()
for n in range(len(nt)):
    t = nt[n]; h0, h1 = ntmpl[t], ntmpl[t + 1]; o = nop[n]
    assert nop[n + 1] - o == h1 - h0
    for j in range(h1 - h0):
        k = hk[h0 + j]
        if k in (1, 2, 3):
            v = ov[o + j]; w = hw[h0 + j]
            c[(k, w, -128 <= v <= 127)] += 1
for key in sorted(c): print('kind %d width %d fits-int8 %s: %d' % (key[0], key[1], key[2], c[key]))
bad = sum(v for (k, w, f), v in c.items() if (w == 1) != f)
print('exceptions to width == (fits int8 ? 1 : 4):', bad, 'of', sum(c.values()))
