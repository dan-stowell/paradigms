// Reference benchmark: p3.c's 3a and 3b loops copied literally below, plus its
// pipeline address propagation. Input mapping / final oracle checks are outside
// timing. Every timed repetition allocates a fresh zero image, computes 3a,
// propagates canonical addresses and computes 3b; frees are included. Ordinary
// 4 KB mappings (no P3_THP/P3_PREFAULT). Build -fopenmp for p3par's same loop.
#define main p3_original_main
#include "p3.c"
#undef main
int main(int argc, char **argv) {
 flatDir=argc>1?argv[1]:"data/minio";
 double seconds=argc>2?atof(argv[2]):1;
 if(seconds<1) seconds=1;
 u32 L=metaGet("p3_placed"), nunits=metaGet("nunits"), minFunc=metaGet("minFunc");
 u32 imageBase=metaGet("imageBase"), N=metaGet("nslots"), nids=metaGet("nids");
 psym=FMAP(u32,"p3_sym.u32",NULL); tlstr=FMAP(u32,"p3_tlstr.u32",NULL);
 soff=FMAP(u64,"p3_str.off.u64",NULL); sbin=FMAP(u8,"p3_str.bin",NULL);
 const u8 *pb=FMAP(u8,"p3_bucket.u8",NULL), *pcl=FMAP(u8,"p3_class.u8",NULL);
 const u32 *psize=FMAP(u32,"p3_size.u32",NULL), *punit=FMAP(u32,"p3_unit.u32",NULL);
 const u32 *bstart=FMAP(u32,"p3_bucket_start.u32",NULL), *bmin=FMAP(u32,"p3_bucket_minalign.u32",NULL);
 const u64 *doff=FMAP(u64,"p3_data.off.u64",NULL);
 const u8 *dbin=FMAP(u8,"p3_data.bin",NULL);
 const u32 *rstart=FMAP(u32,"p3_rel_start.u32",NULL), *roff=FMAP(u32,"p3_rel_off.u32",NULL), *rtgt=FMAP(u32,"p3_rel_tgt.u32",NULL);
 const u8 *rsiz=FMAP(u8,"p3_rel_siz.u8",NULL), *rkind=FMAP(u8,"p3_rel_kind.u8",NULL);
 const i64 *radd=FMAP(i64,"p3_rel_add.i64",NULL);
 const u32 *canon=FMAP(u32,"oracle/p1c_canon.u32",NULL);
 size_t nfix, nimg;
 const u32 *fid=FMAP(u32,"p3_fixed_id.u32",&nfix), *fad=FMAP(u32,"p3_fixed_addr.u32",NULL); nfix/=4;
 const u8 *want=FMAP(u8,"oracle/p3_image.bin",&nimg);
 u32 sect[8]={metaGet("lay_text"),metaGet("lay_rodata"),metaGet("lay_pclntab"),metaGet("lay_types"),metaGet("lay_noptrdata"),metaGet("lay_data"),metaGet("lay_bss"),metaGet("lay_noptrbss")};
 u32 unreach=metaGet("addr_unreachableMethod"),mapinit=metaGet("addr_mapinitnoop");
 double elapsed=0; u32 reps=0; long bad=0;
 for(;;) {
  double start=now(), mark=start, phase[6]={0};
  u32 *addr=NULL, ntl=0;
	addr = calloc(L, 4);
	// bytes per (unit, bucket, class), then each unit's block of each bucket in unit order, classes from the largest
	// alignment down; inside a class the symbols in list order
	u32 *csize = calloc((size_t)nunits * nBuckets * nClasses, 4);
#define CS(u, b, c) csize[((size_t)(u) * nBuckets + (b)) * nClasses + (c)]
	u32 *occ = malloc(4 * (size_t)L);
	phase[0] += now()-mark; mark = now();
	for (u32 i = 0; i < L; i++) {
		u32 s = psize[i], b = pb[i];
		if ((b == bText || b == bTextFIPS) && s < minFunc) s = minFunc;
		occ[i] = rnd(s, 1u << pcl[i]);
		if (b != bTypelink) CS(punit[i], b, pcl[i]) += occ[i];
	}
	phase[1] += now()-mark; mark = now();
	for (u32 b = 1; b < nBuckets; b++) {
		if (b == bTypelink) continue;
		u32 va = bstart[b];
		for (u32 u = 0; u < nunits; u++) {
			int top = -1;
			for (int c = nClasses - 1; c >= 0; c--)
				if (CS(u, b, c)) { top = c; break; }
			if (top >= 0) va = rnd(va, bmin[b] > (1u << top) ? bmin[b] : 1u << top);
			for (int c = nClasses - 1; c >= 0; c--) {
				u32 n = CS(u, b, c);
				CS(u, b, c) = va; // from here on: the class's next address
				va += n;
			}
		}
	}
	phase[2] += now()-mark; mark = now();	ntl = 0;
	u32 *tl = malloc(4 * (size_t)L);
	for (u32 i = 0; i < L; i++) {
		if (pb[i] == bTypelink) { tl[ntl++] = i; continue; }
		addr[i] = CS(punit[i], pb[i], pcl[i]);
		CS(punit[i], pb[i], pcl[i]) += occ[i];
	}
	phase[3] += now()-mark; mark = now();
	qsort(tl, ntl, 4, cmpTL);
	phase[4] += now()-mark; mark = now();
	u32 va = bstart[bTypelink];
	for (u32 k = 0; k < ntl; k++) {
		u32 i = tl[k];
		va = rnd(va, 1u << pcl[i]);
		addr[i] = va;
		va += psize[i];
	}

 free(csize); free(occ); free(tl);
 u32 *placed=calloc(nids,4), *resolved=calloc(nids,4);
 for(u32 i=0;i<L;i++) placed[psym[i]]=addr[i];
 for(size_t i=0;i<nfix;i++) placed[fid[i]]=fad[i];
 for(u32 g=0;g<nids;g++) resolved[g]=placed[g<=N?canon[g]:g];
 free(placed);
 const u32 *all=resolved, *oaddr=addr;
 u8 *img=calloc(nimg,1);
 long nerr=0;
#pragma omp parallel for schedule(dynamic, 1024) reduction(+ : nerr)
	for (u32 i = 0; i < L; i++) {
		u32 b = pb[i];
		if (b == bBSS || b == bNoptrBSS || b == bCov) continue;
		u32 a = oaddr[i];
		u8 *P = img + (a - imageBase);
		memcpy(P, dbin + doff[i], doff[i + 1] - doff[i]);
		for (u32 r = rstart[i]; r < rstart[i + 1]; r++) {
			u32 tg = rtgt[r], ta = all[tg], off = roff[r], sz = rsiz[r];
			i64 v;
			switch (rkind[r]) {
			case 2: if (ta == 0 && tg != 0) ta = unreach; /* fallthrough */
			case 1:
				if (ta == 0 && tg != 0) { nerr++; continue; }
				v = (i64)ta + radd[r];
				break;
			case 4: if (ta == 0) { memset(P + off, 0xFF, 4); continue; } /* fallthrough */
			case 3: {
				if (ta == 0) continue;
				int k = 7;
				while (k > 0 && ta < sect[k]) k--;
				v = (i64)(ta - sect[k]) + radd[r];
				break;
			}
			case 6: if (ta == 0 && tg != 0) ta = mapinit; /* fallthrough */
			case 5:
			case 7:
				if (ta == 0 && tg != 0) { nerr++; continue; }
				v = tg == 0 ? radd[r] - (i64)(a + off + sz) : (i64)ta + radd[r] - (i64)(a + off + sz);
				break;
			case 8: v = radd[r]; break;
			default: nerr++; continue;
			}
			switch (sz) {
			case 8: memcpy(P + off, &v, 8); break;
			case 4: { u32 x = (u32)v; memcpy(P + off, &x, 4); break; }
			case 2: { u16 x = (u16)v; memcpy(P + off, &x, 2); break; }
			case 1: P[off] = (u8)v; break;
			default: nerr++;
			}
		}
	}

 double kernel=now()-start;
 int done=elapsed+kernel>=seconds;
 if(done) {
  bad=check32("addr",addr,L,"oracle/p3_addr.u32")+nerr;
  for(u32 i=0;i<L;i++) if(pb[i]<13) {
   size_t o=addr[i]-imageBase;
   if(memcmp(img+o,want+o,psize[i])) bad++;
  }
 }
 double cleanup=now(); free(img); free(addr); free(resolved);
 elapsed+=kernel+now()-cleanup; reps++;
 if(done) break;
 }
 printf("P3BENCH threads %d reps %u seconds %.6f mean_ms %.6f %s\n",nthreads(),reps,elapsed,1000*elapsed/reps,bad?"FAIL":"OK");
 return bad!=0;
}
