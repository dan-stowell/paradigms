// p2gb.c: problem 2 (reach) in SuiteSparse:GraphBLAS. The flood is a masked BFS over the boolean ANY.PAIR semiring:
//   frontier<!live> = frontier any.pair A0 ;  live<frontier> = true
// and every other rule of ref/flat/README.md is one more matrix (A1 useiface, A2 ifacemethod, A3 namedmethod, AT the
// plain edges between defined gotypes, M type->method, Mout method->its three entries, S signature->matching
// methods) applied to the round's newly live (or newly iface) set. Rounds: BFS to a fixpoint, iface closure,
// then the method check, until no method adds a live symbol. Checks oracle/p2_live.u32 and writes out/p2_live.u32.
//   GB_THREADS=8 GB_DIR=push|pull|col ./p2gb DATA [REPS]
#include "gb.h"

enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16 };

// the flags as boolean vectors: entries where (fl & want) == want
static GrB_Vector flagVec(GrB_Vector fl, u8 want, ix N) {
	GrB_Vector t, v;
	OK(GrB_Vector_new(&t, GrB_UINT8, N));
	OK(GrB_Vector_new(&v, GrB_UINT8, N));
	OK(GrB_Vector_apply_BinaryOp2nd_UINT8(t, NULL, NULL, GrB_BAND_UINT8, fl, want, NULL));
	OK(GrB_Vector_select_UINT8(v, NULL, NULL, GrB_VALUEEQ_UINT8, t, want, NULL));
	GrB_free(&t);
	return v;
}

int main(int argc, char **argv) {
	double tStart = now();
	flatDir = argc > 1 ? argv[1] : "data/minio";
	int reps = argc > 2 ? atoi(argv[2]) : 7;
	const char *dir = getenv("GB_DIR") ? getenv("GB_DIR") : "push";
	ix N = metaGet("nids"), nsl = metaGet("nslots");
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
	gbInit();
	if (getenv("GB_CHUNK")) OK(GxB_Global_Option_set_FP64(GxB_CHUNK, atof(getenv("GB_CHUNK"))));

	// ---- loading: the flat files' CSR rows as (row, col) tuples per edge kind (the only plain loops) ----
	double tLoad = now();
	ix ne = eoff[nsl + 1], cnt[4] = {0}, *EI[4], *EJ[4];
	for (int k = 0; k < 4; k++) EI[k] = malloc(8 * ne), EJ[k] = malloc(8 * ne);
	for (ix g = 1; g <= nsl; g++)
		for (u32 e = eoff[g]; e < eoff[g + 1]; e++) {
			int k = ek[e];
			if (!(fl[g] & fDef)) { fprintf(stderr, "edges on a non-definition %lu\n", (unsigned long)g); return 1; }
			EI[k][cnt[k]] = g, EJ[k][cnt[k]++] = ed[e];
		}
	ix nmt = moff[nsl + 1], *MI = malloc(8 * nmt), *MJ = malloc(8 * nmt), k = 0;
	for (ix t = 1; t <= nsl; t++)
		for (u32 m = moff[t]; m < moff[t + 1]; m++) MI[k] = t, MJ[k++] = m;
	ix *mI = malloc(8 * 3 * nmeth), *mJ = malloc(8 * 3 * nmeth), *tI = malloc(8 * nmeth), *tJ = malloc(8 * nmeth);
	ix *nI = malloc(8 * nmeth), *nJ = malloc(8 * nmeth), *eI = malloc(8 * nmeth), no = 0, nt = 0, nE = 0;
	for (ix m = 0; m < nmeth; m++) {
		u32 x[3] = {mTyp[m], mIfn[m], mTfn[m]};
		for (int d = 0; d < 3; d++) if (x[d]) mI[no] = m, mJ[no++] = x[d];
		if (mTyp[m]) tI[nt] = m, tJ[nt++] = mTyp[m];
		nI[m] = m, nJ[m] = mName[m];
		if (mExp[m]) eI[nE++] = m;
	}
	ix *sI = malloc(8 * nsig), *sN = malloc(8 * nsig), *sT = malloc(8 * nsig), *rI = malloc(8 * nroots), *fI = malloc(8 * (nsl + 1));
	for (ix s = 0; s < nsig; s++) sI[s] = s, sN[s] = sigName[s], sT[s] = sigType[s];
	for (ix i = 0; i < nroots; i++) rI[i] = roots[i];
	for (ix g = 0; g <= nsl; g++) fI[g] = g;
	GrB_Vector flv;
	OK(GrB_Vector_new(&flv, GrB_UINT8, N));
	OK(GrB_Vector_build_UINT8(flv, fI, fl, nsl + 1, GrB_FIRST_UINT8));

	// ---- the rule matrices (GraphBLAS builds: sort + dedup) ----
	GrB_Matrix A0 = boolMatrix(N, N, EI[0], EJ[0], cnt[0]);      // plain: s -> t
	GrB_Matrix A1 = boolMatrix(N, N, EI[1], EJ[1], cnt[1]);      // useiface: s -> type
	GrB_Matrix A2 = boolMatrix(N, nsig, EI[2], EJ[2], cnt[2]);   // ifacemethod: s -> signature
	GrB_Matrix A3 = boolMatrix(N, nstr + 1, EI[3], EJ[3], cnt[3]); // namedmethod: s -> name
	GrB_Matrix M = boolMatrix(N, nmeth, MI, MJ, nmt);            // type -> its methods
	GrB_Matrix Mout = boolMatrix(nmeth, N, mI, mJ, no);           // method -> mtyp, ifn, tfn
	GrB_Matrix Mtyp = boolMatrix(nmeth, N, tI, tJ, nt);           // method -> mtyp
	GrB_Matrix MN = boolMatrix(nmeth, nstr + 1, nI, nJ, nmeth);   // method -> name
	ix *z = malloc(8 * nmeth);
	for (ix m = 0; m < nmeth; m++) z[m] = mTyp[m];
	GrB_Matrix MT0 = boolMatrix(nmeth, N, nI, z, nmeth);          // method -> mtyp, 0 included (for S)
	GrB_Matrix SN = boolMatrix(nsig, nstr + 1, sI, sN, nsig), ST = boolMatrix(nsig, N, sI, sT, nsig);
	GrB_Vector E = boolVector(nmeth, eI, nE), rootv = boolVector(N, rI, nroots);
	GrB_Vector gtdef = flagVec(flv, fGoType | fDef, N), refl = flagVec(flv, fReflect | fDef, N), iface0 = flagVec(flv, fIface, N);
	OK(GrB_Vector_set_INT32(gtdef, GxB_BITMAP, GxB_SPARSITY_CONTROL)); // flag tests are O(1) lookups
	OK(GrB_Vector_set_INT32(refl, GxB_BITMAP, GxB_SPARSITY_CONTROL));
	GrB_Matrix A0T = NULL;
	if (!strcmp(dir, "pull")) { OK(GrB_Matrix_new(&A0T, GrB_BOOL, N, N)); OK(GrB_transpose(A0T, NULL, NULL, A0, NULL)); }
	if (!strcmp(dir, "col")) OK(GrB_Matrix_set_INT32(A0, GrB_COLMAJOR, GrB_STORAGE_ORIENTATION_HINT));
	for (int k = 0; k < 4; k++) free(EI[k]), free(EJ[k]);
	double tBuilt = now();

	GrB_Semiring sr = GxB_ANY_PAIR_BOOL;
	GrB_Vector live, iface, fr, delta, src, t1, t2, cand, call, cm, sig, named, csig, cnam, rv, start = NULL;
	GrB_Matrix S, S1;
	double best = 1e9, bestRules = 1e9, sum = 0;
	long levels = 0, ilevels = 0, rounds = 0, nvxm = 0;
	for (int rep = 0; rep < reps; rep++) {
		// the state (outside the timing, like the C references' clearing): live and iface as bitmaps
		OK(GrB_Vector_dup(&live, rootv));
		OK(GrB_Vector_set_INT32(live, GxB_BITMAP, GxB_SPARSITY_CONTROL));
		OK(GrB_Vector_dup(&iface, iface0));
		OK(GrB_Vector_set_INT32(iface, GxB_BITMAP, GxB_SPARSITY_CONTROL));
		OK(GrB_Vector_dup(&fr, rootv));
		OK(GrB_Vector_new(&delta, GrB_BOOL, N)); OK(GrB_Vector_new(&src, GrB_BOOL, N));
		OK(GrB_Vector_new(&t1, GrB_BOOL, N)); OK(GrB_Vector_new(&t2, GrB_BOOL, N));
		OK(GrB_Vector_new(&cand, GrB_BOOL, nmeth)); OK(GrB_Vector_new(&cm, GrB_BOOL, nmeth));
		OK(GrB_Vector_new(&call, GrB_BOOL, nmeth)); OK(GrB_Vector_new(&csig, GrB_BOOL, nmeth));
		OK(GrB_Vector_new(&cnam, GrB_BOOL, nmeth)); OK(GrB_Vector_new(&rv, GrB_BOOL, N));
		OK(GrB_Vector_new(&sig, GrB_BOOL, nsig)); OK(GrB_Vector_new(&named, GrB_BOOL, nstr + 1));
		OK(GrB_Vector_dup(&start, rootv)); OK(GrB_Vector_clear(start));
		double t0 = now();
		// the rule matrix that is a join: S(sig, m) = same name and same type ((SN MN') .* (ST MT0'))
		OK(GrB_Matrix_new(&S1, GrB_BOOL, nsig, nmeth));
		OK(GrB_Matrix_new(&S, GrB_BOOL, nsig, nmeth));
		OK(GrB_mxm(S1, NULL, NULL, sr, SN, MN, GrB_DESC_T1));
		OK(GrB_mxm(S, S1, NULL, sr, ST, MT0, GrB_DESC_ST1));
		OK(GrB_Matrix_wait(S, GrB_MATERIALIZE));
		double t0b = now();
		int reflect = 0;
		levels = ilevels = rounds = nvxm = 0;
		for (;;) {
			rounds++;
			// 1. the flood over plain edges: a masked BFS to a fixpoint; delta collects this round's new live set
			while (nv(fr)) {
				if (A0T) OK(GrB_mxv(fr, live, NULL, sr, A0T, fr, GrB_DESC_RSC)); // pull: rows of A0' not yet live
				else OK(GrB_vxm(fr, live, NULL, sr, fr, A0, GrB_DESC_RSC));      // push
				OK(GrB_Vector_assign_BOOL(live, fr, NULL, 1, GrB_ALL, N, GrB_DESC_S));
				levels++, nvxm++;
			}
			// delta: what this round made live (the method rule's additions and the BFS)
			OK(GrB_Vector_apply(delta, start, NULL, GrB_IDENTITY_BOOL, live, GrB_DESC_RSC));
			GrB_free(&start);
			OK(GrB_Vector_dup(&start, live));
			// 2. what the new live set says: useiface, signatures, names, reflect
			OK(GrB_vxm(t1, iface, NULL, sr, delta, A1, GrB_DESC_RSC));   // t1 = newly iface
			OK(GrB_Vector_assign_BOOL(iface, t1, NULL, 1, GrB_ALL, N, GrB_DESC_S));
			OK(GrB_vxm(sig, NULL, GrB_LOR, sr, delta, A2, NULL));
			OK(GrB_vxm(named, NULL, GrB_LOR, sr, delta, A3, NULL));
			nvxm += 3;
			if (!reflect) { OK(GrB_eWiseMult(rv, refl, NULL, GrB_LAND, delta, refl, GrB_DESC_RS)); reflect = nv(rv) > 0; }
			// 3. the iface closure: a type becomes a source once it is live, iface and a defined gotype
			OK(GrB_eWiseMult(src, gtdef, NULL, GrB_LAND, delta, iface, GrB_DESC_RS));
			OK(GrB_eWiseMult(t2, gtdef, NULL, GrB_LAND, t1, live, GrB_DESC_RS));
			OK(GrB_eWiseAdd(src, NULL, NULL, GrB_LOR, src, t2, NULL));
			while (nv(src)) {
				ilevels++;
				OK(GrB_vxm(cm, NULL, NULL, sr, src, M, NULL));                 // its methods are candidates
				OK(GrB_Vector_assign_BOOL(cand, cm, NULL, 1, GrB_ALL, nmeth, GrB_DESC_S));
				OK(GrB_vxm(t1, iface, NULL, sr, src, A0, GrB_DESC_RSC));       // iface along plain edges ...
				OK(GrB_eWiseMult(t1, NULL, NULL, GrB_FIRST_BOOL, t1, gtdef, NULL)); // ... to defined gotypes
				OK(GrB_vxm(t2, iface, NULL, sr, cm, Mtyp, GrB_DESC_RSC));      // iface(m.mtyp)
				OK(GrB_eWiseAdd(t1, NULL, NULL, GrB_LOR, t1, t2, NULL));
				OK(GrB_Vector_assign_BOOL(iface, t1, NULL, 1, GrB_ALL, N, GrB_DESC_S));
				OK(GrB_eWiseMult(src, gtdef, NULL, GrB_LAND, t1, live, GrB_DESC_RS));
				nvxm += 3;
			}
			// 4. the methods something can call: reflect & exported, or a live signature, or a called name
			OK(GrB_vxm(csig, NULL, NULL, sr, sig, S, NULL));
			OK(GrB_mxv(cnam, NULL, NULL, sr, MN, named, NULL));
			nvxm += 2;
			OK(GrB_eWiseAdd(csig, NULL, NULL, GrB_LOR, csig, cnam, NULL));
			if (reflect) OK(GrB_eWiseAdd(csig, NULL, NULL, GrB_LOR, csig, E, NULL));
			OK(GrB_eWiseMult(call, NULL, NULL, GrB_LAND, cand, csig, NULL));
			OK(GrB_Vector_apply(cand, call, NULL, GrB_IDENTITY_BOOL, cand, GrB_DESC_RSC)); // called: done
			OK(GrB_vxm(fr, live, NULL, sr, call, Mout, GrB_DESC_RSC));
			nvxm++;
			OK(GrB_Vector_assign_BOOL(live, fr, NULL, 1, GrB_ALL, N, GrB_DESC_S));
			if (nv(fr) == 0) break;
		}
		OK(GrB_Vector_wait(live, GrB_MATERIALIZE));
		double t1t = now();
		if (t1t - t0b < best) best = t1t - t0b;
		if (t1t - t0 < bestRules) bestRules = t1t - t0;
		sum += t1t - t0b;
		GrB_free(&S); GrB_free(&S1);
		if (rep < reps - 1) {
			GrB_Vector *vs[] = {&live, &iface, &fr, &delta, &start, &src, &t1, &t2, &cand, &call, &cm, &sig, &named, &csig, &cnam, &rv};
			for (size_t i = 0; i < sizeof vs / sizeof *vs; i++) GrB_free(vs[i]);
		}
	}
	ix nlive;
	u32 *list = indices32(live, &nlive);
	system("mkdir -p out");
	writeFile("out/p2_live.u32", list, 4 * nlive);
	double tEnd = now();
	printf("p2gb (%d threads, %s): flood best %.1f ms (mean %.1f), with rule join (S) best %.1f ms; %lu live, "
	       "%ld BFS levels + %ld iface levels in %ld rounds, %ld vxm/mxv calls\n",
	       nthreads(), dir, best * 1e3, sum / reps * 1e3, bestRules * 1e3, (unsigned long)nlive, levels, ilevels, rounds, nvxm);
	printf("setup: load+tuples %.1f ms, builds %.1f ms; process to here %.1f ms\n", (tLoad - tStart) * 1e3,
	       (tBuilt - tLoad) * 1e3, (tEnd - tStart) * 1e3);
	long bad = check32("live", list, nlive, "oracle/p2_live.u32");
	printf(bad ? "P2 FAIL\n" : "P2 OK\n");
	return bad != 0;
}
