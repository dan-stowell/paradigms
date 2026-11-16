// Content-hash join: radix partition, then independent small tables. Both the
// serial reference and OpenMP version use the same deterministic reduction.
#ifdef _OPENMP
#define HC_PAR _Pragma("omp parallel for schedule(static)")
#else
#define HC_PAR
#endif
#define HC_NP 256
typedef struct { u64 k1, k2; u32 id; u8 kind, mark; } hrec;
static u64 hhash(hrec r) { return mix64(r.k1 ^ (r.kind == 3 ? r.k2 : 0)); }
static int hequal(hrec a, hrec b) {
	return a.kind == b.kind && a.k1 == b.k1 && (a.kind == 2 || a.k2 == b.k2);
}
static int hbetter(hrec a, hrec b) {
	if (a.mark != b.mark) return a.mark < b.mark;
	if (a.kind == 2 && a.k2 != b.k2) return a.k2 > b.k2;
	return a.id < b.id;
}
static void contentJoin(u32 N, const u8 *kind, const u8 *mark, const u64 *key,
                        const u32 *canon, u32 *out, int chunks) {
	size_t *counts = calloc((size_t)chunks * HC_NP, sizeof(size_t));
	size_t base[HC_NP + 1];
	HC_PAR
	for (int c = 0; c < chunks; c++) {
		u32 lo = 1 + (u64)N * c / chunks, hi = 1 + (u64)N * (c + 1) / chunks;
		size_t *ct = counts + (size_t)c * HC_NP;
		for (u32 g = lo; g < hi; g++) {
			out[g] = canon[g];
			if ((kind[g] != 2 && kind[g] != 3) || !mark[g]) continue;
			hrec r = {key[2*(size_t)g], key[2*(size_t)g+1], g, kind[g], mark[g]};
			ct[hhash(r) >> 56]++;
		}
	}
	out[0] = canon[0];
	size_t n = 0;
	for (int p = 0; p < HC_NP; p++) {
		base[p] = n;
		for (int c = 0; c < chunks; c++) {
			size_t *v = &counts[(size_t)c * HC_NP + p], count = *v;
			*v = n; n += count;
		}
	}
	base[HC_NP] = n;
	hrec *records = malloc(n * sizeof(hrec));
	HC_PAR
	for (int c = 0; c < chunks; c++) {
		u32 lo = 1 + (u64)N * c / chunks, hi = 1 + (u64)N * (c + 1) / chunks;
		size_t *ct = counts + (size_t)c * HC_NP;
		for (u32 g = lo; g < hi; g++) {
			if ((kind[g] != 2 && kind[g] != 3) || !mark[g]) continue;
			hrec r = {key[2*(size_t)g], key[2*(size_t)g+1], g, kind[g], mark[g]};
			records[ct[hhash(r) >> 56]++] = r;
		}
	}
	HC_PAR
	for (int p = 0; p < HC_NP; p++) {
		size_t count = base[p+1] - base[p], sz = 1;
		while (sz < 2 * count) sz <<= 1;
		u32 *table = calloc(sz, sizeof(u32)); // local record index + 1; zero is empty
		hrec *r = records + base[p];
		for (size_t i = 0; i < count; i++) {
			size_t h = hhash(r[i]) & (sz-1);
			while (table[h] && !hequal(r[i], r[table[h]-1])) h = (h+1) & (sz-1);
			if (!table[h] || hbetter(r[i], r[table[h]-1])) table[h] = i+1;
		}
		for (size_t i = 0; i < count; i++) {
			size_t h = hhash(r[i]) & (sz-1);
			while (!hequal(r[i], r[table[h]-1])) h = (h+1) & (sz-1);
			hrec w = r[table[h]-1];
			if (w.mark == 2) continue;
			if (r[i].kind == 2 && r[i].mark == 2 && r[i].k2 > w.k2) continue;
			out[r[i].id] = w.id;
		}
		free(table);
	}
	free(records);
	free(counts);
}
#undef HC_PAR
#undef HC_NP
