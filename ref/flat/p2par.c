// p2par.c: problem 2 (reach) from the flat files, level-synchronous on all cores (OpenMP): each level's frontier
// is split over the threads, marks are atomic byte exchanges, each thread collects its next frontier; methods are
// checked serially between rounds (they are few). Same rules as p2.c; checks oracle/p2_live.u32.
//   gcc -O2 -march=native -fopenmp -o build/p2par ref/flat/p2par.c && build/p2par data/minio
#include "flat.h"
#include <omp.h>

enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16 };
static u8 *live, *iface, *ifaceDone;
static inline int tas(u8 *p) { return __atomic_exchange_n(p, 1, __ATOMIC_RELAXED) == 0; }
// shared flags inside the parallel flood: relaxed atomic loads and stores (plain ones are data races, even same-value)
#define LD(x) __atomic_load_n(&(x), __ATOMIC_RELAXED)
#define ST1(x) __atomic_store_n(&(x), 1, __ATOMIC_RELAXED)

typedef struct { u32 *a; size_t n, cap; } vec;
static inline void push(vec *v, u32 x) {
	if (v->n == v->cap) { v->cap = v->cap ? 2 * v->cap : 4096; v->a = realloc(v->a, 4 * v->cap); }
	v->a[v->n++] = x;
}

int main(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "data/minio";
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
	int T = omp_get_max_threads();
	live = malloc(N), iface = malloc(N), ifaceDone = malloc(N);
	u8 *sigSeen = malloc(nsig), *named = malloc(nstr + 1);
	vec *next = calloc(T, sizeof(vec)), *meth = calloc(T, sizeof(vec)), fr = {0}, mk = {0};
	int *refl = calloc(T, sizeof(int));
	u64 hs = 1;
	while (hs < 4 * nsig + 64) hs <<= 1;
	u64 *sigTab = malloc(hs * 8);
	double best = 1e9;
	size_t nlive = 0;
	int levels = 0, rounds = 0;
	for (int rep = 0; rep < (argc > 2 ? atoi(argv[2]) : 7); rep++) {
		memset(live, 0, N); memset(ifaceDone, 0, N); memset(sigSeen, 0, nsig); memset(named, 0, nstr + 1);
		memset(sigTab, 0, hs * 8); memset(refl, 0, T * sizeof(int));
		for (u32 g = 0; g < N; g++) iface[g] = g <= nsl && (fl[g] & fIface);
		double t0 = now();
		fr.n = mk.n = 0;
		levels = rounds = 0;
		int reflectSeen = 0;
		for (size_t i = 0; i < nroots; i++) if (tas(&live[roots[i]])) push(&fr, roots[i]);
		for (;;) {
			rounds++;
			while (fr.n) {
				levels++;
				#pragma omp parallel
				{
					int w = omp_get_thread_num();
					vec *nx = &next[w];
					#pragma omp for schedule(dynamic, 256) nowait
					for (size_t i = 0; i < fr.n; i++) {
						u32 c = fr.a[i];
						if (c > nsl || !(fl[c] & fDef)) continue;
						if (fl[c] & fReflect) refl[w] = 1;
						int uii = (fl[c] & fGoType) && __atomic_load_n(&iface[c], __ATOMIC_RELAXED);
						if (uii && !tas(&ifaceDone[c])) continue;
						for (u32 e = eoff[c]; e < eoff[c + 1]; e++) {
							u32 t = ed[e];
							switch (ek[e]) {
							case 0:
								if (uii && (fl[t] & fGoType) && (fl[t] & fDef) && !LD(iface[t]) && tas(&iface[t])) {
									tas(&live[t]);
									push(nx, t); // (re)flood it as a type converted to an interface
								} else if (!LD(live[t]) && tas(&live[t])) push(nx, t);
								break;
							case 1:
								if (!LD(iface[t]) && tas(&iface[t]) && LD(live[t])) push(nx, t);
								break;
							case 2: if (!LD(sigSeen[t])) ST1(sigSeen[t]); break;
							case 3: if (!LD(named[t])) ST1(named[t]); break;
							}
						}
						if (uii)
							for (u32 m = moff[c]; m < moff[c + 1]; m++) {
								u32 t = mTyp[m];
								if (t && !LD(iface[t]) && tas(&iface[t]) && LD(live[t])) push(nx, t);
								push(&meth[w], m);
							}
					}
				}
				fr.n = 0;
				for (int w = 0; w < T; w++) {
					for (size_t k = 0; k < next[w].n; k++) push(&fr, next[w].a[k]);
					next[w].n = 0;
					for (size_t k = 0; k < meth[w].n; k++) push(&mk, meth[w].a[k]);
					meth[w].n = 0;
					reflectSeen |= refl[w];
				}
			}
			// signatures seen this round into the table, then the methods something can call
			for (u32 s = 0; s < nsig; s++) {
				if (sigSeen[s] != 1) continue;
				sigSeen[s] = 2;
				u64 k = (u64)sigName[s] << 32 | sigType[s];
				for (u64 h = mix64(k) & (hs - 1);; h = (h + 1) & (hs - 1)) {
					if (sigTab[h] == 0) { sigTab[h] = k + 1; break; }
					if (sigTab[h] == k + 1) break;
				}
			}
			size_t keep = 0;
			for (size_t i = 0; i < mk.n; i++) {
				u32 m = mk.a[i];
				int call = (reflectSeen && mExp[m]) || named[mName[m]];
				if (!call) {
					u64 k = (u64)mName[m] << 32 | mTyp[m];
					for (u64 h = mix64(k) & (hs - 1); sigTab[h]; h = (h + 1) & (hs - 1))
						if (sigTab[h] == k + 1) { call = 1; break; }
				}
				if (call) {
					u32 ts[3] = {mTyp[m], mIfn[m], mTfn[m]};
					for (int d = 0; d < 3; d++) if (ts[d] && tas(&live[ts[d]])) push(&fr, ts[d]);
				} else mk.a[keep++] = m;
			}
			mk.n = keep;
			if (fr.n == 0) break;
		}
		double t1 = now();
		if (t1 - t0 < best) best = t1 - t0;
	}
	nlive = 0;
	for (u32 g = 1; g < N; g++) nlive += live[g];
	u32 *list = malloc(4 * nlive), k = 0;
	for (u32 g = 1; g < N; g++) if (live[g]) list[k++] = g;
	printf("p2par (%d threads): flood best %.1f ms, %zu live, %d levels in %d rounds\n", T, best * 1e3, nlive, levels, rounds);
	long bad = check32("live", list, nlive, "oracle/p2_live.u32");
	printf(bad ? "P2 FAIL\n" : "P2 OK\n");
	return bad != 0;
}
