// p2.c: driver for p2.fut. Loads the flat files, calls the entries, times them, checks against the oracle.
//   p2 DATADIR THREADS REPS CELL [nocheck]     CELL: 2 (flood over p2_edge_*), 2lazy / 2lazy_look (relocation edges decoded
//   from the rel_* columns inside the flood; the aux CSR of funcdata + kinds 1-3 is built once, untimed, by the
//   aux_edges entry, as ref/flat/p2lazy.c does). 2lazy takes 1a's canon/builtin from the oracle.
#include "p2.h"
#include "drv.h"
LOADER(u8, uint8_t) LOADER(u16, uint16_t) LOADER(u32, uint32_t)

int main(int argc, char **argv) {
	fut_init(argc, argv);
	const char *cell = argc > 4 ? argv[4] : "2";
	int check = !(argc > 5 && strcmp(argv[5], "nocheck") == 0);
	int64_t nids = (int64_t)metaGet("nids"), nstr = (int64_t)metaGet("p2_strings");
	double tl0 = now();
	struct futhark_u8_1d *flags = LOAD(u8, "p2_flags.u8"), *mexp = LOAD(u8, "p2_meth_exported.u8");
	struct futhark_u32_1d *roots = LOAD(u32, "p2_roots.u32"), *sname = LOAD(u32, "p2_sig_name.u32"), *stype = LOAD(u32, "p2_sig_type.u32");
	struct futhark_u32_1d *moff = LOAD(u32, "p2_meth_off.u32"), *mname = LOAD(u32, "p2_meth_name.u32"), *mtyp = LOAD(u32, "p2_meth_mtyp.u32");
	struct futhark_u32_1d *mifn = LOAD(u32, "p2_meth_ifn.u32"), *mtfn = LOAD(u32, "p2_meth_tfn.u32");
	struct futhark_u32_1d *eoff = LOAD(u32, "p2_edge_off.u32"), *edst = LOAD(u32, "p2_edge_dst.u32");
	struct futhark_u8_1d *ekind = LOAD(u8, "p2_edge_kind.u8");
	struct futhark_u32_1d *out = NULL;
	if (strcmp(cell, "2") == 0) {
		SYNC();
		printf("load %.1f ms\n", (now() - tl0) * 1e3);
		int64_t levels;
		fut_check(futhark_entry_reach_levels(ctx, &levels, nids, flags, eoff, edst, ekind, roots, sname, stype, moff, mname, mtyp, mifn, mtfn, mexp, nstr), "levels");
		SYNC();
		printf("%lld levels\n", (long long)levels);
		TIMEIT(cell, {
			if (out) futhark_free_u32_1d(ctx, out);
			fut_check(futhark_entry_reach(ctx, &out, nids, flags, eoff, edst, ekind, roots, sname, stype, moff, mname, mtyp, mifn, mtfn, mexp, nstr), cell);
		});
	} else if (strcmp(cell, "2hyb") == 0) {
		// the hybrid flood (ATTACKS.md 3): P2LO (frontier width below which a level is drained sequentially,
		// default 4096), P2HI (the drain hands back once its stack exceeds this, default 4 x P2LO), P2BUDGET
		// (or after this many pops, default unbounded)
		const char *e;
		int64_t lo = (e = getenv("P2LO")) ? atoll(e) : 4096;
		int64_t hi = (e = getenv("P2HI")) ? atoll(e) : 4 * lo;
		int64_t budget = (e = getenv("P2BUDGET")) ? atoll(e) : INT64_MAX;
		SYNC();
		printf("load %.1f ms\n", (now() - tl0) * 1e3);
		struct futhark_i64_1d *st;
		fut_check(futhark_entry_hybrid_stats(ctx, &st, nids, flags, eoff, edst, ekind, roots, sname, stype, moff, mname, mtyp, mifn, mtfn, mexp, nstr, lo, hi, budget), "stats");
		int64_t s[6];
		futhark_values_i64_1d(ctx, st, s);
		SYNC();
		printf("hybrid lo %lld hi %lld budget %lld: %lld data-parallel levels, %lld sequential drains, %lld expanded in levels, %lld pops in drains\n",
		       (long long)lo, (long long)hi, (long long)budget, (long long)s[0], (long long)s[1], (long long)s[2], (long long)s[3]);
		TIMEIT(cell, {
			if (out) futhark_free_u32_1d(ctx, out);
			fut_check(futhark_entry_reach_hybrid(ctx, &out, nids, flags, eoff, edst, ekind, roots, sname, stype, moff, mname, mtyp, mifn, mtfn, mexp, nstr, lo, hi, budget), cell);
		});
	} else {
		struct futhark_u32_1d *canon = LOAD(u32, "oracle/p1_canon.u32"), *builtin = LOAD(u32, "oracle/p1_builtin.u32");
		struct futhark_u32_1d *srel = LOAD(u32, "slot_rel.u32"), *sobj = LOAD(u32, "slot_obj.u32");
		struct futhark_u32_1d *poff = LOAD(u32, "obj_pkg_off.u32"), *pkg = LOAD(u32, "obj_pkg.u32");
		struct futhark_u32_1d *rpkg = LOAD(u32, "rel_pkg.u32"), *ridx = LOAD(u32, "rel_idx.u32");
		struct futhark_u16_1d *rtype = LOAD(u16, "rel_type.u16");
		SYNC();
		printf("load %.1f ms\n", (now() - tl0) * 1e3);
		// setup, untimed: the aux rows (needs the decoded targets once, from 1b's oracle)
		struct futhark_u32_1d *rtgt = LOAD(u32, "oracle/p1_reltarget.u32");
		struct futhark_opaque_tup2_arr1d_u32_arr1d_u8 *aux;
		double ts = now();
		fut_check(futhark_entry_aux_edges(ctx, &aux, flags, canon, srel, rtype, rtgt, eoff, edst, ekind), "aux");
		struct futhark_u32_1d *xod;
		struct futhark_u8_1d *xk;
		futhark_project_opaque_tup2_arr1d_u32_arr1d_u8_0(ctx, &xod, aux);
		futhark_project_opaque_tup2_arr1d_u32_arr1d_u8_1(ctx, &xk, aux);
		SYNC();
		printf("setup (aux rows, untimed) %.1f ms, %lld aux edges\n", (now() - ts) * 1e3, (long long)futhark_shape_u8_1d(ctx, xk)[0]);
		TIMEIT(cell, {
			if (out) futhark_free_u32_1d(ctx, out);
			fut_check(futhark_entry_reach_lazy(ctx, &out, nids, flags, canon, builtin, srel, sobj, poff, pkg, rpkg, ridx, rtype, xod, xk,
			                                   roots, sname, stype, moff, mname, mtyp, mifn, mtfn, mexp, nstr, strcmp(cell, "2lazy_look") == 0), cell);
		});
	}
	size_t n = (size_t)futhark_shape_u32_1d(ctx, out)[0];
	u32 *live = malloc(4 * n + 4);
	futhark_values_u32_1d(ctx, out, live);
	SYNC();
	printf("%zu live\n", n);
	long bad = check ? check32("live", live, n, "oracle/p2_live.u32") : 0;
	fut_report();
	if (check) printf(bad ? "P%s FAIL\n" : "P%s OK\n", cell);
	return bad != 0;
}
