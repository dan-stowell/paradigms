// p1.c: driver for p1.fut. Loads the flat files, calls the entries, times them, checks against the oracles.
//   p1 DATADIR THREADS REPS CELL [nocheck]     CELL: 1a 1b 1c
// 1b and 1c take their inputs from 1a's oracle (as ref/flat/p3.c's 3b takes 3a's), so each cell stands alone.
#include "p1.h"
#include "drv.h"
LOADER(u8, uint8_t) LOADER(u32, uint32_t) LOADER(u64, uint64_t)

int main(int argc, char **argv) {
	fut_init(argc, argv);
	const char *cell = argc > 4 ? argv[4] : "1a";
	int check = !(argc > 5 && strcmp(argv[5], "nocheck") == 0);
	size_t N = (size_t)metaGet("nslots") + 1, nrel = (size_t)metaGet("nrel"), nb = (size_t)metaGet("nbuiltin");
	long bad = 0;
	double tl0 = now();
	if (strcmp(cell, "1a") == 0) {
		struct futhark_u8_1d *kind = LOAD(u8, "p1_kind.u8"), *flags = LOAD(u8, "p1_flags.u8");
		struct futhark_u64_1d *key = LOAD(u64, "p1_namekey.u64x2"), *want = LOAD(u64, "p1_want.u64x2"), *bkey = LOAD(u64, "p1_builtin.u64x2");
		struct futhark_u32_1d *rank = LOAD(u32, "p1_rank.u32");
		SYNC();
		printf("load %.1f ms\n", (now() - tl0) * 1e3);
		struct futhark_opaque_tup2_arr1d_u32_arr1d_u32 *out = NULL;
		TIMEIT(cell, {
			if (out) futhark_free_opaque_tup2_arr1d_u32_arr1d_u32(ctx, out);
			fut_check(futhark_entry_resolve(ctx, &out, kind, flags, key, rank, want, bkey), cell);
		});
		struct futhark_u32_1d *fc, *fb;
		futhark_project_opaque_tup2_arr1d_u32_arr1d_u32_0(ctx, &fc, out);
		futhark_project_opaque_tup2_arr1d_u32_arr1d_u32_1(ctx, &fb, out);
		u32 *canon = malloc(4 * N), *builtin = malloc(4 * nb);
		futhark_values_u32_1d(ctx, fc, canon);
		futhark_values_u32_1d(ctx, fb, builtin);
		SYNC();
		if (check) bad += check32("canon", canon, N, "oracle/p1_canon.u32") + check32("builtin", builtin, nb, "oracle/p1_builtin.u32");
	} else if (strcmp(cell, "1b") == 0) {
		struct futhark_u32_1d *canon = LOAD(u32, "oracle/p1_canon.u32"), *builtin = LOAD(u32, "oracle/p1_builtin.u32");
		struct futhark_u32_1d *srel = LOAD(u32, "slot_rel.u32"), *sobj = LOAD(u32, "slot_obj.u32");
		struct futhark_u32_1d *poff = LOAD(u32, "obj_pkg_off.u32"), *pkg = LOAD(u32, "obj_pkg.u32");
		struct futhark_u32_1d *rpkg = LOAD(u32, "rel_pkg.u32"), *ridx = LOAD(u32, "rel_idx.u32");
		SYNC();
		printf("load %.1f ms\n", (now() - tl0) * 1e3);
		struct futhark_u32_1d *out = NULL;
		TIMEIT(cell, {
			if (out) futhark_free_u32_1d(ctx, out);
			fut_check(futhark_entry_reltargets(ctx, &out, canon, builtin, srel, sobj, poff, pkg, rpkg, ridx), cell);
		});
		u32 *tgt = malloc(4 * nrel);
		futhark_values_u32_1d(ctx, out, tgt);
		SYNC();
		if (check) bad += check32("reltarget", tgt, nrel, "oracle/p1_reltarget.u32");
	} else {
		struct futhark_u32_1d *canon = LOAD(u32, "oracle/p1_canon.u32");
		struct futhark_u8_1d *kind = LOAD(u8, "p1_kind.u8"), *mark = LOAD(u8, "p1c_mark.u8");
		struct futhark_u64_1d *hkey = LOAD(u64, "p1_hashkey.u64x2");
		SYNC();
		printf("load %.1f ms\n", (now() - tl0) * 1e3);
		struct futhark_u32_1d *out = NULL;
		TIMEIT(cell, {
			if (out) futhark_free_u32_1d(ctx, out);
			fut_check(futhark_entry_content_join(ctx, &out, canon, kind, hkey, mark), cell);
		});
		u32 *c2 = malloc(4 * N);
		futhark_values_u32_1d(ctx, out, c2);
		SYNC();
		if (check) bad += check32("canon after 1c", c2, N, "oracle/p1c_canon.u32");
	}
	fut_report();
	if (check) printf(bad ? "P%s FAIL\n" : "P%s OK\n", cell);
	return bad != 0;
}
