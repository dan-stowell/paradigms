// p4.c: problem 4 (code generation by shape) from the flat files, plainly, on one core: every function body
// fc's tree walker wrote, rebuilt from byte templates and operands. Checks itself against oracle/p4_code.bin
// (byte for byte) and the relocations against oracle/p4_rel_* (as a sorted list).
//   ref/p4/p4 data/minio-p4 [out.bin]
#include "../flat/flat.h"

enum { encInt = 1, encRel32 = 2, encReloc = 3 };

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

	// ---- 1. the length of every item (a glue run, or one segment of a node's template), then where it goes
	double t0 = now();
	u64 *at = malloc(8 * ((size_t)nitem + 1));
	u64 pos = 0;
	for (u32 i = 0; i < nitem; i++) {
		at[i] = pos;
		u32 r = itemRef[i];
		if (itemKind[i] == 0) {
			pos += glueOff[r + 1] - glueOff[r];
		} else {
			u32 t = nodeTmpl[r], s = itemSeg[i], b = tmplSegOff[t];
			pos += tsegEnd[b + s] - (s ? tsegEnd[b + s - 1] : 0);
		}
	}
	at[nitem] = pos;
	double t1 = now();

	// ---- 2. emit: copy each item's bytes, patch the holes of node segments
	u8 *code = malloc(pos ? pos : 1);
	Rel *rel = malloc(sizeof(Rel) * ((size_t)nrel + 1));
	u64 nr = 0, nerr = 0;
	for (u32 f = 0; f < nfn; f++) {
		u64 fstart = at[fnItem[f]];
		for (u32 m = fnMarkOff[f]; m < fnMarkOff[f + 1]; m++)
			rel[nr++] = (Rel){fstart + markOff[m], markAdd[m], markSym[m], markType[m], 0};
		for (u32 i = fnItem[f]; i < fnItem[f + 1]; i++) {
			u32 r = itemRef[i];
			u8 *P = code + at[i];
			if (itemKind[i] == 0) {
				memcpy(P, glueBin + glueOff[r], glueOff[r + 1] - glueOff[r]);
				for (u32 k = glueRelOff[r]; k < glueRelOff[r + 1]; k++)
					rel[nr++] = (Rel){at[i] + grelOff[k], grelAdd[k], grelSym[k], grelType[k], grelSiz[k]};
				continue;
			}
			u32 t = nodeTmpl[r], s = itemSeg[i], b = tmplSegOff[t];
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
					if (nr < nrel) rel[nr] = (Rel){hp, opAux[o], (u32)v, holeRtype[h], holeW[h]};
					nr++;
					break;
				default: nerr++;
				}
			}
		}
	}
	double t2 = now();
	qsort(rel, nr < nrel ? nr : nrel, sizeof(Rel), cmpRel);
	double t3 = now();
	printf("p4 (1 core): lay out %.1f ms, emit %.1f ms (%llu bytes, %u items), sort relocations %.1f ms (%llu); %llu errors\n",
	       (t1 - t0) * 1e3, (t2 - t1) * 1e3, (unsigned long long)pos, nitem, (t3 - t2) * 1e3, (unsigned long long)nr,
	       (unsigned long long)nerr);

	// ---- check
	long bad = nerr != 0;
	size_t m;
	const u8 *want = FMAP(u8, "oracle/p4_code.bin", &m);
	const u64 *wfn = FMAP(u64, "oracle/p4_fn_off.u64", NULL);
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
	printf(bad ? "P4 FAIL\n" : "P4 OK\n");
	return bad != 0;
}
