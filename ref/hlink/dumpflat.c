// dumpflat.c (paradigms repo; not part of bottom-up's hlink): -dumpflat DIR writes the flat files that every
// system of the paradigm grid reads, and the oracles, from hlink's own state at the phase boundaries.
// data/README.md (written by ref/flat/readme.sh) says what each file is. Snapshots: after resolve (canon and
// flags, before the content-hash join), at the end of the flood (the marks, before the content-hash join);
// everything else is read at the end of the link.

static u32 *flatCanon1; // canon after resolve (content-addressed copies are still themselves)
static u8 *flatFL1;     // FL after resolve
static u8 *flatFL2;     // FL at the end of the flood

static void flatSnapResolve(void) {
	flatCanon1 = malloc((size_t)nids * 4);
	memcpy(flatCanon1, canon, (size_t)nids * 4);
	flatFL1 = malloc(nids);
	for (u32 g = 0; g < nids; g++) flatFL1[g] = FL(g);
}
static void flatSnapFlood(void) {
	flatFL2 = malloc(nids);
	for (u32 g = 0; g < nids; g++) flatFL2[g] = FL(g);
}

static const char *flatDir;
static void wfile(const char *name, const void *p, size_t n) {
	char path[4096];
	snprintf(path, sizeof path, "%s/%s", flatDir, name);
	FILE *f = fopen(path, "wb");
	if (!f) fatalf("dumpflat: %s: %s", path, strerror(errno));
	if (n && fwrite(p, 1, n, f) != n) fatalf("dumpflat: write %s", path);
	fclose(f);
}
#define WVEC(name, v) wfile(name, (v).a, (v).n * sizeof *(v).a)
static FILE *metaF;
static void meta(const char *k, unsigned long long v) { fprintf(metaF, "%s %llu\n", k, v); }

// a quiet oref: 0 for anything hlink would report as an error
static u32 orefQ(const object *o, u32 pkg, u32 idx) {
	u32 sp = pkg - PkgIdxSelf;
	u32 t = sp < 5 ? o->npkg + sp : pkg;
	if (t >= o->npkg + 5) return 0;
	u32 b = o->pkgBase[t];
	if (b == badBase) return 0;
	if (sp == 1) return idx < NBUILTIN ? builtins[idx] : 0;
	return b + idx;
}
static inline u32 c1(u32 g) { return g < nids ? flatCanon1[g] : 0; }

// interned strings
typedef VEC(u64) vecu64;
typedef struct {
	strmap m;
	vecu8 bin;
	vecu64 off;
} strtab;
static void stInit(strtab *t) {
	smInit(&t->m, 1 << 16);
	u64 z = 0;
	VPUSH(t->off, z); // string 0 is ""
	u64 e = 0;
	VPUSH(t->off, e);
	bool added;
	smPut(&t->m, (str){"", 0}, 0, &added);
}
static u32 stId(strtab *t, str s) {
	bool added;
	long *v = smPut(&t->m, s, (long)(t->off.n - 1), &added);
	if (added) {
		for (u32 i = 0; i < s.n; i++) VPUSH(t->bin, (u8)s.p[i]);
		u64 e = t->bin.n;
		VPUSH(t->off, e);
	}
	return (u32)*v;
}
static void stWrite(strtab *t, const char *prefix) {
	char a[256];
	snprintf(a, sizeof a, "%s.off.u64", prefix);
	WVEC(a, t->off);
	snprintf(a, sizeof a, "%s.bin", prefix);
	WVEC(a, t->bin);
}

static void dumpFlat(void) {
	flatDir = flagDumpFlat;
	mkdir(flatDir, 0777);
	char mp[4096];
	snprintf(mp, sizeof mp, "%s/meta.txt", flatDir);
	metaF = fopen(mp, "w");
	u32 N = nslots;
	meta("nslots", N);
	meta("nids", nids);
	meta("nobjs", (u64)nobjs);
	meta("nunits", (u64)nunits);
	meta("imageBase", imageBase);

	// ---------------------------------------------------------------- common: slots
	u32 *slotObj = calloc(N + 1, 4);
	vecu64 nameOff = {0};
	vecu8 nameBin = {0};
	u64 z = 0;
	VPUSH(nameOff, z); // slot 0
	for (int i = 0; i < nobjs; i++) {
		object *o = objs[i];
		for (u32 j = 0; j < o->total; j++) {
			u32 g = o->base + j;
			slotObj[g] = (u32)i;
			str n = symNameOf(o, osym(o, j));
			for (u32 k = 0; k < n.n; k++) VPUSH(nameBin, (u8)n.p[k]);
			u64 e = nameBin.n;
			VPUSH(nameOff, e);
		}
	}
	u64 e0 = nameBin.n;
	VPUSH(nameOff, e0);
	wfile("slot_obj.u32", slotObj, (size_t)(N + 1) * 4);
	WVEC("name.off.u64", nameOff);
	WVEC("name.bin", nameBin);

	// ---------------------------------------------------------------- P1: resolve
	u8 *kind = calloc(N + 1, 1), *p1f = calloc(N + 1, 1), *hmark = calloc(N + 1, 1);
	u64 *nkey = calloc(2 * (size_t)(N + 1), 8), *hkey = calloc(2 * (size_t)(N + 1), 8);
	u32 *rank = calloc(N + 1, 4);
	u32 *slotRel = calloc(N + 2, 4);
	VEC(i32) relOff = {0};
	vecu8 relSiz = {0};
	VEC(u16) relType = {0};
	VEC(i64) relAdd = {0};
	vecu32 relPkg = {0}, relIdx = {0}, relTgt = {0};
	u32 *objBase = calloc((size_t)nobjs, 4), *objPkgOff = calloc((size_t)nobjs + 1, 4);
	vecu32 objPkg = {0};
	long nNamed = 0, nPkgCand = 0;
	for (int i = 0; i < nobjs; i++) {
		object *o = objs[i];
		objBase[i] = o->base;
		objPkgOff[i] = (u32)objPkg.n;
		for (u32 t = 0; t < o->npkg + 5; t++) VPUSH(objPkg, o->pkgBase[t]);
		u32 h64Lo = o->ndef, hLo = o->ndef + o->nh64, npLo = hLo + o->nh;
		const u8 *h64 = o->b + o->off[BlkHash64], *hh = o->b + o->off[BlkHash];
		for (u32 j = 0; j < o->total; j++) {
			u32 g = o->base + j;
			const u8 *s = osym(o, j);
			slotRel[g] = (u32)relOff.n;
			u8 k = j < h64Lo ? 1 : j < hLo ? 2 : j < npLo ? 3 : j < o->nalldef ? 4 : 5;
			kind[g] = k;
			bool dwarf = isDwarfKind(symType(s));
			if (symUsedInIface(s)) p1f[g] |= 1;
			if (k == 5 && dwarf) p1f[g] |= 2;
			bool named = false;
			if (k == 1 || k == 4) named = !dwarf && symABI(s) != SymABIstatic && symNameLen(s) != 0;
			if (k == 5) named = !dwarf;
			if (named) {
				key128 kk = nameKey128(o->b + ld32(s + 4), symNameLen(s), symABI(s));
				nkey[2 * (size_t)g] = kk.k1;
				nkey[2 * (size_t)g + 1] = kk.k2;
				if (k == 1) nPkgCand++;
				else nNamed++;
			}
			if (k == 1 || k == 4) rank[g] = rankOf(o, j, s) & ~rankIface;
			if (k == 2) {
				hkey[2 * (size_t)g] = ld64(h64 + (size_t)(j - h64Lo) * 8);
				hkey[2 * (size_t)g + 1] = symSiz(s);
			} else if (k == 3) {
				const u8 *h = hh + (size_t)(j - hLo) * HashSize;
				hkey[2 * (size_t)g] = ld64(h);
				hkey[2 * (size_t)g + 1] = ld32(h + 8);
			}
			if (k == 2 || k == 3) {
				u8 f = flatFL2[g];
				hmark[g] = (f & hashedFlags) ? 1 : (f & flRef) ? 2 : 0;
			}
			if (j < o->nalldef) {
				u32 nr;
				relocp rs = oRelocs(o, j, &nr);
				for (u32 r = 0; r < nr; r++) {
					relocp rl = RELOC(rs, r);
					VPUSH(relOff, rOff(rl));
					VPUSH(relSiz, rSiz(rl));
					VPUSH(relType, rType(rl));
					VPUSH(relAdd, rAdd(rl));
					VPUSH(relPkg, rPkg(rl));
					VPUSH(relIdx, rIdx(rl));
					u32 tg = orefQ(o, rPkg(rl), rIdx(rl));
					VPUSH(relTgt, c1(tg));
				}
			}
		}
	}
	objPkgOff[nobjs] = (u32)objPkg.n;
	slotRel[N + 1] = (u32)relOff.n;
	slotRel[0] = 0;
	meta("nrel", relOff.n);
	meta("p1_named", (u64)nNamed);
	meta("p1_pkgdef_candidates", (u64)nPkgCand);
	wfile("p1_kind.u8", kind, N + 1);
	wfile("p1_flags.u8", p1f, N + 1);
	wfile("p1_namekey.u64x2", nkey, 16 * (size_t)(N + 1));
	wfile("p1_rank.u32", rank, 4 * (size_t)(N + 1));
	wfile("p1_hashkey.u64x2", hkey, 16 * (size_t)(N + 1));
	wfile("p1c_mark.u8", hmark, N + 1);
	wfile("slot_rel.u32", slotRel, 4 * (size_t)(N + 2));
	WVEC("rel_off.i32", relOff);
	WVEC("rel_siz.u8", relSiz);
	WVEC("rel_type.u16", relType);
	WVEC("rel_add.i64", relAdd);
	WVEC("rel_pkg.u32", relPkg);
	WVEC("rel_idx.u32", relIdx);
	wfile("obj_base.u32", objBase, 4 * (size_t)nobjs);
	wfile("obj_pkg_off.u32", objPkgOff, 4 * ((size_t)nobjs + 1));
	WVEC("obj_pkg.u32", objPkg);
	{
		// the names the linker joins without a slot of its own (resolve's wantName calls, in order)
		VEC(u64) want = {0};
#define WANT(nm, abi)                          \
	do {                                       \
		key128 kk = keyOfStr(cstr(nm), (abi)); \
		VPUSH(want, kk.k1);                    \
		VPUSH(want, kk.k2);                    \
	} while (0)
		for (int i = 0; i < NBUILTIN; i++) WANT(builtinNames[i].name, (u16)builtinNames[i].abi);
		for (size_t i = 0; i < sizeof linkerNames / sizeof linkerNames[0]; i++) {
			WANT(linkerNames[i], 0);
			WANT(linkerNames[i], 1);
		}
		for (size_t i = 0; i < strdata.n; i++) WANT(strdata.a[i].name, 0);
		WANT(flagEntry, 0);
		WANT(flagEntry, 1);
		WVEC("p1_want.u64x2", want);
		VEC(u64) bk = {0};
		for (int i = 0; i < NBUILTIN; i++) {
			key128 kk = keyOfStr(cstr(builtinNames[i].name), (u16)builtinNames[i].abi);
			VPUSH(bk, kk.k1);
			VPUSH(bk, kk.k2);
		}
		WVEC("p1_builtin.u64x2", bk);
		meta("nbuiltin", NBUILTIN);
#undef WANT
	}
	mkdir((snprintf(mp, sizeof mp, "%s/oracle", flatDir), mp), 0777);
	wfile("oracle/p1_canon.u32", flatCanon1, 4 * (size_t)(N + 1));
	WVEC("oracle/p1_reltarget.u32", relTgt);
	wfile("oracle/p1c_canon.u32", canon, 4 * (size_t)(N + 1));
	{
		u32 *bt = calloc(NBUILTIN, 4);
		memcpy(bt, builtins, 4 * NBUILTIN);
		wfile("oracle/p1_builtin.u32", bt, 4 * NBUILTIN);
	}

	// ---------------------------------------------------------------- P2: reach
	strtab S;
	memset(&S, 0, sizeof S);
	stInit(&S);
	u8 *nf = calloc(N + 1, 1);
	u32 *eoff = calloc(N + 2, 4), *moff = calloc(N + 2, 4);
	vecu32 edst = {0}, sigName = {0}, sigType = {0}, mName = {0}, mTyp = {0}, mIfn = {0}, mTfn = {0};
	vecu8 ekind = {0}, mExp = {0};
	u32map sigs; // (name id, type) -> sig id: key = index+1 in a side table, found by linear probing on a hash
	umInit(&sigs, 1 << 12);
	long nMethErr = 0;
	vecu32 tmp = {0};
	struct timespec p2t0, p2t1; // decode-to-CSR cost, for ATTACKS 1 (one core, eager: every canonical definition with relocations)
	clock_gettime(CLOCK_MONOTONIC, &p2t0);
	for (u32 g = 1; g <= N; g++) {
		u8 f1 = flatFL1[g];
		u8 sf = SF(g);
		bool def = (f1 & stUndef) == 0 && kind[g] != 5; // a canonical reference is an undefined symbol
		u8 fl = (def ? 1 : 0) | (sf & sfText ? 2 : 0) | (sf & sfReflect ? 4 : 0) | (sf & sfGoType ? 8 : 0) | (f1 & stUsedInIface ? 16 : 0) |
		        (sf & (sfHasRel | sfText | sfReflect) ? 32 : 0);
		if (kind[g] == 5) fl = (f1 & stUsedInIface ? 16 : 0); // references carry no record flags
		nf[g] = fl;
		eoff[g] = (u32)edst.n;
		moff[g] = (u32)mName.n;
		if (!def || flatCanon1[g] != g || (fl & 32) == 0) continue;
		u32 j;
		object *o = objOf(g, &j);
		if (j >= o->nalldef) continue;
		u32 nr;
		relocp rs = oRelocs(o, j, &nr);
		tmp.n = 0;
#define EMIT(k, d)                                     \
	do {                                               \
		u8 k_ = (k);                                   \
		u32 d_ = (d);                                  \
		VPUSH(ekind, k_);                              \
		VPUSH(edst, d_);                               \
	} while (0)
		for (u32 i = 0; i < nr; i++) {
			relocp rl = RELOC(rs, i);
			u16 t = rType(rl);
			if (t & R_WEAK) continue;
			switch (t) {
			case R_METHODOFF:
				if (i + 2 >= nr) {
					nMethErr++;
					i = nr;
					continue;
				}
				VPUSH(tmp, i);
				i += 2;
				continue;
			case R_USETYPE:
			case R_INITORDER: continue;
			case R_USEIFACE: {
				u32 rsym = c1(orefQ(o, rPkg(rl), rIdx(rl)));
				if (!isDef(rsym)) continue;
				if (SF(rsym) & sfItab) {
					u32 j2, nr2;
					object *o2 = objOf(rsym, &j2);
					relocp rs2 = oRelocs(o2, j2, &nr2);
					relocp rl2 = relocAt(rs2, nr2, 8);
					if (!rl2) continue;
					rsym = c1(orefQ(o2, rPkg(rl2), rIdx(rl2)));
					if (!isDef(rsym)) continue;
				}
				EMIT(1, rsym);
				continue;
			}
			case R_USEIFACEMETHOD: {
				u32 rsym = c1(orefQ(o, rPkg(rl), rIdx(rl)));
				if (isDef(rsym)) {
					u32 j2, nr2;
					object *o2 = objOf(rsym, &j2);
					relocp rr = oRelocs(o2, j2, &nr2);
					i32 off = (i32)rAdd(rl);
					u32 nm = stId(&S, nameAt(o2, rr, nr2, off));
					relocp rl2 = relocAt(rr, nr2, off + 4);
					u32 ty = rl2 ? c1(orefQ(o2, rPkg(rl2), rIdx(rl2))) : 0;
					// sig id: search the side table
					u32 key = (u32)(mix64((u64)nm << 32 | ty) | 1);
					int *v;
					u32 id = 0;
					for (;; key = key * 2654435761u | 1) {
						v = umFind(&sigs, key);
						if (!v) break;
						if (sigName.a[*v] == nm && sigType.a[*v] == ty) {
							id = (u32)*v;
							break;
						}
					}
					if (!v) {
						id = (u32)sigName.n;
						VPUSH(sigName, nm);
						VPUSH(sigType, ty);
						umPut(&sigs, key, (int)id);
					}
					EMIT(2, id);
				}
				continue;
			}
			case R_USENAMEDMETHOD: {
				bytes d = dataOf(c1(orefQ(o, rPkg(rl), rIdx(rl))));
				EMIT(3, stId(&S, (str){(const char *)d.p, d.n}));
				continue;
			}
			}
			u32 rsym = c1(orefQ(o, rPkg(rl), rIdx(rl)));
			if (rsym == 0) continue;
			EMIT(0, rsym);
		}
		if (sf & sfText) {
			u32 na;
			auxp as = oAuxs(o, j, &na);
			for (u32 i = 0; i < na; i++) {
				auxp a = AUX(as, i);
				if (aType(a) == AuxFuncdata) {
					u32 cc = c1(orefQ(o, aPkg(a), aIdx(a)));
					if (cc != 0) EMIT(0, cc);
				}
			}
		}
		if (tmp.n != 0) {
			bytes p = oData(o, j);
			if (p.n <= ABI_TFlagOff8 || (p.p[ABI_TFlagOff8] & ABI_TFlagUncommon) == 0) {
				nMethErr++;
				continue;
			}
			u8 tk = typeKind(p.p);
			int off = tk < ABI_NKIND ? abiRTypeSize8[tk] : 48;
			u32 mcount = ld16(p.p + off + 4);
			u32 mo = ld32(p.p + off + 8);
			if (mcount != tmp.n) {
				nMethErr++;
				continue;
			}
			for (size_t i = 0; i < tmp.n; i++) {
				u32 ri = tmp.a[i];
				i32 at = (i32)(off + mo + 16 * i);
				str name;
				if (ri > 0 && rOff(RELOC(rs, ri - 1)) == at) {
					relocp rp = RELOC(rs, ri - 1);
					name = nameData(dataOf(c1(orefQ(o, rPkg(rp), rIdx(rp)))));
				} else {
					name = nameAt(o, rs, nr, at);
				}
				u32 tg[3];
				for (int d = 0; d < 3; d++) {
					relocp rd = RELOC(rs, ri + d);
					tg[d] = c1(orefQ(o, rPkg(rd), rIdx(rd)));
				}
				u32 nm = stId(&S, name);
				u8 ex = isExported(name);
				VPUSH(mName, nm);
				VPUSH(mTyp, tg[0]);
				VPUSH(mIfn, tg[1]);
				VPUSH(mTfn, tg[2]);
				VPUSH(mExp, ex);
			}
		}
#undef EMIT
	}
	clock_gettime(CLOCK_MONOTONIC, &p2t1);
	fprintf(stderr, "dumpflat: P2 decode to CSR (1 core): %.1f ms, %zu edges, %zu methods\n",
	        (p2t1.tv_sec - p2t0.tv_sec) * 1e3 + (p2t1.tv_nsec - p2t0.tv_nsec) / 1e6, edst.n, mName.n);
	eoff[N + 1] = (u32)edst.n;
	moff[N + 1] = (u32)mName.n;
	eoff[0] = moff[0] = 0;
	meta("p2_edges", edst.n);
	meta("p2_methods", mName.n);
	meta("p2_sigs", sigName.n);
	meta("p2_strings", S.off.n - 1);
	meta("p2_method_errors", (u64)nMethErr);
	wfile("p2_flags.u8", nf, N + 1);
	wfile("p2_edge_off.u32", eoff, 4 * (size_t)(N + 2));
	WVEC("p2_edge_dst.u32", edst);
	WVEC("p2_edge_kind.u8", ekind);
	WVEC("p2_sig_name.u32", sigName);
	WVEC("p2_sig_type.u32", sigType);
	wfile("p2_meth_off.u32", moff, 4 * (size_t)(N + 2));
	WVEC("p2_meth_name.u32", mName);
	WVEC("p2_meth_mtyp.u32", mTyp);
	WVEC("p2_meth_ifn.u32", mIfn);
	WVEC("p2_meth_tfn.u32", mTfn);
	WVEC("p2_meth_exported.u8", mExp);
	stWrite(&S, "p2_str");
	{
		vecu32 roots = {0};
#define R(c)                                \
	do {                                    \
		u32 c_ = (c);                       \
		if (c_ != 0) VPUSH(roots, c1(c_));  \
	} while (0)
		R(lookupC(flagEntry, 0));
		R(lookupC(flagEntry, 1));
		R(lookupC("runtime.unreachableMethod", 0));
		R(lookupC("runtime.unreachableMethod", 1));
		R(lookupC("runtime.mapinitnoop", 1));
		R(lookupC("runtime.firstmoduledata", 0));
		for (size_t i = 0; i < mainInittasks.n; i++) R(mainInittasks.a[i]);
		for (size_t i = 0; i < runtimeInittasks.n; i++) R(runtimeInittasks.a[i]);
#undef R
		WVEC("p2_roots.u32", roots);
		vecu32 live = {0};
		for (u32 g = 1; g < nids; g++)
			if (flatFL2[g] & stLive) VPUSH(live, g);
		WVEC("oracle/p2_live.u32", live);
		meta("p2_live", live.n);
	}

	// ---------------------------------------------------------------- P3: layout and relocate
	vecu32 psym = {0}, psize = {0}, punit = {0}, ptl = {0}, rOffv = {0}, prel = {0}, rTgt = {0}, paddr = {0};
	vecu8 pb = {0}, pcl = {0}, rSizv = {0}, rKind = {0};
	VEC(i64) rAddv = {0};
	vecu64 doff = {0};
	vecu8 dbin = {0};
	strtab T;
	memset(&T, 0, sizeof T);
	stInit(&T);
	long nUnsup = 0;
	VPUSH(doff, z);
	for (long ui = 0; ui < nunits; ui++) {
		unit *u = &units[ui];
		object *o = u->o;
		for (u32 j = u->lo; j < MIN(u->hi, o->nalldef); j++) {
			u32 g = o->base + j;
			u32 st = state[g];
			if ((st & (stLive | stPC | stNameNeeded)) == 0 || canon[g] != g) continue;
			int b = (int)(st >> stBucketShift & 31);
			if (b == bNone) continue;
			const u8 *s = osym(o, j);
			u32 cl = b == bTypelink ? (u32)__builtin_ctz(MAX(8, symAlign(s))) : (st >> stClassShft & 15);
			u32 siz = symSiz(s);
			u32 a = addr[g];
			u8 b8 = (u8)b, cl8 = (u8)cl;
			u32 uix = (u32)ui;
			VPUSH(psym, g);
			VPUSH(pb, b8);
			VPUSH(pcl, cl8);
			VPUSH(psize, siz);
			VPUSH(punit, uix);
			VPUSH(paddr, a);
			u32 tls_ = b == bTypelink ? stId(&T, typeString(g)) : 0;
			VPUSH(ptl, tls_);
			u32 rstart = (u32)rOffv.n;
			VPUSH(prel, rstart);
			if (b != bBSS && b != bNoptrBSS && b != bCov) {
				bytes d = oData(o, j);
				for (u32 k = 0; k < d.n; k++) VPUSH(dbin, d.p[k]);
				u32 nr;
				relocp rs = oRelocs(o, j, &nr);
				for (u32 i = 0; i < nr; i++) {
					relocp rl = RELOC(rs, i);
					u32 rsiz = rSiz(rl);
					if (rsiz == 0) continue;
					u16 t = rType(rl);
					bool weak = (t & R_WEAK) != 0;
					t &= (u16)~R_WEAK;
					u32 tg = orefQ(o, rPkg(rl), rIdx(rl));
					i64 ad = rAdd(rl);
					u8 kk;
					switch (t) {
					case R_ADDR: kk = weak ? 2 : 1; break;
					case R_ADDROFF: kk = 3; break;
					case R_METHODOFF: kk = 4; break;
					case R_CALL: kk = weak ? 6 : 5; break;
					case R_PCREL: kk = 7; break;
					case R_TLS_LE: kk = 8; ad = -8 + ad; break;
					case R_CONST: kk = 8; break;
					case R_SIZE: {
						u32 c = canon[tg];
						i64 v = 0;
						if (isDef(c)) {
							u32 j2;
							object *o2 = objOf(c, &j2);
							v = symSiz(osym(o2, j2));
						}
						kk = 8;
						ad = v + ad;
						break;
					}
					default: kk = 0; nUnsup++; break;
					}
					u32 ro = (u32)rOff(rl);
					u8 rs8 = (u8)rsiz;
					VPUSH(rOffv, ro);
					VPUSH(rSizv, rs8);
					VPUSH(rKind, kk);
					VPUSH(rAddv, ad);
					VPUSH(rTgt, tg);
				}
			}
			u64 de = dbin.n;
			VPUSH(doff, de);
		}
	}
	u32 rend = (u32)rOffv.n;
	VPUSH(prel, rend);
	meta("p3_placed", psym.n);
	// Relocation targets that the linker places outside 3a's buckets (its own symbols: runtime.firstmoduledata,
	// runtime.buildVersion, ...): their addresses are a given input of 3b, like the bucket starts are of 3a.
	{
		u8 *isPlaced = calloc(nids, 1), *isFixed = calloc(nids, 1);
		for (size_t i = 0; i < psym.n; i++) isPlaced[psym.a[i]] = 1;
		vecu32 fid = {0}, fad = {0};
		for (size_t i = 0; i < rTgt.n; i++) {
			u32 tg = rTgt.a[i], c = tg <= N ? canon[tg] : tg;
			if (c < nids && !isPlaced[c] && addr[c] != 0) isFixed[c] = 1;
		}
		for (u32 c = 0; c < nids; c++)
			if (isFixed[c]) {
				u32 a_ = addr[c];
				VPUSH(fid, c);
				VPUSH(fad, a_);
			}
		meta("p3_fixed", fid.n);
		WVEC("p3_fixed_id.u32", fid);
		WVEC("p3_fixed_addr.u32", fad);
		free(isPlaced);
		free(isFixed);
	}
	meta("p3_rel", rOffv.n);
	meta("p3_rel_unsupported", (u64)nUnsup);
	meta("p3_data_bytes", dbin.n);
	meta("p3_typelinks", (u64)ntls);
	meta("p3_unitSlots", unitSlots);
	meta("lay_text", lay.text);
	meta("lay_rodata", lay.rodata);
	meta("lay_pclntab", lay.pclntab);
	meta("lay_types", lay.types);
	meta("lay_noptrdata", lay.noptrdata);
	meta("lay_data", lay.data);
	meta("lay_bss", lay.bss);
	meta("lay_noptrbss", lay.noptrbss);
	meta("lay_end", lay.end);
	meta("addr_unreachableMethod", unreachableMethodAddr);
	meta("addr_mapinitnoop", mapinitnoopAddr);
	meta("minFunc", minFunc);
	flatBucketStart[bTypelink] = lay.types + 8;
	flatBucketMinAlign[bTypelink] = 8;
	WVEC("p3_sym.u32", psym);
	WVEC("p3_bucket.u8", pb);
	WVEC("p3_class.u8", pcl);
	WVEC("p3_size.u32", psize);
	WVEC("p3_unit.u32", punit);
	WVEC("p3_tlstr.u32", ptl);
	stWrite(&T, "p3_str");
	wfile("p3_bucket_start.u32", flatBucketStart, sizeof flatBucketStart);
	wfile("p3_bucket_minalign.u32", flatBucketMinAlign, sizeof flatBucketMinAlign);
	WVEC("p3_data.off.u64", doff);
	WVEC("p3_data.bin", dbin);
	WVEC("p3_rel_start.u32", prel);
	WVEC("p3_rel_off.u32", rOffv);
	WVEC("p3_rel_siz.u8", rSizv);
	WVEC("p3_rel_kind.u8", rKind);
	WVEC("p3_rel_add.i64", rAddv);
	WVEC("p3_rel_tgt.u32", rTgt);
	wfile("p3_addr_all.u32", addr, 4 * (size_t)nids);
	WVEC("oracle/p3_addr.u32", paddr);
	wfile("oracle/p3_image.bin", out, (size_t)outSize);
	meta("image_bytes", (u64)outSize);
	fclose(metaF);
	fprintf(stderr, "dumpflat: wrote %s\n", flatDir);
}
