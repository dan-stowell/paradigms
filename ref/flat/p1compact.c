// p1compact.c: one dense pass materializes keyed members and candidates in
// per-chunk regions. Scatter both compact lists, then build, candidate-max and
// read back each partition under one owner (no CAS or global candidate probes).
// Relocation targets, content join and oracle checks are identical to p1par.
#include "flat.h"
#include <omp.h>
#include <sys/resource.h>
#include "p1c.h"

#define NP 256
#define rankDef (1u << 29)
#define PkgIdxSelf 2147483643u
typedef struct { u64 k1, k2, val; } rec; // val = rank << 32 | ~id; id 0: a wanted name

typedef struct { u64 k1, k2, val; u32 slot; } compact; // 32 bytes including padding

// Optional diagnostics keep perf counters confined to the name join. Run perf
// stat -D -1 --control=fifo:CTL,ACK with P1_PERF_CTL/ACK naming those
// FIFOs, P1_PROFILE=1 and OMP_NUM_THREADS=1. P1_WARMUP=1 runs
// one complete excluded iteration, touching precisely the pages the kernel uses.
static void perf_gate(int enable) {
	static FILE *ctl, *ack;
	if (!getenv("P1_PERF_CTL")) return;
	if (!ctl) {
		if (!getenv("P1_PERF_ACK")) { fprintf(stderr, "P1_PERF_ACK is required\n"); exit(2); }
		ctl = fopen(getenv("P1_PERF_CTL"), "w");
		ack = fopen(getenv("P1_PERF_ACK"), "r");
		if (!ctl || !ack) { perror("perf control"); exit(2); }
	}
	fprintf(ctl, "%s\n", enable ? "enable" : "disable"); fflush(ctl);
	char reply[32];
	if (read(fileno(ack), reply, sizeof reply) < 3 || strncmp(reply, "ack", 3)) {
		fprintf(stderr, "perf acknowledgement failed\n"); exit(2);
	}
}
static void page_report(void) {
	FILE *f = fopen("/proc/self/smaps", "r");
	char line[512]; unsigned long anon = 0, priv = 0, shared = 0, kb;
	if (!f) { perror("smaps"); exit(2); }
	while (fgets(line, sizeof line, f)) {
		if (sscanf(line, "AnonHugePages: %lu kB", &kb) == 1) anon += kb;
		if (sscanf(line, "Private_Hugetlb: %lu kB", &kb) == 1) priv += kb;
		if (sscanf(line, "Shared_Hugetlb: %lu kB", &kb) == 1) shared += kb;
	}
	fclose(f);
	printf("pages: AnonHugePages=%lu Private_Hugetlb=%lu Shared_Hugetlb=%lu kB\n", anon, priv, shared);
}
int main(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "data/minio";
	u32 N = (u32)metaGet("nslots"), nb = (u32)metaGet("nbuiltin");
	size_t nw, nrel;
	const u8 *kind = FMAP(u8, "p1_kind.u8", NULL), *flags = FMAP(u8, "p1_flags.u8", NULL);
	const u64 *key = FMAP(u64, "p1_namekey.u64x2", NULL), *want = FMAP(u64, "p1_want.u64x2", &nw);
	const u64 *bkey = FMAP(u64, "p1_builtin.u64x2", NULL);
	const u32 *rank = FMAP(u32, "p1_rank.u32", NULL), *slotRel = FMAP(u32, "slot_rel.u32", NULL);
	const u32 *slotObj = FMAP(u32, "slot_obj.u32", NULL), *pkgOff = FMAP(u32, "obj_pkg_off.u32", NULL);
	const u32 *pkg = FMAP(u32, "obj_pkg.u32", NULL);
	const u32 *rPkg = FMAP(u32, "rel_pkg.u32", &nrel), *rIdx = FMAP(u32, "rel_idx.u32", NULL);
	nrel /= 4, nw /= 16;
	int T = omp_get_max_threads(), C = T * 8; // chunks of the slot range
	u32 *canon = malloc(4 * ((size_t)N + 1)), *tgt = malloc(4 * nrel), *builtin = calloc(nb, 4);
	size_t *cnt = malloc(sizeof(size_t) * C * NP * 2);
	// Each chunk owns its dense-range capacity in each temporary list. Only
	// initialized prefixes are read; no dense arrays are revisited by the join.
	compact *members = malloc(sizeof(compact) * ((size_t)N + nw));
	compact *candidates = malloc(sizeof(compact) * N);
	compact *recs = malloc(sizeof(compact) * ((size_t)N + nw));
	compact *crecs = malloc(sizeof(compact) * N);
	size_t *used = calloc((size_t)C * 2, sizeof(size_t));
	size_t pbase[NP + 1], cbase[NP + 1];
	double best = 1e9, bj = 0, bt = 0, sumj = 0, sumt = 0;
	int reps = argc > 2 ? atoi(argv[2]) : 7;
	if (reps < 1) { fprintf(stderr, "rep count must be positive\n"); return 2; }
	int profile = getenv("P1_PROFILE") && atoi(getenv("P1_PROFILE"));
	int warmup = getenv("P1_WARMUP") && atoi(getenv("P1_WARMUP"));
	if (profile && T != 1) { fprintf(stderr, "P1_PROFILE requires one thread\n"); return 2; }
	long join_faults = 0, first_faults = 0;
	double first_join = 0;
	for (int rep = -warmup; rep < reps; rep++) {
		struct rusage before, after;
		if (profile && rep >= 0) {
			perf_gate(1);
			getrusage(RUSAGE_SELF, &before);
		}
		double t0 = now();
		memset(cnt, 0, sizeof(size_t) * C * NP * 2);
		#pragma omp parallel for schedule(static, 1)
		for (int c = 0; c < C; c++) {
			size_t *ct = cnt + (size_t)c * NP * 2, *cc = ct + NP;
			u32 lo = 1 + (u32)((u64)N * c / C), hi = 1 + (u32)((u64)N * (c + 1) / C);
			size_t m = (size_t)lo - 1 + (c ? nw : 0), a = lo - 1;
			size_t mstart = m, astart = a;
			if (c == 0) for (size_t i = 0; i < nw; i++) {
				members[m++] = (compact){want[2*i], want[2*i+1], 0, 0};
				ct[want[2*i] >> 56]++;
			}
			for (u32 g = lo; g < hi; g++) {
				canon[g] = (flags[g] & 2) ? 0 : g;
				u64 k1 = key[2 * (size_t)g];
				if (!k1) continue;
				u8 k = kind[g];
				if (k != 1 && k != 4 && k != 5) continue;
				compact r = {k1, key[2*(size_t)g+1], (u64)rank[g] << 32 | (u32)~g, g};
				if (k == 1) { candidates[a++] = r; cc[k1 >> 56]++; }
				else { members[m++] = r; ct[k1 >> 56]++; }
			}
			used[2*c] = m - mstart; used[2*c+1] = a - astart;
		}
		canon[0] = 0;
		size_t s = 0, cs = 0;
		for (int p = 0; p < NP; p++) {
			pbase[p] = s; cbase[p] = cs;
			for (int c = 0; c < C; c++) {
				size_t *ct = cnt + (size_t)c * NP * 2;
				size_t n = ct[p], cn = ct[NP+p];
				ct[p] = s; s += n; ct[NP+p] = cs; cs += cn;
			}
		}
		pbase[NP] = s; cbase[NP] = cs;
		#pragma omp parallel for schedule(static, 1)
		for (int c = 0; c < C; c++) {
			size_t *ct = cnt + (size_t)c * NP * 2;
			size_t start = (u64)N * c / C;
			for (size_t i = 0; i < used[2*c]; i++) {
				compact r = members[start + (c ? nw : 0) + i];
				recs[ct[r.k1 >> 56]++] = r;
			}
			for (size_t i = 0; i < used[2*c+1]; i++) {
				compact r = candidates[start + i];
				crecs[ct[NP + (r.k1 >> 56)]++] = r;
			}
		}
		static rec *tabs[NP];
		static u32 masks[NP];
		#define FIND(K1, K2, e) do { u64 a_ = (K1), b_ = (K2); int p_ = (int)(a_ >> 56); rec *t_ = tabs[p_]; e = NULL; \
			for (u32 h = (u32)(a_ >> 8) & masks[p_]; t_[h].k1; h = (h + 1) & masks[p_]) \
				if (t_[h].k1 == a_ && t_[h].k2 == b_) { e = &t_[h]; break; } } while (0)
		#pragma omp parallel for schedule(dynamic, 1)
		for (int p = 0; p < NP; p++) {
			size_t n = pbase[p + 1] - pbase[p];
			u32 sz = 64;
			while (sz < 2 * n) sz <<= 1;
			rec *t = tabs[p] = realloc(tabs[p], sizeof(rec) * sz);
			memset(t, 0, sizeof(rec) * sz);
			masks[p] = sz - 1;
			for (size_t i = pbase[p]; i < pbase[p + 1]; i++) {
				compact *r = &recs[i];
				for (u32 h = (u32)(r->k1 >> 8) & (sz - 1);; h = (h + 1) & (sz - 1)) {
					if (t[h].k1 == 0) { t[h] = (rec){r->k1, r->k2, r->val}; break; }
					if (t[h].k1 == r->k1 && t[h].k2 == r->k2) { if (r->val > t[h].val) t[h].val = r->val; break; }
				}
			}
			for (size_t i = cbase[p]; i < cbase[p+1]; i++) {
				compact *r = &crecs[i]; rec *e;
				FIND(r->k1, r->k2, e);
				if (e && r->val > e->val) e->val = r->val;
			}
			for (size_t i = cbase[p]; i < cbase[p+1]; i++) {
				compact *r = &crecs[i]; rec *e;
				FIND(r->k1, r->k2, e);
				if (e) canon[r->slot] = ~(u32)e->val;
			}
			for (size_t i = pbase[p]; i < pbase[p+1]; i++) {
				compact *r = &recs[i];
				if (!r->slot) continue;
				rec *e; FIND(r->k1, r->k2, e);
				canon[r->slot] = ~(u32)e->val;
			}
		}
		for (u32 i = 0; i < nb; i++) {
			rec *e;
			FIND(bkey[2 * i], bkey[2 * i + 1], e);
			builtin[i] = (e && e->val && ((e->val >> 32) & rankDef)) ? ~(u32)e->val : 0;
		}
		double t1 = now();
		if (profile && rep >= 0) {
			perf_gate(0);
			getrusage(RUSAGE_SELF, &after);
			join_faults += after.ru_minflt - before.ru_minflt;
			if (!rep) first_faults = after.ru_minflt - before.ru_minflt;
		}
		if (!rep) first_join = t1 - t0;
		// 4. relocation targets: a gather through the object's package table, then canon
		#pragma omp parallel for schedule(static, 65536)
		for (u32 g = 1; g <= N; g++) {
			u32 o = slotObj[g];
			const u32 *pb = pkg + pkgOff[o];
			u32 npk = pkgOff[o + 1] - pkgOff[o] - 5;
			for (u32 r = slotRel[g]; r < slotRel[g + 1]; r++) {
				u32 p = rPkg[r], idx = rIdx[r], sp = p - PkgIdxSelf, t = sp < 5 ? npk + sp : p, x = 0;
				if (t < npk + 5 && pb[t] != 0xFFFFFFFFu) x = sp == 1 ? (idx < nb ? builtin[idx] : 0) : pb[t] + idx;
				tgt[r] = x ? canon[x] : 0;
			}
		}
		double t2 = now();
		if (rep < 0) continue;
		sumj += t1 - t0; sumt += t2 - t0;
		if (t2 - t0 < best) best = t2 - t0, bj = t1 - t0, bt = t2 - t1;
	}
	if (profile) {
		printf("first join: %.6f ms, %ld minor faults\n", first_join*1e3, first_faults);
		printf("join minor faults: %ld total, %.3f per rep\n", join_faults, (double)join_faults/reps);
		page_report();
	}
	printf("p1compact (%d threads): best %.1f ms = join by name %.1f + relocation targets %.1f\n", T, best * 1e3, bj * 1e3, bt * 1e3);
	printf("p1compact mean: join %.6f ms total %.6f ms (%d reps, %.6f s kernel)\n", sumj/reps*1e3, sumt/reps*1e3, reps, sumt);
	long bad = check32("canon", canon, (size_t)N + 1, "oracle/p1_canon.u32");
	bad += check32("builtin", builtin, nb, "oracle/p1_builtin.u32");
	bad += check32("reltarget", tgt, nrel, "oracle/p1_reltarget.u32");
	const u64 *hkey = FMAP(u64, "p1_hashkey.u64x2", NULL);
	const u8 *hmark = FMAP(u8, "p1c_mark.u8", NULL);
	u32 *canon2 = malloc(4 * ((size_t)N + 1));
	double hcBest = 1e9;
	for (int rep = 0; rep < (argc > 2 ? atoi(argv[2]) : 7); rep++) {
		double t0 = now();
		contentJoin(N, kind, hmark, hkey, canon, canon2, C);
		double elapsed = now() - t0;
		if (elapsed < hcBest) hcBest = elapsed;
	}
	printf("p1cpar (%d threads): content-hash join %.1f ms\n", T, hcBest * 1e3);
	bad += check32("canon after 1c", canon2, (size_t)N + 1, "oracle/p1c_canon.u32");
	printf(bad ? "P1 FAIL\n" : "P1 OK\n");
	return bad != 0;
}
