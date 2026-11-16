// p1par.c: problem 1a+1b from the flat files on all cores (OpenMP): a partitioned hash join. Pass 1 counts the
// keyed slots per partition (top 8 bits of k1) per thread-chunk, a prefix sum places them, pass 2 scatters
// (key, val) records; each partition is joined in a table that fits in L2; pass 3 gives every slot its winner;
// then the relocation targets are a parallel gather. Checks oracle/p1_canon.u32 and oracle/p1_reltarget.u32.
//   gcc -O2 -march=native -fopenmp -o build/p1par ref/flat/p1par.c && build/p1par data/minio
#include "flat.h"
#include <omp.h>

#define NP 256
#define rankDef (1u << 29)
#define PkgIdxSelf 2147483643u
typedef struct { u64 k1, k2, val; } rec; // val = rank << 32 | ~id; id 0: a wanted name

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
	size_t *cnt = malloc(sizeof(size_t) * C * NP);
	rec *recs = malloc(sizeof(rec) * ((size_t)N + nw)); // more than enough
	u32 *slotOf = malloc(4 * ((size_t)N + nw));          // the slot of each record (0: wanted)
	size_t pbase[NP + 1];
	double best = 1e9, bj = 0, bt = 0;
	for (int rep = 0; rep < (argc > 2 ? atoi(argv[2]) : 7); rep++) {
		double t0 = now();
		memset(cnt, 0, sizeof(size_t) * C * NP);
		// 1. count per (chunk, partition): slots of kinds 4, 5 with a key; the wanted names go in chunk 0
		#pragma omp parallel for schedule(static, 1)
		for (int c = 0; c < C; c++) {
			size_t *ct = cnt + (size_t)c * NP;
			if (c == 0) for (size_t i = 0; i < nw; i++) ct[want[2 * i] >> 56]++;
			u32 lo = 1 + (u32)((u64)N * c / C), hi = 1 + (u32)((u64)N * (c + 1) / C);
			for (u32 g = lo; g < hi; g++) {
				canon[g] = (flags[g] & 2) ? 0 : g;
				u64 k1 = key[2 * (size_t)g];
				if ((kind[g] == 4 || kind[g] == 5) && k1) ct[k1 >> 56]++;
			}
		}
		canon[0] = 0;
		size_t s = 0;
		for (int p = 0; p < NP; p++) {
			pbase[p] = s;
			for (int c = 0; c < C; c++) { size_t n = cnt[(size_t)c * NP + p]; cnt[(size_t)c * NP + p] = s; s += n; }
		}
		pbase[NP] = s;
		// 2. scatter the records into their partitions (chunk order inside a partition: deterministic)
		#pragma omp parallel for schedule(static, 1)
		for (int c = 0; c < C; c++) {
			size_t *ct = cnt + (size_t)c * NP;
			if (c == 0)
				for (size_t i = 0; i < nw; i++) {
					size_t at = ct[want[2 * i] >> 56]++;
					recs[at] = (rec){want[2 * i], want[2 * i + 1], 0};
					slotOf[at] = 0;
				}
			u32 lo = 1 + (u32)((u64)N * c / C), hi = 1 + (u32)((u64)N * (c + 1) / C);
			for (u32 g = lo; g < hi; g++) {
				u64 k1 = key[2 * (size_t)g];
				if (!((kind[g] == 4 || kind[g] == 5) && k1)) continue;
				size_t at = ct[k1 >> 56]++;
				recs[at] = (rec){k1, key[2 * (size_t)g + 1], (u64)rank[g] << 32 | (u32)~g};
				slotOf[at] = g;
			}
		}
		// 3. join each partition in its own table; the kind-1 candidates probe all partitions' tables after
		static rec *tabs[NP];
		static u32 masks[NP];
		#pragma omp parallel for schedule(dynamic, 1)
		for (int p = 0; p < NP; p++) {
			size_t n = pbase[p + 1] - pbase[p];
			u32 sz = 64;
			while (sz < 2 * n) sz <<= 1;
			rec *t = tabs[p] = realloc(tabs[p], sizeof(rec) * sz);
			memset(t, 0, sizeof(rec) * sz);
			masks[p] = sz - 1;
			for (size_t i = pbase[p]; i < pbase[p + 1]; i++) {
				rec *r = &recs[i];
				for (u32 h = (u32)(r->k1 >> 8) & (sz - 1);; h = (h + 1) & (sz - 1)) {
					if (t[h].k1 == 0) { t[h] = *r; break; }
					if (t[h].k1 == r->k1 && t[h].k2 == r->k2) { if (r->val > t[h].val) t[h].val = r->val; break; }
				}
			}
		}
		#define FIND(K1, K2, e) do { u64 a_ = (K1), b_ = (K2); int p_ = (int)(a_ >> 56); rec *t_ = tabs[p_]; e = NULL; \
			for (u32 h = (u32)(a_ >> 8) & masks[p_]; t_[h].k1; h = (h + 1) & masks[p_])                            \
				if (t_[h].k1 == a_ && t_[h].k2 == b_) { e = &t_[h]; break; } } while (0)
		// candidates: a CAS-max into the table entry, then (after all are in) every candidate reads its winner
		#pragma omp parallel for schedule(static, 65536)
		for (u32 g = 1; g <= N; g++) {
			if (kind[g] != 1 || !key[2 * (size_t)g]) continue;
			rec *e;
			FIND(key[2 * (size_t)g], key[2 * (size_t)g + 1], e);
			if (!e) continue;
			u64 v = (u64)rank[g] << 32 | (u32)~g, old = __atomic_load_n(&e->val, __ATOMIC_RELAXED);
			while (v > old && !__atomic_compare_exchange_n(&e->val, &old, v, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
		}
		#pragma omp parallel for schedule(static, 65536)
		for (u32 g = 1; g <= N; g++) {
			if (kind[g] != 1 || !key[2 * (size_t)g]) continue;
			rec *e;
			FIND(key[2 * (size_t)g], key[2 * (size_t)g + 1], e);
			if (e) canon[g] = ~(u32)e->val;
		}
		#pragma omp parallel for schedule(dynamic, 1)
		for (int p = 0; p < NP; p++) {
			rec *t = tabs[p];
			for (size_t i = pbase[p]; i < pbase[p + 1]; i++) {
				if (!slotOf[i]) continue;
				rec *r = &recs[i];
				for (u32 h = (u32)(r->k1 >> 8) & masks[p];; h = (h + 1) & masks[p])
					if (t[h].k1 == r->k1 && t[h].k2 == r->k2) { canon[slotOf[i]] = ~(u32)t[h].val; break; }
			}
		}
		for (u32 i = 0; i < nb; i++) {
			rec *e;
			FIND(bkey[2 * i], bkey[2 * i + 1], e);
			builtin[i] = (e && e->val && ((e->val >> 32) & rankDef)) ? ~(u32)e->val : 0;
		}
		double t1 = now();
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
		if (t2 - t0 < best) best = t2 - t0, bj = t1 - t0, bt = t2 - t1;
	}
	printf("p1par (%d threads): best %.1f ms = join by name %.1f + relocation targets %.1f\n", T, best * 1e3, bj * 1e3, bt * 1e3);
	long bad = check32("canon", canon, (size_t)N + 1, "oracle/p1_canon.u32");
	bad += check32("builtin", builtin, nb, "oracle/p1_builtin.u32");
	bad += check32("reltarget", tgt, nrel, "oracle/p1_reltarget.u32");
	printf(bad ? "P1 FAIL\n" : "P1 OK\n");
	return bad != 0;
}
