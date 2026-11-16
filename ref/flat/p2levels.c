// p2levels.c: p2.c made level-synchronous (process the whole frontier, then the next) to print one line per level
// on stderr: "L round level frontier edges". Same live set (checks the oracle). Lead, 2026-11-16T17:35Z.
// p2.c: problem 2 (reach) from the flat files, plainly, on one core, with a worklist. Checks itself against
// oracle/p2_live.u32. The rules are data/README.md's (they are cmd/link's deadcode.go):
//   live(r)  for every root r
//   live(t)  <- live(s), plain(s,t)
//   iface(t) <- live(s), iface(s), gotype(s), plain(s,t), gotype(t), def(t)
//   iface(t) <- live(s), useiface(s,t)
//   sig(x)   <- live(s), ifacemethod(s,x);   named(n) <- live(s), namedmethod(s,n);   reflect <- live(s), reflect(s)
//   iface(m.mtyp) and markable(m) <- live(s), iface(s), gotype(s), method(s,m)
//   live(m.mtyp), live(m.ifn), live(m.tfn) <- markable(m), (reflect & exported(m) | sig(m.name, m.mtyp) | named(m.name))
// (only defined symbols have edges; edges are given only for them).
//   ref/flat/p2 data/minio
#include "flat.h"
#include <string.h>

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

	double t0 = now();
	live = calloc(N, 1), iface = calloc(N, 1), ifaceDone = calloc(N, 1);
	stack = malloc(4 * (size_t)N * 3);
	for (u32 g = 1; g <= nsl; g++) iface[g] = (fl[g] & fIface) != 0;
	u8 *sigSeen = calloc(nsig, 1), *named = calloc(nstr + 1, 1);
	// sig ids are (name, type) pairs; a method's (name, mtyp) is looked up in a hash of the live sigs
	u64 hs = 1;
	while (hs < 4 * nsig + 64) hs <<= 1;
	u64 *sigTab = calloc(hs, 8);
	u32 *markable = malloc(4 * (nmeth + 1)), nm = 0;
	int reflectSeen = 0, rounds = 0, levels = 0;
	for (size_t i = 0; i < nroots; i++) mark(roots[i]);
	for (;;) {
		rounds++;
		while (sp) {
		size_t lvsz = sp; u32 *cur = malloc(4*sp); memcpy(cur, stack, 4*sp); sp = 0;
		size_t lvedges = 0;
		for (size_t q = 0; q < lvsz; q++) {
			u32 c = cur[q];
			if (c > nsl || !(fl[c] & fDef)) continue;
			if (fl[c] & fReflect) reflectSeen = 1;
			int uii = (fl[c] & fGoType) && iface[c];
			if (uii) {
				if (ifaceDone[c]) continue;
				ifaceDone[c] = 1;
			}
			lvedges += eoff[c + 1] - eoff[c];
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
		free(cur); levels++;
		fprintf(stderr, "L %d %d %zu %zu\n", rounds, levels, lvsz, lvedges);
		}
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
	double t1 = now();
	size_t nlive = 0;
	for (u32 g = 1; g < N; g++) nlive += live[g];
	u32 *list = malloc(4 * nlive), k = 0;
	for (u32 g = 1; g < N; g++)
		if (live[g]) list[k++] = g;
	printf("p2 (1 core): flood %.1f ms, %zu live, %d rounds, %u methods pruned, reflect %d\n", (t1 - t0) * 1e3, nlive, rounds, nm, reflectSeen);
	long bad = check32("live", list, nlive, "oracle/p2_live.u32");
	printf(bad ? "P2 FAIL\n" : "P2 OK\n");
	return bad != 0;
}
