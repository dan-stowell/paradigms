# relocation kind x width histogram for problem 3b (lead). python3 -I results/relkinds.py data/minio
import sys, array, collections
D = sys.argv[1] + '/'
def rd(f, t):
    a = array.array(t); a.frombytes(open(D + f, 'rb').read()); return a
k, s, t, a = rd('p3_rel_kind.u8', 'B'), rd('p3_rel_siz.u8', 'B'), rd('p3_rel_tgt.u32', 'I'), rd('p3_rel_add.i64', 'q')
n = len(k); print(D, n, 'relocations')
for (kk, ss), c in sorted(collections.Counter(zip(k, s)).items(), key=lambda x: -x[1]):
    print(f'  kind {kk} siz {ss}: {c} ({100*c/n:.1f}%)')
cnt = sorted(collections.Counter(t).values(), reverse=True)
print('  distinct targets', len(cnt), 'top 100 cover %.3f' % (sum(cnt[:100]) / n), 'top 1000 %.3f' % (sum(cnt[:1000]) / n),
      'addend==0 %.3f' % (sum(1 for x in a if x == 0) / n))
