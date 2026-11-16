// Prepared B hole experiment. H1 pre-bakes integers; H2 coalesces integer
// patches into masked eight-byte stores; H0 retains traversal but omits stores.
// Preparation reads operands, never oracle bytes. Little-endian x86 host, like p4runs.
#include "../flat/flat.h"
#include <omp.h>

enum { encInt = 1, encRel32 = 2, encReloc = 3 };

typedef struct {
	u64 off;
	i64 add;
	u32 sym;
	u16 type;
	u8 siz;
} Rel;
// Each worker owns a contiguous range of functions and a reusable relocation vector.
// Concatenating worker vectors in range order preserves serial traversal order.
typedef struct { Rel *a; size_t n, cap; } RelVec;
static void push(RelVec *v, Rel r) {
	if (v->n == v->cap) {
		size_t cap = v->cap ? v->cap * 2 : 4096;
		Rel *a = realloc(v->a, cap * sizeof(Rel));
		if (!a) { perror("relocations"); exit(2); }
		v->a = a; v->cap = cap;
	}
	v->a[v->n++] = r;
}
static u8 *output(size_t size) {
	size_t page = 2u << 20, n = (size + page - 1) & ~(page - 1);
	if (!n) n = page;
	u8 *p = MAP_FAILED;
	if (!getenv("P4_HUGE") || atoi(getenv("P4_HUGE")))
		p = mmap(NULL, n, PROT_READ | PROT_WRITE,
		         MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | (21 << MAP_HUGE_SHIFT), -1, 0);
	if (p != MAP_FAILED) { printf("output: MAP_HUGETLB 2 MB\n"); return p; }
	p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) { perror("output"); exit(2); }
	int advised = madvise(p, n, MADV_HUGEPAGE) == 0;
	printf("output: anonymous fallback, MADV_HUGEPAGE %s (THP not guaranteed)\n", advised ? "accepted" : "failed");
	return p;
}
static int cmpRel(const void *a, const void *b) {
	const Rel *x = a, *y = b;
	if (x->off != y->off) return x->off < y->off ? -1 : 1;
	if (x->type != y->type) return x->type < y->type ? -1 : 1;
	if (x->sym != y->sym) return x->sym < y->sym ? -1 : 1;
	if (x->add != y->add) return x->add < y->add ? -1 : 1;
	return (int)x->siz - (int)y->siz;
}


// Prepared input: one copy plus a linear hole list per run. Values remain dynamic
// patch records; no oracle data is used until after emission.
typedef struct { u64 at; i64 v, aux; u16 type; u8 w, enc; } Hole;
typedef struct { u64 src, dst, len; u32 h0, h1; } Run;
// H2 uses aux as the bit mask and v as the already positioned integer bits.
// memcpy makes unaligned 64-bit loads/stores well-defined in C.
static int byAt(const void *a, const void *b) {
    const Hole *x=a,*y=b; return (x->at>y->at)-(x->at<y->at);
}
int main(int argc, char **argv) {
 double prepStart=now();
 char mode=argc>2 && argv[2][0]=='H' ? argv[2][1] : '1';
 if (!mode || !strchr("012",mode)) return 2;
 flatDir = argc > 1 ? argv[1] : "data/minio-p4";
 int count=getenv("P4_COUNTS")!=NULL;
 u64 widths[9]={0}, encs[4]={0}, repeat=0, constant=0, either=0;
 u64 *seen=NULL; i64 *previous=NULL;
 if(count) {
     size_t n=metaGet("nthole");
     seen=calloc(n,sizeof(u64)); previous=calloc(n,sizeof(i64));
     if(!seen||!previous)return 2;
 }
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


    int reps=getenv("P4_REPS") ? atoi(getenv("P4_REPS")) : 1;
    if (reps<1) return 2;
    u64 *at=calloc((size_t)nitem+1,sizeof(u64));
    u32 *fnRun=calloc((size_t)nfn+1,sizeof(u32));
    Run *runs=calloc((size_t)nitem+nfn+1,sizeof(Run));
    size_t hcap=metaGet("nop")+metaGet("ngrel")+1;
    Hole *holes=calloc(hcap,sizeof(Hole));
    u8 *blob=calloc(ncode ? ncode : 1,1);
    if (!at || !fnRun || !runs || !holes || !blob) return 2;
    u32 nh=0, nrun=0; u64 pos=0;
    for (u32 f=0;f<nfn;f++) {
        fnRun[f]=nrun;
        for (u32 i=fnItem[f];i<fnItem[f+1];i++) {
            u32 r=itemRef[i], h0=0,h1=0,lo=0,hi=0,t=0;
            const u8 *src; u64 len;
            if (!itemKind[i]) {
                src=glueBin+glueOff[r]; len=glueOff[r+1]-glueOff[r];
                h0=glueRelOff[r]; h1=glueRelOff[r+1];
            } else {
                t=nodeTmpl[r]; u32 seg=itemSeg[i], b=tmplSegOff[t];
                lo=seg ? tsegEnd[b+seg-1]:0; hi=tsegEnd[b+seg];
                src=tmplBin+tmplOff[t]+lo; len=hi-lo;
                h0=tmplHoleOff[t]; h1=tmplHoleOff[t+1];
                while (h0<h1) { u32 m=(h0+h1)/2; if(holeAt[m]<lo)h0=m+1;else h1=m; }
                h1=h0; while(h1<tmplHoleOff[t+1] && holeAt[h1]<hi)h1++;
            }
            if (i==fnItem[f]) {
                runs[nrun++]=(Run){pos,pos,0,nh,nh};
            }
            Run *run=&runs[nrun-1];
            at[i]=pos; memcpy(blob+pos,src,len);
            for (u32 h=h0;h<h1;h++) {
                if (nh>=hcap) { fprintf(stderr,"hole capacity\n"); return 2; }
                Hole x={0};
                if (!itemKind[i]) {
                    x=(Hole){pos-run->dst+grelOff[h],grelSym[h],grelAdd[h],grelType[h],grelSiz[h],encReloc};
                } else {
                    u32 o=nodeOp[r]+h-tmplHoleOff[t];
                    x=(Hole){pos-run->dst+holeAt[h]-lo,opVal[o],opAux[o],holeRtype[h],holeW[h],holeEnc[h]};
                }
                if(count) {
                    if(x.w>8 || x.enc<1 || x.enc>3)return 2;
                    widths[x.w]++; encs[x.enc]++;
                    if(x.enc==encInt) {
                        int eq=seen[h]==(u64)f+1 && previous[h]==x.v;
                        int zero=memcmp(blob+run->dst+x.at,&x.v,x.w)==0;
                        repeat+=eq; constant+=zero; either+=eq||zero;
                        seen[h]=(u64)f+1; previous[h]=x.v;
                    }
                }
                if (x.at+x.w > run->len+len) { fprintf(stderr,"hole outside run\n"); return 2; }
                // Relocation fields must already be zero in this extracted input.
                if(x.enc==encReloc) for(u32 k=0;k<x.w;k++)
                    if(blob[run->dst+x.at+k]) { fprintf(stderr,"nonzero relocation field\n"); return 2; }
                memset(blob+run->dst+x.at,0,x.w);
                holes[nh++]=x;
            }
            run->h1=nh; run->len+=len; pos+=len;
        }
        // Keep exactly one run for empty functions, as in B.
        if (fnRun[f]==nrun) runs[nrun++]=(Run){pos,pos,0,nh,nh};
    }
    at[nitem]=pos; fnRun[nfn]=nrun;
    if(pos!=ncode) return 2;
    double constructEnd=now();
    if(count) {
        u64 pairs=0,touch=0,near=0,fit=0,ipairs=0;
        for(u32 r=0;r<nrun;r++) {
            Run *run=&runs[r]; size_t n=run->h1-run->h0;
            Hole *sorted=malloc((n?n:1)*sizeof(Hole));
            if(!sorted)return 2;
            memcpy(sorted,holes+run->h0,n*sizeof(Hole));
            qsort(sorted,n,sizeof(Hole),byAt);
            Hole *prev=NULL;
            for(size_t j=0;j<n;j++) {
                Hole *x=&sorted[j]; if(x->enc==encReloc)continue;
                if(prev && x->at>=prev->at+prev->w) {
                    u64 gap=x->at-(prev->at+prev->w); pairs++;
                    touch+=gap==0; near+=gap<=8;
                    if(x->at+x->w-prev->at<=8 && prev->at+8<=run->len) {
                        fit++; ipairs+=prev->enc==encInt && x->enc==encInt;
                    }
                }
                prev=x;
            }
            free(sorted);
        }
        printf("counts: records %u integer %llu rel32 %llu relocation %llu markers %llu\n",
            nh,(unsigned long long)encs[1],(unsigned long long)encs[2],
            (unsigned long long)encs[3],(unsigned long long)metaGet("nmark"));
        printf("integer reuse: previous_same_value %llu template_bytes_equal %llu union %llu\n",
            (unsigned long long)repeat,(unsigned long long)constant,(unsigned long long)either);
        printf("adjacent writable pairs: total %llu touching %llu gap_le_8 %llu span_le_8_in_function %llu integer_pairs_fit %llu\n",
            (unsigned long long)pairs,(unsigned long long)touch,(unsigned long long)near,
            (unsigned long long)fit,(unsigned long long)ipairs);
        for(int w=0;w<=8;w++)printf("width %d: %llu\n",w,(unsigned long long)widths[w]);
        free(seen);free(previous);
    }
    // The sorted adjacency analysis is excluded here; inline counts affect construct.
    // P4_COUNTS is disabled for every preparation/emit benchmark.
    double transformStart=now();
    u32 out=0; u64 groups=0,merged=0,ints=0,jumps=0,relocs=0;
    for(u32 r=0;r<nrun;r++) {
        Run *run=&runs[r]; u32 end=run->h1,begin=run->h0; run->h0=out;
        for(u32 h=begin;h<end;h++) {
            Hole x=holes[h];
            if(x.enc==encInt)ints++;
            if(x.enc==encRel32)jumps++;
            if(x.enc==encReloc)relocs++;
            if(x.enc==encInt && mode!='2') {
                memcpy(blob+run->src+x.at,&x.v,x.w);
                if(mode=='1')continue;
            }
            if(mode=='2' && x.enc==encInt && x.at+8<=run->len) {
                u64 mask=0,value=0; u32 k=h;
                while(k<end && holes[k].enc==encInt && holes[k].at>=x.at &&
                      holes[k].at+holes[k].w<=x.at+8) {
                    const Hole *z=&holes[k];
                    u64 bits=z->w==8 ? UINT64_MAX : (1ull<<(8*z->w))-1;
                    unsigned shift=8*(z->at-x.at);
                    // Extracted holes must not overlap.
                    if(mask & (bits<<shift)) {fprintf(stderr,"overlapping holes\n");return 2;}
                    mask|=bits<<shift; value|=((u64)z->v&bits)<<shift; k++;
                }
                if(k>h+1) {
                    ints+=k-h-1; groups++; merged+=k-h;
                    x.enc=4; x.w=8; x.v=(i64)value; x.aux=(i64)mask; h=k-1;
                }
            }
            holes[out++]=x;
        }
        run->h1=out;
    }
    nh=out;
    double prepEnd=now();
    printf("prep H%c: construct %.3f ms transform %.3f ms total %.3f ms; blob_bytes %llu records %u integer_holes %llu rel32 %llu relocations %llu merged_holes %llu groups %llu record_bytes %llu\n",
        mode,(constructEnd-prepStart)*1e3,(prepEnd-transformStart)*1e3,
        (constructEnd-prepStart+prepEnd-transformStart)*1e3,(unsigned long long)ncode,nh,
        (unsigned long long)ints,(unsigned long long)jumps,(unsigned long long)relocs,
        (unsigned long long)merged,(unsigned long long)groups,(unsigned long long)nh*sizeof(Hole));
    omp_set_dynamic(0); int T=1;
    #pragma omp parallel
    {
        #pragma omp single
        T=omp_get_num_threads();
    }
    u8 *code=output(ncode);
    RelVec *vec=calloc(T,sizeof(RelVec));
    u64 *relOff=calloc(T+1,sizeof(u64));
    u32 *firstFn=calloc(T+1,sizeof(u32));
    Rel *rel=malloc(sizeof(Rel)*((size_t)nrel+1));
    if(!vec||!relOff||!firstFn||!rel) return 2;
    // Same original-item quantile partition as p4par for every variant.
    for(int w=0;w<=T;w++) {
        u32 lo=0,hi=nfn; u64 target=(u64)nitem*w/T;
        while(lo<hi) {u32 m=lo+(hi-lo)/2;if(fnItem[m]<target)lo=m+1;else hi=m;}
        firstFn[w]=lo;
    }
    firstFn[0]=0;firstFn[T]=nfn;
    double emitBest=1e9,total=0; u64 nr=0,nerr=0;
    for(int rep=0;rep<=reps;rep++) {
        double start=now(); nerr=0;
        #pragma omp parallel for schedule(static,1) num_threads(T) reduction(+:nerr)
        for(int worker=0;worker<T;worker++) {
            RelVec local=vec[worker];local.n=0;
            for(u32 f=firstFn[worker];f<firstFn[worker+1];f++) {
                u64 fstart=at[fnItem[f]];
                for(u32 m=fnMarkOff[f];m<fnMarkOff[f+1];m++)
                    push(&local,(Rel){fstart+markOff[m],markAdd[m],markSym[m],markType[m],0});
                for(u32 r=fnRun[f];r<fnRun[f+1];r++) {
                    const Run *run=&runs[r];
                    memcpy(code+run->dst,blob+run->src,run->len);
                    for(u32 h=run->h0;h<run->h1;h++) {
                        const Hole *x=&holes[h]; u64 hp=run->dst+x->at;
                        switch(x->enc) {
                        case encInt:
                            if(mode=='0')break; // Checked store-free diagnostic; integers pre-baked.
                            switch(x->w) {
                            case 1: code[hp]=(u8)x->v;break;
                            case 2: {u16 v=x->v;memcpy(code+hp,&v,2);break;}
                            case 4: {u32 v=x->v;memcpy(code+hp,&v,4);break;}
                            case 8: memcpy(code+hp,&x->v,8);break;
                            default:nerr++;
                            } break;
                        case 4: {u64 v;memcpy(&v,code+hp,8);v=(v & ~(u64)x->aux)|(u64)x->v;memcpy(code+hp,&v,8);break;}
                        case encRel32: {i32 v=(i32)((i64)at[x->v]+x->aux-(i64)(hp+4));memcpy(code+hp,&v,4);break;}
                        case encReloc:push(&local,(Rel){hp,x->aux,(u32)x->v,x->type,x->w});break;
                        default:nerr++;
                        }
                    }
                }
            }
            vec[worker]=local;
        }
        for(int w=0;w<T;w++)relOff[w+1]=relOff[w]+vec[w].n;
        nr=relOff[T]; if(nr!=nrel)return 2;
        #pragma omp parallel for schedule(static) num_threads(T)
        for(int w=0;w<T;w++)if(vec[w].n)memcpy(rel+relOff[w],vec[w].a,vec[w].n*sizeof(Rel));
        double elapsed=now()-start;
        if(rep) {if(elapsed<emitBest)emitBest=elapsed;total+=elapsed;}
    }
    printf("p4holes H%c (%d threads): emit %.3f ms; GB/s %.6f; reps %d total %.3f s\n",mode,T,emitBest*1e3,ncode/emitBest/1e9,reps,total);
    // Deliberate post-emit faults exercise each independent oracle check.
    const char *fault=getenv("P4_FAULT");
    if(fault && !strcmp(fault,"byte") && ncode)code[0]^=1;
    if(fault && !strcmp(fault,"offset") && nfn)at[fnItem[0]]++;
    if(fault && !strcmp(fault,"relocation") && nr)rel[0].add++;
    qsort(rel,nr,sizeof(Rel),cmpRel);
	// ---- check
	long bad = nerr != 0;
	size_t m;
	const u8 *want = FMAP(u8, "oracle/p4_code.bin", &m);
	const u64 *wfn = FMAP(u64, "oracle/p4_fn_off.u64", NULL);
	for(u32 f=0;f<=nfn;f++) if(at[fnItem[f]]!=wfn[f]) bad++;
	printf("function offsets: %u checked, %ld errors\n",nfn+1,bad);
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
	printf(bad ? "P4 FAIL\n" : "P4 OK\n");
	return bad != 0;
}
