// p4cp4.c: problem 4 (code generation by shape) from the flat files, with OpenMP: every function body
// fc's tree walker wrote, rebuilt from byte templates and operands. Checks itself against oracle/p4_code.bin
// (byte for byte) and the relocations against oracle/p4_rel_* (as a sorted list).
//   gcc -O2 -march=native -fopenmp -o build/p4cp4 ref/p4/p4cp4.c
//   OMP_NUM_THREADS=8 OMP_WAIT_POLICY=passive P4_REPS=40 build/p4cp4 data/minio-p4
// Generate build/cp4_generated.h with gen_cp4.py before compiling.
// P4_CP4_GENERIC=1 disables specialization (control); P4_CP4_COVERED=1 additionally
// measures the covered-item kernel after the full oracle succeeds.
// P4_HUGE=0 forces the fallback for testing. Sorting/checking is outside timed kernels.
#include "../flat/flat.h"
#include <omp.h>

enum { encInt = 1, encRel32 = 2, encReloc = 3 };

typedef struct {
	u64 off;
	i64 add;
	u32 sym;
	u16 type;
	u8 siz;
} Rel;
// Each worker owns a contiguous range of functions and a reusable relocation vector.
// Concatenating worker vectors in range order preserves serial traversal order.
typedef struct { Rel *a; size_t n, cap; } RelVec;
static void push(RelVec *v, Rel r) {
	if (v->n == v->cap) {
		size_t cap = v->cap ? v->cap * 2 : 4096;
		Rel *a = realloc(v->a, cap * sizeof(Rel));
		if (!a) { perror("relocations"); exit(2); }
		v->a = a; v->cap = cap;
	}
	v->a[v->n++] = r;
}
#include "../../build/cp4_generated.h"

static u8 *output(size_t size) {
	size_t page = 2u << 20, n = (size + page - 1) & ~(page - 1);
	if (!n) n = page;
	u8 *p = MAP_FAILED;
	if (!getenv("P4_HUGE") || atoi(getenv("P4_HUGE")))
		p = mmap(NULL, n, PROT_READ | PROT_WRITE,
		         MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | (21 << MAP_HUGE_SHIFT), -1, 0);
	if (p != MAP_FAILED) { printf("output: MAP_HUGETLB 2 MB\n"); return p; }
	p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { perror("output"); exit(2); }
	int advised = madvise(p, n, MADV_HUGEPAGE) == 0;
	printf("output: anonymous fallback, MADV_HUGEPAGE %s (THP not guaranteed)\n", advised ? "accepted" : "failed");
	return p;
}
static int cmpRel(const void *a, const void *b) {
	const Rel *x = a, *y = b;
	if (x->off != y->off) return x->off < y->off ? -1 : 1;
	if (x->type != y->type) return x->type < y->type ? -1 : 1;
	if (x->sym != y->sym) return x->sym < y->sym ? -1 : 1;
	if (x->add != y->add) return x->add < y->add ? -1 : 1;
	return (int)x->siz - (int)y->siz;
}

int main(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "data/minio-p4";
	u32 nfn = (u32)metaGet("nfn"), nitem = (u32)metaGet("nitem"), nrel = (u32)metaGet("nrel");
	u64 ncode = metaGet("code_bytes");
	const u32 *fnItem = FMAP(u32, "fn_item.u32", NULL), *fnMarkOff = FMAP(u32, "fn_mark_off.u32", NULL);
	const u32 *markOff = FMAP(u32, "mark_off.u32", NULL), *markSym = FMAP(u32, "mark_sym.u32", NULL);
	const u16 *markType = FMAP(u16, "mark_type.u16", NULL);
	const i64 *markAdd = FMAP(i64, "mark_add.i64", NULL);
	const u8 *itemKind = FMAP(u8, "item_kind.u8", NULL);
	const u32 *itemRef = FMAP(u32, "item_ref.u32", NULL), *itemSeg = FMAP(u32, "item_seg.u32", NULL);
	const u64 *glueOff = FMAP(u64, "glue.off.u64", NULL);
	const u8 *glueBin = FMAP(u8, "glue.bin", NULL);
	const u32 *glueRelOff = FMAP(u32, "glue_rel_off.u32", NULL), *grelOff = FMAP(u32, "grel_off.u32", NULL);
	const u8 *grelSiz = FMAP(u8, "grel_siz.u8", NULL);
	const u16 *grelType = FMAP(u16, "grel_type.u16", NULL);
	const u32 *grelSym = FMAP(u32, "grel_sym.u32", NULL);
	const i64 *grelAdd = FMAP(i64, "grel_add.i64", NULL);
	const u32 *nodeTmpl = FMAP(u32, "node_tmpl.u32", NULL), *nodeOp = FMAP(u32, "node_op.u32", NULL);
	const i64 *opVal = FMAP(i64, "op_val.i64", NULL), *opAux = FMAP(i64, "op_aux.i64", NULL);
	const u64 *tmplOff = FMAP(u64, "tmpl.off.u64", NULL);
	const u8 *tmplBin = FMAP(u8, "tmpl.bin", NULL);
	const u32 *tmplSegOff = FMAP(u32, "tmpl_seg_off.u32", NULL), *tsegEnd = FMAP(u32, "tseg_end.u32", NULL);
	const u32 *tmplHoleOff = FMAP(u32, "tmpl_hole_off.u32", NULL), *holeAt = FMAP(u32, "hole_at.u32", NULL);
	const u8 *holeW = FMAP(u8, "hole_w.u8", NULL), *holeEnc = FMAP(u8, "hole_enc.u8", NULL);
	const u16 *holeRtype = FMAP(u16, "hole_rtype.u16", NULL);

	if (metaGet("ntmpl") != sizeof(cp4_table) / sizeof(cp4_table[0])) { fprintf(stderr, "regenerate CP4 for this dataset\n"); return 2; }

	// ---- 1. the length of every item (a glue run, or one segment of a node's template), then where it goes
	int reps = getenv("P4_REPS") ? atoi(getenv("P4_REPS")) : 1;
	if (reps < 1) { fprintf(stderr, "P4_REPS must be positive\n"); return 2; }
	u64 *at = malloc(8 * ((size_t)nitem + 1));
	omp_set_dynamic(0);
	int T = 1;
	#pragma omp parallel
	{
		#pragma omp single
		T = omp_get_num_threads();
	}
	int generic = getenv("P4_CP4_GENERIC") && atoi(getenv("P4_CP4_GENERIC"));
	u8 *code = output(ncode);
	RelVec *vec = calloc(T, sizeof(RelVec));
	u64 *sum = calloc(T + 1, sizeof(u64)), *relOff = calloc(T + 1, sizeof(u64));
	u32 *firstFn = calloc(T + 1, sizeof(u32));
	if (!at || !vec || !sum || !relOff || !firstFn) { perror("buffers"); return 2; }
	// Function boundaries nearest the item quantiles avoid splitting a function's markers.
	for (int w = 0; w <= T; w++) {
		u32 lo = 0, hi = nfn;
		u64 target = (u64)nitem * w / T;
		while (lo < hi) {
			u32 mid = lo + (hi - lo) / 2;
			if (fnItem[mid] < target) lo = mid + 1; else hi = mid;
		}
		firstFn[w] = lo;
	}
	firstFn[0] = 0; firstFn[T] = nfn;
	Rel *rel = malloc(sizeof(Rel) * ((size_t)nrel + 1));
	if (!rel) { perror("relocations"); return 2; }
	double layoutBest = 1e9, emitBest = 1e9, total = 0;
	u64 pos = 0, nr = 0, nerr = 0;
	for (int rep = 0; rep <= reps; rep++) { // first iteration warms buffers; excluded
		double t0 = now();
		// Local scans, scan the T totals, then add the chunk bases.
		#pragma omp parallel num_threads(T)
		{
			int w = omp_get_thread_num(), nt = omp_get_num_threads();
			u32 begin = (u64)nitem * w / nt, end = (u64)nitem * (w + 1) / nt;
			u64 pos = 0;
			for (u32 i = begin; i < end; i++) {
				at[i] = pos;
				u32 r = itemRef[i];
				if (itemKind[i] == 0) {
					pos += glueOff[r + 1] - glueOff[r];
				} else {
					u32 t = nodeTmpl[r], s = itemSeg[i], b = tmplSegOff[t];
					pos += tsegEnd[b + s] - (s ? tsegEnd[b + s - 1] : 0);
				}
			}
			sum[w + 1] = pos;
			#pragma omp barrier
			#pragma omp single
			{
				for (int j = 1; j <= nt; j++) sum[j] += sum[j - 1];
				at[nitem] = sum[nt];
			}
			for (u32 i = begin; i < end; i++) at[i] += sum[w];
		}
		pos = at[nitem];
		if (pos != ncode) { fprintf(stderr, "layout size differs from metadata\n"); return 2; }
		double t1 = now();

		// ---- 2. emit: copy each item's bytes, patch the holes of node segments
		nr = nerr = 0;
		#pragma omp parallel for schedule(static, 1) num_threads(T) reduction(+:nerr)
		for (int worker = 0; worker < T; worker++) {
			RelVec local = vec[worker];
			local.n = 0;
			for (u32 f = firstFn[worker]; f < firstFn[worker + 1]; f++) {
				u64 fstart = at[fnItem[f]];
				for (u32 m = fnMarkOff[f]; m < fnMarkOff[f + 1]; m++)
					push(&local, (Rel){fstart + markOff[m], markAdd[m], markSym[m], markType[m], 0});
				for (u32 i = fnItem[f]; i < fnItem[f + 1]; i++) {
					u32 r = itemRef[i];
					u8 *P = code + at[i];
					if (itemKind[i] == 0) {
						memcpy(P, glueBin + glueOff[r], glueOff[r + 1] - glueOff[r]);
						for (u32 k = glueRelOff[r]; k < glueRelOff[r + 1]; k++)
							push(&local, (Rel){at[i] + grelOff[k], grelAdd[k], grelSym[k], grelType[k], grelSiz[k]});
						continue;
					}
					u32 t = nodeTmpl[r], s = itemSeg[i], b = tmplSegOff[t];
					if (!generic && cp4_table[t]) {
						cp4_table[t](P, at[i], s, opVal + nodeOp[r], opAux + nodeOp[r], at, &local);
						continue;
					}
					u32 lo = s ? tsegEnd[b + s - 1] : 0, hi = tsegEnd[b + s];
					memcpy(P, tmplBin + tmplOff[t] + lo, hi - lo);
					// the template's holes in [lo, hi): they are sorted by offset
					u32 h0 = tmplHoleOff[t], h1 = tmplHoleOff[t + 1];
					while (h0 < h1) {
						u32 mid = (h0 + h1) / 2;
						if (holeAt[mid] < lo) h0 = mid + 1; else h1 = mid;
					}
					for (u32 h = h0; h < tmplHoleOff[t + 1] && holeAt[h] < hi; h++) {
						u32 o = nodeOp[r] + (h - tmplHoleOff[t]);
						u64 hp = at[i] + holeAt[h] - lo;
						i64 v = opVal[o];
						switch (holeEnc[h]) {
						case encInt:
							switch (holeW[h]) {
							case 1: code[hp] = (u8)v; break;
							case 2: { u16 x = (u16)v; memcpy(code + hp, &x, 2); break; }
							case 4: { u32 x = (u32)v; memcpy(code + hp, &x, 4); break; }
							case 8: memcpy(code + hp, &v, 8); break;
							default: nerr++;
							}
							break;
						case encRel32: { // a jump to (item, offset) of the same function
							i32 x = (i32)((i64)(at[v] + opAux[o]) - (i64)(hp + 4));
							memcpy(code + hp, &x, 4);
							break;
						}
						case encReloc:
							push(&local, (Rel){hp, opAux[o], (u32)v, holeRtype[h], holeW[h]});
							break;
						default: nerr++;
						}
					}
				}
			}
			vec[worker] = local;
		}
		for (int w = 0; w < T; w++) relOff[w + 1] = relOff[w] + vec[w].n;
		nr = relOff[T];
		if (nr != nrel) { fprintf(stderr, "relocation count differs from metadata\n"); return 2; }
		#pragma omp parallel for schedule(static) num_threads(T)
		for (int w = 0; w < T; w++)
			if (vec[w].n) memcpy(rel + relOff[w], vec[w].a, vec[w].n * sizeof(Rel));
		double t2 = now();
		if (rep) {
			if (t1 - t0 < layoutBest) layoutBest = t1 - t0;
			if (t2 - t1 < emitBest) emitBest = t2 - t1;
			total += t2 - t0;
		}
	}
	double ts = now();
	qsort(rel, nr < nrel ? nr : nrel, sizeof(Rel), cmpRel);
	printf("p4cp4 (%d threads): lay out %.3f ms, emit %.3f ms (%llu bytes, %u items), sort relocations %.3f ms (%llu); %llu errors; reps %d total %.3f s\n",
	       T, layoutBest * 1e3, emitBest * 1e3, (unsigned long long)pos, nitem, (now() - ts) * 1e3,
	       (unsigned long long)nr, (unsigned long long)nerr, reps, total);

	// ---- check
	long bad = nerr != 0;
	size_t m;
	const u8 *want = FMAP(u8, "oracle/p4_code.bin", &m);
	const u64 *wfn = FMAP(u64, "oracle/p4_fn_off.u64", NULL);
    u32 fbad = 0;
    for (u32 f = 0; f <= nfn; f++)
        if (at[fnItem[f]] != wfn[f]) fbad++;
    printf("function offsets: %u differ\n", fbad);
    bad += fbad;
	if (m != pos || pos != ncode) {
		printf("code: %llu bytes, oracle %zu\n", (unsigned long long)pos, m);
		bad++;
	} else if (memcmp(code, want, pos) != 0) {
		long nf = 0;
		for (u32 f = 0; f < nfn; f++)
			if (at[fnItem[f]] != wfn[f] || memcmp(code + wfn[f], want + wfn[f], wfn[f + 1] - wfn[f]) != 0)
				if (nf++ < 5) printf("  function %u differs\n", f);
		printf("code: %ld functions differ\n", nf);
		bad++;
	}
	printf("code: %llu bytes in %u functions, %s oracle/p4_code.bin\n", (unsigned long long)pos, nfn, bad ? "NOT equal to" : "equal to");
	const u64 *wo = FMAP(u64, "oracle/p4_rel_off.u64", NULL);
	const u8 *wsiz = FMAP(u8, "oracle/p4_rel_siz.u8", NULL);
	const u16 *wtype = FMAP(u16, "oracle/p4_rel_type.u16", NULL);
	const u32 *wsym = FMAP(u32, "oracle/p4_rel_sym.u32", NULL);
	const i64 *wadd = FMAP(i64, "oracle/p4_rel_add.i64", NULL);
	Rel *w = malloc(sizeof(Rel) * ((size_t)nrel + 1));
	for (u32 i = 0; i < nrel; i++) w[i] = (Rel){wo[i], wadd[i], wsym[i], wtype[i], wsiz[i]};
	qsort(w, nrel, sizeof(Rel), cmpRel);
	long rbad = nr != nrel;
	for (u32 i = 0; i < nrel && i < nr; i++)
		if (cmpRel(&rel[i], &w[i]) != 0 && rbad++ < 5) printf("  relocation %u differs (offset %llu)\n", i, (unsigned long long)rel[i].off);
	printf("relocations: %llu, oracle %u, %ld differ\n", (unsigned long long)nr, nrel, rbad);
	bad += rbad;
	if (argc > 2) {
		FILE *f = fopen(argv[2], "wb");
		fwrite(code, 1, pos, f);
		fclose(f);
	}

	// Independently time only the hot node segments, after a complete oracle check.
	// The same item list and order are used for generic and specialized trials;
	// gather once outside timing so glue/slow-item traversal is excluded.
	if (!bad && getenv("P4_CP4_COVERED") && atoi(getenv("P4_CP4_COVERED"))) {
		u32 *hotItem = malloc((size_t)nitem * sizeof(u32));
		u32 nhot = 0;
		for (u32 i = 0; i < nitem; i++)
			if (itemKind[i] && cp4_table[nodeTmpl[itemRef[i]]]) hotItem[nhot++] = i;
		double best = 1e9, elapsed = 0;
		for (int rep = 0; rep <= reps; rep++) {
			double t0 = now();
			#pragma omp parallel num_threads(T) reduction(+:nerr)
			{
				int worker = omp_get_thread_num(), nt = omp_get_num_threads();
				RelVec local = vec[worker];
				local.n = 0;
				for (u32 q = (u64)nhot * worker / nt; q < (u64)nhot * (worker + 1) / nt; q++) {
					u32 i = hotItem[q], r = itemRef[i];
					u8 *P = code + at[i];
					u32 t = nodeTmpl[r], s = itemSeg[i], b = tmplSegOff[t];
					if (!generic && cp4_table[t]) {
						cp4_table[t](P, at[i], s, opVal + nodeOp[r], opAux + nodeOp[r], at, &local);
						continue;
					}
					u32 lo = s ? tsegEnd[b + s - 1] : 0, hi = tsegEnd[b + s];
					memcpy(P, tmplBin + tmplOff[t] + lo, hi - lo);
					// the template's holes in [lo, hi): they are sorted by offset
					u32 h0 = tmplHoleOff[t], h1 = tmplHoleOff[t + 1];
					while (h0 < h1) {
						u32 mid = (h0 + h1) / 2;
						if (holeAt[mid] < lo) h0 = mid + 1; else h1 = mid;
					}
					for (u32 h = h0; h < tmplHoleOff[t + 1] && holeAt[h] < hi; h++) {
						u32 o = nodeOp[r] + (h - tmplHoleOff[t]);
						u64 hp = at[i] + holeAt[h] - lo;
						i64 v = opVal[o];
						switch (holeEnc[h]) {
						case encInt:
							switch (holeW[h]) {
							case 1: code[hp] = (u8)v; break;
							case 2: { u16 x = (u16)v; memcpy(code + hp, &x, 2); break; }
							case 4: { u32 x = (u32)v; memcpy(code + hp, &x, 4); break; }
							case 8: memcpy(code + hp, &v, 8); break;
							default: nerr++;
							}
							break;
						case encRel32: { // a jump to (item, offset) of the same function
							i32 x = (i32)((i64)(at[v] + opAux[o]) - (i64)(hp + 4));
							memcpy(code + hp, &x, 4);
							break;
						}
						case encReloc:
							push(&local, (Rel){hp, opAux[o], (u32)v, holeRtype[h], holeW[h]});
							break;
						default: nerr++;
						}
					}
				}
				vec[worker] = local;
			}
			double dt = now() - t0;
			if (rep) { if (dt < best) best = dt; elapsed += dt; }
		}
		printf("cp4 covered (%d threads, %s): emit %.3f ms; %u items; reps %d total %.3f s\n",
		       T, generic ? "generic" : "specialized", best * 1e3, nhot, reps, elapsed);
		if (memcmp(code, want, ncode)) { fprintf(stderr, "covered code check failed\n"); bad++; }
		free(hotItem);
	}
	printf(bad ? "P4 FAIL\n" : "P4 OK\n");
	return bad != 0;
}
