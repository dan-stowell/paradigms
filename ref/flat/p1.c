// p1.c: problem 1 (resolve) from the flat files, plainly, on one core: the reference that shows the flat files
// say everything resolve needs. Checks itself against oracle/p1_canon.u32, oracle/p1_builtin.u32,
// oracle/p1_reltarget.u32 and (the content-hash join, 1c) oracle/p1c_canon.u32.
//   ref/flat/p1 data/minio
#include "flat.h"

#define rankDef (1u << 29)
#define PkgIdxSelf 2147483643u

typedef struct {
	u64 k1, k2, val; // val = rank << 32 | ~id: the maximum is the winner
} ent;
static ent *tab;
static u64 mask;
static ent *find(u64 k1, u64 k2, int add) {
	for (u64 h = mix64(k1) & mask;; h = (h + 1) & mask) {
		if (tab[h].k1 == 0) {
			if (!add) return NULL;
			tab[h].k1 = k1, tab[h].k2 = k2, tab[h].val = 0;
			return &tab[h];
		}
		if (tab[h].k1 == k1 && tab[h].k2 == k2) return &tab[h];
	}
}

int main(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "data/minio";
	u32 N = (u32)metaGet("nslots"), nobjs = (u32)metaGet("nobjs"), nb = (u32)metaGet("nbuiltin");
	size_t nw, nrel;
	const u8 *kind = FMAP(u8, "p1_kind.u8", NULL), *flags = FMAP(u8, "p1_flags.u8", NULL);
	const u64 *key = FMAP(u64, "p1_namekey.u64x2", NULL), *want = FMAP(u64, "p1_want.u64x2", &nw);
	const u64 *bkey = FMAP(u64, "p1_builtin.u64x2", NULL);
	const u32 *rank = FMAP(u32, "p1_rank.u32", NULL), *slotRel = FMAP(u32, "slot_rel.u32", NULL);
	const u32 *slotObj = FMAP(u32, "slot_obj.u32", NULL), *pkgOff = FMAP(u32, "obj_pkg_off.u32", NULL);
	const u32 *pkg = FMAP(u32, "obj_pkg.u32", NULL);
	const u32 *rPkg = FMAP(u32, "rel_pkg.u32", &nrel), *rIdx = FMAP(u32, "rel_idx.u32", NULL);
	nrel /= 4;
	nw /= 16;
	const u64 *hkey = FMAP(u64, "p1_hashkey.u64x2", NULL);
	const u8 *hmark = FMAP(u8, "p1c_mark.u8", NULL);

	double t0 = now();
	// 1a. the join by name: the linker's wanted names, then every definition and reference by name
	u64 sz = 1;
	while (sz < 4 * (u64)(N / 4 + nw)) sz <<= 1;
	tab = calloc(sz, sizeof(ent));
	mask = sz - 1;
	for (size_t i = 0; i < nw; i++) find(want[2 * i], want[2 * i + 1], 1);
	u32 *canon = malloc(4 * ((size_t)N + 1));
	canon[0] = 0;
	for (u32 g = 1; g <= N; g++) {
		canon[g] = g;
		if ((flags[g] & 2)) canon[g] = 0; // a reference that only debug information makes
		if ((kind[g] != 4 && kind[g] != 5) || key[2 * (size_t)g] == 0) continue;
		ent *e = find(key[2 * (size_t)g], key[2 * (size_t)g + 1], 1);
		u64 v = (u64)rank[g] << 32 | (u32)~g;
		if (v > e->val) e->val = v;
	}
	// definitions by package index join only a name that is already there
	for (u32 g = 1; g <= N; g++) {
		if (kind[g] != 1 || key[2 * (size_t)g] == 0) continue;
		ent *e = find(key[2 * (size_t)g], key[2 * (size_t)g + 1], 0);
		if (!e) continue;
		u64 v = (u64)rank[g] << 32 | (u32)~g;
		if (v > e->val) e->val = v;
	}
	for (u32 g = 1; g <= N; g++) {
		if (kind[g] == 2 || kind[g] == 3 || key[2 * (size_t)g] == 0) continue;
		ent *e = find(key[2 * (size_t)g], key[2 * (size_t)g + 1], 0);
		if (e) canon[g] = ~(u32)e->val;
	}
	u32 *builtin = calloc(nb, 4);
	for (u32 i = 0; i < nb; i++) {
		ent *e = find(bkey[2 * i], bkey[2 * i + 1], 0);
		if (e && e->val && ((e->val >> 32) & rankDef)) builtin[i] = ~(u32)e->val;
	}
	double t1 = now();
	// 1b. every relocation's target: a package-indexed reference is a read of the object's package table
	u32 *tgt = malloc(4 * nrel);
	for (u32 g = 1; g <= N; g++) {
		u32 o = slotObj[g];
		const u32 *pb = pkg + pkgOff[o];
		u32 npk = pkgOff[o + 1] - pkgOff[o] - 5;
		for (u32 r = slotRel[g]; r < slotRel[g + 1]; r++) {
			u32 p = rPkg[r], idx = rIdx[r], sp = p - PkgIdxSelf, t = sp < 5 ? npk + sp : p, s = 0;
			if (t < npk + 5 && pb[t] != 0xFFFFFFFFu) s = sp == 1 ? (idx < nb ? builtin[idx] : 0) : pb[t] + idx;
			tgt[r] = s ? canon[s] : 0;
		}
	}
	double t2 = now();
	printf("p1 (1 core): join by name %.1f ms, relocation targets %.1f ms\n", (t1 - t0) * 1e3, (t2 - t1) * 1e3);
	long bad = check32("canon", canon, (size_t)N + 1, "oracle/p1_canon.u32");
	bad += check32("builtin", builtin, nb, "oracle/p1_builtin.u32");
	bad += check32("reltarget", tgt, nrel, "oracle/p1_reltarget.u32");

	// 1c. the content-hash join over the copies the flood reached (mark 1) or names without following (2)
	double t3 = now();
	typedef struct {
		u64 k1, k2;
		u32 win;
	} hent;
	u64 hs = 1;
	while (hs < 4 * (u64)(N / 2)) hs <<= 1;
	hent *ht = calloc(hs, sizeof(hent));
	u64 hm = hs - 1;
	// a group: the same kind (short or long hash) and key; short: the largest copy wins, then the lowest id;
	// long: the lowest id; a copy marked 2 never beats one marked 1
#define HFIND(g, e)                                                                                         \
	do {                                                                                                    \
		u64 k1 = hkey[2 * (size_t)(g)], k2 = kind[g] == 2 ? 0 : hkey[2 * (size_t)(g) + 1] | (1ull << 63);  \
		for (u64 h = mix64(k1 ^ k2) & hm;; h = (h + 1) & hm) {                                             \
			if (ht[h].win == 0) {                                                                           \
				ht[h].k1 = k1, ht[h].k2 = k2;                                                               \
				e = &ht[h];                                                                                 \
				break;                                                                                      \
			}                                                                                               \
			if (ht[h].k1 == k1 && ht[h].k2 == k2) {                                                         \
				e = &ht[h];                                                                                 \
				break;                                                                                      \
			}                                                                                               \
		}                                                                                                   \
	} while (0)
	for (u32 g = 1; g <= N; g++) {
		if ((kind[g] != 2 && kind[g] != 3) || hmark[g] == 0) continue;
		hent *e;
		HFIND(g, e);
		if (e->win == 0) { e->win = g; continue; }
		u32 w = e->win;
		int wr = hmark[w] == 2, gr = hmark[g] == 2;
		int better;
		if (wr != gr) better = wr; // a reached copy beats one only named
		else if (kind[g] == 2) better = hkey[2 * (size_t)g + 1] > hkey[2 * (size_t)w + 1]; // larger; ties: lower id (g > w)
		else better = 0;
		if (better) e->win = g;
	}
	u32 *canon2 = malloc(4 * ((size_t)N + 1));
	memcpy(canon2, canon, 4 * ((size_t)N + 1));
	for (u32 g = 1; g <= N; g++) {
		if ((kind[g] != 2 && kind[g] != 3) || hmark[g] == 0) continue;
		hent *e;
		HFIND(g, e);
		u32 w = e->win;
		if (hmark[w] == 2 || w == g) continue; // nothing reached the class: not in the output
		if (kind[g] == 2 && hmark[g] == 2 && hkey[2 * (size_t)g + 1] > hkey[2 * (size_t)w + 1]) continue;
		canon2[g] = w;
	}
	double t4 = now();
	printf("p1c (1 core): content-hash join %.1f ms\n", (t4 - t3) * 1e3);
	bad += check32("canon after 1c", canon2, (size_t)N + 1, "oracle/p1c_canon.u32");
	printf(bad ? "P1 FAIL\n" : "P1 OK\n");
	return bad != 0;
}
