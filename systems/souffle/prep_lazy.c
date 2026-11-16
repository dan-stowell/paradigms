// prep_lazy.c: the untimed setup of the lazy reach (reach_lazy.dl, ATTACKS 1), as ref/flat/p2lazy.c does it.
// p2_edge's kind-0 rows are the relocation edges followed by the funcdata aux edges; the lazy flood decodes the
// relocation edges itself, so this writes the rest (aux + kinds 1-3) as a CSR p2x_edge_{off,dst,kind}, into a fact
// dir of symlinks to every flat file:  prep_lazy DATA OUTDIR.  Also checks that the relocations the walk skips
// ("an R_METHODOFF (26) skips the next two") are all type 26 themselves, so "plain" is a filter on the type alone
// and the Datalog rule needs no walk.
#include "../../ref/flat/flat.h"

static int plain(u16 t) { return !(t & 32768) && t != 22 && t != 23 && t != 24 && t != 25 && t != 26 && t != 102; }

int main(int argc, char **argv) {
	if (argc < 3) { fprintf(stderr, "usage: prep_lazy DATA OUTDIR\n"); return 2; }
	flatDir = argv[1];
	const char *out = argv[2];
	char cmd[8192];
	snprintf(cmd, sizeof cmd, "mkdir -p '%s' && for f in '%s'/*; do ln -sfn \"$f\" '%s'/; done", out, flatDir, out);
	if (system(cmd)) return 2;
	u32 nsl = (u32)metaGet("nslots"), nb = (u32)metaGet("nbuiltin");
	const u8 *fl = FMAP(u8, "p2_flags.u8", NULL), *ek = FMAP(u8, "p2_edge_kind.u8", NULL);
	const u32 *eoff = FMAP(u32, "p2_edge_off.u32", NULL), *ed = FMAP(u32, "p2_edge_dst.u32", NULL);
	const u32 *canon = FMAP(u32, "oracle/p1_canon.u32", NULL), *builtin = FMAP(u32, "oracle/p1_builtin.u32", NULL);
	const u32 *sr = FMAP(u32, "slot_rel.u32", NULL), *so = FMAP(u32, "slot_obj.u32", NULL);
	const u32 *po = FMAP(u32, "obj_pkg_off.u32", NULL), *pb = FMAP(u32, "obj_pkg.u32", NULL);
	const u32 *rp = FMAP(u32, "rel_pkg.u32", NULL), *ri = FMAP(u32, "rel_idx.u32", NULL);
	const u16 *rt = FMAP(u16, "rel_type.u16", NULL);
	u32 *xoff = calloc((size_t)nsl + 2, 4), *xdst = malloc((size_t)eoff[nsl + 1] * 4);
	u8 *xk = malloc(eoff[nsl + 1]);
	size_t nx = 0, bad26 = 0;
	for (u32 g = 0; g <= nsl; g++) {
		xoff[g] = nx;
		for (u32 r = sr[g]; r < sr[g + 1]; r++) {
			if (rt[r] & 32768) continue;
			if (rt[r] == 26) {
				for (u32 k = 1; k <= 2; k++) bad26 += r + k >= sr[g + 1] || rt[r + k] != 26;
				r += 2;
			}
		}
		if (g == 0 || !(fl[g] & 1) || !(fl[g] & 32) || canon[g] != g) continue;
		size_t nplain = 0;
		u32 o = so[g], np = po[o + 1] - po[o] - 5;
		const u32 *bases = pb + po[o];
		for (u32 r = sr[g]; r < sr[g + 1]; r++) {
			if (rt[r] & 32768) continue;
			if (rt[r] == 26) { r += 2; continue; }
			if (!plain(rt[r])) continue;
			u32 sp = rp[r] - 2147483643u, t = sp < 5 ? np + sp : rp[r], x = 0;
			if (t < np + 5 && bases[t] != 0xffffffffu) x = sp == 1 ? (ri[r] < nb ? builtin[ri[r]] : 0) : bases[t] + ri[r];
			if (x && canon[x]) nplain++;
		}
		for (u32 e = eoff[g]; e < eoff[g + 1]; e++) {
			if (ek[e] == 0 && nplain) { nplain--; continue; }
			xdst[nx] = ed[e], xk[nx++] = ek[e];
		}
	}
	xoff[nsl + 1] = nx;
	if (bad26) { fprintf(stderr, "prep_lazy: %zu skipped relocations are not R_METHODOFF\n", bad26); return 1; }
	struct { const char *n; const void *p; size_t b; } fs[] = {
	    {"p2x_edge_off.u32", xoff, 4 * ((size_t)nsl + 2)}, {"p2x_edge_dst.u32", xdst, 4 * nx}, {"p2x_edge_kind.u8", xk, nx}};
	for (int i = 0; i < 3; i++) {
		snprintf(cmd, sizeof cmd, "%s/%s", out, fs[i].n);
		FILE *f = fopen(cmd, "wb");
		if (!f || fwrite(fs[i].p, 1, fs[i].b, f) != fs[i].b) { perror(cmd); return 2; }
		fclose(f);
	}
	printf("prep_lazy: %zu edges kept (aux + kinds 1-3) of %u\n", nx, eoff[nsl + 1]);
	return 0;
}
