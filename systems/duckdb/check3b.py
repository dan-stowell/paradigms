# check a problem-3b image: every placed symbol's [addr, addr+size) must equal oracle/p3_image.bin (not buckets 13-15)
#   python3 -I check3b.py DATA image.bin
import array, sys
d, img = sys.argv[1], open(sys.argv[2], 'rb').read()
want = open(d + '/oracle/p3_image.bin', 'rb').read()
addr = array.array('I', open(d + '/oracle/p3_addr.u32', 'rb').read())
size = array.array('I', open(d + '/p3_size.u32', 'rb').read())
bucket = open(d + '/p3_bucket.u8', 'rb').read()
base = int([l.split()[1] for l in open(d + '/meta.txt') if l.startswith('imageBase ')][0])
bad = n = 0
for i in range(len(addr)):
    if bucket[i] >= 13: continue
    o = addr[i] - base
    n += size[i]
    if img[o:o + size[i]] != want[o:o + size[i]]:
        bad += 1
        if bad <= 5: print('symbol', i, 'bucket', bucket[i], 'differs')
print('%d bytes checked, %d placed symbols differ' % (n, bad))
print('P3b OK' if bad == 0 else 'P3b FAIL')
sys.exit(bad != 0)
