// p3.c: driver for p3.fut. Loads the flat files, calls the entries, times them, checks against the oracles.
//   p3 DATADIR THREADS REPS CELL [nocheck]     CELL: 3a 3a_loop 3b 3b_bytes 3b_blk
#include "p3.h"
#include "drv.h"
LOADER(u8, uint8_t) LOADER(u32, uint32_t) LOADER(u64, uint64_t) LOADER(i64, int64_t)

int main(int argc, char **argv) {
	fut_init(argc, argv);
	const char *cell = argc > 4 ? argv[4] : "3a";
	int check = !(argc > 5 && strcmp(argv[5], "nocheck") == 0);
	u32 L = (u32)metaGet("p3_placed");
	long bad = 0;
	double tl0 = now();
	struct futhark_u8_1d *bucket = LOAD(u8, "p3_bucket.u8");
	if (strncmp(cell, "3a", 2) == 0) {
		int64_t nunits = (int64_t)metaGet("nunits");
		u32 minFunc = (u32)metaGet("minFunc");
		struct futhark_u32_1d *bstart = LOAD(u32, "p3_bucket_start.u32"), *bmin = LOAD(u32, "p3_bucket_minalign.u32");
		struct futhark_u8_1d *cls = LOAD(u8, "p3_class.u8"), *sbin = LOAD(u8, "p3_str.bin");
		struct futhark_u32_1d *size = LOAD(u32, "p3_size.u32"), *unit = LOAD(u32, "p3_unit.u32");
		struct futhark_u32_1d *sym = LOAD(u32, "p3_sym.u32"), *tlstr = LOAD(u32, "p3_tlstr.u32");
		struct futhark_u64_1d *soff = LOAD(u64, "p3_str.off.u64");
		SYNC();
		printf("load %.1f ms\n", (now() - tl0) * 1e3);
		// 3a: radix sort + segmented scan; 3a_loop: sequential cursor loop; 3a_cells, 3a_tl, 3a_radix: pieces (no check)
		typeof(futhark_entry_layout) *entry = futhark_entry_layout;
		if (strcmp(cell, "3a_loop") == 0) entry = futhark_entry_layout_loop;
		if (strcmp(cell, "3a_cells") == 0) entry = futhark_entry_piece_cells, check = 0;
		if (strcmp(cell, "3a_tl") == 0) entry = futhark_entry_piece_typelinks, check = 0;
		if (strcmp(cell, "3a_radix") == 0) entry = futhark_entry_piece_radix, check = 0;
		struct futhark_u32_1d *out = NULL;
		TIMEIT(cell, {
			if (out) futhark_free_u32_1d(ctx, out);
			fut_check(entry(ctx, &out, nunits, minFunc, bstart, bmin, bucket, cls, size, unit, sym, tlstr, soff, sbin), cell);
		});
		u32 *addr = malloc(4 * (size_t)L);
		futhark_values_u32_1d(ctx, out, addr);
		SYNC();
		if (check) bad += check32("addr", addr, L, "oracle/p3_addr.u32");
	} else {
		int64_t nimg = (int64_t)metaGet("image_bytes"), imageBase = (int64_t)metaGet("imageBase");
		// 3b runs on the oracle's addresses, as ref/flat/p3.c does, so it does not depend on 3a
		struct futhark_u32_1d *addr = LOAD(u32, "oracle/p3_addr.u32"), *all = LOAD(u32, "p3_addr_all.u32");
		struct futhark_u64_1d *doff = LOAD(u64, "p3_data.off.u64");
		struct futhark_u8_1d *dbin = LOAD(u8, "p3_data.bin");
		struct futhark_u32_1d *rstart = LOAD(u32, "p3_rel_start.u32"), *roff = LOAD(u32, "p3_rel_off.u32"), *rtgt = LOAD(u32, "p3_rel_tgt.u32");
		struct futhark_u8_1d *rsiz = LOAD(u8, "p3_rel_siz.u8"), *rkind = LOAD(u8, "p3_rel_kind.u8");
		struct futhark_i64_1d *radd = LOAD(i64, "p3_rel_add.i64");
		u32 sect[8] = {(u32)metaGet("lay_text"), (u32)metaGet("lay_rodata"), (u32)metaGet("lay_pclntab"), (u32)metaGet("lay_types"),
		               (u32)metaGet("lay_noptrdata"), (u32)metaGet("lay_data"), (u32)metaGet("lay_bss"), (u32)metaGet("lay_noptrbss")};
		struct futhark_u32_1d *fsect = futhark_new_u32_1d(ctx, sect, 8);
		u32 unreach = (u32)metaGet("addr_unreachableMethod"), mapinit = (u32)metaGet("addr_mapinitnoop");
		SYNC();
		printf("load %.1f ms\n", (now() - tl0) * 1e3);
		// 3b: chunked copy + patch; 3b_bytes: per-byte copy + patch; 3b_blk: blocked copy + patch; 3b_copy, 3b_patch: the halves alone (no check)
		typeof(futhark_entry_relocate) *entry = futhark_entry_relocate;
		if (strcmp(cell, "3b_bytes") == 0) entry = futhark_entry_relocate_bytes;
		if (strcmp(cell, "3b_blk") == 0) entry = futhark_entry_relocate_blocks;     // copy in 64 blocks (memcpy)
		if (strcmp(cell, "3b_blkcopy") == 0) entry = futhark_entry_copy_blocks_only, check = 0;
		if (strcmp(cell, "3b_copy") == 0) entry = futhark_entry_copy_only, check = 0;
		if (strcmp(cell, "3b_patch") == 0) entry = futhark_entry_patch_only, check = 0;
		struct futhark_opaque_tup2_arr1d_u8_i64 *out = NULL;
		// FUT_FRESH=1: empty the runtime's free list each rep, so the image is a fresh allocation whose page
		// faults are inside the timed region (as ref/flat/p3.c's calloc); else reps 2+ reuse the cached block
		int fresh = getenv("FUT_FRESH") != NULL;
		TIMEIT(cell, {
			if (out) futhark_free_opaque_tup2_arr1d_u8_i64(ctx, out);
			if (fresh) fut_check(futhark_context_clear_caches(ctx), "clear_caches");
			fut_check(entry(ctx, &out, nimg, imageBase, addr, bucket, doff, dbin, rstart, roff, rsiz, rkind, radd, rtgt, all, fsect,
			                unreach, mapinit), cell);
		});
		struct futhark_u8_1d *fimg;
		int64_t nerr;
		futhark_project_opaque_tup2_arr1d_u8_i64_0(ctx, &fimg, out);
		futhark_project_opaque_tup2_arr1d_u8_i64_1(ctx, &nerr, out);
		u8 *img = malloc(nimg);
		double tc = now();
		futhark_values_u8_1d(ctx, fimg, img);
		SYNC();
		printf("copy out %.1f ms, %ld errors\n", (now() - tc) * 1e3, (long)nerr);
		if (check) {
			const u8 *want = FMAP(u8, "oracle/p3_image.bin", NULL), *pb = FMAP(u8, "p3_bucket.u8", NULL);
			const u32 *oaddr = FMAP(u32, "oracle/p3_addr.u32", NULL), *psize = FMAP(u32, "p3_size.u32", NULL);
			long nbad = 0;
			for (u32 i = 0; i < L; i++) {
				if (pb[i] >= 13) continue;
				size_t o = oaddr[i] - imageBase;
				if (memcmp(img + o, want + o, psize[i]) != 0 && nbad++ < 5) printf("  symbol %u (bucket %u) differs\n", i, pb[i]);
			}
			printf("image: %ld placed symbols differ from oracle/p3_image.bin\n", nbad);
			bad += nbad + (nerr != 0);
		}
	}
	fut_report();
	if (check) printf(bad ? "P%s FAIL\n" : "P%s OK\n", cell);
	return bad != 0;
}
