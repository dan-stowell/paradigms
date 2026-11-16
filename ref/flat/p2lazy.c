// Lazy decode flood: p2pipe's flood, but a live source's plain relocation edges are decoded from the flat relocation
// columns when it is flooded ((pkg, idx) -> slot -> canon), as hlink does on raw objects; nothing is pre-decoded.
// --aux decodes funcdata from raw aux columns too; only kinds 1-3 and method tables
// remain precomputed edges/rules. Without --aux, retain the original aux CSR mode.
// Uses the 1a canon/builtin oracles; no 1a cost. Build: gcc -O2 -march=native -fopenmp ref/flat/p2lazy.c -o build/p2lazy
#include "flat.h"
#include <omp.h>

enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16 };
static u8 *live, *iface, *ifaceDone;
static inline int tas(u8 *p) { return __atomic_exchange_n(p, 1, __ATOMIC_RELAXED) == 0; }
// shared flags inside the parallel flood: relaxed atomic loads and stores (plain ones are data races, even same-value)
#define LD(x) __atomic_load_n(&(x), __ATOMIC_RELAXED)
#define ST1(x) __atomic_store_n(&(x), 1, __ATOMIC_RELAXED)

typedef struct { u32 *a; size_t n, cap; } vec;
static inline void push(vec *v, u32 x) {
	if (v->n == v->cap) { v->cap = v->cap ? 2 * v->cap : 4096; v->a = realloc(v->a, 4 * v->cap); }
	v->a[v->n++] = x;
}


/* dumpflat emits all relocation edges before funcdata aux edges. The first
 * nplain kind-0 entries supply the relocation-edge oracle (no aux labels exist).
 * Offsets, sizes and addends do not affect plain-edge eligibility. */
static int cmp32(const void *a, const void *b) {
 u32 x=*(const u32 *)a, y=*(const u32 *)b; return (x>y)-(x<y);
}
static int cmp64(const void *a, const void *b) {
 u64 x=*(const u64 *)a, y=*(const u64 *)b; return (x>y)-(x<y);
}
static int plain(u16 t) {
 return !(t & 32768) && t != 22 && t != 23 && t != 24 && t != 25 && t != 26 && t != 102;
}
static inline u32 decode(u32 pkg, u32 idx, u32 np, const u32 *bases,
                         const u32 *builtin, u32 nb, const u32 *canon) {
 u32 sp=pkg-2147483643u, t=sp<5?np+sp:pkg, x=0;
 if(t<np+5 && bases[t]!=0xffffffffu) x=sp==1?(idx<nb?builtin[idx]:0):bases[t]+idx;
 return x?canon[x]:0;
}
int main(int argc, char **argv) {
 int auxMode=0, reps=7;
 for(int i=2;i<argc;i++) {
  if(!strcmp(argv[i],"--aux")) auxMode=1;
  else if((reps=atoi(argv[i]))<=0) {fprintf(stderr,"expected positive repetitions or --aux\n");return 2;}
 }
	flatDir = argc > 1 ? argv[1] : "data/minio";
	u32 N = (u32)metaGet("nids"), nsl = (u32)metaGet("nslots");
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
	
 size_t nr;
 const u32 *canon=FMAP(u32,"oracle/p1_canon.u32",NULL);
 const u32 *builtin=FMAP(u32,"oracle/p1_builtin.u32",NULL);
 u32 nb=metaGet("nbuiltin");
 const u32 *sr=FMAP(u32,"slot_rel.u32",NULL), *so=FMAP(u32,"slot_obj.u32",NULL);
 const u32 *po=FMAP(u32,"obj_pkg_off.u32",NULL), *pb=FMAP(u32,"obj_pkg.u32",NULL);
 const u32 *rp=FMAP(u32,"rel_pkg.u32",&nr), *ri=FMAP(u32,"rel_idx.u32",NULL);
 const u16 *rt=FMAP(u16,"rel_type.u16",NULL);
 const u32 *ast=NULL, *ap=NULL, *ai=NULL;
 if(auxMode) {
  ast=FMAP(u32,"aux_start.u32",NULL);
  ap=FMAP(u32,"aux_pkg.u32",NULL); ai=FMAP(u32,"aux_idx.u32",NULL);
 }
 nr/=4;
 u32 *target=calloc(nr,4);
 const u32 *oldoff=eoff, *olddst=ed; const u8 *oldkind=ek;
 u32 *extraoff=calloc((size_t)nsl+2,4), *extra=malloc((size_t)eoff[nsl+1]*4);
 u8 *extrak=malloc(eoff[nsl+1]);
 size_t inherited[4]={0}, rebuilt=0;
 int T = omp_get_max_threads();
 // Setup validates relocation and aux multisets; the timed flood decodes on demand.
 {
  #pragma omp parallel for schedule(static,65536)
  for(u32 g=1;g<=nsl;g++) {
   u32 o=so[g], np=po[o+1]-po[o]-5; const u32 *bases=pb+po[o];
   for(u32 r=sr[g];r<sr[g+1];r++) {
    u32 sp=rp[r]-2147483643u, t=sp<5?np+sp:rp[r], x=0;
    if(t<np+5 && bases[t]!=0xffffffffu) x=sp==1?(ri[r]<nb?builtin[ri[r]]:0):bases[t]+ri[r];
    target[r]=x?canon[x]:0;
   }
  }
 }
 if(check32("decoded targets",target,nr,"oracle/p1_reltarget.u32")) return 1;
 size_t nx=0, verifiedAux=0;
 // the timed decoder's check (Codex review): per source, the sum of mix64 over the targets it must decode, from the
 // oracle-checked targets and dumpflat's aux edges (independent of the timed code and of the aux_* columns)
 u64 *expH=calloc((size_t)nsl+1,8), *gotH=calloc((size_t)nsl+1,8);
 for(u32 g=1;g<=nsl;g++) {
  extraoff[g]=nx;
  if(!(fl[g]&1) || !(fl[g]&32) || canon[g]!=g) continue;
  vec a={0},b={0},aux={0},wantAux={0};
  for(u32 r=sr[g];r<sr[g+1];r++) {
   if(rt[r]&32768) continue;
   if(rt[r]==26) {r+=2;continue;}
   if(plain(rt[r]) && target[r]) push(&a,target[r]);
  }
  for(u32 e=oldoff[g];e<oldoff[g+1];e++) {
   if(oldkind[e]==0 && b.n<a.n) push(&b,olddst[e]);
   else {
    if(oldkind[e]==0 && !(fl[g]&fText)) {fprintf(stderr,"aux on non-text row %u\n",g);return 1;}
    if(oldkind[e]==0) push(&wantAux,olddst[e]);
    if(!auxMode || oldkind[e]!=0) {
     extra[nx]=olddst[e]; extrak[nx++]=oldkind[e]; inherited[oldkind[e]]++;
    }}
  }
  if(a.n!=b.n) {fprintf(stderr,"row %u count mismatch\n",g);return 1;}
  for(size_t k=0;k<a.n;k++) expH[g]+=mix64(a.a[k]);
  if(auxMode) for(size_t k=0;k<wantAux.n;k++) expH[g]+=mix64(wantAux.a[k]);
  qsort(a.a,a.n,4,cmp32); qsort(b.a,b.n,4,cmp32);
  if(a.n && memcmp(a.a,b.a,a.n*4)) {fprintf(stderr,"row %u multiset mismatch\n",g);return 1;}
  if(auxMode) {
   u32 o=so[g], np=po[o+1]-po[o]-5;
   if(fl[g]&fText) for(u32 r=ast[g];r<ast[g+1];r++) {
    u32 t=decode(ap[r],ai[r],np,pb+po[o],builtin,nb,canon);
    if(t) push(&aux,t);
   }
   qsort(aux.a,aux.n,4,cmp32); qsort(wantAux.a,wantAux.n,4,cmp32);
   if(aux.n!=wantAux.n || (aux.n && memcmp(aux.a,wantAux.a,aux.n*4))) {
    fprintf(stderr,"aux row %u multiset mismatch\n",g);return 1;
   }
   verifiedAux+=aux.n;
  }
  rebuilt+=a.n; free(a.a); free(b.a); free(aux.a); free(wantAux.a);
 }
 extraoff[nsl+1]=nx;
 printf("verified relocation multisets: %zu; inherited aux=%zu kinds1/2/3=%zu/%zu/%zu methods=%zu\n",rebuilt,inherited[0],inherited[1],inherited[2],inherited[3],nmeth);
 if(auxMode) printf("verified aux multisets: %zu; extra CSR contains only kinds 1-3\n",verifiedAux);

	live = malloc(N), iface = malloc(N), ifaceDone = malloc(N);
	u8 *sigSeen = malloc(nsig), *named = malloc(nstr + 1);
	vec *next = calloc(T, sizeof(vec)), *meth = calloc(T, sizeof(vec)), fr = {0}, mk = {0};
	int *refl = calloc(T, sizeof(int));
	u64 hs = 1;
	while (hs < 4 * nsig + 64) hs <<= 1;
	u64 *sigTab = malloc(hs * 8);
	double best = 1e9;
	size_t nlive = 0;
	int levels = 0, rounds = 0;
	for (int rep = 0; rep <= reps; rep++) { // rep == reps: untimed, records what the timed decoder decodes
		const int chk = rep == reps;
		memset(live, 0, N); memset(ifaceDone, 0, N); memset(sigSeen, 0, nsig); memset(named, 0, nstr + 1);
		memset(sigTab, 0, hs * 8); memset(refl, 0, T * sizeof(int));
		for (u32 g = 0; g < N; g++) iface[g] = g <= nsl && (fl[g] & fIface);
		
  double t0 = now();
		fr.n = mk.n = 0;
		levels = rounds = 0;
		int reflectSeen = 0;
		for (size_t i = 0; i < nroots; i++) if (tas(&live[roots[i]])) push(&fr, roots[i]);
		for (;;) {
			rounds++;
			while (fr.n) {
				levels++;
				#pragma omp parallel
				{
					int w = omp_get_thread_num();
					vec *nx = &next[w];
					#pragma omp for schedule(dynamic, 256) nowait
					for (size_t i = 0; i < fr.n; i++) {
						u32 c = fr.a[i];
						if (c > nsl || !(fl[c] & fDef)) continue;
						if (fl[c] & fReflect) refl[w] = 1;
						int uii = (fl[c] & fGoType) && __atomic_load_n(&iface[c], __ATOMIC_RELAXED);
						if (uii && !tas(&ifaceDone[c])) continue;
						if ((fl[c] & 32) && canon[c] == c) {
							u32 o = so[c], np = po[o + 1] - po[o] - 5; const u32 *bases = pb + po[o];
							u64 hsum = 0;
							for (u32 r = sr[c]; r < sr[c + 1]; r++) {
								u16 ty = rt[r];
								if (ty & 32768) continue;
								if (ty == 26) { r += 2; continue; }
								if (!plain(ty)) continue;
								u32 sp = rp[r] - 2147483643u, tt = sp < 5 ? np + sp : rp[r], x = 0;
								if (tt < np + 5 && bases[tt] != 0xffffffffu) x = sp == 1 ? (ri[r] < nb ? builtin[ri[r]] : 0) : bases[tt] + ri[r];
								u32 t = x ? canon[x] : 0;
								if (!t) continue;
								if (chk) hsum += mix64(t);
								if (uii && (fl[t] & fGoType) && (fl[t] & fDef) && !LD(iface[t]) && tas(&iface[t])) {
									tas(&live[t]);
									push(nx, t);
								} else if (!LD(live[t]) && tas(&live[t])) push(nx, t);
							}
							if (auxMode && (fl[c] & fText))
								for (u32 r=ast[c]; r<ast[c+1]; r++) {
									u32 t=decode(ap[r],ai[r],np,bases,builtin,nb,canon);
									if (chk && t) hsum += mix64(t);
									if(t && !LD(live[t]) && tas(&live[t])) push(nx,t);
								}
							if (chk) __atomic_store_n(&gotH[c], hsum | 1ull << 63, __ATOMIC_RELAXED); // a source may be flooded twice: same value
						}
						for (u32 e = extraoff[c]; e < extraoff[c + 1]; e++) {
							u32 t = extra[e];
							switch (extrak[e]) {
							case 0:
								if (uii && (fl[t] & fGoType) && (fl[t] & fDef) && !LD(iface[t]) && tas(&iface[t])) {
									tas(&live[t]);
									push(nx, t); // (re)flood it as a type converted to an interface
								} else if (!LD(live[t]) && tas(&live[t])) push(nx, t);
								break;
							case 1:
								if (!LD(iface[t]) && tas(&iface[t]) && LD(live[t])) push(nx, t);
								break;
							case 2: if (!LD(sigSeen[t])) ST1(sigSeen[t]); break;
							case 3: if (!LD(named[t])) ST1(named[t]); break;
							}
						}
						if (uii)
							for (u32 m = moff[c]; m < moff[c + 1]; m++) {
								u32 t = mTyp[m];
								if (t && !LD(iface[t]) && tas(&iface[t]) && LD(live[t])) push(nx, t);
								push(&meth[w], m);
							}
					}
				}
				fr.n = 0;
				for (int w = 0; w < T; w++) {
					for (size_t k = 0; k < next[w].n; k++) push(&fr, next[w].a[k]);
					next[w].n = 0;
					for (size_t k = 0; k < meth[w].n; k++) push(&mk, meth[w].a[k]);
					meth[w].n = 0;
					reflectSeen |= refl[w];
				}
			}
			// signatures seen this round into the table, then the methods something can call
			for (u32 s = 0; s < nsig; s++) {
				if (sigSeen[s] != 1) continue;
				sigSeen[s] = 2;
				u64 k = (u64)sigName[s] << 32 | sigType[s];
				for (u64 h = mix64(k) & (hs - 1);; h = (h + 1) & (hs - 1)) {
					if (sigTab[h] == 0) { sigTab[h] = k + 1; break; }
					if (sigTab[h] == k + 1) break;
				}
			}
			size_t keep = 0;
			for (size_t i = 0; i < mk.n; i++) {
				u32 m = mk.a[i];
				int call = (reflectSeen && mExp[m]) || named[mName[m]];
				if (!call) {
					u64 k = (u64)mName[m] << 32 | mTyp[m];
					for (u64 h = mix64(k) & (hs - 1); sigTab[h]; h = (h + 1) & (hs - 1))
						if (sigTab[h] == k + 1) { call = 1; break; }
				}
				if (call) {
					u32 ts[3] = {mTyp[m], mIfn[m], mTfn[m]};
					for (int d = 0; d < 3; d++) if (ts[d] && tas(&live[ts[d]])) push(&fr, ts[d]);
				} else mk.a[keep++] = m;
			}
			mk.n = keep;
			if (fr.n == 0) break;
		}
		double t1 = now();
		if (!chk && t1 - t0 < best) best = t1 - t0;

	}
	nlive = 0;
	for (u32 g = 1; g < N; g++) nlive += live[g];
	u32 *list = malloc(4 * nlive), k = 0;
	for (u32 g = 1; g < N; g++) if (live[g]) list[k++] = g;
	printf("p2lazy%s (%d threads): lazy-decode flood best %.1f ms, %zu live, %d levels in %d rounds\n", auxMode?" --aux":"", T, best * 1e3, nlive, levels, rounds);
	
	long bad = check32("live", list, nlive, "oracle/p2_live.u32");
	size_t nvis = 0, hbad = 0, missed = 0;
	for (u32 g = 1; g <= nsl; g++) {
		if (gotH[g]) { nvis++; if ((gotH[g] & ~(1ull << 63)) != (expH[g] & ~(1ull << 63))) hbad++; }
		else if (live[g] && (fl[g] & 1) && (fl[g] & 32) && canon[g] == g) missed++;
	}
	printf("timed decoder: %zu sources decoded, %zu with a wrong edge hash, %zu live sources never decoded\n", nvis, hbad, missed);
	bad += hbad + missed;
	printf(bad ? "P2 FAIL\n" : "P2 OK\n");
	return bad != 0;
}
