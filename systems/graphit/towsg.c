// towsg.c: problem 2's flat files -> one GraphIt (GAPBS .wsg) graph whose plain reachability from vertex 0 is
// the live set. GraphIt floods a fixed graph with one rule, "dst becomes reached from a reached src"; the P2
// rules have two coupled sets (live, iface), two ANDs (live and iface; candidate method and called method) and a
// join (a method's (name, type) against the live signatures). So the converter compiles the rules into the graph:
//   vertex 0          Z: the super-root (slot 0 is never live, so it doubles as Z)
//   1..nsl            L(s): s is live
//   I(s), U(s)        per defined gotype s: s is converted to an interface; s is live and iface (an AND)
//   Sig(x), Name(n), R  a signature id, a method-name string id, "reflect's Method is live"
//   C(m), F(m)        per method of a defined gotype: something can call m; m is a candidate and called (an AND)
// Edge weight w: the guard vertex: dst is reached through this edge only if w is reached too (an AND of src and
// w; each of the two inputs carries the other as its guard, so whichever is reached last sees the first).
// A plain edge has w = 0 = Z, which is always reached. Edges (rule from ref/flat/README.md on the right):
//   Z -> L(r) roots; Z -> I(t) flag 16
//   L(s) -> L(t) plain(s,t);  U(s) -> I(t) plain(s,t), gotype(t), def(t)
//   L(s) -> I(t) useiface;  L(s) -> Sig(x) ifacemethod;  L(s) -> Name(n) namedmethod;  L(s) -> R  reflect(s)
//   L(s) -> U(s) [guard I(s)];  I(s) -> U(s) [guard L(s)]
//   U(s) -> I(m.mtyp), U(s) -> F(m) [guard C(m)] for s's methods m;  C(m) -> F(m) [guard U(s)]
//   Sig(x) -> C(m) for m with (name, mtyp) = x's;  Name(n) -> C(m) for m named n;  R -> C(m) if m exported
//   F(m) -> L(m.mtyp), L(m.ifn), L(m.tfn)
// iface on a symbol that is not a defined gotype has no consequence, so only those get I and U vertices.
//   towsg DATA OUT.wsg      (writes OUT.wsg; prints counts and its own time)
#include "../../ref/flat/flat.h"

enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16 };
typedef struct { u32 s, d; i32 w; } E;
static E *es;
static size_t ne, cap;
static void add(u32 s, u32 d, i32 w) {
	if (ne == cap) { cap = cap ? 2 * cap : 1 << 20; es = realloc(es, cap * sizeof(E)); }
	es[ne++] = (E){s, d, w};
}
static u64 *keyOf;
static int cmpm(const void *a, const void *b) {
	u64 x = keyOf[*(const u32 *)a], y = keyOf[*(const u32 *)b];
	return x < y ? -1 : x > y;
}
// write the CSR (by src if !inv, by dst if inv) as GAPBS writes it: offsets (i64, n+1) then (vertex, weight) pairs
static void writeCSR(FILE *f, u32 nv, int inv) {
	i64 *off = calloc(nv + 1, 8);
	for (size_t i = 0; i < ne; i++) off[(inv ? es[i].d : es[i].s) + 1]++;
	for (u32 v = 0; v < nv; v++) off[v + 1] += off[v];
	i32 *nb = malloc(ne * 8);
	i64 *pos = malloc((size_t)nv * 8);
	memcpy(pos, off, (size_t)nv * 8);
	for (size_t i = 0; i < ne; i++) {
		u32 k = inv ? es[i].d : es[i].s;
		i64 p = pos[k]++;
		nb[2 * p] = (i32)(inv ? es[i].s : es[i].d), nb[2 * p + 1] = es[i].w;
	}
	fwrite(off, 8, nv + 1, f);
	fwrite(nb, 8, ne, f);
	free(off), free(nb), free(pos);
}

int main(int argc, char **argv) {
	if (argc < 3) { fprintf(stderr, "usage: towsg DATA OUT.wsg\n"); return 2; }
	flatDir = argv[1];
	double t0 = now();
	u32 nsl = (u32)metaGet("nslots");
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
	double t1 = now();

	// vertex numbering
	u32 *gi = calloc(nsl + 1, 4); // per slot: its index among the defined gotypes + 1 (0: none)
	u32 ng = 0;
	for (u32 s = 1; s <= nsl; s++)
		if ((fl[s] & fDef) && (fl[s] & fGoType)) gi[s] = ++ng;
	u32 baseI = nsl + 1, baseU = baseI + ng, baseSig = baseU + ng, baseName = baseSig + (u32)nsig,
	    R = baseName + (u32)nstr + 1, baseC = R + 1, baseF = baseC + (u32)nmeth, nv = baseF + (u32)nmeth;
#define I(s) (baseI + gi[s] - 1)
#define U(s) (baseU + gi[s] - 1)
#define GT(t) ((t) && (t) <= nsl && gi[t])
	u32 *mOwner = calloc(nmeth, 4); // per method: its type if that is a defined gotype, else 0
	for (size_t r = 0; r < nroots; r++) add(0, roots[r], 0);
	for (u32 s = 1; s <= nsl; s++) {
		if (gi[s] && (fl[s] & fIface)) add(0, I(s), 0);
		if (!(fl[s] & fDef)) continue;
		for (u32 e = eoff[s]; e < eoff[s + 1]; e++) {
			u32 t = ed[e];
			switch (ek[e]) {
			case 0:
				if (t) add(s, t, 0);
				if (gi[s] && GT(t)) add(U(s), I(t), 0);
				break;
			case 1: if (GT(t)) add(s, I(t), 0); break;
			case 2: add(s, baseSig + t, 0); break;
			case 3: add(s, baseName + t, 0); break;
			}
		}
		if (fl[s] & fReflect) add(s, R, 0);
		if (!gi[s]) continue;
		add(s, U(s), (i32)I(s));
		add(I(s), U(s), (i32)s);
		for (u32 m = moff[s]; m < moff[s + 1]; m++) {
			mOwner[m] = s;
			if (GT(mTyp[m])) add(U(s), I(mTyp[m]), 0);
			add(U(s), baseF + m, (i32)(baseC + m));
			add(baseC + m, baseF + m, (i32)U(s));
		}
	}
	// the method side: who can call m, and what calling m keeps
	keyOf = malloc(nmeth * 8);
	u32 *ord = malloc(nmeth * 4), no = 0;
	for (u32 m = 0; m < nmeth; m++) {
		if (!mOwner[m]) continue;
		keyOf[m] = (u64)mName[m] << 32 | mTyp[m];
		ord[no++] = m;
		add(baseName + mName[m], baseC + m, 0);
		if (mExp[m]) add(R, baseC + m, 0);
		u32 ts[3] = {mTyp[m], mIfn[m], mTfn[m]};
		for (int k = 0; k < 3; k++) if (ts[k]) add(baseF + m, ts[k], 0);
	}
	qsort(ord, no, 4, cmpm); // the (name, type) join, as a sort and a binary search per signature
	for (u32 x = 0; x < nsig; x++) {
		u64 k = (u64)sigName[x] << 32 | sigType[x];
		u32 lo = 0, hi = no;
		while (lo < hi) { u32 mid = (lo + hi) / 2; if (keyOf[ord[mid]] < k) lo = mid + 1; else hi = mid; }
		for (; lo < no && keyOf[ord[lo]] == k; lo++) add(baseSig + x, baseC + ord[lo], 0);
	}
	double t2 = now();
	FILE *f = fopen(argv[2], "wb");
	if (!f) { perror(argv[2]); return 2; }
	u8 directed = 1;
	i64 nE = (i64)ne, nV = nv;
	fwrite(&directed, 1, 1, f);
	fwrite(&nE, 8, 1, f);
	fwrite(&nV, 8, 1, f);
	writeCSR(f, nv, 0);
	writeCSR(f, nv, 1);
	fclose(f);
	double t3 = now();
	printf("towsg: %u vertices (%u slots, %u gotypes, %zu sigs, %zu names, %u methods kept), %zu edges; "
	       "map %.1f ms, build %.1f ms, write %.1f ms\n",
	       nv, nsl, ng, nsig, nstr + 1, no, ne, (t1 - t0) * 1e3, (t2 - t1) * 1e3, (t3 - t2) * 1e3);
	return 0;
}
