// p1gb.c: problem 1 (resolve) in SuiteSparse:GraphBLAS, as whole-vector operations over sparse vectors.
//   1a: the winner rule is a vector BUILD with dup = MAX (GraphBLAS sorts the (key, rank<<32|~id) tuples and
//       reduces each key with the MAX monoid); the join back to the members is an EXTRACT (a gather w(key)).
//       Keys are 128 bits; GraphBLAS indices are 60, so the key is hashed into a 60-bit index and every group is
//       checked to hold one full key (the guard: MIN and MAX of k1 and k2 per group must agree).
//   1b: relocation targets as a chain of gathers (extract) and elementwise ops over the 9.4 M relocations.
//   1c: the content-hash join: the same BUILD-with-MAX over (mark, size, ~id), masked by the reached copies.
// Checks oracle/p1_canon.u32, p1_builtin.u32, p1_reltarget.u32, p1c_canon.u32; writes them to out/.
//   GB_THREADS=8 ./p1gb DATA [REPS]
#include "gb.h"

static GrB_Vector V(GrB_Type t, ix n) { GrB_Vector v; OK(GrB_Vector_new(&v, t, n)); return v; }
// a read-only full vector over a mapped flat file (no copy)
static GrB_Vector load(GrB_Type t, const void *p, ix n, size_t esz) {
	GrB_Vector v = V(t, n);
	void *x = (void *)p;
	OK(GxB_Vector_load(v, &x, t, n, n * esz, GxB_IS_READONLY, NULL));
	return v;
}
#define AP2(w, m, op, u, s, T, d) OK(GrB_Vector_apply_BinaryOp2nd_##T(w, m, NULL, op, u, s, d))
#define AP1(w, m, op, s, u, T, d) OK(GrB_Vector_apply_BinaryOp1st_##T(w, m, NULL, op, s, u, d))
#define SEL(w, m, op, u, s, T, d) OK(GrB_Vector_select_##T(w, m, NULL, op, u, s, d))
static GrB_Descriptor useIdx; // GrB_Vector index lists: use the vector's indices, not its values
static long collisions;

// the 128-bit key (k1, k2) as a 60-bit index: ((k1 ^ k2 * C2) * C1) >> 4
static GrB_Vector keyIndex(GrB_Vector k1, GrB_Vector k2, ix n) {
	GrB_Vector t = V(GrB_UINT64, n), h = V(GrB_UINT64, n);
	AP2(t, NULL, GrB_TIMES_UINT64, k2, 0x9E3779B97F4A7C15ull, UINT64, NULL);
	OK(GrB_eWiseMult(t, NULL, NULL, GrB_BXOR_UINT64, k1, t, NULL));
	AP2(t, NULL, GrB_TIMES_UINT64, t, 0xBF58476D1CE4E5B9ull, UINT64, NULL);
	AP2(h, NULL, GxB_BSHIFT_UINT64, t, (int8_t)-4, INT8, NULL);
	GrB_free(&t);
	return h;
}
// a strided slice of an interleaved u64x2 column: x[part::2]
static GrB_Vector half(GrB_Vector kk, ix n, int part) {
	GrB_Vector v = V(GrB_UINT64, n);
	ix I[3] = {part, 2 * n - 2 + part, 2};
	OK(GrB_Vector_extract(v, NULL, NULL, kk, I, GxB_STRIDE, NULL));
	return v;
}
// group by key index: w(h(i)) = MAX over i of x(i); h and x have the same pattern. With the guard, also check that
// each group holds a single full key (the MIN and MAX of each key word agree)
static GrB_Vector groupMax(GrB_Vector h, GrB_Vector x, GrB_Vector k1, GrB_Vector k2, int guard) {
	const ix KEYS = (ix)1 << 60;
	GrB_Vector w = V(GrB_UINT64, KEYS);
	OK(GxB_Vector_build_Vector(w, h, x, GrB_MAX_UINT64, NULL));
	if (guard) {
		GrB_Vector ks[2] = {k1, k2};
		for (int j = 0; j < 2; j++) {
			if (!ks[j]) continue;
			ix n; OK(GrB_Vector_size(&n, h));
			GrB_Vector kx = V(GrB_UINT64, n), lo = V(GrB_UINT64, KEYS), hi = V(GrB_UINT64, KEYS);
			OK(GrB_eWiseMult(kx, NULL, NULL, GrB_SECOND_UINT64, h, ks[j], NULL)); // the key word on h's pattern
			OK(GxB_Vector_build_Vector(lo, h, kx, GrB_MIN_UINT64, NULL));
			OK(GxB_Vector_build_Vector(hi, h, kx, GrB_MAX_UINT64, NULL));
			OK(GrB_eWiseMult(lo, NULL, NULL, GrB_NE_UINT64, lo, hi, NULL));
			GrB_Vector bad = V(GrB_BOOL, KEYS);
			SEL(bad, NULL, GrB_VALUEEQ_BOOL, lo, 1, BOOL, NULL);
			collisions += nv(bad);
			GrB_free(&kx), GrB_free(&lo), GrB_free(&hi), GrB_free(&bad);
		}
	}
	return w;
}
// ~(u32)v: the id in the low half of rank << 32 | ~id
static void lowId(GrB_Vector out, GrB_Vector mask, GrB_Vector v, GrB_Descriptor d) {
	ix n; OK(GrB_Vector_size(&n, v));
	GrB_Vector t = V(GrB_UINT64, n);
	AP2(t, NULL, GrB_BAND_UINT64, v, 0xFFFFFFFFull, UINT64, NULL);
	AP1(out, mask, GrB_MINUS_UINT64, 0xFFFFFFFFull, t, UINT64, d);
	GrB_free(&t);
}

int main(int argc, char **argv) {
	double tStart = now();
	flatDir = argc > 1 ? argv[1] : "data/minio";
	int reps = argc > 2 ? atoi(argv[2]) : 5, guard = !getenv("GB_NOGUARD");
	ix ns = metaGet("nslots"), N = ns + 1, nobj = metaGet("nobjs"), nb = metaGet("nbuiltin");
	size_t nw, nrel;
	const u8 *kindp = FMAP(u8, "p1_kind.u8", NULL), *flagp = FMAP(u8, "p1_flags.u8", NULL);
	const u64 *keyp = FMAP(u64, "p1_namekey.u64x2", NULL), *wantp = FMAP(u64, "p1_want.u64x2", &nw);
	const u64 *bkeyp = FMAP(u64, "p1_builtin.u64x2", NULL), *hkeyp = FMAP(u64, "p1_hashkey.u64x2", NULL);
	const u32 *rankp = FMAP(u32, "p1_rank.u32", NULL), *slotRel = FMAP(u32, "slot_rel.u32", NULL);
	const u32 *slotObj = FMAP(u32, "slot_obj.u32", NULL), *pkgOff = FMAP(u32, "obj_pkg_off.u32", NULL);
	size_t npk;
	const u32 *pkgp = FMAP(u32, "obj_pkg.u32", &npk);
	const u32 *rPkg = FMAP(u32, "rel_pkg.u32", &nrel), *rIdx = FMAP(u32, "rel_idx.u32", NULL);
	const u8 *markp = FMAP(u8, "p1c_mark.u8", NULL);
	nrel /= 4, nw /= 16, npk /= 4;
	gbInit();
	OK(GrB_Descriptor_new(&useIdx));
	OK(GrB_Descriptor_set_INT32(useIdx, GxB_USE_INDICES, GxB_ROWINDEX_LIST));
	// loading: the relocations' source slot (slot_rel's CSR rows expanded: the only plain loop)
	u32 *relSlotp = malloc(4 * nrel);
	for (ix g = 1; g <= ns; g++)
		for (u32 r = slotRel[g]; r < slotRel[g + 1]; r++) relSlotp[r] = (u32)g;
	u32 *bOut = malloc(4 * nb);
	double tLoaded = now();
	double best[4] = {1e9, 1e9, 1e9, 1e9}, tg[4];
	GrB_Vector canon = NULL, builtin = NULL, tgt = NULL, canon2 = NULL;
	for (int rep = 0; rep < reps; rep++) {
		GrB_free(&canon), GrB_free(&builtin), GrB_free(&tgt), GrB_free(&canon2);
		double t0 = now();
		collisions = 0;
		// ---------------- 1a: the join by name ----------------
		GrB_Vector kind = load(GrB_UINT8, kindp, N, 1), flags = load(GrB_UINT8, flagp, N, 1);
		GrB_Vector rank = load(GrB_UINT32, rankp, N, 4), kk = load(GrB_UINT64, keyp, 2 * N, 8);
		GrB_Vector k1 = half(kk, N, 0), k2 = half(kk, N, 1), h = keyIndex(k1, k2, N);
		GrB_Vector keyed = V(GrB_BOOL, N), id = V(GrB_UINT64, N), val = V(GrB_UINT64, N), t = V(GrB_UINT64, N);
		SEL(keyed, NULL, GrB_VALUENE_UINT64, k1, 0, UINT64, NULL);
		OK(GrB_Vector_apply_IndexOp_INT64(id, NULL, NULL, GrB_ROWINDEX_INT64, kind, 0, NULL));
		AP2(val, NULL, GrB_TIMES_UINT64, rank, 1ull << 32, UINT64, NULL);           // rank << 32
		AP1(t, NULL, GrB_MINUS_UINT64, 0xFFFFFFFFull, id, UINT64, NULL);           // ~id
		OK(GrB_eWiseAdd(val, NULL, NULL, GrB_PLUS_UINT64, val, t, NULL));
		GrB_Vector m45 = V(GrB_BOOL, N), m1 = V(GrB_BOOL, N), m23 = V(GrB_BOOL, N), h45 = V(GrB_UINT64, N);
		GrB_Vector hc = V(GrB_UINT64, N), hall = V(GrB_UINT64, N), x = V(GrB_UINT64, N);
		SEL(m45, NULL, GrB_VALUEGE_UINT8, kind, 4, UINT8, NULL);
		SEL(m1, NULL, GrB_VALUEEQ_UINT8, kind, 1, UINT8, NULL);
		{ GrB_Vector ge2 = V(GrB_BOOL, N); // kinds 2..3: the content-addressed slots
		  SEL(ge2, NULL, GrB_VALUEGE_UINT8, kind, 2, UINT8, NULL);
		  SEL(m23, ge2, GrB_VALUELE_UINT8, kind, 3, UINT8, GrB_DESC_RS); GrB_free(&ge2); }
		OK(GrB_eWiseMult(h45, m45, NULL, GrB_FIRST_UINT64, h, keyed, GrB_DESC_RS));   // kinds 4, 5 with a key
		OK(GrB_eWiseMult(hc, m1, NULL, GrB_FIRST_UINT64, h, keyed, GrB_DESC_RS));     // kind-1 candidates
		OK(GrB_eWiseMult(hall, m23, NULL, GrB_FIRST_UINT64, h, keyed, GrB_DESC_RSC)); // every keyed slot but 2, 3
		OK(GrB_eWiseMult(x, NULL, NULL, GrB_SECOND_UINT64, h45, val, NULL));
		GrB_Vector w = groupMax(h45, x, k1, k2, guard);
		// the linker's wanted names: members with value 0 and no slot
		GrB_Vector wk = load(GrB_UINT64, wantp, 2 * nw, 8), wk1 = half(wk, nw, 0), wk2 = half(wk, nw, 1);
		GrB_Vector hw = keyIndex(wk1, wk2, nw), zero = V(GrB_UINT64, nw), ww;
		OK(GrB_Vector_assign_UINT64(zero, NULL, NULL, 0, GrB_ALL, nw, NULL));
		ww = groupMax(hw, zero, wk1, wk2, guard);
		OK(GrB_eWiseAdd(w, NULL, NULL, GrB_MAX_UINT64, w, ww, NULL));
		// candidates join a group that is already there (mask: w's structure)
		OK(GrB_eWiseMult(x, NULL, NULL, GrB_SECOND_UINT64, hc, val, GrB_DESC_R));
		GrB_Vector wc = groupMax(hc, x, k1, k2, guard);
		OK(GrB_eWiseAdd(w, w, NULL, GrB_MAX_UINT64, w, wc, GrB_DESC_S));
		// every member's canonical id: the gather w(h(member)), scattered back to the member's slot
		ix nall = nv(hall);
		GrB_Vector cw = V(GrB_UINT64, nall), cid = V(GrB_UINT32, nall);
		OK(GxB_Vector_extract_Vector(cw, NULL, NULL, w, hall, NULL));
		lowId(cid, NULL, cw, NULL);
		canon = V(GrB_UINT32, N);
		OK(GrB_Vector_apply(canon, NULL, NULL, GrB_IDENTITY_UINT32, id, NULL));
		GrB_Vector dbg = V(GrB_UINT8, N);
		AP2(dbg, NULL, GrB_BAND_UINT8, flags, 2, UINT8, NULL);
		SEL(dbg, NULL, GrB_VALUENE_UINT8, dbg, 0, UINT8, NULL);
		OK(GrB_Vector_assign_UINT32(canon, dbg, NULL, 0, GrB_ALL, N, GrB_DESC_S));   // debug-only references: 0
		OK(GxB_Vector_assign_Vector(canon, NULL, GrB_SECOND_UINT32, cid, hall, useIdx));
		// builtins: the winner if it is a definition (rank bit 29 = value bit 61)
		GrB_Vector bk = load(GrB_UINT64, bkeyp, 2 * nb, 8), bk1 = half(bk, nb, 0), bk2 = half(bk, nb, 1);
		GrB_Vector hb = keyIndex(bk1, bk2, nb), bw = V(GrB_UINT64, nb), bdef = V(GrB_UINT64, nb);
		OK(GxB_Vector_extract_Vector(bw, NULL, NULL, w, hb, NULL));
		AP2(bdef, NULL, GrB_BAND_UINT64, bw, 1ull << 61, UINT64, NULL);
		SEL(bdef, NULL, GrB_VALUENE_UINT64, bdef, 0, UINT64, NULL);
		builtin = V(GrB_UINT32, nb);
		OK(GrB_Vector_assign_UINT32(builtin, NULL, NULL, 0, GrB_ALL, nb, NULL));
		lowId(builtin, bdef, bw, GrB_DESC_S);
		OK(GrB_Vector_wait(canon, GrB_MATERIALIZE));
		double t1 = now();

		// ---------------- 1b: relocation targets ----------------
		GrB_Vector rs = load(GrB_UINT32, relSlotp, nrel, 4), rp = load(GrB_UINT32, rPkg, nrel, 4), ri = load(GrB_UINT32, rIdx, nrel, 4);
		GrB_Vector so = load(GrB_UINT32, slotObj, N, 4), po = load(GrB_UINT32, pkgOff, nobj + 1, 4), pk = load(GrB_UINT32, pkgp, npk, 4);
		GrB_Vector o = V(GrB_UINT32, nrel), a = V(GrB_UINT32, nrel), np = V(GrB_UINT32, nrel), sp = V(GrB_UINT32, nrel);
		GrB_Vector tt = V(GrB_UINT32, nrel), ok = V(GrB_BOOL, nrel), base = V(GrB_UINT32, nrel), xx = V(GrB_UINT32, nrel);
		GrB_Vector sm = V(GrB_UINT32, nrel), lim = V(GrB_UINT32, nrel), bi = V(GrB_UINT32, nrel), bx = V(GrB_UINT32, nrel);
		OK(GxB_Vector_extract_Vector(o, NULL, NULL, so, rs, NULL));      // o = slot_obj(slot)
		OK(GxB_Vector_extract_Vector(a, NULL, NULL, po, o, NULL));       // a = obj_pkg_off(o)
		AP2(np, NULL, GrB_PLUS_UINT32, o, 1, UINT32, NULL);
		OK(GxB_Vector_extract_Vector(np, NULL, NULL, po, np, NULL));     // obj_pkg_off(o + 1)
		OK(GrB_eWiseMult(np, NULL, NULL, GrB_MINUS_UINT32, np, a, NULL));
		AP2(np, NULL, GrB_MINUS_UINT32, np, 5, UINT32, NULL);            // np = npkg of o
		AP2(sp, NULL, GrB_MINUS_UINT32, rp, 2147483643u, UINT32, NULL);  // sp = pkg - PkgIdxSelf
		SEL(sm, NULL, GrB_VALUELT_UINT32, sp, 5, UINT32, NULL);          // special package indices
		OK(GrB_eWiseMult(sm, NULL, NULL, GrB_PLUS_UINT32, sm, np, NULL));
		OK(GrB_eWiseAdd(tt, NULL, NULL, GrB_SECOND_UINT32, rp, sm, NULL)); // t = sp < 5 ? np + sp : pkg
		AP2(lim, NULL, GrB_PLUS_UINT32, np, 5, UINT32, NULL);
		OK(GrB_eWiseMult(ok, NULL, NULL, GrB_LT_UINT32, tt, lim, NULL));   // t < np + 5
		AP2(lim, NULL, GrB_MINUS_UINT32, lim, 1, UINT32, NULL);
		OK(GrB_eWiseMult(tt, NULL, NULL, GrB_MIN_UINT32, tt, lim, NULL));  // (clamped, so every gather is in range)
		OK(GrB_eWiseMult(tt, NULL, NULL, GrB_PLUS_UINT32, tt, a, NULL));
		OK(GxB_Vector_extract_Vector(base, NULL, NULL, pk, tt, NULL));     // base = obj_pkg[o][t]
		GrB_Vector okb = V(GrB_BOOL, nrel);
		AP2(okb, NULL, GrB_NE_UINT32, base, 0xFFFFFFFFu, UINT32, NULL);
		OK(GrB_eWiseMult(ok, NULL, NULL, GrB_LAND, ok, okb, NULL));
		OK(GrB_eWiseMult(xx, NULL, NULL, GrB_PLUS_UINT32, base, ri, NULL)); // base + idx
		// builtin package: builtin(idx), 0 past the end (a zero appended at nb)
		GrB_Vector bz = V(GrB_UINT32, nb + 1);
		OK(GrB_Vector_assign_UINT32(bz, NULL, NULL, 0, GrB_ALL, nb + 1, NULL));
		{ ix I[2] = {0, nb - 1}; OK(GrB_Vector_assign(bz, NULL, NULL, builtin, I, GxB_RANGE, NULL)); }
		GrB_Vector s1 = V(GrB_BOOL, nrel);
		SEL(s1, NULL, GrB_VALUEEQ_UINT32, sp, 1, UINT32, NULL);
		AP2(bi, NULL, GrB_MIN_UINT32, ri, (u32)nb, UINT32, NULL);
		OK(GxB_Vector_extract_Vector(bx, NULL, NULL, bz, bi, NULL));
		OK(GrB_Vector_assign(xx, s1, NULL, bx, GrB_ALL, nrel, GrB_DESC_S));     // sp == 1: the builtin
		OK(GrB_Vector_assign_UINT32(xx, ok, NULL, 0, GrB_ALL, nrel, GrB_DESC_C)); // not linked: 0
		tgt = V(GrB_UINT32, nrel);
		OK(GxB_Vector_extract_Vector(tgt, NULL, NULL, canon, xx, NULL));  // canon(x); canon(0) = 0
		OK(GrB_Vector_wait(tgt, GrB_MATERIALIZE));
		double t2 = now();

		// ---------------- 1c: the content-hash join ----------------
		GrB_Vector mark = load(GrB_UINT8, markp, N, 1), hk = load(GrB_UINT64, hkeyp, 2 * N, 8);
		GrB_Vector c1 = half(hk, N, 0), c2 = half(hk, N, 1);
		GrB_Vector mk = V(GrB_UINT8, N), m1c = V(GrB_BOOL, N);
		OK(GrB_eWiseMult(mk, m23, NULL, GrB_FIRST_UINT8, mark, kind, GrB_DESC_RS)); // kinds 2, 3 ...
		SEL(mk, NULL, GrB_VALUENE_UINT8, mk, 0, UINT8, NULL);                       // ... with mark 1 or 2
		SEL(m1c, NULL, GrB_VALUEEQ_UINT8, mk, 1, UINT8, NULL);
		OK(GrB_Vector_dup(&canon2, canon));
		for (int kd = 2; kd <= 3; kd++) {
			GrB_Vector mkd = V(GrB_BOOL, N), hx = V(GrB_UINT64, N), vx = V(GrB_UINT64, N), zk = V(GrB_UINT64, N);
			SEL(mkd, mk, GrB_VALUEEQ_UINT8, kind, kd, UINT8, GrB_DESC_S);
			// kind 2 groups by k1 alone, kind 3 by (k1, k2)
			OK(GrB_Vector_assign_UINT64(zk, NULL, NULL, 0, GrB_ALL, N, NULL));
			GrB_Vector hh = keyIndex(c1, kd == 3 ? c2 : zk, N);
			OK(GrB_Vector_apply(hx, mkd, NULL, GrB_IDENTITY_UINT64, hh, GrB_DESC_S));
			// value: mark 1 << 63 | (kind 2: size << 32) | ~id; the maximum wins
			AP2(vx, mkd, GrB_TIMES_UINT64, c2, kd == 2 ? 1ull << 32 : 0, UINT64, GrB_DESC_S);
			OK(GrB_eWiseMult(vx, NULL, NULL, GrB_PLUS_UINT64, vx, t, NULL));
			if (kd == 2) { // sizes must fit in 31 bits for the value encoding
				GrB_Vector big = V(GrB_UINT64, N);
				SEL(big, mkd, GrB_VALUEGT_UINT64, c2, 0x7FFFFFFFull, UINT64, GrB_DESC_S);
				collisions += nv(big); GrB_free(&big); }
			AP2(vx, m1c, GrB_PLUS_UINT64, vx, 1ull << 63, UINT64, GrB_DESC_S);   // (accumulates in place)
			GrB_Vector wv = groupMax(hx, vx, c1, kd == 3 ? c2 : NULL, guard);
			// drop groups whose winner was not reached (mark 2)
			SEL(wv, NULL, GrB_VALUEGE_UINT64, wv, 1ull << 63, UINT64, NULL);
			ix nx = nv(hx);
			GrB_Vector cw2 = V(GrB_UINT64, nx), cid2 = V(GrB_UINT32, nx), keep = V(GrB_BOOL, nx);
			OK(GxB_Vector_extract_Vector(cw2, NULL, NULL, wv, hx, NULL));
			lowId(cid2, NULL, cw2, NULL);
			if (kd == 2) { // a mark-2 copy larger than the winner keeps itself
				GrB_Vector vxp = V(GrB_UINT64, nx), ws = V(GrB_UINT64, nx), ms = V(GrB_UINT64, nx);
				OK(GxB_Vector_extract_Vector(vxp, NULL, NULL, vx, hx, useIdx)); // each member's own value, in order
				AP2(ws, NULL, GrB_BAND_UINT64, cw2, 0x7FFFFFFF00000000ull, UINT64, NULL);  // winner's size
				AP2(ms, NULL, GrB_BAND_UINT64, vxp, 0x7FFFFFFF00000000ull, UINT64, NULL);  // member's size
				GrB_Vector big = V(GrB_BOOL, nx);
				OK(GrB_eWiseMult(big, NULL, NULL, GrB_GT_UINT64, ms, ws, NULL));
				GrB_Vector m2 = V(GrB_BOOL, nx);
				SEL(m2, NULL, GrB_VALUELT_UINT64, vxp, 1ull << 63, UINT64, NULL);
				OK(GrB_eWiseMult(big, m2, NULL, GrB_LAND, big, m2, GrB_DESC_RS));
				SEL(big, NULL, GrB_VALUEEQ_BOOL, big, 1, BOOL, NULL);
				OK(GrB_Vector_apply(cid2, big, NULL, GrB_IDENTITY_UINT32, cid2, GrB_DESC_RSC));
				GrB_free(&vxp), GrB_free(&ws), GrB_free(&ms), GrB_free(&big), GrB_free(&m2);
			}
			OK(GxB_Vector_assign_Vector(canon2, NULL, GrB_SECOND_UINT32, cid2, hx, useIdx));
			GrB_free(&mkd), GrB_free(&hx), GrB_free(&vx), GrB_free(&zk), GrB_free(&hh), GrB_free(&wv);
			GrB_free(&cw2), GrB_free(&cid2), GrB_free(&keep);
		}
		OK(GrB_Vector_wait(canon2, GrB_MATERIALIZE));
		double t3 = now();
		tg[0] = t1 - t0, tg[1] = t2 - t1, tg[2] = t3 - t2, tg[3] = t3 - t0;
		for (int i = 0; i < 4; i++) if (tg[i] < best[i]) best[i] = tg[i];
		GrB_Vector fr[] = {kind, flags, rank, kk, k1, k2, h, keyed, id, val, t, m45, m1, m23, h45, hc, hall, x, w, wk, wk1, wk2,
			hw, zero, ww, wc, cw, cid, dbg, bk, bk1, bk2, hb, bw, bdef, rs, rp, ri, so, po, pk, o, a, np, sp, tt, ok, base, xx,
			sm, lim, bi, bx, okb, bz, s1, mark, hk, c1, c2, mk, m1c};
		for (size_t i = 0; i < sizeof fr / sizeof *fr; i++) GrB_free(&fr[i]);
	}
	// outputs
	u32 *cOut = malloc(4 * N), *tOut = malloc(4 * nrel), *c2Out = malloc(4 * N);
	ix n = N; OK(GrB_Vector_extractTuples_UINT32(NULL, cOut, &n, canon));
	ix nbv = nb; OK(GrB_Vector_extractTuples_UINT32(NULL, bOut, &nbv, builtin));
	ix nr = nrel; OK(GrB_Vector_extractTuples_UINT32(NULL, tOut, &nr, tgt));
	n = N; OK(GrB_Vector_extractTuples_UINT32(NULL, c2Out, &n, canon2));
	system("mkdir -p out");
	writeFile("out/p1_canon.u32", cOut, 4 * N), writeFile("out/p1_builtin.u32", bOut, 4 * nb);
	writeFile("out/p1_reltarget.u32", tOut, 4 * nrel), writeFile("out/p1c_canon.u32", c2Out, 4 * N);
	double tEnd = now();
	printf("p1gb (%d threads, guard %s): 1a %.1f ms, 1b %.1f ms, 1c %.1f ms, all %.1f ms (best of %d); key collisions %ld\n",
	       nthreads(), guard ? "on" : "off", best[0] * 1e3, best[1] * 1e3, best[2] * 1e3, best[3] * 1e3, reps, collisions);
	printf("setup: load %.1f ms; process to here %.1f ms\n", (tLoaded - tStart) * 1e3, (tEnd - tStart) * 1e3);
	long bad = (n != N || nbv != nb || nr != nrel) ? 1 : 0;
	bad += check32("canon", cOut, N, "oracle/p1_canon.u32");
	bad += check32("builtin", bOut, nb, "oracle/p1_builtin.u32");
	bad += check32("reltarget", tOut, nrel, "oracle/p1_reltarget.u32");
	bad += check32("canon after 1c", c2Out, N, "oracle/p1c_canon.u32");
	bad += collisions;
	printf(bad ? "P1 FAIL\n" : "P1 OK\n");
	return bad != 0;
}
