// p4.c: driver for p4.fut. Loads the flat files, calls the entry, times it, checks against the oracles
// (the code byte for byte, the function offsets, the relocations as a sorted list, as ref/p4/p4.c).
//   p4 DATADIR THREADS REPS CELL [nocheck]     CELL: 4 (chunked scatter copy, CHUNK=32), 4g (gather copy), 4lay (lay out only)
#include "p4.h"
#include "drv.h"
LOADER(u8, uint8_t) LOADER(u16, uint16_t) LOADER(u32, uint32_t) LOADER(u64, uint64_t) LOADER(i64, int64_t)

#define TUP futhark_opaque_tup8_arr1d_u8_arr1d_i64_arr1d_i64_arr1d_u8_arr1d_u16_arr1d_u32_arr1d_i64_i64
#define PROJ(k) futhark_project_opaque_tup8_arr1d_u8_arr1d_i64_arr1d_i64_arr1d_u8_arr1d_u16_arr1d_u32_arr1d_i64_i64_##k

typedef struct {
	u64 off;
	i64 add;
	u32 sym;
	u16 type;
	u8 siz;
} Rel;
static int cmpRel(const void *a, const void *b) {
	const Rel *x = a, *y = b;
	if (x->off != y->off) return x->off < y->off ? -1 : 1;
	if (x->type != y->type) return x->type < y->type ? -1 : 1;
	if (x->sym != y->sym) return x->sym < y->sym ? -1 : 1;
	if (x->add != y->add) return x->add < y->add ? -1 : 1;
	return (int)x->siz - (int)y->siz;
}

int main(int argc, char **argv) {
	fut_init(argc, argv);
	const char *cell = argc > 4 ? argv[4] : "4";
	int check = !(argc > 5 && strcmp(argv[5], "nocheck") == 0);
	int64_t chunk = strcmp(cell, "4g") == 0 ? 0 : getenv("CHUNK") ? atol(getenv("CHUNK")) : 32;
	int64_t parts = getenv("PARTS") ? atol(getenv("PARTS")) : 7;
	if ((parts & 7) != 7) check = 0;
	u32 nfn = (u32)metaGet("nfn"), nrel = (u32)metaGet("nrel");
	u64 ncode = metaGet("code_bytes");
	double tl0 = now();
	struct futhark_u32_1d *fn_item = LOAD(u32, "fn_item.u32"), *fn_mark_off = LOAD(u32, "fn_mark_off.u32");
	struct futhark_u32_1d *mark_off = LOAD(u32, "mark_off.u32"), *mark_sym = LOAD(u32, "mark_sym.u32");
	struct futhark_u16_1d *mark_type = LOAD(u16, "mark_type.u16");
	struct futhark_i64_1d *mark_add = LOAD(i64, "mark_add.i64");
	struct futhark_u8_1d *item_kind = LOAD(u8, "item_kind.u8");
	struct futhark_u32_1d *item_ref = LOAD(u32, "item_ref.u32"), *item_seg = LOAD(u32, "item_seg.u32");
	struct futhark_u64_1d *glue_off = LOAD(u64, "glue.off.u64");
	struct futhark_u8_1d *glue_bin = LOAD(u8, "glue.bin");
	struct futhark_u32_1d *glue_rel_off = LOAD(u32, "glue_rel_off.u32"), *grel_off = LOAD(u32, "grel_off.u32");
	struct futhark_u8_1d *grel_siz = LOAD(u8, "grel_siz.u8");
	struct futhark_u16_1d *grel_type = LOAD(u16, "grel_type.u16");
	struct futhark_u32_1d *grel_sym = LOAD(u32, "grel_sym.u32");
	struct futhark_i64_1d *grel_add = LOAD(i64, "grel_add.i64");
	struct futhark_u32_1d *node_tmpl = LOAD(u32, "node_tmpl.u32"), *node_op = LOAD(u32, "node_op.u32");
	struct futhark_i64_1d *op_val = LOAD(i64, "op_val.i64"), *op_aux = LOAD(i64, "op_aux.i64");
	struct futhark_u64_1d *tmpl_off = LOAD(u64, "tmpl.off.u64");
	struct futhark_u8_1d *tmpl_bin = LOAD(u8, "tmpl.bin");
	struct futhark_u32_1d *tmpl_seg_off = LOAD(u32, "tmpl_seg_off.u32"), *tseg_end = LOAD(u32, "tseg_end.u32");
	struct futhark_u32_1d *tmpl_hole_off = LOAD(u32, "tmpl_hole_off.u32"), *hole_at = LOAD(u32, "hole_at.u32");
	struct futhark_u8_1d *hole_w = LOAD(u8, "hole_w.u8"), *hole_enc = LOAD(u8, "hole_enc.u8");
	struct futhark_u16_1d *hole_rtype = LOAD(u16, "hole_rtype.u16");
	SYNC();
	printf("load %.1f ms\n", (now() - tl0) * 1e3);

	if (strcmp(cell, "4lay") == 0) {
		struct futhark_i64_1d *at = NULL;
		TIMEIT(cell, {
			if (at) futhark_free_i64_1d(ctx, at);
			fut_check(futhark_entry_layout(ctx, &at, item_kind, item_ref, item_seg, glue_off, node_tmpl, tmpl_off, tmpl_seg_off, tseg_end), cell);
		});
		fut_report();
		return 0;
	}
	struct TUP *out = NULL;
	TIMEIT(cell, {
		if (out) futhark_free_opaque_tup8_arr1d_u8_arr1d_i64_arr1d_i64_arr1d_u8_arr1d_u16_arr1d_u32_arr1d_i64_i64(ctx, out);
		fut_check(futhark_entry_emit(ctx, &out, parts, chunk, fn_item, fn_mark_off, mark_off, mark_type, mark_sym, mark_add, item_kind,
		                             item_ref, item_seg, glue_off, glue_bin, glue_rel_off, grel_off, grel_siz, grel_type, grel_sym,
		                             grel_add, node_tmpl, node_op, op_val, op_aux, tmpl_off, tmpl_bin, tmpl_seg_off, tseg_end,
		                             tmpl_hole_off, hole_at, hole_w, hole_enc, hole_rtype),
		          cell);
	});
	fut_report();
	struct futhark_u8_1d *fcode, *frs;
	struct futhark_i64_1d *ffn, *fro, *fra;
	struct futhark_u16_1d *frt;
	struct futhark_u32_1d *fry;
	int64_t nerr;
	PROJ(0)(ctx, &fcode, out); PROJ(1)(ctx, &ffn, out); PROJ(2)(ctx, &fro, out); PROJ(3)(ctx, &frs, out);
	PROJ(4)(ctx, &frt, out); PROJ(5)(ctx, &fry, out); PROJ(6)(ctx, &fra, out); PROJ(7)(ctx, &nerr, out);
	int64_t pos = futhark_shape_u8_1d(ctx, fcode)[0], nr = futhark_shape_i64_1d(ctx, fro)[0];
	double tc = now();
	u8 *code = malloc(pos ? pos : 1);
	futhark_values_u8_1d(ctx, fcode, code);
	u64 *fnoff = malloc(8 * ((size_t)nfn + 1));
	futhark_values_i64_1d(ctx, ffn, (int64_t *)fnoff);
	SYNC();
	printf("copy out %.1f ms, %lld bytes, %lld relocations, %lld bad holes\n", (now() - tc) * 1e3, (long long)pos, (long long)nr,
	       (long long)nerr);
	if (!check) return 0;

	long bad = nerr != 0;
	size_t m;
	const u8 *want = FMAP(u8, "oracle/p4_code.bin", &m);
	const u64 *wfn = FMAP(u64, "oracle/p4_fn_off.u64", NULL);
	if ((u64)pos != m || (u64)pos != ncode || memcmp(code, want, m) != 0) printf("code differs\n"), bad++;
	if (memcmp(fnoff, wfn, 8 * ((size_t)nfn + 1)) != 0) printf("function offsets differ\n"), bad++;
	printf("code: %lld bytes in %u functions, %s oracle/p4_code.bin\n", (long long)pos, nfn, bad ? "NOT equal to" : "equal to");
	// relocations, as sorted lists
	u64 *ro = malloc(8 * nr); u8 *rs = malloc(nr); u16 *rt = malloc(2 * nr); u32 *ry = malloc(4 * nr); i64 *ra = malloc(8 * nr);
	futhark_values_i64_1d(ctx, fro, (int64_t *)ro); futhark_values_u8_1d(ctx, frs, rs); futhark_values_u16_1d(ctx, frt, rt);
	futhark_values_u32_1d(ctx, fry, ry); futhark_values_i64_1d(ctx, fra, ra);
	SYNC();
	Rel *got = malloc(sizeof(Rel) * (nr + 1)), *w = malloc(sizeof(Rel) * ((size_t)nrel + 1));
	for (int64_t i = 0; i < nr; i++) got[i] = (Rel){ro[i], ra[i], ry[i], rt[i], rs[i]};
	const u64 *wo = FMAP(u64, "oracle/p4_rel_off.u64", NULL);
	const u8 *wsiz = FMAP(u8, "oracle/p4_rel_siz.u8", NULL);
	const u16 *wtype = FMAP(u16, "oracle/p4_rel_type.u16", NULL);
	const u32 *wsym = FMAP(u32, "oracle/p4_rel_sym.u32", NULL);
	const i64 *wadd = FMAP(i64, "oracle/p4_rel_add.i64", NULL);
	for (u32 i = 0; i < nrel; i++) w[i] = (Rel){wo[i], wadd[i], wsym[i], wtype[i], wsiz[i]};
	qsort(got, nr, sizeof(Rel), cmpRel);
	qsort(w, nrel, sizeof(Rel), cmpRel);
	long rbad = nr != (int64_t)nrel;
	for (int64_t i = 0; i < nr && i < (int64_t)nrel; i++)
		if (cmpRel(&got[i], &w[i]) != 0 && rbad++ < 5) printf("  relocation %lld differs (offset %llu)\n", (long long)i, (unsigned long long)got[i].off);
	printf("relocations: %lld, oracle %u, %ld differ\n", (long long)nr, nrel, rbad);
	bad += rbad;
	printf(bad ? "P%s FAIL\n" : "P%s OK\n", cell);
	return bad != 0;
}
