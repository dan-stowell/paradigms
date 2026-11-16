// p3.c: problem 3 (lay out, then relocate) from the flat files, plainly, on one core. Checks itself against
// oracle/p3_addr.u32 (3a) and the bytes of every placed symbol in oracle/p3_image.bin (3b).
//   ref/flat/p3 data/minio [--pipeline] [out.img]
#include "flat.h"
#include <sys/resource.h>

enum { bText = 1, bTextFIPS = 2, bTypelink = 6, bBSS = 13, bNoptrBSS = 14, bCov = 15, nBuckets = 16, nClasses = 13 };
static inline u32 rnd(u32 v, u32 a) { return (v + a - 1) & ~(a - 1); }

static const u64 *soff;
static const u8 *sbin;
static const u32 *tlstr, *psym;
static int cmpTL(const void *a, const void *b) {
	u32 x = *(const u32 *)a, y = *(const u32 *)b;
	u64 xo = soff[tlstr[x]], xn = soff[tlstr[x] + 1] - xo, yo = soff[tlstr[y]], yn = soff[tlstr[y] + 1] - yo;
	u64 n = xn < yn ? xn : yn;
	int c = n ? memcmp(sbin + xo, sbin + yo, n) : 0;
	if (c) return c;
	if (xn != yn) return xn < yn ? -1 : 1;
	return psym[x] < psym[y] ? -1 : psym[x] > psym[y];
}

#ifdef _OPENMP
#include <omp.h>
static int nthreads(void) { return omp_get_max_threads(); }
#else
static int nthreads(void) { return 1; }
#endif
int main(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "data/minio";
	int pipeline = argc > 2 && strcmp(argv[2], "--pipeline") == 0;
	const char *output = argc > 2 + pipeline ? argv[2 + pipeline] : NULL;
	u32 L = (u32)metaGet("p3_placed"), nunits = (u32)metaGet("nunits"), minFunc = (u32)metaGet("minFunc");
	u32 imageBase = (u32)metaGet("imageBase");
	psym = FMAP(u32, "p3_sym.u32", NULL);
	const u8 *pb = FMAP(u8, "p3_bucket.u8", NULL), *pcl = FMAP(u8, "p3_class.u8", NULL);
	const u32 *psize = FMAP(u32, "p3_size.u32", NULL), *punit = FMAP(u32, "p3_unit.u32", NULL);
	tlstr = FMAP(u32, "p3_tlstr.u32", NULL);
	soff = FMAP(u64, "p3_str.off.u64", NULL);
	sbin = FMAP(u8, "p3_str.bin", NULL);
	const u32 *bstart = FMAP(u32, "p3_bucket_start.u32", NULL), *bmin = FMAP(u32, "p3_bucket_minalign.u32", NULL);

	// ---- 3a. addresses
	double t0 = now();
	u32 *addr = calloc(L, 4);
	// bytes per (unit, bucket, class), then each unit's block of each bucket in unit order, classes from the largest
	// alignment down; inside a class the symbols in list order
	u32 *csize = calloc((size_t)nunits * nBuckets * nClasses, 4);
#define CS(u, b, c) csize[((size_t)(u) * nBuckets + (b)) * nClasses + (c)]
	u32 *occ = malloc(4 * (size_t)L);
	for (u32 i = 0; i < L; i++) {
		u32 s = psize[i], b = pb[i];
		if ((b == bText || b == bTextFIPS) && s < minFunc) s = minFunc;
		occ[i] = rnd(s, 1u << pcl[i]);
		if (b != bTypelink) CS(punit[i], b, pcl[i]) += occ[i];
	}
	for (u32 b = 1; b < nBuckets; b++) {
		if (b == bTypelink) continue;
		u32 va = bstart[b];
		for (u32 u = 0; u < nunits; u++) {
			int top = -1;
			for (int c = nClasses - 1; c >= 0; c--)
				if (CS(u, b, c)) { top = c; break; }
			if (top >= 0) va = rnd(va, bmin[b] > (1u << top) ? bmin[b] : 1u << top);
			for (int c = nClasses - 1; c >= 0; c--) {
				u32 n = CS(u, b, c);
				CS(u, b, c) = va; // from here on: the class's next address
				va += n;
			}
		}
	}
	u32 ntl = 0;
	u32 *tl = malloc(4 * (size_t)L);
	for (u32 i = 0; i < L; i++) {
		if (pb[i] == bTypelink) { tl[ntl++] = i; continue; }
		addr[i] = CS(punit[i], pb[i], pcl[i]);
		CS(punit[i], pb[i], pcl[i]) += occ[i];
	}
	qsort(tl, ntl, 4, cmpTL);
	u32 va = bstart[bTypelink];
	for (u32 k = 0; k < ntl; k++) {
		u32 i = tl[k];
		va = rnd(va, 1u << pcl[i]);
		addr[i] = va;
		va += psize[i];
	}
	double t1 = now();
	printf("p3a (1 core): layout %.1f ms (%u symbols, %u sorted by type string)\n", (t1 - t0) * 1e3, L, ntl);
	long bad = 0;

	// ---- 3b. isolated oracle addresses, or our layout propagated through final 1c canon.
	size_t nimg;
	const u8 *want = FMAP(u8, "oracle/p3_image.bin", &nimg);
	const u32 *oaddr, *all;
	if (pipeline) {
		u32 N = (u32)metaGet("nslots"), nids = (u32)metaGet("nids");
		const u32 *canon = FMAP(u32, "oracle/p1c_canon.u32", NULL);
		// The final 1c canon includes the name join. Unplaced ids remain zero.
		// The linker's own symbols (runtime.firstmoduledata, ...) are placed outside 3a's buckets: their addresses
		// are a given input (p3_fixed_*), like the bucket starts.
		size_t nfix;
		const u32 *fid = FMAP(u32, "p3_fixed_id.u32", &nfix), *fad = FMAP(u32, "p3_fixed_addr.u32", NULL);
		nfix /= 4;
		u32 *placed = calloc(nids, sizeof(u32)), *resolved = calloc(nids, sizeof(u32));
		for (u32 i = 0; i < L; i++) placed[psym[i]] = addr[i];
		for (size_t i = 0; i < nfix; i++) placed[fid[i]] = fad[i];
		for (u32 g = 0; g < nids; g++) resolved[g] = placed[g <= N ? canon[g] : g];
		free(placed);
		oaddr = addr;
		all = resolved;
	} else {
		oaddr = FMAP(u32, "oracle/p3_addr.u32", NULL);
		all = FMAP(u32, "p3_addr_all.u32", NULL);
	}
	const u64 *doff = FMAP(u64, "p3_data.off.u64", NULL);
	const u8 *dbin = FMAP(u8, "p3_data.bin", NULL);
	const u32 *rstart = FMAP(u32, "p3_rel_start.u32", NULL), *roff = FMAP(u32, "p3_rel_off.u32", NULL), *rtgt = FMAP(u32, "p3_rel_tgt.u32", NULL);
	const u8 *rsiz = FMAP(u8, "p3_rel_siz.u8", NULL), *rkind = FMAP(u8, "p3_rel_kind.u8", NULL);
	const i64 *radd = FMAP(i64, "p3_rel_add.i64", NULL);
	u32 sect[8] = {(u32)metaGet("lay_text"), (u32)metaGet("lay_rodata"), (u32)metaGet("lay_pclntab"), (u32)metaGet("lay_types"),
	               (u32)metaGet("lay_noptrdata"), (u32)metaGet("lay_data"), (u32)metaGet("lay_bss"), (u32)metaGet("lay_noptrbss")};
	u32 unreach = (u32)metaGet("addr_unreachableMethod"), mapinit = (u32)metaGet("addr_mapinitnoop");
	// P3_REPS=n: relocate n times into a fresh image each time (its faults included), report min/median/max per rep
	// and the minor page faults of one rep (getrusage), so a run lasts long enough to mean something (CHARTER).
	int reps = getenv("P3_REPS") ? atoi(getenv("P3_REPS")) : 1;
	double rt[1024];
	long rflt = 0;
	u8 *img = NULL;
	long nerr = 0;
	double t2 = 0, t3 = 0;
	size_t imgMapped = 0;
	for (int rep = 0; rep < reps && rep < 1024; rep++) {
	if (img) { if (imgMapped) munmap(img, imgMapped); else free(img); img = NULL; imgMapped = 0; }
	struct rusage ru0, ru1;
	getrusage(RUSAGE_SELF, &ru0);
	nerr = 0;
	if (getenv("P3_THP")) { // the output on 2 MB huge pages (MAP_HUGETLB): 53 faults instead of 26,700 (ATTACKS 2)
		size_t hp = 2u << 20, n = (nimg + hp - 1) / hp * hp;
		imgMapped = n;
		img = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
		if (img == MAP_FAILED) { // no reserved huge pages (vm.nr_hugepages): transparent ones if the kernel has them
			img = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			madvise(img, n, MADV_HUGEPAGE);
		}
	} else img = calloc(nimg, 1);
	double tf = now();
	if (getenv("P3_PREFAULT")) { // first-touch the output and the input bytes outside the timed region (ATTACKS 2)
		memset(img, 0, nimg);
		volatile u8 sink = 0;
		for (size_t k = 0; k < doff[L]; k += 4096) sink ^= dbin[k];
	}
	t2 = now();
	if (getenv("P3_PREFAULT")) printf("p3b prefault %.1f ms\n", (t2 - tf) * 1e3);
	// Built with -fopenmp (build/p3par) the placed symbols are split over threads: their byte ranges are disjoint.
#pragma omp parallel for schedule(dynamic, 1024) reduction(+ : nerr)
	for (u32 i = 0; i < L; i++) {
		u32 b = pb[i];
		if (b == bBSS || b == bNoptrBSS || b == bCov) continue;
		u32 a = oaddr[i];
		u8 *P = img + (a - imageBase);
		memcpy(P, dbin + doff[i], doff[i + 1] - doff[i]);
		for (u32 r = rstart[i]; r < rstart[i + 1]; r++) {
			u32 tg = rtgt[r], ta = all[tg], off = roff[r], sz = rsiz[r];
			i64 v;
			switch (rkind[r]) {
			case 2: if (ta == 0 && tg != 0) ta = unreach; /* fallthrough */
			case 1:
				if (ta == 0 && tg != 0) { nerr++; continue; }
				v = (i64)ta + radd[r];
				break;
			case 4: if (ta == 0) { memset(P + off, 0xFF, 4); continue; } /* fallthrough */
			case 3: {
				if (ta == 0) continue;
				int k = 7;
				while (k > 0 && ta < sect[k]) k--;
				v = (i64)(ta - sect[k]) + radd[r];
				break;
			}
			case 6: if (ta == 0 && tg != 0) ta = mapinit; /* fallthrough */
			case 5:
			case 7:
				if (ta == 0 && tg != 0) { nerr++; continue; }
				v = tg == 0 ? radd[r] - (i64)(a + off + sz) : (i64)ta + radd[r] - (i64)(a + off + sz);
				break;
			case 8: v = radd[r]; break;
			default: nerr++; continue;
			}
			switch (sz) {
			case 8: memcpy(P + off, &v, 8); break;
			case 4: { u32 x = (u32)v; memcpy(P + off, &x, 4); break; }
			case 2: { u16 x = (u16)v; memcpy(P + off, &x, 2); break; }
			case 1: P[off] = (u8)v; break;
			default: nerr++;
			}
		}
	}
	t3 = now();
	getrusage(RUSAGE_SELF, &ru1);
	rt[rep] = (t3 - t2) * 1e3;
	rflt = ru1.ru_minflt - ru0.ru_minflt;
	}
	if (reps > 1) {
		int n = reps < 1024 ? reps : 1024;
		for (int i = 1; i < n; i++) for (int j = i; j > 0 && rt[j] < rt[j - 1]; j--) { double x = rt[j]; rt[j] = rt[j - 1]; rt[j - 1] = x; }
		printf("p3b reps %d (%s pages, %d threads): min %.1f median %.1f max %.1f ms per rep, %ld minor faults per rep\n", n,
		       imgMapped ? "2 MB" : "4 KB", nthreads(), rt[0], rt[n / 2], rt[n - 1], rflt);
	}
	long nbad = 0;
	u64 nbytes = 0, sum = 0;
	for (u32 i = 0; i < L; i++) {
		u32 b = pb[i];
		if (b == bBSS || b == bNoptrBSS || b == bCov) continue;
		size_t o = oaddr[i] - imageBase, n = psize[i];
		nbytes += n;
		for (size_t k = 0; k < n; k++) sum = sum * 1099511628211ull + img[o + k];
		if (memcmp(img + o, want + o, n) != 0 && nbad++ < 5) printf("  symbol %u (id %u, bucket %u) differs\n", i, psym[i], b);
	}
	printf("p3b (%d threads): copy + relocate %.1f ms, %llu bytes, %u relocations, %ld errors; checksum %016llx\n", nthreads(), (t3 - t2) * 1e3,
	       (unsigned long long)nbytes, rstart[L], nerr, (unsigned long long)sum);
	printf("image: %ld placed symbols differ from oracle/p3_image.bin\n", nbad);
	if (output) {
		FILE *f = fopen(output, "wb");
		fwrite(img, 1, nimg, f);
		fclose(f);
	}
	bad += check32("addr", addr, L, "oracle/p3_addr.u32");
	// (No comparison with p3_addr_all: hlink's raw addr array also holds PC-table offsets of non-placed symbols,
	// which no relocation reads; the image is the oracle.)
	bad += nbad + (nerr != 0);
	if (pipeline) printf(bad ? "P3 pipeline FAIL\n" : "P3 pipeline OK\n");
	else printf(bad ? "P3 FAIL\n" : "P3 OK\n");
	return bad != 0;
}
