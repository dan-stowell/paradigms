// check_p3.c: compare an image with oracle/p3_image.bin on every placed symbol's [addr, addr+size) (README 3b),
// and optionally a 3a address array with oracle/p3_addr.u32.   check_p3 DATA IMAGE [ADDR.u32]
#include "../../ref/flat/flat.h"
int main(int argc, char **argv) {
	if (argc < 3) { fprintf(stderr, "usage: check_p3 DATA IMAGE [ADDR.u32]\n"); return 2; }
	flatDir = argv[1];
	u32 L = (u32)metaGet("p3_placed"), base = (u32)metaGet("imageBase");
	size_t nw, ng, na;
	const u8 *want = FMAP(u8, "oracle/p3_image.bin", &nw), *b = FMAP(u8, "p3_bucket.u8", NULL);
	const u32 *addr = FMAP(u32, "oracle/p3_addr.u32", NULL), *size = FMAP(u32, "p3_size.u32", NULL);
	int bad = 0;
	if (argc > 3) {
		const char *d = flatDir;
		flatDir = ".";
		const u32 *got = FMAP(u32, argv[3], &na);
		flatDir = d;
		long n = 0;
		for (u32 i = 0; i < L; i++) n += i >= na / 4 || got[i] != addr[i];
		printf("3a: %ld of %u addresses differ%s\n", n, L, na / 4 != L ? " (length differs)" : "");
		bad |= n != 0 || na / 4 != L;
	}
	flatDir = ".";
	const u8 *got = FMAP(u8, argv[2], &ng);
	long n = 0;
	for (u32 i = 0; i < L; i++) {
		if (b[i] >= 13) continue;
		size_t o = addr[i] - base;
		if (o + size[i] > ng || memcmp(got + o, want + o, size[i])) n++;
	}
	printf("3b: %ld of %u placed symbols differ\n", n, L);
	bad |= n != 0;
	printf(bad ? "P3 FAIL\n" : "P3 OK\n");
	return bad;
}
