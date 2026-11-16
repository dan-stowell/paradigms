// unify.c: stable symbol identity across two dumps, in plain C. The same mapping as systems/ddflow's
// facts::unify (B id -> unified id in A's space; B-only slots get ids from nids_a up; linker-made ids by
// position; p2 method-name strings by content), checked byte for byte against data/<B>/diff/{idmap_b,strmap_b}.u32.
//
//   build/unify A B [--reps N] [--check] [--stats] [--exact] [--fp] [--fpdir DIR] [--write DIR]
//
// A slot's key is (kind, name bytes, 128-bit name key, 128-bit content hash, rank if unnamed). Objects pair by
// index. Fast path: an object whose key columns are identical (memcmp of the column ranges) maps by position.
// Slow path, per differing object: an open-addressing table counts each key on both sides; a key that occurs once
// on each side (and is named or content-hashed) is matched; the rest is aligned with Myers' O(ND) diff (the same
// forward pass and backtrack as ddflow, so ties break the same way); unmatched B slots get fresh ids in order.
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/resource.h>
#ifdef _OPENMP
#include <omp.h>
#endif
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t i64;

static double now(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec * 1e-9;
}
static size_t bytesMapped;
static int lazy; // --fpdir: map without MAP_POPULATE, so only the pages the run touches are read
static const void *mapf(const char *dir, const char *name, size_t *n) {
	char p[4096];
	snprintf(p, sizeof p, "%s/%s", dir, name);
	int fd = open(p, O_RDONLY);
	if (fd < 0) { perror(p); exit(2); }
	struct stat st;
	fstat(fd, &st);
	size_t sz = (size_t)st.st_size;
	if (n) *n = sz;
	bytesMapped += sz;
	if (sz == 0) { close(fd); return ""; }
	void *m = mmap(NULL, sz, PROT_READ, MAP_PRIVATE | (lazy ? 0 : MAP_POPULATE), fd, 0);
	if (m == MAP_FAILED) { perror(p); exit(2); }
	close(fd);
	return m;
}
static u64 meta(const char *dir, const char *key) {
	char p[4096], k[128];
	unsigned long long v;
	snprintf(p, sizeof p, "%s/meta.txt", dir);
	FILE *f = fopen(p, "r");
	if (!f) { perror(p); exit(2); }
	while (fscanf(f, "%127s %llu", k, &v) == 2)
		if (strcmp(k, key) == 0) { fclose(f); return v; }
	fprintf(stderr, "meta %s: no %s\n", dir, key);
	exit(2);
}

typedef struct {
	const char *dir;
	u32 nslots, nids, nobjs, nstr;
	const u8 *kind, *nbin;
	const u64 *noff, *nk, *hk; // noff: slot g's name is nbin[noff[g-1] : noff[g]]
	const u32 *rank, *obase;   // obase: per object its first slot
	const u64 *soff;
	const u8 *sbin;
	u32 *base; // nobjs+1: obase, then nslots+1
	u64 *fp;   // --fp: per object, a 64-bit fingerprint of its key columns (what an extractor could emit)
} Ver;

static void load(Ver *v, const char *dir) {
	v->dir = dir;
	v->nslots = meta(dir, "nslots");
	v->nids = meta(dir, "nids");
	v->nobjs = meta(dir, "nobjs");
	size_t n;
	v->kind = mapf(dir, "p1_kind.u8", NULL);
	v->noff = mapf(dir, "name.off.u64", NULL);
	v->nbin = mapf(dir, "name.bin", NULL);
	v->nk = mapf(dir, "p1_namekey.u64x2", NULL);
	v->hk = mapf(dir, "p1_hashkey.u64x2", NULL);
	v->rank = mapf(dir, "p1_rank.u32", NULL);
	v->obase = mapf(dir, "obj_base.u32", NULL);
	v->soff = mapf(dir, "p2_str.off.u64", &n);
	v->nstr = n / 8 - 1;
	v->sbin = mapf(dir, "p2_str.bin", NULL);
	v->base = malloc((v->nobjs + 1) * 4);
	memcpy(v->base, v->obase, v->nobjs * 4);
	v->base[v->nobjs] = v->nslots + 1;
}

static inline u32 nlen(const Ver *v, u32 g) { return (u32)(v->noff[g] - v->noff[g - 1]); }
static inline const u8 *nptr(const Ver *v, u32 g) { return v->nbin + v->noff[g - 1]; }
static inline int keq(const Ver *a, u32 g, const Ver *b, u32 h) {
	if (a->kind[g] != b->kind[h]) return 0;
	if (a->nk[2 * g] != b->nk[2 * h] || a->nk[2 * g + 1] != b->nk[2 * h + 1]) return 0;
	if (a->hk[2 * g] != b->hk[2 * h] || a->hk[2 * g + 1] != b->hk[2 * h + 1]) return 0;
	u32 la = nlen(a, g);
	if (la != nlen(b, h)) return 0;
	if (la == 0) return a->rank[g] == b->rank[h];
	return memcmp(nptr(a, g), nptr(b, h), la) == 0;
}
static inline u64 mix64(u64 h) {
	h ^= h >> 30;
	h *= 0xBF58476D1CE4E5B9ull;
	h ^= h >> 27;
	h *= 0x94D049BB133111EBull;
	return h ^ (h >> 31);
}
static u64 hbytes(const u8 *p, u32 n) {
	u64 h = 0x9E3779B97F4A7C15ull ^ n;
	while (n >= 16) { // two independent multiply chains per 16 bytes (the final mix64 makes up for the weak rounds)
		u64 w, x;
		memcpy(&w, p, 8), memcpy(&x, p + 8, 8);
		h = (h ^ w) * 0x9FB21C651E98DF25ull ^ ((h >> 29) + x) * 0xC2B2AE3D27D4EB4Full;
		p += 16, n -= 16;
	}
	if (n >= 8) { u64 w; memcpy(&w, p, 8); h = (h ^ w) * 0x9FB21C651E98DF25ull; h ^= h >> 32; p += 8, n -= 8; }
	u64 w = 0;
	memcpy(&w, p, n);
	return mix64(h ^ w);
}
static inline u64 khash(const Ver *v, u32 g) {
	u64 h = mix64(v->kind[g] ^ v->nk[2 * g] * 31 ^ mix64(v->nk[2 * g + 1]));
	h = mix64(h ^ v->hk[2 * g] ^ mix64(v->hk[2 * g + 1] + 7));
	u32 l = nlen(v, g);
	return l ? mix64(h ^ hbytes(nptr(v, g), l)) : mix64(h ^ v->rank[g] ^ 0xABCDull);
}

// identical: same slot count and same key at every position (the fast path)
static int same(const Ver *A, const Ver *B, u32 o) {
	u32 a0 = A->base[o], a1 = A->base[o + 1], b0 = B->base[o], b1 = B->base[o + 1];
	u32 n = a1 - a0;
	if (n != b1 - b0) return 0;
	if (n == 0) return 1;
	if (memcmp(A->kind + a0, B->kind + b0, n)) return 0;
	if (memcmp(A->nk + 2 * a0, B->nk + 2 * b0, 16 * (size_t)n)) return 0;
	if (memcmp(A->hk + 2 * a0, B->hk + 2 * b0, 16 * (size_t)n)) return 0;
	u64 sa = A->noff[a0 - 1], sb = B->noff[b0 - 1];
	if (A->noff[a1 - 1] - sa != B->noff[b1 - 1] - sb) return 0;
	if (memcmp(A->nbin + sa, B->nbin + sb, A->noff[a1 - 1] - sa)) return 0;
	// name boundaries and, for unnamed slots, rank
	const u64 *oa = A->noff + a0 - 1, *ob = B->noff + b0 - 1; // oa[i+1] - oa[i]: slot a0+i's name length
	const u32 *ra = A->rank + a0, *rb = B->rank + b0;
	u64 d = sa - sb, bad = 0;
	for (u32 i = 0; i < n; i++) {
		u64 la = oa[i + 1] - oa[i];
		bad |= (oa[i + 1] - ob[i + 1] - d) | (la == 0 ? (u64)(ra[i] ^ rb[i]) : 0);
	}
	return bad == 0;
}

// ---------------------------------------------------------------- Myers (a port of ddflow's, same tie-breaks)
// a, b: the key class of each slot on each side (equal class = equal key). Fills (pi, pj) with matched index pairs.
typedef struct { i64 *v, *trace; size_t vcap, tcap; } MyersBuf;
static u64 myersD;
static size_t myers(const u32 *a, i64 n, const u32 *b, i64 m, u32 *pi, u32 *pj, MyersBuf *mb) {
	i64 off = n + m + 1;
	size_t vn = 2 * off + 1;
	if (mb->vcap < vn) { mb->v = realloc(mb->v, vn * 8); mb->vcap = vn; }
	i64 *v = mb->v;
	memset(v, 0, vn * 8);
	size_t tlen = 0; // trace[d] occupies (2d+1) entries starting at d*d
	i64 dmax = 0;
	for (i64 d = 0; d <= n + m; d++) {
		size_t need = (size_t)(d + 1) * (d + 1);
		if (mb->tcap < need) { mb->tcap = need * 2; mb->trace = realloc(mb->trace, mb->tcap * 8); }
		memcpy(mb->trace + d * d, v + off - d, (2 * d + 1) * 8);
		tlen = need;
		int done = 0;
		for (i64 k = -d; k <= d; k += 2) {
			i64 x = (k == -d || (k != d && v[off + k - 1] < v[off + k + 1])) ? v[off + k + 1] : v[off + k - 1] + 1;
			i64 y = x - k;
			while (x < n && y < m && a[x] == b[y]) x++, y++;
			v[off + k] = x;
			if (x >= n && y >= m) { dmax = d; done = 1; break; }
		}
		if (done) break;
	}
	(void)tlen;
	myersD += dmax;
	i64 x = n, y = m;
	size_t np = 0;
	for (i64 d = dmax; d >= 1; d--) {
		const i64 *tv = mb->trace + d * d; // g(k) = tv[k + d]
		i64 k = x - y;
		i64 pk = (k == -d || (k != d && tv[k - 1 + d] < tv[k + 1 + d])) ? k + 1 : k - 1;
		i64 px = tv[pk + d], py = px - pk;
		i64 sx = pk == k + 1 ? px : px + 1, sy = pk == k + 1 ? py + 1 : py;
		while (x > sx && y > sy) { x--, y--; pi[np] = x, pj[np] = y, np++; }
		x = px, y = py;
	}
	while (x > 0 && y > 0) { x--, y--; pi[np] = x, pj[np] = y, np++; }
	return np;
}

// ---------------------------------------------------------------- the slow path: one differing object
// Step 1 gives every slot of the object (both sides) a key class: equal keys <=> equal class. Two ways:
//  classifyHashed: a 64-bit hash per slot (sequential pass; the name bytes are hashed only when the slot has no
//    128-bit name key, since keq compares that too), a u32 open-addressing table with prefetch, dense classes in A
//    order; then every slot is verified against its class representative with keq, so a hash collision cannot go
//    unnoticed (it falls back to classifyExact, which compares keys on insertion).
// Step 2 (same as ddflow): unique-on-both-sides named/hashed classes match; Myers on the rest; new ids in B order.
typedef struct { u64 h; u32 ca, cb, lastA, rep; } Cls; // rep: slot id, bit 31 set if a B slot
typedef struct {
	u64 objs, slotsA, slotsB, uniq, myersA, myersB, myersPairs, aOnly, bOnly, probes, verify, exact, prefix, suffix;
} Count;
static Count C;
static double tHash, tMyers, tFast;
static int forceExact;

typedef struct { // scratch, grown as needed and kept across calls
	u64 *hA, *hB;
	u32 *cA, *cB, *tab, *ra, *rb, *xa, *xb, *pi, *pj;
	u8 *ma, *mb;
	Cls *cls;
	size_t nA, nB, ntab;
	MyersBuf my;
} Scratch;
static Scratch S;
static void grow(u32 na, u32 nb, size_t cap) {
	if (S.nA < na) {
		S.nA = na;
		S.hA = realloc(S.hA, na * 8), S.cA = realloc(S.cA, na * 4), S.ra = realloc(S.ra, na * 4), S.xa = realloc(S.xa, na * 4);
		S.pi = realloc(S.pi, na * 4), S.ma = realloc(S.ma, na);
	}
	if (S.nB < nb) {
		S.nB = nb;
		S.hB = realloc(S.hB, nb * 8), S.cB = realloc(S.cB, nb * 4), S.rb = realloc(S.rb, nb * 4), S.xb = realloc(S.xb, nb * 4);
		S.pj = realloc(S.pj, nb * 4), S.mb = realloc(S.mb, nb);
	}
	if (S.ntab < cap) {
		S.ntab = cap;
		S.tab = realloc(S.tab, cap * 4), S.cls = realloc(S.cls, cap / 2 * sizeof(Cls));
	}
}

static inline u64 khash2(const Ver *v, u32 g) {
	u64 h = mix64(v->nk[2 * g] ^ ((u64)v->kind[g] << 56)) ^ v->nk[2 * g + 1];
	h = mix64(h ^ v->hk[2 * g]) ^ v->hk[2 * g + 1];
	u32 l = nlen(v, g);
	if (l == 0) h ^= (u64)v->rank[g] << 1 | 1;
	else if (v->nk[2 * g] == 0) h ^= hbytes(nptr(v, g), l);
	return mix64(h);
}
static inline int keqRep(const Ver *A, const Ver *B, const Ver *V, u32 g, u32 rep) {
	return rep >> 31 ? keq(V, g, B, rep & 0x7fffffff) : keq(V, g, A, rep);
}

#define PF 16
static u32 lookupOrAdd(u64 h, u32 *ncls, size_t mask) {
	u32 tag = (u32)(h >> 54) << 22;
	for (size_t p = h & mask;; p = (p + 1) & mask) {
		C.probes++;
		u32 e = S.tab[p];
		if (e == 0) {
			u32 c = (*ncls)++;
			S.tab[p] = tag | (c + 1);
			S.cls[c] = (Cls){h, 0, 0, 0, 0xffffffffu};
			return c;
		}
		if ((e & 0xffc00000u) == tag && S.cls[(e & 0x3fffff) - 1].h == h) return (e & 0x3fffff) - 1;
	}
}
static int classifyHashed(const Ver *A, const Ver *B, u32 a0, u32 na, u32 b0, u32 nb, u32 *ncls) {
	size_t cap = S.ntab, mask = cap - 1;
	// the common prefix and suffix (equal keys at the same position from either end): a B slot there takes its A
	// counterpart's class, so it needs no hash, no lookup and no verification
	u32 mn = na < nb ? na : nb, pre = 0, suf = 0;
	while (pre < mn && keq(A, a0 + pre, B, b0 + pre)) pre++;
	while (suf < mn - pre && keq(A, a0 + na - 1 - suf, B, b0 + nb - 1 - suf)) suf++;
	C.prefix += pre, C.suffix += suf;
	for (u32 i = 0; i < na; i++) S.hA[i] = khash2(A, a0 + i);
	for (u32 j = pre; j < nb - suf; j++) S.hB[j] = khash2(B, b0 + j);
	memset(S.tab, 0, cap * 4);
	*ncls = 0;
	for (u32 i = 0; i < na; i++) {
		if (i + PF < na) __builtin_prefetch(&S.tab[S.hA[i + PF] & mask]);
		u32 c = lookupOrAdd(S.hA[i], ncls, mask);
		Cls *k = &S.cls[c];
		if (k->rep == 0xffffffffu) k->rep = a0 + i;
		k->ca++, k->lastA = i, S.cA[i] = c;
	}
	for (u32 j = 0; j < pre; j++) S.cls[S.cB[j] = S.cA[j]].cb++;
	for (u32 j = nb - suf; j < nb; j++) S.cls[S.cB[j] = S.cA[j - nb + na]].cb++;
	for (u32 j = pre; j < nb - suf; j++) {
		if (j + PF < nb - suf) __builtin_prefetch(&S.tab[S.hB[j + PF] & mask]);
		u32 c = lookupOrAdd(S.hB[j], ncls, mask);
		Cls *k = &S.cls[c];
		if (k->rep == 0xffffffffu) k->rep = (b0 + j) | 0x80000000u;
		k->cb++, S.cB[j] = c;
	}
	// verify: every member equals its class's representative (so equal class <=> equal key, exactly)
	for (u32 i = 0; i < na; i++) {
		u32 rep = S.cls[S.cA[i]].rep;
		if (rep != a0 + i && (C.verify++, !keqRep(A, B, A, a0 + i, rep))) return 0;
	}
	for (u32 j = pre; j < nb - suf; j++) {
		u32 rep = S.cls[S.cB[j]].rep;
		if (rep != ((b0 + j) | 0x80000000u) && (C.verify++, !keqRep(A, B, B, b0 + j, rep))) return 0;
	}
	return 1;
}
static void classifyExact(const Ver *A, const Ver *B, u32 a0, u32 na, u32 b0, u32 nb, u32 *ncls) {
	size_t cap = S.ntab, mask = cap - 1;
	C.exact++;
	memset(S.tab, 0, cap * 4);
	*ncls = 0;
	for (int side = 0; side < 2; side++) {
		const Ver *V = side ? B : A;
		u32 s0 = side ? b0 : a0, n = side ? nb : na;
		for (u32 i = 0; i < n; i++) {
			u32 g = s0 + i;
			u64 h = khash(V, g);
			u32 c;
			for (size_t p = h & mask;; p = (p + 1) & mask) {
				C.probes++;
				u32 e = S.tab[p];
				if (e == 0) {
					c = (*ncls)++;
					S.tab[p] = c + 1;
					S.cls[c] = (Cls){h, 0, 0, 0, side ? g | 0x80000000u : g};
					break;
				}
				if (keqRep(A, B, V, g, S.cls[e - 1].rep)) { c = e - 1; break; }
			}
			if (side) S.cls[c].cb++, S.cB[i] = c;
			else S.cls[c].ca++, S.cls[c].lastA = i, S.cA[i] = c;
		}
	}
}

static u32 slowObject(const Ver *A, const Ver *B, u32 o, u32 *idm, u32 next) {
	u32 a0 = A->base[o], a1 = A->base[o + 1], b0 = B->base[o], b1 = B->base[o + 1];
	u32 na = a1 - a0, nb = b1 - b0;
	C.objs++, C.slotsA += na, C.slotsB += nb;
	size_t cap = 16;
	while (cap < 2 * (size_t)(na + nb)) cap <<= 1;
	if (na + nb >= 0x3fffff) { fprintf(stderr, "object %u too big for the class table\n", o); exit(1); }
	grow(na + 1, nb + 1, cap);
	double q0 = now();
	u32 ncls;
	if (forceExact || !classifyHashed(A, B, a0, na, b0, nb, &ncls)) classifyExact(A, B, a0, na, b0, nb, &ncls);
	u8 *ma = S.ma, *mbm = S.mb;
	memset(ma, 0, na), memset(mbm, 0, nb);
	for (u32 j = 0; j < nb; j++) {
		Cls *e = &S.cls[S.cB[j]];
		u32 g = b0 + j;
		if (e->ca == 1 && e->cb == 1 && (nlen(B, g) != 0 || B->hk[2 * g] != 0)) {
			ma[e->lastA] = 1, mbm[j] = 1, idm[g] = a0 + e->lastA;
			C.uniq++;
		}
	}
	u32 nra = 0, nrb = 0;
	for (u32 i = 0; i < na; i++)
		if (!ma[i]) S.ra[nra] = i, S.xa[nra++] = S.cA[i];
	for (u32 j = 0; j < nb; j++)
		if (!mbm[j]) S.rb[nrb] = j, S.xb[nrb++] = S.cB[j];
	double q1 = now();
	tHash += q1 - q0;
	C.myersA += nra, C.myersB += nrb;
	size_t np = myers(S.xa, nra, S.xb, nrb, S.pi, S.pj, &S.my);
	C.myersPairs += np;
	tMyers += now() - q1;
	for (size_t p = 0; p < np; p++) {
		u32 i = S.ra[S.pi[p]], j = S.rb[S.pj[p]];
		ma[i] = 1, mbm[j] = 1, idm[b0 + j] = a0 + i;
	}
	for (u32 j = 0; j < nb; j++)
		if (!mbm[j]) idm[b0 + j] = next++, C.bOnly++;
	for (u32 i = 0; i < na; i++) C.aOnly += !ma[i];
	return next;
}

// --fp: the fingerprint an extractor would write per object (computed here outside the timing)
static int useFp;
static void fingerprints(Ver *v) {
	v->fp = malloc(v->nobjs * 8);
	for (u32 o = 0; o < v->nobjs; o++) {
		u32 a0 = v->base[o], n = v->base[o + 1] - a0;
		u64 h = mix64(n);
		if (n) {
			h = mix64(h ^ hbytes(v->kind + a0, n));
			h = mix64(h ^ hbytes((const u8 *)(v->nk + 2 * a0), 16 * n));
			h = mix64(h ^ hbytes((const u8 *)(v->hk + 2 * a0), 16 * n));
			u64 s0 = v->noff[a0 - 1];
			h = mix64(h ^ hbytes(v->nbin + s0, v->noff[a0 + n - 1] - s0));
			for (u32 i = 0; i < n; i++) {
				u32 g = a0 + i, l = nlen(v, g);
				h = mix64(h ^ ((u64)l << 32 | (l ? 0 : v->rank[g])));
			}
		}
		v->fp[o] = h;
	}
}

// ---------------------------------------------------------------- the whole mapping
static u8 *differ;
static u32 nDiffer;
static void unify(const Ver *A, const Ver *B, u32 *idm, u32 *strm) {
	memset(&C, 0, sizeof C);
	tHash = tMyers = tFast = 0;
	myersD = 0;
	u32 nobj = A->nobjs;
	idm[0] = 0;
	double f0 = now();
#pragma omp parallel for schedule(dynamic, 8)
	for (u32 o = 0; o < nobj; o++) {
		differ[o] = useFp ? A->fp[o] != B->fp[o] || A->base[o + 1] - A->base[o] != B->base[o + 1] - B->base[o] : !same(A, B, o);
		if (!differ[o]) {
			u32 a0 = A->base[o], b0 = B->base[o], n = A->base[o + 1] - a0;
			for (u32 j = 0; j < n; j++) idm[b0 + j] = a0 + j;
		}
	}
	double f1 = now();
	tFast += f1 - f0;
	u32 next = A->nids;
	nDiffer = 0;
	for (u32 o = 0; o < nobj; o++)
		if (differ[o]) next = slowObject(A, B, o, idm, next), nDiffer++;
	for (u32 k = 0; k + 1 < B->nids - B->nslots; k++) idm[B->nslots + 1 + k] = A->nslots + 1 + k;
	// strings by content (18,937: a tiny table)
	size_t cap = 16;
	while (cap < 2 * (size_t)A->nstr) cap <<= 1;
	u32 *t = malloc(cap * 4);
	memset(t, 0xff, cap * 4);
	for (u32 i = 0; i < A->nstr; i++) {
		const u8 *p = A->sbin + A->soff[i];
		u32 l = A->soff[i + 1] - A->soff[i];
		size_t h = hbytes(p, l) & (cap - 1);
		for (;; h = (h + 1) & (cap - 1)) {
			if (t[h] == 0xffffffffu) { t[h] = i; break; }
			u32 q = t[h];
			if (A->soff[q + 1] - A->soff[q] == l && !memcmp(A->sbin + A->soff[q], p, l)) break; // keep first (strings are interned)
		}
	}
	u32 snext = A->nstr;
	for (u32 i = 0; i < B->nstr; i++) {
		const u8 *p = B->sbin + B->soff[i];
		u32 l = B->soff[i + 1] - B->soff[i];
		size_t h = hbytes(p, l) & (cap - 1);
		u32 r = 0xffffffffu;
		for (;; h = (h + 1) & (cap - 1)) {
			u32 q = t[h];
			if (q == 0xffffffffu) break;
			if (A->soff[q + 1] - A->soff[q] == l && !memcmp(A->sbin + A->soff[q], p, l)) { r = q; break; }
		}
		strm[i] = r != 0xffffffffu ? r : snext++;
	}
	free(t);
}

static long cmpFile(const char *path, const u32 *got, size_t n) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) { perror(path); return -1; }
	struct stat st;
	fstat(fd, &st);
	if ((size_t)st.st_size != n * 4) { printf("%s: size %zu, want %zu\n", path, (size_t)st.st_size, n * 4); close(fd); return -1; }
	const u32 *w = mmap(NULL, n * 4 + 1, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	long bad = 0;
	for (size_t i = 0; i < n; i++)
		if (got[i] != w[i] && bad++ < 10) printf("  [%zu] = %u, ddflow %u\n", i, got[i], w[i]);
	printf("%s: %zu entries, %ld differ%s\n", path, n, bad, bad ? "" : " (byte-identical)");
	return bad;
}

// --stats: which slots of a dump have no key that is unique in its object (what a stable-id extractor must invent)
static void stats(const Ver *V) {
	u64 byKind[6][4] = {{0}}; // [kind][0 unique named, 1 unique hashed, 2 dup key, 3 unnamed+unhashed]
	u64 unnamedUnhashedUniqueKey = 0, named = 0, hashed = 0, total = 0;
	for (u32 o = 0; o < V->nobjs; o++) {
		u32 a0 = V->base[o], a1 = V->base[o + 1], n = a1 - a0;
		size_t cap = 16;
		while (cap < 2 * (size_t)n) cap <<= 1;
		u32 *rep = malloc(cap * 4), *cnt = calloc(cap, 4), *at = malloc((size_t)n * 4 + 4);
		for (u32 i = 0; i < n; i++) {
			u32 g = a0 + i;
			size_t h = khash(V, g) & (cap - 1);
			for (;; h = (h + 1) & (cap - 1)) {
				if (!cnt[h]) { rep[h] = g, cnt[h] = 1; break; }
				if (keq(V, g, V, rep[h])) { cnt[h]++; break; }
			}
			at[i] = h;
		}
		for (u32 i = 0; i < n; i++) {
			u32 g = a0 + i, k = V->kind[g] <= 5 ? V->kind[g] : 0;
			int nm = nlen(V, g) != 0, hs = V->hk[2 * g] != 0;
			total++, named += nm, hashed += !nm && hs;
			int c;
			if (!nm && !hs) c = 3, unnamedUnhashedUniqueKey += cnt[at[i]] == 1;
			else if (cnt[at[i]] > 1) c = 2;
			else c = nm ? 0 : 1;
			byKind[k][c]++;
		}
		free(rep), free(cnt), free(at);
	}
	printf("stats %s: %llu slots; named %llu; unnamed with content hash %llu\n", V->dir, (unsigned long long)total,
	       (unsigned long long)named, (unsigned long long)hashed);
	printf("  kind: unique-named unique-hashed dup-key unnamed-unhashed\n");
	for (int k = 0; k < 6; k++)
		if (byKind[k][0] + byKind[k][1] + byKind[k][2] + byKind[k][3])
			printf("  %d: %llu %llu %llu %llu\n", k, (unsigned long long)byKind[k][0], (unsigned long long)byKind[k][1],
			       (unsigned long long)byKind[k][2], (unsigned long long)byKind[k][3]);
	printf("  unnamed-unhashed whose (kind, rank) is still unique in its object: %llu\n", (unsigned long long)unnamedUnhashedUniqueKey);
}

int main(int argc, char **argv) {
	if (argc < 3) { fprintf(stderr, "usage: unify A B [--reps N] [--check] [--stats] [--write DIR]\n"); return 2; }
	int reps = 1, check = 0, st = 0;
	const char *wdir = NULL, *fpdir = NULL;
	for (int i = 3; i < argc; i++) {
		if (!strcmp(argv[i], "--reps") && i + 1 < argc) reps = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--check")) check = 1;
		else if (!strcmp(argv[i], "--stats")) st = 1;
		else if (!strcmp(argv[i], "--exact")) forceExact = 1;
		else if (!strcmp(argv[i], "--fp")) useFp = 1;
		else if (!strcmp(argv[i], "--fpdir") && i + 1 < argc) fpdir = argv[++i], useFp = 1, lazy = 1;
		else if (!strcmp(argv[i], "--write") && i + 1 < argc) wdir = argv[++i];
	}
	struct rusage ru0, ru1;
	getrusage(RUSAGE_SELF, &ru0);
	double t0 = now();
	Ver A, B;
	load(&A, argv[1]);
	load(&B, argv[2]);
	if (A.nobjs != B.nobjs || A.nids - A.nslots != B.nids - B.nslots) { fprintf(stderr, "object or linker-made counts differ\n"); return 1; }
	if (fpdir) { // fingerprints as an extractor would write them: one u64 per object, in a file next to the dump
		for (int s = 0; s < 2; s++) {
			Ver *v = s ? &B : &A;
			char p[4096], dcopy[4096];
			snprintf(dcopy, sizeof dcopy, "%s", v->dir);
			snprintf(p, sizeof p, "%s/%s.objfp.u64", fpdir, basename(dcopy));
			FILE *f = fopen(p, "rb");
			v->fp = malloc(v->nobjs * 8);
			if (!f || fread(v->fp, 8, v->nobjs, f) != v->nobjs) {
				if (f) fclose(f);
				fingerprints(v);
				f = fopen(p, "wb");
				fwrite(v->fp, 8, v->nobjs, f);
				fclose(f);
				printf("wrote %s: rerun for a lazy end-to-end time\n", p);
			} else
				fclose(f), bytesMapped += v->nobjs * 8;
		}
	} else if (useFp) {
		double f0 = now();
		fingerprints(&A), fingerprints(&B);
		t0 += now() - f0; // not part of end-to-end
		printf("--fp: per-object fingerprints of both dumps, %.2f ms (outside the timing: an extractor would write them)\n", (now() - f0) * 1e3);
	}
	u32 *idm = malloc((size_t)B.nids * 4), *strm = malloc((size_t)B.nstr * 4 + 4);
	differ = malloc(A.nobjs);
	double t1 = now();
	unify(&A, &B, idm, strm);
	double t2 = now();
	getrusage(RUSAGE_SELF, &ru1);
	int threads = 1;
#ifdef _OPENMP
	threads = omp_get_max_threads();
#endif
	printf("end-to-end: map %.2f ms (%zu bytes, %s) + unify %.2f ms = %.2f ms, %d thread(s); %ld minor faults (%.1f MB of pages touched)\n",
	       (t1 - t0) * 1e3, bytesMapped, lazy ? "lazy" : "MAP_POPULATE", (t2 - t1) * 1e3, (t2 - t0) * 1e3, threads,
	       ru1.ru_minflt - ru0.ru_minflt, (ru1.ru_minflt - ru0.ru_minflt) * 4096.0 / 1e6);
	printf("objects differ %u of %u; slow path: A %llu + B %llu slots, unique-key matched %llu, myers input %llu + %llu (D sum %llu), myers pairs %llu, a_only %llu, b_only %llu, probes %llu, verify keq %llu, exact fallbacks %llu, common prefix %llu suffix %llu\n",
	       nDiffer, A.nobjs, (unsigned long long)C.slotsA, (unsigned long long)C.slotsB, (unsigned long long)C.uniq,
	       (unsigned long long)C.myersA, (unsigned long long)C.myersB, (unsigned long long)myersD, (unsigned long long)C.myersPairs,
	       (unsigned long long)C.aOnly, (unsigned long long)C.bOnly, (unsigned long long)C.probes, (unsigned long long)C.verify, (unsigned long long)C.exact, (unsigned long long)C.prefix, (unsigned long long)C.suffix);
	printf("phases (first run): fast path %.2f ms, slow-path classes %.2f ms, myers %.2f ms\n", tFast * 1e3, tHash * 1e3, tMyers * 1e3);
	if (reps > 0) { // warm: at least reps iterations and at least 1 s
		int r = 0;
		double w0 = now(), w1, sf = 0, sh = 0, sm = 0;
		do {
			unify(&A, &B, idm, strm);
			sf += tFast, sh += tHash, sm += tMyers;
			r++;
			w1 = now();
		} while (r < reps || w1 - w0 < 1.0);
		printf("warm: %d reps in %.2f s, %.3f ms per unify (fast path %.3f, slow-path classes %.3f, myers %.3f)\n", r, w1 - w0,
		       (w1 - w0) * 1e3 / r, sf * 1e3 / r, sh * 1e3 / r, sm * 1e3 / r);
	}
	int rc = 0;
	if (check) {
		char p[4096];
		snprintf(p, sizeof p, "%s/diff/idmap_b.u32", B.dir);
		rc |= cmpFile(p, idm, B.nids) != 0;
		snprintf(p, sizeof p, "%s/diff/strmap_b.u32", B.dir);
		rc |= cmpFile(p, strm, B.nstr) != 0;
		printf(rc ? "CHECK FAIL\n" : "CHECK OK\n");
	}
	if (wdir) {
		char p[4096];
		snprintf(p, sizeof p, "%s/idmap_b.u32", wdir);
		FILE *f = fopen(p, "wb");
		fwrite(idm, 4, B.nids, f);
		fclose(f);
	}
	if (st) stats(&A), stats(&B);
	return rc;
}
