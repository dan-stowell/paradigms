// check_p4.c: compare a problem-4 output with the oracle (ref/p4/README.md): the code bytes with oracle/p4_code.bin,
// the function offsets with oracle/p4_fn_off.u64, and the relocations, both sides sorted as (offset, type, symbol,
// addend, width), with oracle/p4_rel_*. Reads OUT/p4_code.out.bin, p4_fn_off.out.u64, p4_rel_*.out.*.
// Prints P4 OK or P4 FAIL.   check_p4 DATA OUT
#include "../../ref/flat/flat.h"
typedef struct { u64 off; i64 add; u32 sym; u16 type; u8 siz; } Rel;
static int cmpRel(const void *a, const void *b) {
	const Rel *x = a, *y = b;
	if (x->off != y->off) return x->off < y->off ? -1 : 1;
	if (x->type != y->type) return x->type < y->type ? -1 : 1;
	if (x->sym != y->sym) return x->sym < y->sym ? -1 : 1;
	if (x->add != y->add) return x->add < y->add ? -1 : 1;
	return (int)x->siz - (int)y->siz;
}
// the relocation columns DIR/PRE p4_rel_<col>SFX.<type>, sorted
static Rel *rels(const char *dir, const char *pre, const char *sfx, size_t *n) {
	const char *d = flatDir;
	flatDir = dir;
	char q[512];
	size_t b;
#define COL(T, name, ty) (snprintf(q, sizeof q, "%sp4_rel_%s%s.%s", pre, name, sfx, ty), FMAP(T, q, &b))
	const u64 *o = COL(u64, "off", "u64");
	*n = b / 8;
	const u16 *t = COL(u16, "type", "u16");
	const u32 *s = COL(u32, "sym", "u32");
	const i64 *a = COL(i64, "add", "i64");
	const u8 *z = COL(u8, "siz", "u8");
#undef COL
	flatDir = d;
	Rel *r = malloc(sizeof(Rel) * (*n + 1));
	for (size_t i = 0; i < *n; i++) r[i] = (Rel){o[i], a[i], s[i], t[i], z[i]};
	qsort(r, *n, sizeof(Rel), cmpRel);
	return r;
}
int main(int argc, char **argv) {
	if (argc < 3) { fprintf(stderr, "usage: check_p4 DATA OUT\n"); return 2; }
	const char *data = argv[1], *out = argv[2];
	size_t nw, ng, nf, nfo;
	flatDir = data;
	const u8 *want = FMAP(u8, "oracle/p4_code.bin", &nw);
	const u64 *wfn = FMAP(u64, "oracle/p4_fn_off.u64", &nfo);
	flatDir = out;
	const u8 *got = FMAP(u8, "p4_code.out.bin", &ng);
	const u64 *gfn = FMAP(u64, "p4_fn_off.out.u64", &nf);
	int bad = 0;
	long nd = ng != nw;
	for (size_t i = 0; i < nw && i < ng; i++) nd += got[i] != want[i];
	printf("code: %zu bytes (oracle %zu), %ld differ\n", ng, nw, nd);
	bad |= nd != 0;
	long fd = nf != nfo;
	for (size_t i = 0; i < nfo / 8 && i < nf / 8; i++) fd += gfn[i] != wfn[i];
	printf("function offsets: %zu (oracle %zu), %ld differ\n", nf / 8, nfo / 8, fd);
	bad |= fd != 0;
	size_t nr, nwr;
	Rel *r = rels(out, "", ".out", &nr), *w = rels(data, "oracle/", "", &nwr);
	long rd = nr != nwr;
	for (size_t i = 0; i < nr && i < nwr; i++) rd += cmpRel(&r[i], &w[i]) != 0;
	long dup = 0;
	for (size_t i = 1; i < nwr; i++) dup += cmpRel(&w[i - 1], &w[i]) == 0;
	printf("relocations: %zu (oracle %zu, %ld duplicates in it), %ld differ\n", nr, nwr, dup, rd);
	bad |= rd != 0;
	printf(bad ? "P4 FAIL\n" : "P4 OK\n");
	return bad;
}
