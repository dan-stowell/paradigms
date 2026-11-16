// p2inc.c: problem 2 (reach) maintained under an edit, plainly, on one core. It floods version A (the previous
// link's state, kept in memory as a resident linker would), then applies a stored diff (DATA_B/diff/, written by
// `ddflow writediff`, see its README.md) as an update, and back, REPS times; every result is checked against B's
// oracle in A's id space (diff/live_b.u32) or A's oracle.
//   gcc -O2 -march=native -o build/p2inc ref/flat/p2inc.c && build/p2inc data/minio data/minio-edit [REPS]
//
// The rules are p2.c's (data/README.md). The state kept between links, all derived by the same rules: live, iface,
// typeDone (live, iface, type descriptor: its type edges and methods have been applied), called (per method),
// support counts of the aggregate facts: sig(name, type), named(name), reflect = how many live sources assert them.
//
// An update is DRed (delete and rederive, Gupta-Mumick-Subrahmanian 1993), in three steps:
//  1. over-delete: with the old flags, every fact derived through a removed edge, root or changed flag, and
//     everything derived from those, transitively, is retracted (live, iface, typeDone, called), and the counts of
//     sig/named/reflect are decremented for every source that stops being live; a method whose condition loses
//     its last support is retracted;
//  2. rederive: with the new flags and edges, each retracted fact that still has a derivation from facts that
//     are true is set again (one reverse lookup: plain/useiface predecessors, calling methods, roots);
//  3. insert: added edges from live sources, added roots, and everything that follows, by the same worklist as
//     the initial flood (only new targets are pushed).
// Why DRed and not counts: liveness is cyclic (a function and its closure keep each other alive), so a count of
// live predecessors never reaches zero for a dead cycle; DRed is exact with cycles. Why not "recompute when a live
// edge is removed": every real edit removes live edges (the edited function's GC-locals symbols are renamed), so
// that policy would recompute every time. The cost of DRed is the over-deleted cone: printed below.
#include "flat.h"

enum { fDef = 1, fReflect = 4, fGoType = 8, fIface = 16 };
#define ISTYPE(x) ((flags[x] & (fDef | fGoType)) == (fDef | fGoType))

static u32 N, nslA, nmeth, nstrU;
static u8 *flags;
// A's edges, deduplicated per source: CSR eoff/ekind/eval (kind 2 val = name << 32 | type; kind 3 val = name)
static u32 *eoff;
static u8 *ekind;
static u64 *eval;
// reverse of A's kind 0 and 1 edges: roff/rsrc/rkind
static u32 *roff, *rsrc;
static u8 *rkind;
// methods (A's tables; method-table diffs are not supported: both edits have none)
static u32 *moff, *mown, *mname, *mtyp, *mifn, *mtfn, *revoff, *revm, *nameoff, *namem, *expm, nexp;
static u8 *mexp;
// state
static u8 *live, *iface, *typeDone, *called, *isRoot;
// derivation order: a fact's stamp is the clock when it was set, so it was derived from facts with smaller stamps
static u32 clk, *ordL, *ordI, *ordT, *ordM;
// where the last support check of x found its support in x's reverse list: entries before it can never support x
// again while x keeps its stamp (a retracted predecessor comes back only with a larger stamp), so the scan resumes
// there; reset when x is stamped anew. This bounds the checks of x to one pass over its predecessors per stamp.
static u32 *posL, *posI;
static int useOrder = 1; // argv[4] "plain": textbook DRed (retract everything downstream, then rederive)
static u64 cChecks;
static u32 *namedCnt, reflectCnt;
// sig table: key -> count, and the methods with that (name, mtyp) as a sorted-run range
typedef struct { u64 key; u32 cnt, mstart, mlen, sstart, slen; } sigent;
static sigent *sigs;
static u64 smask;
static u32 *bysig;
// the sources asserting each aggregate fact in A (for well-founded checks of a method's condition): sig key ->
// sources (sigent.sstart/slen into sigsrc), name -> sources (namesrc CSR), reflect callers (reflsrc)
static u32 *sigsrc, *nsoff, *namesrc, *reflsrc, nrefl;
static u32 *msusp;
static size_t nmsusp, msuspcap;
// overlay: edges removed from A (hash set), edges added (per-source and per-target lists)
typedef struct { u32 src; u8 kind, alive; u64 val; } remEnt;
static remEnt *rem;
static u64 rmask;
static u8 *maybeRem;
typedef struct { u32 src, next, rnext; u8 kind, alive; u64 val; } addEnt;
static addEnt *pool;
static u32 npool, *addHead, *raddHead;
// worklist and counters
enum { LIVE, IFACE, KL, KI };
static u64 *wl;
static size_t wn, wcap;
static u32 epoch, *expanded;
static u64 cEdges, cRev, cKilled, cRestored, cSet;
static u32 *susp;
static size_t nsusp, suspcap;

static void push(int t, u32 x) {
	if (wn == wcap) wl = realloc(wl, 8 * (wcap = wcap ? 2 * wcap : 1 << 20));
	wl[wn++] = (u64)t << 32 | x;
}
static void suspect(u32 x) {
	if (nsusp == suspcap) susp = realloc(susp, 4 * (suspcap = suspcap ? 2 * suspcap : 1 << 16));
	susp[nsusp++] = x;
}
static sigent *sig(u64 k) {
	for (u64 h = mix64(k) & smask;; h = (h + 1) & smask) {
		if (sigs[h].key == k + 1) return &sigs[h];
		if (sigs[h].key == 0) { sigs[h].key = k + 1; return &sigs[h]; }
	}
}
static remEnt *remFind(u32 s, u8 k, u64 v, int add) {
	for (u64 h = mix64(v * 31 + s * 7 + k) & rmask;; h = (h + 1) & rmask) {
		remEnt *e = &rem[h];
		if (e->alive != 0xff && e->src == s && e->kind == k && e->val == v) return e;
		if (e->alive == 0xff) {
			if (!add) return NULL;
			e->src = s, e->kind = k, e->val = v, e->alive = 0;
			return e;
		}
	}
}
static inline int removed(u32 s, u8 k, u64 v) {
	if (!maybeRem[s]) return 0;
	remEnt *e = remFind(s, k, v, 0);
	return e && e->alive;
}
// iterate the current out-edges of x: A's minus removed, plus added
#define FOR_EDGES(x, K, V, BODY)                                                       \
	do {                                                                               \
		if ((x) <= nslA)                                                               \
			for (u32 e_ = eoff[x]; e_ < eoff[(x) + 1]; e_++) {                          \
				u8 K = ekind[e_];                                                      \
				u64 V = eval[e_];                                                      \
				cEdges++;                                                              \
				if (removed(x, K, V)) continue;                                        \
				BODY                                                                   \
			}                                                                          \
		for (u32 a_ = addHead[x]; a_; a_ = pool[a_].next) {                            \
			if (!pool[a_].alive) continue;                                             \
			u8 K = pool[a_].kind;                                                      \
			u64 V = pool[a_].val;                                                      \
			cEdges++;                                                                  \
			BODY                                                                       \
		}                                                                              \
	} while (0)
// iterate the current kind 0/1 predecessors of x
#define FOR_PREDS(x, S, K, BODY)                                                       \
	do {                                                                               \
		if ((x) <= nslA)                                                               \
			for (u32 r_ = roff[x]; r_ < roff[(x) + 1]; r_++) {                          \
				u32 S = rsrc[r_];                                                      \
				u8 K = rkind[r_];                                                      \
				cRev++;                                                                \
				if (removed(S, K, x)) continue;                                        \
				BODY                                                                   \
			}                                                                          \
		for (u32 a_ = raddHead[x]; a_; a_ = pool[a_].rnext) {                          \
			if (!pool[a_].alive) continue;                                             \
			u32 S = pool[a_].src;                                                      \
			u8 K = pool[a_].kind;                                                      \
			cRev++;                                                                    \
			BODY                                                                       \
		}                                                                              \
	} while (0)

// ---------------------------------------------------------------- additive rules (the flood)
static inline void setLive(u32 x) {
	if (x && !live[x]) { live[x] = 1; ordL[x] = ++clk; posL[x] = 0; cSet++; push(LIVE, x); }
}
static inline void setIface(u32 x) {
	if (x && !iface[x]) { iface[x] = 1; ordI[x] = ++clk; posI[x] = 0; if (live[x]) push(IFACE, x); }
}
static inline int cond(u32 m) {
	return (reflectCnt && mexp[m]) || sig((u64)mname[m] << 32 | mtyp[m])->cnt || namedCnt[mname[m]];
}
static void tryCall(u32 m) {
	if (called[m] || !typeDone[mown[m]] || !cond(m)) return;
	called[m] = 1;
	ordM[m] = ++clk;
	setLive(mtyp[m]), setLive(mifn[m]), setLive(mtfn[m]);
}
static void typeAct(u32 x) {
	if (typeDone[x]) return;
	typeDone[x] = 1;
	ordT[x] = ++clk;
	FOR_EDGES(x, k, v, if (k == 0 && ISTYPE((u32)v)) setIface((u32)v););
	if (x <= nslA)
		for (u32 m = moff[x]; m < moff[x + 1]; m++) setIface(mtyp[m]), tryCall(m);
}
static void sigInc(u64 k) {
	sigent *s = sig(k);
	if (s->cnt++ == 0)
		for (u32 i = 0; i < s->mlen; i++) tryCall(bysig[s->mstart + i]);
}
static void namedInc(u32 n) {
	if (namedCnt[n]++ == 0)
		for (u32 i = nameoff[n]; i < nameoff[n + 1]; i++) tryCall(namem[i]);
}
static void reflectInc(void) {
	if (reflectCnt++ == 0)
		for (u32 i = 0; i < nexp; i++) tryCall(expm[i]);
}
static void edgeOn(u32 x, u8 k, u64 v) {
	switch (k) {
	case 0: setLive((u32)v); if (typeDone[x] && ISTYPE((u32)v)) setIface((u32)v); break;
	case 1: setIface((u32)v); break;
	case 2: sigInc(v); break;
	case 3: namedInc((u32)v); break;
	}
}
static void expandLive(u32 x) {
	if (!(flags[x] & fDef)) return;
	expanded[x] = epoch;
	FOR_EDGES(x, k, v, edgeOn(x, k, v););
	if (flags[x] & fReflect) reflectInc();
	if (ISTYPE(x) && iface[x]) typeAct(x);
}

// ---------------------------------------------------------------- retraction
// a well-founded support: a derivation from a true fact with a smaller stamp. Keeping x when it has one, and
// re-checking x whenever one of its supports is retracted, leaves only facts with a chain of decreasing stamps
// down to a root (or a flag): derivable. Everything else is retracted, then step 2 rederives what has a support
// with a larger stamp (a cycle-free path that was not the first one found).
static int supLive(u32 x) {
	if (!useOrder) return 0;
	cChecks++;
	if (isRoot[x]) return 1;
	u32 o = ordL[x];
	if (x <= nslA)
		for (u32 r = roff[x] + posL[x]; r < roff[x + 1]; r++) {
			u32 s = rsrc[r];
			cRev++;
			if (rkind[r] == 0 && live[s] && (flags[s] & fDef) && ordL[s] < o && !removed(s, 0, x)) { posL[x] = r - roff[x]; return 1; }
		}
	for (u32 a = raddHead[x]; a; a = pool[a].rnext)
		if (pool[a].alive && pool[a].kind == 0 && live[pool[a].src] && (flags[pool[a].src] & fDef) && ordL[pool[a].src] < o) return 1;
	if (x <= nslA)
		for (u32 i = revoff[x]; i < revoff[x + 1]; i++) if (called[revm[i]] && ordM[revm[i]] < o) return 1;
	return 0;
}
static int supIface(u32 x) {
	if (!useOrder) return 0;
	cChecks++;
	if (flags[x] & fIface) return 1;
	u32 o = ordI[x];
#define IOK(s, k) (live[s] && (flags[s] & fDef) && (((k) == 1 && ordL[s] < o) || ((k) == 0 && typeDone[s] && ISTYPE(x) && ordT[s] < o)))
	if (x <= nslA)
		for (u32 r = roff[x] + posI[x]; r < roff[x + 1]; r++) {
			u32 s = rsrc[r];
			cRev++;
			if (IOK(s, rkind[r]) && !removed(s, rkind[r], x)) { posI[x] = r - roff[x]; return 1; }
		}
	for (u32 a = raddHead[x]; a; a = pool[a].rnext)
		if (pool[a].alive && IOK(pool[a].src, pool[a].kind)) return 1;
	if (x <= nslA)
		for (u32 i = revoff[x]; i < revoff[x + 1]; i++) if (mtyp[revm[i]] == x && typeDone[mown[revm[i]]] && ordT[mown[revm[i]]] < o) return 1;
	return 0;
}
static int forceKill;
static inline void killLive(u32 x) {
	if (x && live[x] && (forceKill || !supLive(x))) { live[x] = 0; cKilled++; suspect(x); push(KL, x); }
}
static inline void killIface(u32 x) {
	if (x && iface[x] && (forceKill || !supIface(x))) { iface[x] = 0; suspect(x); push(KI, x); }
}
static void uncall(u32 m) {
	if (!called[m]) return;
	called[m] = 0;
	if (nmsusp == msuspcap) msusp = realloc(msusp, 4 * (msuspcap = msuspcap ? 2 * msuspcap : 1 << 12));
	msusp[nmsusp++] = m;
	killLive(mtyp[m]), killLive(mifn[m]), killLive(mtfn[m]);
}
static void typeDeact(u32 x) {
	if (!typeDone[x]) return;
	typeDone[x] = 0;
	FOR_EDGES(x, k, v, if (k == 0 && ISTYPE((u32)v)) killIface((u32)v););
	if (x <= nslA)
		for (u32 m = moff[x]; m < moff[x + 1]; m++) killIface(mtyp[m]), uncall(m);
}
// Is called method m still supported by facts with smaller stamps? Its type is (typeDone: else it was uncalled);
// its condition needs a live source asserting it with a stamp below m's. A count alone is not enough: the
// remaining sources may be m's own targets (a cycle through the method). Plain DRed: never (retract, rederive).
static inline int srcOK(u32 s, u8 k, u64 v, u32 o) { return live[s] && (flags[s] & fDef) && ordL[s] < o && !removed(s, k, v); }
static int condWF(u32 m) {
	if (!useOrder) return 0;
	cChecks++;
	u32 o = ordM[m];
	u64 key = (u64)mname[m] << 32 | mtyp[m];
	sigent *se = sig(key);
	if (se->cnt)
		for (u32 i = 0; i < se->slen; i++) { cRev++; if (srcOK(sigsrc[se->sstart + i], 2, key, o)) return 1; }
	u32 n = mname[m];
	if (namedCnt[n])
		for (u32 i = nsoff[n]; i < nsoff[n + 1]; i++) { cRev++; if (srcOK(namesrc[i], 3, n, o)) return 1; }
	if (mexp[m] && reflectCnt)
		for (u32 i = 0; i < nrefl; i++) { cRev++; u32 r = reflsrc[i]; if (live[r] && (flags[r] & (fDef | fReflect)) == (fDef | fReflect) && ordL[r] < o) return 1; }
	for (u32 a = 1; a <= npool; a++) // added edges asserting it
		if (pool[a].alive && ((pool[a].kind == 2 && pool[a].val == key) || (pool[a].kind == 3 && pool[a].val == n)) && live[pool[a].src] && ordL[pool[a].src] < o) return 1;
	return 0;
}
static void sigDec(u64 k) {
	sigent *s = sig(k);
	--s->cnt;
	for (u32 i = 0; i < s->mlen; i++) { u32 m = bysig[s->mstart + i]; if (called[m] && !condWF(m)) uncall(m); }
}
static void namedDec(u32 n) {
	--namedCnt[n];
	for (u32 i = nameoff[n]; i < nameoff[n + 1]; i++) if (called[namem[i]] && !condWF(namem[i])) uncall(namem[i]);
}
static void reflectDec(void) {
	--reflectCnt;
	for (u32 i = 0; i < nexp; i++) if (called[expm[i]] && !condWF(expm[i])) uncall(expm[i]);
}
static void edgeOff(u32 x, u8 k, u64 v) {
	switch (k) {
	case 0: killLive((u32)v); if (typeDone[x] && ISTYPE((u32)v)) killIface((u32)v); break;
	case 1: killIface((u32)v); break;
	case 2: sigDec(v); break;
	case 3: namedDec((u32)v); break;
	}
}

static void drain(void) {
	while (wn) {
		u64 it = wl[--wn];
		u32 x = (u32)it;
		switch ((int)(it >> 32)) {
		case LIVE: if (live[x]) expandLive(x); break;
		case IFACE: if (live[x] && iface[x] && ISTYPE(x) && (flags[x] & fDef)) typeAct(x); break;
		case KL:
			if (flags[x] & fDef) {
				FOR_EDGES(x, k, v, edgeOff(x, k, v););
				if (flags[x] & fReflect) reflectDec();
			}
			typeDeact(x);
			break;
		case KI: typeDeact(x); break;
		}
	}
}

// ---------------------------------------------------------------- rederivation checks
static int derivLive(u32 x) {
	if (isRoot[x]) return 1;
	FOR_PREDS(x, s, k, if (k == 0 && live[s] && (flags[s] & fDef)) return 1;);
	if (x <= nslA)
		for (u32 i = revoff[x]; i < revoff[x + 1]; i++) if (called[revm[i]]) return 1;
	return 0;
}
static int derivIface(u32 x) {
	if (flags[x] & fIface) return 1;
	FOR_PREDS(x, s, k, if (live[s] && (flags[s] & fDef) && (k == 1 || (typeDone[s] && ISTYPE(x)))) return 1;);
	if (x <= nslA)
		for (u32 i = revoff[x]; i < revoff[x + 1]; i++) if (mtyp[revm[i]] == x && typeDone[mown[revm[i]]]) return 1;
	return 0;
}

// ---------------------------------------------------------------- the diff
typedef struct {
	size_t ne, nf, nr;
	const u32 *esrc, *fid, *root;
	const u8 *ekind, *fval;
	const u64 *eval;
} side;
static side loadSide(const char *s) {
	side d;
	char p[64];
	size_t n;
	snprintf(p, sizeof p, "diff/edge_%s_src.u32", s), d.esrc = FMAP(u32, p, &n), d.ne = n / 4;
	snprintf(p, sizeof p, "diff/edge_%s_kind.u8", s), d.ekind = FMAP(u8, p, NULL);
	snprintf(p, sizeof p, "diff/edge_%s_val.u64", s), d.eval = FMAP(u64, p, NULL);
	snprintf(p, sizeof p, "diff/flag_%s_id.u32", s), d.fid = FMAP(u32, p, &n), d.nf = n / 4;
	snprintf(p, sizeof p, "diff/flag_%s_val.u8", s), d.fval = FMAP(u8, p, NULL);
	snprintf(p, sizeof p, "diff/root_%s.u32", s), d.root = FMAP(u32, p, &n), d.nr = n / 4;
	snprintf(p, sizeof p, "diff/meth_%s_type.u32", s), FMAP(u32, p, &n);
	if (n) { fprintf(stderr, "method-table diffs are not supported (recompute instead)\n"); exit(3); }
	return d;
}
static int inA(u32 s, u8 k, u64 v) {
	if (s > nslA) return 0;
	for (u32 e = eoff[s]; e < eoff[s + 1]; e++) if (ekind[e] == k && eval[e] == v) return 1;
	return 0;
}
static void edgeRemove(u32 s, u8 k, u64 v) {
	if (inA(s, k, v)) { remFind(s, k, v, 1)->alive = 1; maybeRem[s] = 1; return; }
	for (u32 a = addHead[s]; a; a = pool[a].next)
		if (pool[a].alive && pool[a].kind == k && pool[a].val == v) pool[a].alive = 0;
}
static void edgeAdd(u32 s, u8 k, u64 v) {
	if (inA(s, k, v)) { remFind(s, k, v, 1)->alive = 0; return; }
	addEnt *a = &pool[++npool];
	a->src = s, a->kind = k, a->val = v, a->alive = 1;
	a->next = addHead[s], addHead[s] = npool;
	if (k <= 1) a->rnext = raddHead[(u32)v], raddHead[(u32)v] = npool;
}

// apply minus then plus as one update
static void update(const side *mi, const side *pl) {
	epoch++;
	nsusp = nmsusp = 0;
	// 1. over-delete, with the old flags
	// (all removed edges leave the graph before any retraction: a source retracted by an earlier removal must
	// not leave a later removed edge's target unchecked)
	static u8 *was;
	static size_t wasCap;
	if (mi->ne > wasCap) was = realloc(was, wasCap = mi->ne);
	for (size_t i = 0; i < mi->ne; i++) {
		u32 s = mi->esrc[i];
		was[i] = live[s] && (flags[s] & fDef);
		edgeRemove(s, mi->ekind[i], mi->eval[i]);
	}
	for (size_t i = 0; i < mi->ne; i++)
		if (was[i]) edgeOff(mi->esrc[i], mi->ekind[i], mi->eval[i]);
	forceKill = 1;
	for (size_t i = 0; i < mi->nr; i++) isRoot[mi->root[i]] = 0, killLive(mi->root[i]);
	for (size_t i = 0; i < mi->nf; i++) killLive(mi->fid[i]), killIface(mi->fid[i]);
	for (size_t i = 0; i < pl->nf; i++) killLive(pl->fid[i]), killIface(pl->fid[i]);
	forceKill = 0;
	drain();
	// the new flags, edges, roots
	for (size_t i = 0; i < mi->nf; i++) flags[mi->fid[i]] = 0;
	for (size_t i = 0; i < pl->nf; i++) flags[pl->fid[i]] = pl->fval[i];
	for (size_t i = 0; i < pl->ne; i++) edgeAdd(pl->esrc[i], pl->ekind[i], pl->eval[i]);
	for (size_t i = 0; i < pl->nr; i++) isRoot[pl->root[i]] = 1;
	// 2. rederive what still has a derivation; the worklist restores what follows from it
	for (size_t i = 0; i < pl->nf; i++) suspect(pl->fid[i]);
	for (size_t i = 0; i < nsusp; i++) {
		u32 x = susp[i];
		if (!live[x] && derivLive(x)) setLive(x), cRestored++;
		if (!iface[x] && derivIface(x)) setIface(x);
	}
	// methods whose condition lost its well-founded support were uncalled; their types may still be active
	for (size_t i = 0; i < nmsusp; i++) tryCall(msusp[i]);
	drain();
	// 3. insert: added edges of live sources not yet expanded with them, added roots
	for (size_t i = 0; i < pl->ne; i++) {
		u32 s = pl->esrc[i];
		if (live[s] && (flags[s] & fDef) && expanded[s] != epoch) edgeOn(s, pl->ekind[i], pl->eval[i]);
	}
	for (size_t i = 0; i < pl->nr; i++) setLive(pl->root[i]);
	drain();
	// a method of a still-active type whose condition came back (sig/named counts restored) is retried by
	// sigInc/namedInc; one whose type was never deactivated but lost its condition and regained it, too.
}

static long checkLive(const char *what, const u32 *want, size_t nw) {
	size_t k = 0, bad = 0;
	for (u32 g = 1; g < N; g++)
		if (live[g]) { if (k >= nw || want[k] != g) bad++; else k++; }
	bad += nw - k;
	printf("check %s: %zu oracle entries, %zu differ -> %s\n", what, nw, bad, bad ? "FAIL" : "OK");
	return (long)bad;
}

int main(int argc, char **argv) {
	const char *da = argv[1], *db = argv[2];
	int reps = argc > 3 ? atoi(argv[3]) : 5;
	{
		char p[4096];
		snprintf(p, sizeof p, "%s/diff", db);
		flatDir = p;
		N = (u32)metaGet("n_unified");
		nstrU = (u32)metaGet("nstr_unified");
		flatDir = strdup(db);
	}
	side mi = loadSide("minus"), pl = loadSide("plus");
	size_t nlb;
	const u32 *liveB = FMAP(u32, "diff/live_b.u32", &nlb);
	nlb /= 4;
	flatDir = da;
	nslA = (u32)metaGet("nslots");
	size_t nroots, nlA;
	const u8 *fl = FMAP(u8, "p2_flags.u8", NULL), *ek = FMAP(u8, "p2_edge_kind.u8", NULL);
	const u32 *off = FMAP(u32, "p2_edge_off.u32", NULL), *ed = FMAP(u32, "p2_edge_dst.u32", NULL);
	const u32 *sigName = FMAP(u32, "p2_sig_name.u32", NULL), *sigType = FMAP(u32, "p2_sig_type.u32", NULL);
	size_t nm;
	moff = (u32 *)FMAP(u32, "p2_meth_off.u32", NULL), mname = (u32 *)FMAP(u32, "p2_meth_name.u32", &nm);
	nmeth = (u32)(nm / 4);
	mtyp = (u32 *)FMAP(u32, "p2_meth_mtyp.u32", NULL), mifn = (u32 *)FMAP(u32, "p2_meth_ifn.u32", NULL);
	mtfn = (u32 *)FMAP(u32, "p2_meth_tfn.u32", NULL), mexp = (u8 *)FMAP(u8, "p2_meth_exported.u8", NULL);
	const u32 *roots = FMAP(u32, "p2_roots.u32", &nroots);
	const u32 *liveA = FMAP(u32, "oracle/p2_live.u32", &nlA);
	nroots /= 4, nlA /= 4;

	double ts = now();
	// ---- setup (the resident state of the previous link): dedup'd CSR, reverse CSR, method indexes
	flags = calloc(N + 1, 1);
	for (u32 g = 1; g <= nslA; g++) flags[g] = fl[g] & 0x1d;
	eoff = malloc(4 * ((size_t)nslA + 2));
	ekind = malloc(off[nslA + 1]), eval = malloc(8 * (size_t)off[nslA + 1]);
	u32 ne = 0;
	typedef struct { u64 v; u32 k; } kv;
	int ckv(const void *a, const void *b) {
		const kv *x = a, *y = b;
		return x->k != y->k ? (x->k < y->k ? -1 : 1) : x->v < y->v ? -1 : x->v > y->v;
	}
	kv *tmp = malloc(sizeof(kv) * 65536);
	size_t tcap = 65536;
	for (u32 s = 0; s <= nslA; s++) {
		eoff[s] = ne;
		size_t n = 0;
		if (off[s + 1] - off[s] > tcap) tmp = realloc(tmp, sizeof(kv) * (tcap = off[s + 1] - off[s]));
		for (u32 e = off[s]; e < off[s + 1]; e++) {
			u8 k = ek[e];
			u32 t = ed[e];
			if (k <= 1 && t == 0) continue;
			tmp[n].k = k, tmp[n++].v = k == 2 ? (u64)sigName[t] << 32 | sigType[t] : t;
		}
		if (n > 1) qsort(tmp, n, sizeof(kv), ckv);
		for (size_t i = 0; i < n; i++)
			if (i == 0 || tmp[i].k != tmp[i - 1].k || tmp[i].v != tmp[i - 1].v) ekind[ne] = (u8)tmp[i].k, eval[ne++] = tmp[i].v;
	}
	free(tmp);
	eoff[nslA + 1] = ne;
	roff = calloc((size_t)nslA + 2, 4);
	for (u32 e = 0; e < ne; e++) if (ekind[e] <= 1 && eval[e] <= nslA) roff[eval[e] + 1]++;
	for (u32 g = 0; g <= nslA; g++) roff[g + 1] += roff[g];
	rsrc = malloc(4 * (size_t)roff[nslA + 1]), rkind = malloc(roff[nslA + 1]);
	{
		u32 *fill = malloc(4 * ((size_t)nslA + 1));
		memcpy(fill, roff, 4 * ((size_t)nslA + 1));
		for (u32 s = 0; s <= nslA; s++)
			for (u32 e = eoff[s]; e < eoff[s + 1]; e++)
				if (ekind[e] <= 1 && eval[e] <= nslA) rsrc[fill[eval[e]]] = s, rkind[fill[eval[e]]++] = ekind[e];
		free(fill);
	}
	mown = malloc(4 * (size_t)nmeth);
	for (u32 t = 0; t <= nslA; t++) for (u32 m = moff[t]; m < moff[t + 1]; m++) mown[m] = t;
	revoff = calloc((size_t)nslA + 2, 4);
	for (u32 m = 0; m < nmeth; m++) {
		u32 x[3] = {mtyp[m], mifn[m], mtfn[m]};
		for (int j = 0; j < 3; j++) if (x[j] && x[j] <= nslA) revoff[x[j] + 1]++;
	}
	for (u32 g = 0; g <= nslA; g++) revoff[g + 1] += revoff[g];
	revm = malloc(4 * (size_t)revoff[nslA + 1]);
	{
		u32 *fill = malloc(4 * ((size_t)nslA + 1));
		memcpy(fill, revoff, 4 * ((size_t)nslA + 1));
		for (u32 m = 0; m < nmeth; m++) {
			u32 x[3] = {mtyp[m], mifn[m], mtfn[m]};
			for (int j = 0; j < 3; j++) if (x[j] && x[j] <= nslA) revm[fill[x[j]]++] = m;
		}
		free(fill);
	}
	nameoff = calloc(nstrU + 2, 4);
	for (u32 m = 0; m < nmeth; m++) nameoff[mname[m] + 1]++;
	for (u32 i = 0; i <= nstrU; i++) nameoff[i + 1] += nameoff[i];
	namem = malloc(4 * (size_t)nmeth);
	{
		u32 *fill = malloc(4 * (nstrU + 1));
		memcpy(fill, nameoff, 4 * (nstrU + 1));
		for (u32 m = 0; m < nmeth; m++) namem[fill[mname[m]]++] = m;
		free(fill);
	}
	expm = malloc(4 * (size_t)nmeth);
	for (u32 m = 0; m < nmeth; m++) if (mexp[m]) expm[nexp++] = m;
	smask = (1u << 19) - 1;
	sigs = calloc(smask + 1, sizeof(sigent));
	{
		// group methods by (name, mtyp): sort method ids by key
		u64 *km = malloc(8 * (size_t)nmeth * 2);
		for (u32 m = 0; m < nmeth; m++) km[2 * m] = (u64)mname[m] << 32 | mtyp[m], km[2 * m + 1] = m;
		int c2(const void *a, const void *b) { u64 x = *(u64 *)a, y = *(u64 *)b; return x < y ? -1 : x > y; }
		qsort(km, nmeth, 16, c2);
		bysig = malloc(4 * (size_t)nmeth);
		for (u32 i = 0; i < nmeth; i++) bysig[i] = (u32)km[2 * i + 1];
		for (u32 i = 0; i < nmeth;) {
			u32 j = i;
			while (j < nmeth && km[2 * j] == km[2 * i]) j++;
			sigent *s = sig(km[2 * i]);
			s->mstart = i, s->mlen = j - i;
			i = j;
		}
		free(km);
	}
	{
		// sources of each sig key and name, reflect callers
		u64 *ks = malloc(16 * (size_t)ne);
		size_t nk = 0;
		nsoff = calloc(nstrU + 2, 4);
		for (u32 x = 0; x <= nslA; x++)
			for (u32 e = eoff[x]; e < eoff[x + 1]; e++) {
				if (ekind[e] == 2) ks[2 * nk] = eval[e], ks[2 * nk++ + 1] = x;
				if (ekind[e] == 3) nsoff[eval[e] + 1]++;
			}
		int c3(const void *a, const void *b) { u64 x = *(u64 *)a, y = *(u64 *)b; return x < y ? -1 : x > y; }
		qsort(ks, nk, 16, c3);
		sigsrc = malloc(4 * (nk + 1));
		for (size_t i = 0; i < nk;) {
			size_t j = i;
			while (j < nk && ks[2 * j] == ks[2 * i]) sigsrc[j] = (u32)ks[2 * j + 1], j++;
			sigent *se = sig(ks[2 * i]);
			se->sstart = (u32)i, se->slen = (u32)(j - i);
			i = j;
		}
		free(ks);
		for (u32 i = 0; i <= nstrU; i++) nsoff[i + 1] += nsoff[i];
		namesrc = malloc(4 * ((size_t)nsoff[nstrU + 1] + 1));
		u32 *fill = malloc(4 * (nstrU + 1));
		memcpy(fill, nsoff, 4 * (nstrU + 1));
		for (u32 x = 0; x <= nslA; x++)
			for (u32 e = eoff[x]; e < eoff[x + 1]; e++) if (ekind[e] == 3) namesrc[fill[eval[e]]++] = x;
		free(fill);
		reflsrc = malloc(4 * ((size_t)N + 1));
		for (u32 g = 1; g <= nslA; g++) if ((flags[g] & (fDef | fReflect)) == (fDef | fReflect)) reflsrc[nrefl++] = g;
		for (size_t i = 0; i < pl.nf; i++) if ((pl.fval[i] & (fDef | fReflect)) == (fDef | fReflect)) reflsrc[nrefl++] = pl.fid[i];
		for (size_t i = 0; i < mi.nf; i++) if ((mi.fval[i] & (fDef | fReflect)) == (fDef | fReflect) && mi.fid[i] > nslA) reflsrc[nrefl++] = mi.fid[i];
	}
	rmask = (1u << 16) - 1;
	rem = malloc((rmask + 1) * sizeof(remEnt));
	for (u64 i = 0; i <= rmask; i++) rem[i].alive = 0xff;
	maybeRem = calloc(N + 1, 1);
	pool = calloc(1 + 4 * (mi.ne + pl.ne + 16) + (getenv("FUZZ") ? 256 * (size_t)atoi(getenv("FUZZ")) : 0), sizeof(addEnt));
	addHead = calloc(N + 1, 4), raddHead = calloc(N + 1, 4);
	live = calloc(N + 1, 1), iface = calloc(N + 1, 1), typeDone = calloc(N + 1, 1), isRoot = calloc(N + 1, 1);
	posL = calloc(N + 1, 4), posI = calloc(N + 1, 4);
	ordL = calloc(N + 1, 4), ordI = calloc(N + 1, 4), ordT = calloc(N + 1, 4), ordM = calloc(nmeth, 4);
	if (argc > 4 && strcmp(argv[4], "plain") == 0) useOrder = 0;
	called = calloc(nmeth, 1), namedCnt = calloc(nstrU + 1, 4), expanded = calloc(N + 1, 4);
	double tsetup = now() - ts;

	// ---- the previous link: flood A with the same rules (its state is what an update maintains)
	double t0 = now();
	epoch = 1;
	for (u32 g = 1; g <= nslA; g++) if (flags[g] & fIface) iface[g] = 1;
	for (size_t i = 0; i < nroots; i++) isRoot[roots[i]] = 1, setLive(roots[i]);
	drain();
	double tfull = now() - t0;
	printf("p2inc (%s): setup %.1f ms, flood A (same rules, worklist) %.1f ms, %llu edges read\n", useOrder ? "DRed + derivation-order supports" : "plain DRed", tsetup * 1e3, tfull * 1e3,
	       (unsigned long long)cEdges);
	long bad = checkLive("A", liveA, nlA);
	printf("diff: edges -%zu +%zu, flags -%zu +%zu, roots -%zu +%zu\n", mi.ne, pl.ne, mi.nf, pl.nf, mi.nr, pl.nr);
	double best[2] = {1e9, 1e9};
	for (int r = 0; r < reps; r++)
		for (int dir = 0; dir < 2; dir++) {
			cEdges = cRev = cKilled = cRestored = cSet = cChecks = 0;
			double t = now();
			if (dir == 0) update(&mi, &pl); else update(&pl, &mi);
			t = now() - t;
			if (t < best[dir]) best[dir] = t;
			printf("update %s\t%.3f ms\trep %d\tover-deleted %llu live, rederived %llu directly, set %llu, edges read %llu, "
			       "reverse entries %llu, support checks %llu\n",
			       dir ? "B->A" : "A->B", t * 1e3, r, (unsigned long long)cKilled, (unsigned long long)cRestored,
			       (unsigned long long)cSet, (unsigned long long)cEdges, (unsigned long long)cRev, (unsigned long long)cChecks);
			bad += dir ? checkLive("B->A vs A oracle", liveA, nlA) : checkLive("A->B vs B oracle (unified ids)", liveB, nlb);
		}
	printf("best update A->B %.3f ms, B->A %.3f ms\n", best[0] * 1e3, best[1] * 1e3);
	// FUZZ=K: K random diffs of random size (up to 200 edges removed from live sources, up to 200 added, all
	// kinds), each applied as an update on top of B and checked against a from-scratch flood of the same
	// graph by the same rules, then reverted and checked against B again
	int fz = getenv("FUZZ") ? atoi(getenv("FUZZ")) : 0;
	if (fz) {
		update(&mi, &pl);
		u64 rs = 88172645463325252ull;
#define RND() (rs ^= rs << 13, rs ^= rs >> 7, rs ^= rs << 17)
		u8 *snap = malloc(N + 1);
		long fbad = 0;
		for (int f = 0; f < fz; f++) {
			size_t nr = RND() % 200, na = RND() % 200;
			u32 *rsrc_ = malloc(4 * (nr + na)), *asrc = rsrc_ + nr;
			u8 *rk = malloc(nr + na), *ak = rk + nr;
			u64 *rv = malloc(8 * (nr + na)), *av = rv + nr;
			size_t n1 = 0, n2 = 0;
			while (n1 < nr) { // an existing A edge of a live source, not already removed
				u32 sx = (u32)(RND() % nslA) + 1;
				if (!live[sx] || eoff[sx] == eoff[sx + 1]) continue;
				u32 e = eoff[sx] + (u32)(RND() % (eoff[sx + 1] - eoff[sx]));
				if (removed(sx, ekind[e], eval[e])) continue;
				int dup = 0;
				for (size_t j = 0; j < n1; j++) dup |= rsrc_[j] == sx && rk[j] == ekind[e] && rv[j] == eval[e];
				if (dup) continue;
				rsrc_[n1] = sx, rk[n1] = ekind[e], rv[n1++] = eval[e];
			}
			while (n2 < na) { // a new edge from a random defined source to a random id, or a random existing payload
				u32 sx = (u32)(RND() % nslA) + 1, sy = (u32)(RND() % nslA) + 1;
				if (!(flags[sx] & fDef) || eoff[sy] == eoff[sy + 1]) continue;
				u32 e = eoff[sy] + (u32)(RND() % (eoff[sy + 1] - eoff[sy]));
				u8 k = ekind[e];
				u64 v = (k <= 1 && RND() % 2) ? (u64)(RND() % nslA) + 1 : eval[e];
				if (inA(sx, k, v)) continue;
				int dup = 0;
				for (size_t j = 0; j < n2; j++) dup |= asrc[j] == sx && ak[j] == k && av[j] == v;
				for (u32 a = addHead[sx]; a; a = pool[a].next) dup |= pool[a].alive && pool[a].kind == k && pool[a].val == v;
				if (dup) continue;
				asrc[n2] = sx, ak[n2] = k, av[n2++] = v;
			}
			side m2 = {nr, 0, 0, rsrc_, NULL, NULL, rk, NULL, rv}, p2 = {na, 0, 0, asrc, NULL, NULL, ak, NULL, av};
			update(&m2, &p2);
			memcpy(snap, live, N + 1);
			u8 *sI = getenv("FUZZV") ? malloc(N + 1) : NULL, *sT = sI ? malloc(N + 1) : NULL, *sC = sI ? malloc(nmeth) : NULL;
			if (sI) memcpy(sI, iface, N + 1), memcpy(sT, typeDone, N + 1), memcpy(sC, called, nmeth);
			u32 *sS = sI ? malloc(4 * (smask + 1)) : NULL, *sN = sI ? malloc(4 * (nstrU + 1)) : NULL, sR = reflectCnt;
			if (sI) { for (u64 h = 0; h <= smask; h++) sS[h] = sigs[h].cnt; memcpy(sN, namedCnt, 4 * (nstrU + 1)); }
			// from scratch on the same (current) graph
			memset(live, 0, N + 1), memset(iface, 0, N + 1), memset(typeDone, 0, N + 1), memset(called, 0, nmeth);
			memset(namedCnt, 0, 4 * (nstrU + 1)), reflectCnt = 0;
			for (u64 h = 0; h <= smask; h++) sigs[h].cnt = 0;
			for (u32 g = 1; g <= N; g++) if (flags[g] & fIface) iface[g] = 1;
			epoch++;
			for (u32 g = 1; g <= N; g++) if (isRoot[g]) setLive(g);
			drain();
			long d = 0;
			for (u32 g = 0; g <= N; g++) d += snap[g] != live[g];
			if (d) printf("fuzz %d: -%zu +%zu edges: %ld ids differ from scratch\n", f, nr, na, d);
			if (d && getenv("FUZZV"))
				for (u32 g = 0, c = 0; g <= N && c < 12; g++)
					if (snap[g] != live[g]) {
						c++, printf("  id %u: update %d scratch %d flags %d iface %d type %d revm %u\n", g, snap[g], live[g], flags[g], iface[g], ISTYPE(g), g <= nslA ? revoff[g + 1] - revoff[g] : 0);
						FOR_PREDS(g, ps, pk, if (pk == 0 && snap[ps]) printf("    pred %u (update-live, scratch %d)\n", ps, live[ps]););
						if (g <= nslA) for (u32 i = revoff[g]; i < revoff[g + 1]; i++) { u32 m = revm[i]; printf("    meth %u owner %u: update called %d typeDone %d live %d iface %d; scratch called %d typeDone %d\n", m, mown[m], sC[m], sT[mown[m]], snap[mown[m]], sI[mown[m]], called[m], typeDone[mown[m]]);
								sigent *se = sig((u64)mname[m] << 32 | mtyp[m]); printf("      exp %d reflect %u/%u sig %u/%u named %u/%u\n", mexp[m], sR, reflectCnt, sS[se - sigs], se->cnt, sN[mname[m]], namedCnt[mname[m]]); }
					}
			fbad += d != 0;
			update(&p2, &m2); // revert: must be B again
			if (checkLive("fuzz revert vs B oracle", liveB, nlb)) fbad++;
			free(rsrc_), free(rk), free(rv);
		}
		printf("fuzz: %d random diffs, %ld failed\n", fz, fbad);
		bad += fbad;
	}
	printf(bad ? "P2INC FAIL\n" : "P2INC OK\n");
	return bad != 0;
}
