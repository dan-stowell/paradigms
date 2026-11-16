// p4ord.c: are glue ids and nodes laid out in item (function) order? (so a worker can read ranges)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static void *rd(const char *n, long *sz) {
	char p[512];
	sprintf(p, "/home/exedev/paradigms/data/minio-p4/%s", n);
	FILE *f = fopen(p, "rb");
	fseek(f, 0, 2); *sz = ftell(f); rewind(f);
	void *b = malloc(*sz);
	if (fread(b, 1, *sz, f) != (size_t)*sz) exit(1);
	fclose(f);
	return b;
}
int main(void) {
	long a, b, c, d;
	uint8_t *k = rd("item_kind.u8", &a);
	uint32_t *r = rd("item_ref.u32", &b), *s = rd("item_seg.u32", &c), *nf = rd("node_fn.u32", &d);
	long nextg = 0, badg = 0, nextn = 0, badn = 0, maxseg = 0, nn = d / 4, nzero = 0;
	for (long i = 0; i < a; i++) {
		if (k[i] == 0) { if (r[i] != nextg) badg++; nextg = r[i] + 1; }
		else {
			if (s[i] == 0) { if (r[i] != nextn) badn++; nextn = r[i] + 1; }
			if (s[i] > maxseg) maxseg = s[i];
		}
	}
	long bnf = 0;
	for (long i = 1; i < nn; i++) if (nf[i] < nf[i - 1]) bnf++;
	printf("glue out of order %ld (last %ld), node seg0 out of seq %ld (next %ld of %ld), node_fn unsorted %ld, maxseg %ld\n",
	       badg, nextg, badn, nextn, nn, bnf, maxseg);
	return 0;
}
