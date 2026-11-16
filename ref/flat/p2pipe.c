// Eager 1b + CSR + p2par flood. Uses 1a canon/builtin oracles; no 1a cost.
// Build: gcc -O2 -march=native -fopenmp ref/flat/p2pipe.c -o build/p2pipe
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
int main(int argc, char **argv) {
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
 nr/=4;
 u32 *target=calloc(nr,4), *off=calloc((size_t)nsl+2,4);
 u32 *dst=malloc((size_t)eoff[nsl+1]*4);
 u8 *kind=malloc(eoff[nsl+1]);
 const u32 *oldoff=eoff, *olddst=ed; const u8 *oldkind=ek;
 u32 *extraoff=calloc((size_t)nsl+2,4), *extra=malloc((size_t)eoff[nsl+1]*4);
 u8 *extrak=malloc(eoff[nsl+1]);
 size_t inherited[4]={0}, rebuilt=0;
 int T = omp_get_max_threads();
 // Setup pass decodes and validates all targets, then extracts the aux suffix.
 // Timed passes repeat decode and construct CSR from scratch; setup is excluded.
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
 size_t nx=0;
 for(u32 g=1;g<=nsl;g++) {
  extraoff[g]=nx;
  if(!(fl[g]&1) || !(fl[g]&32) || canon[g]!=g) continue;
  vec a={0},b={0};
  for(u32 r=sr[g];r<sr[g+1];r++) {
   if(rt[r]&32768) continue;
   if(rt[r]==26) {r+=2;continue;}
   if(plain(rt[r]) && target[r]) push(&a,target[r]);
  }
  for(u32 e=oldoff[g];e<oldoff[g+1];e++) {
   if(oldkind[e]==0 && b.n<a.n) push(&b,olddst[e]);
   else {
    if(oldkind[e]==0 && !(fl[g]&fText)) {fprintf(stderr,"aux on non-text row %u\n",g);return 1;}
    extra[nx]=olddst[e]; extrak[nx++]=oldkind[e]; inherited[oldkind[e]]++;}
  }
  if(a.n!=b.n) {fprintf(stderr,"row %u count mismatch\n",g);return 1;}
  qsort(a.a,a.n,4,cmp32); qsort(b.a,b.n,4,cmp32);
  if(a.n && memcmp(a.a,b.a,a.n*4)) {fprintf(stderr,"row %u multiset mismatch\n",g);return 1;}
  rebuilt+=a.n; free(a.a); free(b.a);
 }
 extraoff[nsl+1]=nx;
 printf("verified relocation multisets: %zu; inherited aux=%zu kinds1/2/3=%zu/%zu/%zu methods=%zu\n",rebuilt,inherited[0],inherited[1],inherited[2],inherited[3],nmeth);
 eoff=off; ed=dst; ek=kind;
 double bd=1e9,bc=1e9,bf=1e9,bt=1e9;

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
	for (int rep = 0; rep < (argc > 2 ? atoi(argv[2]) : 7); rep++) {
		memset(live, 0, N); memset(ifaceDone, 0, N); memset(sigSeen, 0, nsig); memset(named, 0, nstr + 1);
		memset(sigTab, 0, hs * 8); memset(refl, 0, T * sizeof(int));
		for (u32 g = 0; g < N; g++) iface[g] = g <= nsl && (fl[g] & fIface);

  double start=now();
  #pragma omp parallel for schedule(static,65536)
  for(u32 g=1;g<=nsl;g++) {
   u32 o=so[g], np=po[o+1]-po[o]-5; const u32 *bases=pb+po[o];
   for(u32 r=sr[g];r<sr[g+1];r++) {
    u32 sp=rp[r]-2147483643u, t=sp<5?np+sp:rp[r], x=0;
    if(t<np+5 && bases[t]!=0xffffffffu) x=sp==1?(ri[r]<nb?builtin[ri[r]]:0):bases[t]+ri[r];
    target[r]=x?canon[x]:0;
   }
  }
  double decoded=now();
  #pragma omp parallel for schedule(static,65536)
  for(u32 g=1;g<=nsl;g++) {
   u32 n=extraoff[g+1]-extraoff[g];
   if((fl[g]&1) && (fl[g]&32) && canon[g]==g)
    for(u32 r=sr[g];r<sr[g+1];r++) {
     if(rt[r]&32768) continue;
     if(rt[r]==26) {r+=2;continue;}
     n+=plain(rt[r]) && target[r];
    }
   off[g+1]=n;
  }
  off[0]=off[1]=0;
  for(u32 g=1;g<=nsl;g++) off[g+1]+=off[g];
  #pragma omp parallel for schedule(static,65536)
  for(u32 g=1;g<=nsl;g++) {
   u32 at=off[g];
   if((fl[g]&1) && (fl[g]&32) && canon[g]==g)
    for(u32 r=sr[g];r<sr[g+1];r++) {
     if(rt[r]&32768) continue;
     if(rt[r]==26) {r+=2;continue;}
     if(plain(rt[r]) && target[r]) {dst[at]=target[r];kind[at++]=0;}
    }
   for(u32 e=extraoff[g];e<extraoff[g+1];e++) {dst[at]=extra[e];kind[at++]=extrak[e];}
  }
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
						for (u32 e = eoff[c]; e < eoff[c + 1]; e++) {
							u32 t = ed[e];
							switch (ek[e]) {
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
		if (t1 - t0 < best) best = t1 - t0;
  double d=decoded-start,c=t0-decoded,f=t1-t0,t=t1-start;
  if(d<bd)bd=d;
  if(c<bc)bc=c;
  if(f<bf)bf=f;
  if(t<bt)bt=t;
  printf("sample decode=%.3f csr=%.3f flood=%.3f total=%.3f ms\n",d*1000,c*1000,f*1000,t*1000);
	}
 // Independently compare the final materialized CSR, preserving multiplicity
 // and kind as well as destination. This check is outside every timer.
 for(u32 g=1;g<=nsl;g++) {
  size_t n=off[g+1]-off[g];
  if(n!=oldoff[g+1]-oldoff[g]) {fprintf(stderr,"merged row count mismatch %u\n",g);return 1;}
  if(!n) continue;
  u64 *a=malloc(n*8), *b=malloc(n*8);
  for(size_t i=0;i<n;i++) {
   a[i]=((u64)kind[off[g]+i]<<32)|dst[off[g]+i];
   b[i]=((u64)oldkind[oldoff[g]+i]<<32)|olddst[oldoff[g]+i];
  }
  qsort(a,n,8,cmp64); qsort(b,n,8,cmp64);
  if(memcmp(a,b,n*8)) {fprintf(stderr,"merged row multiset mismatch %u\n",g);return 1;}
  free(a);free(b);
 }
 printf("merged CSR: all source multisets match (%u edges)\n",off[nsl+1]);
	nlive = 0;
	for (u32 g = 1; g < N; g++) nlive += live[g];
	u32 *list = malloc(4 * nlive), k = 0;
	for (u32 g = 1; g < N; g++) if (live[g]) list[k++] = g;
	printf("p2pipe (%d threads): flood best %.1f ms, %zu live, %d levels in %d rounds\n", T, best * 1e3, nlive, levels, rounds);
	printf("best decode=%.3f csr=%.3f flood=%.3f total=%.3f ms\n",bd*1000,bc*1000,bf*1000,bt*1000);
	long bad = check32("live", list, nlive, "oracle/p2_live.u32");
	printf(bad ? "P2 FAIL\n" : "P2 OK\n");
	return bad != 0;
}
