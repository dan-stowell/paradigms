// Single-threaded flat-file statistics. Build/run commands are in the request;
// BFS uses FIFO level boundaries, retaining p2.c's interface revisits and method rounds.
#include "flat.h"

enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16 };

static u8 *live, *iface, *ifaceDone;
static u32 *stack;
static size_t sp;
static inline void mark(u32 c) {
	if (c && !live[c]) {
		live[c] = 1;
		stack[sp++] = c;
	}
}
static inline void setIface(u32 c) {
	if (c && !iface[c]) {
		iface[c] = 1;
		if (live[c]) stack[sp++] = c;
	}
}

static int reach(void) {
	u32 N = (u32)metaGet("nids"), nsl = (u32)metaGet("nslots");
	size_t nroots, nsig, nstr, nmeth;
	const u8 *fl = FMAP(u8, "p2_flags.u8", NULL), *ek = FMAP(u8, "p2_edge_kind.u8", NULL);
	const u32 *eoff = FMAP(u32, "p2_edge_off.u32", NULL), *ed = FMAP(u32, "p2_edge_dst.u32", NULL);
	const u32 *sigName = FMAP(u32, "p2_sig_name.u32", &nsig), *sigType = FMAP(u32, "p2_sig_type.u32", NULL);
	const u32 *moff = FMAP(u32, "p2_meth_off.u32", NULL), *mName = FMAP(u32, "p2_meth_name.u32", &nmeth);
	const u32 *mTyp = FMAP(u32, "p2_meth_mtyp.u32", NULL), *mIfn = FMAP(u32, "p2_meth_ifn.u32", NULL), *mTfn = FMAP(u32, "p2_meth_tfn.u32", NULL);
	const u8 *mExp = FMAP(u8, "p2_meth_exported.u8", NULL);
	const u32 *roots = FMAP(u32, "p2_roots.u32", &nroots);
	FMAP(u64, "p2_str.off.u64", &nstr);
	nroots /= 4, nsig /= 4, nmeth /= 4, nstr = nstr / 8 - 1;

	u64 levels[6] = {0}, symbols[6] = {0}, total = 0;
	live = calloc(N, 1), iface = calloc(N, 1), ifaceDone = calloc(N, 1);
	stack = malloc(4 * (size_t)N * 3);
	for (u32 g = 1; g <= nsl; g++) iface[g] = (fl[g] & fIface) != 0;
	u8 *sigSeen = calloc(nsig, 1), *named = calloc(nstr + 1, 1);
	// sig ids are (name, type) pairs; a method's (name, mtyp) is looked up in a hash of the live sigs
	u64 hs = 1;
	while (hs < 4 * nsig + 64) hs <<= 1;
	u64 *sigTab = calloc(hs, 8);
	u32 *markable = malloc(4 * (nmeth + 1)), nm = 0;
	int reflectSeen = 0, rounds = 0;
	for (size_t i = 0; i < nroots; i++) mark(roots[i]);
	for (;;) {
		rounds++;
		size_t head = 0;
		u32 depth = 0;
		while (head < sp) {
			size_t end = sp, width = end - head;
			int b = width == 1 ? 0 : width < 16 ? 1 : width < 256 ? 2 : width < 4096 ? 3 : width < 65536 ? 4 : 5;
			levels[b]++, symbols[b] += width, depth++, total++;
			while (head < end) {
			u32 c = stack[head++];
			if (c > nsl || !(fl[c] & fDef)) continue;
			if (fl[c] & fReflect) reflectSeen = 1;
			int uii = (fl[c] & fGoType) && iface[c];
			if (uii) {
				if (ifaceDone[c]) continue;
				ifaceDone[c] = 1;
			}
			for (u32 e = eoff[c]; e < eoff[c + 1]; e++) {
				u32 t = ed[e];
				switch (ek[e]) {
				case 0:
					if (uii && (fl[t] & fGoType) && (fl[t] & fDef) && !iface[t]) {
						iface[t] = 1;
						if (live[t]) stack[sp++] = t;
					}
					mark(t);
					break;
				case 1: setIface(t); break;
				case 2:
					if (!sigSeen[t]) {
						sigSeen[t] = 1;
						u64 k = (u64)sigName[t] << 32 | sigType[t];
						for (u64 h = mix64(k) & (hs - 1);; h = (h + 1) & (hs - 1)) {
							if (sigTab[h] == 0) { sigTab[h] = k + 1; break; }
							if (sigTab[h] == k + 1) break;
						}
					}
					break;
				case 3: named[t] = 1; break;
				}
			}
			if (uii)
				for (u32 m = moff[c]; m < moff[c + 1]; m++) {
					setIface(mTyp[m]);
					markable[nm++] = m;
				}
		}
		}
		printf("round %d: %u levels\n", rounds, depth);
		sp = 0;
		// the methods something can now call
		u32 keep = 0;
		size_t before = sp;
		for (u32 i = 0; i < nm; i++) {
			u32 m = markable[i];
			int call = (reflectSeen && mExp[m]) || named[mName[m]];
			if (!call) {
				u64 k = (u64)mName[m] << 32 | mTyp[m];
				for (u64 h = mix64(k) & (hs - 1); sigTab[h]; h = (h + 1) & (hs - 1))
					if (sigTab[h] == k + 1) { call = 1; break; }
			}
			if (call) {
				mark(mTyp[m]);
				mark(mIfn[m]);
				mark(mTfn[m]);
			} else {
				markable[keep++] = m;
			}
		}
		nm = keep;
		if (sp == before && sp == 0) break;
	}

	size_t nlive = 0;
	for (u32 g = 1; g < N; g++) nlive += live[g];
	u32 *list = malloc(4 * nlive), k = 0;
	for (u32 g = 1; g < N; g++)
		if (live[g]) list[k++] = g;
	printf("P2 BFS: %d rounds, %llu total levels, %zu live\n", rounds, (unsigned long long)total, nlive);
	puts("frontier size | levels | symbol visits (includes interface revisits)");
	const char *labels[] = {"1", "2-15", "16-255", "256-4095", "4096-65535", "65536+"};
	for (int b = 0; b < 6; b++)
		printf("%s %llu %llu\n", labels[b], (unsigned long long)levels[b], (unsigned long long)symbols[b]);
	long bad = check32("live", list, nlive, "oracle/p2_live.u32");
	printf(bad ? "P2 FAIL\n" : "P2 OK\n");
	return bad != 0;
}

// Lower-bound powers: 0, 1, 2-3, 4-7, ... . Upper-bound powers for groups:
// 0, 1, 2, 3-4, 5-8, ... . Always show zero buckets through the maximum.
static unsigned bin(u64 n) {
	unsigned b = 0;
	for (; n; n >>= 1) b++;
	return b;
}
static void histogram(const char *title, const u64 *h, u64 max, int upper) {
	puts(title);
	unsigned last = upper && max > 1 ? bin(max - 1) + 1 : bin(max);
	for (unsigned b = 0; b <= last; b++) {
		u64 lo = b ? 1ull << (b - 1) : 0, hi = b ? (1ull << b) - 1 : 0;
		if (upper && b > 1) lo = (1ull << (b - 2)) + 1, hi = 1ull << (b - 1);
		if (lo == hi) printf("%llu", (unsigned long long)lo);
		else printf("%llu-%llu", (unsigned long long)lo, (unsigned long long)hi);
		printf(" %llu\n", (unsigned long long)h[b]);
	}
	printf("max %llu\n", (unsigned long long)max);
}
static void record(u64 *h, u64 n, u64 *max, int upper) {
	h[upper && n > 1 ? bin(n - 1) + 1 : bin(n)]++;
	if (n > *max) *max = n;
}

static void graph(void) {
	u32 n = metaGet("nslots");
	const u32 *off = FMAP(u32, "p2_edge_off.u32", NULL), *dst = FMAP(u32, "p2_edge_dst.u32", NULL);
	const u8 *kind = FMAP(u8, "p2_edge_kind.u8", NULL);
	u64 h[34] = {0}, max = 0, sources = 0, kinds[4] = {0};
	u32 *in = calloc((size_t)n + 1, sizeof *in), top[5] = {0};
	for (u32 g = 1; g <= n; g++) {
		u32 degree = off[g + 1] - off[g];
		sources += degree != 0;
		record(h, degree, &max, 0);
		for (u32 e = off[g]; e < off[g + 1]; e++) {
			kinds[kind[e]]++;
			// Kinds 2 and 3 target signatures/strings, not symbol ids.
			if (kind[e] <= 1 && dst[e]) in[dst[e]]++;
		}
	}
	printf("\nP2 graph: %llu sources with edges; all %u slots included\n", (unsigned long long)sources, n);
	histogram("edges per source (all four kinds): count", h, max, 0);
	for (int k = 0; k < 4; k++) printf("edge kind %d: %llu\n", k, (unsigned long long)kinds[k]);
	for (u32 g = 1; g <= n; g++)
		for (int k = 0; k < 5; k++)
			if (in[g] > in[top[k]]) {
				for (int j = 4; j > k; j--) top[j] = top[j - 1];
				top[k] = g;
				break;
			}
	printf("symbol in-degree max (kinds 0+1, duplicate edges counted): %u\n", in[top[0]]);
	const u64 *no = FMAP(u64, "name.off.u64", NULL);
	const char *names = FMAP(char, "name.bin", NULL);
	puts("top targets: id in-degree name (ties: lowest id first)");
	for (int k = 0; k < 5; k++) {
		u32 g = top[k];
		printf("%u %u ", g, in[g]);
		fwrite(names + no[g], 1, no[g + 1] - no[g], stdout);
		putchar('\n');
	}
	free(in);
}

typedef struct { u64 a, b; u32 slots, seed, def; } Group;
static int compare(const void *a, const void *b) {
	const Group *x = a, *y = b;
	if (x->a != y->a) return x->a < y->a ? -1 : 1;
	return x->b < y->b ? -1 : x->b > y->b;
}
static void groups(Group *g, size_t n, int byname) {
	qsort(g, n, sizeof *g, compare);
	u64 h[34] = {0}, max = 0, defs = 0, refs = 0, empty = 0, count = 0, copies = 0;
	for (size_t i = 0, j; i < n; i = j) {
		u64 slots = 0;
		int seed = 0, def = 0;
		for (j = i; j < n && !compare(g + i, g + j); j++) {
			slots += g[j].slots, seed |= g[j].seed, def |= g[j].def;
		}
		if (!seed) continue; // Kind-1 candidates never create name groups.
		record(h, slots, &max, 1);
		count++, copies += slots;
		if (def) defs++;
		else if (slots) refs++;
		else empty++;
	}
	histogram(byname ? "\nP1 by-name group sizes (slots; wanted keys establish groups but add no slot):" :
	          "\nP1 content-hash class sizes (marked copies, both kinds; 64-bit hashes ignore size):", h, max, 1);
	printf("groups %llu, members %llu\n", (unsigned long long)count, (unsigned long long)copies);
	if (byname) printf("with definition %llu; only references %llu; wanted-only %llu\n",
		(unsigned long long)defs, (unsigned long long)refs, (unsigned long long)empty);
}
static void resolve(void) {
	u32 n = metaGet("nslots");
	size_t nw;
	const u64 *key = FMAP(u64, "p1_namekey.u64x2", NULL), *want = FMAP(u64, "p1_want.u64x2", &nw);
	const u8 *kind = FMAP(u8, "p1_kind.u8", NULL), *mark = FMAP(u8, "p1c_mark.u8", NULL);
	const u32 *rank = FMAP(u32, "p1_rank.u32", NULL);
	nw /= 16;
	Group *g = malloc(((size_t)n + nw) * sizeof *g);
	size_t k = 0;
	for (size_t i = 0; i < nw; i++) g[k++] = (Group){want[2*i], want[2*i+1], 0, 1, 0};
	for (u32 i = 1; i <= n; i++)
		if ((kind[i] == 1 || kind[i] == 4 || kind[i] == 5) && key[2*(size_t)i])
			g[k++] = (Group){key[2*(size_t)i], key[2*(size_t)i+1], 1, kind[i] != 1, !!(rank[i] & (1u << 29))};
	groups(g, k, 1);
	key = FMAP(u64, "p1_hashkey.u64x2", NULL);
	k = 0;
	for (u32 i = 1; i <= n; i++)
		if ((kind[i] == 2 || kind[i] == 3) && mark[i])
			g[k++] = (Group){key[2*(size_t)i], kind[i] == 2 ? 0 : key[2*(size_t)i+1] | (1ull << 63), 1, 1, 0};
	groups(g, k, 0);
	free(g);
}

static void layout(void) {
	u32 n = metaGet("p3_placed");
	const u8 *bucket = FMAP(u8, "p3_bucket.u8", NULL), *kind = FMAP(u8, "p3_rel_kind.u8", NULL);
	const u8 *width = FMAP(u8, "p3_rel_siz.u8", NULL);
	const u32 *size = FMAP(u32, "p3_size.u32", NULL), *start = FMAP(u32, "p3_rel_start.u32", NULL);
	const u64 *data = FMAP(u64, "p3_data.off.u64", NULL);
	u64 rel[9][9] = {{0}}, bytes[16] = {0}, stored[16] = {0}, placed[16] = {0};
	u64 sizes[34] = {0}, rels[34] = {0}, maxsize = 0, maxrel = 0;
	for (u32 i = 0; i < n; i++) {
		bytes[bucket[i]] += size[i], stored[bucket[i]] += data[i+1] - data[i], placed[bucket[i]]++;
		record(sizes, size[i], &maxsize, 0);
		record(rels, start[i+1] - start[i], &maxrel, 0);
		for (u32 r = start[i]; r < start[i+1]; r++) rel[kind[r]][width[r]]++;
	}
	puts("\nP3 relocation kind x width: kind width count bytes (nonempty cells)");
	for (int k = 1; k <= 8; k++)
		for (int w = 1; w <= 8; w *= 2)
			if (rel[k][w]) printf("%d %d %llu %llu\n", k, w, (unsigned long long)rel[k][w], (unsigned long long)(rel[k][w]*w));
	puts("bucket placed-symbols size-bytes stored-bytes (size includes zero-fill; excludes alignment padding)");
	for (int b = 1; b < 16; b++) printf("%d %llu %llu %llu\n", b, (unsigned long long)placed[b],
		(unsigned long long)bytes[b], (unsigned long long)stored[b]);
	histogram("placed symbol sizes: bytes count", sizes, maxsize, 0);
	histogram("relocations per placed symbol: relocations count", rels, maxrel, 0);
}
int main(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "data/minio";
	int bad = reach();
	graph();
	resolve();
	layout();
	return bad;
}
