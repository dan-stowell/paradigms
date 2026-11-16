// CP7 run preparation and emission; A: absorb hole-free neighbours, B: function
// blobs, C: deduplicated B by template sequence and byte/hole shape,
// D: prepared but unmerged control. Preparation never reads the oracle.
// check-runs.sh builds and checks; bench-runs.sh measures interleaved warm kernels.
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
static int sameShape(const Run *a, const Run *b, const u8 *blob, const Hole *holes) {
    if (a->len != b->len || a->h1-a->h0 != b->h1-b->h0) return 0;
    if (memcmp(blob+a->src, blob+b->src, a->len)) return 0;
    for (u32 j=0; j<a->h1-a->h0; j++) {
        const Hole *x=&holes[a->h0+j], *y=&holes[b->h0+j];
        if (x->at!=y->at || x->w!=y->w || x->enc!=y->enc || x->type!=y->type) return 0;
    }
    return 1;
}
static u64 shapeHash(const Run *r, const u8 *blob, const Hole *holes) {
    u64 h=1469598103934665603ull;
    for (u64 j=0;j<r->len;j++) h=(h^blob[r->src+j])*1099511628211ull;
    for (u32 j=r->h0;j<r->h1;j++) {
        const Hole *x=&holes[j];
        h=mix64(h^x->at); h=mix64(h^x->w^((u64)x->enc<<8)^((u64)x->type<<16));
    }
    return h;
}
int main(int argc, char **argv) {
 double prepStart=now();
 char mode=argc>2 ? argv[2][0] : 'B';
 if (!strchr("ABCD",mode)) return 2;
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
        int hasSensitive=0;
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
            int sensitive=h1>h0;
            if (i==fnItem[f] || mode=='D' || (mode=='A' && sensitive && hasSensitive)) {
                runs[nrun++]=(Run){pos,pos,0,nh,nh}; hasSensitive=0;
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
                if (x.at+x.w > run->len+len) { fprintf(stderr,"hole outside run\n"); return 2; }
                // Relocation fields must already be zero in this extracted input.
                if(x.enc==encReloc) for(u32 k=0;k<x.w;k++)
                    if(blob[run->dst+x.at+k]) { fprintf(stderr,"nonzero relocation field\n"); return 2; }
                memset(blob+run->dst+x.at,0,x.w);
                holes[nh++]=x;
            }
            run->h1=nh; run->len+=len; pos+=len; hasSensitive|=sensitive;
        }
        // Keep exactly one run for empty functions in B/C.
        if ((mode=='B'||mode=='C') && fnRun[f]==nrun) runs[nrun++]=(Run){pos,pos,0,nh,nh};
    }
    at[nitem]=pos; fnRun[nfn]=nrun;
    if(pos!=ncode) return 2;
    u32 distinct=nrun; u64 blobBytes=ncode;
    double constructEnd=now();
    if(mode=='C') {
        size_t cap=1; while(cap<(size_t)nrun*2)cap*=2;
        u32 *table=calloc(cap,sizeof(u32));
        u64 *canonical=calloc(nrun,sizeof(u64));
        u8 *packed=malloc(ncode ? ncode:1);
        if(!table||!canonical||!packed) return 2;
        distinct=0; blobBytes=0;
        for(u32 i=0;i<nrun;i++) {
            u64 hash=shapeHash(&runs[i],blob,holes);
            for(u32 j=fnItem[i];j<fnItem[i+1];j++) {
                u64 key=itemKind[j] ? ((u64)nodeTmpl[itemRef[j]]<<32)|itemSeg[j] : at[j+1]-at[j];
                hash=mix64(hash^key^((u64)itemKind[j]<<63));
            }
            size_t slot=hash&(cap-1);
            while(table[slot]) {
                u32 candidate=table[slot]-1;
                int same=fnItem[i+1]-fnItem[i]==fnItem[candidate+1]-fnItem[candidate];
                for(u32 j=0;same && j<fnItem[i+1]-fnItem[i];j++) {
                    u32 x=fnItem[i]+j,y=fnItem[candidate]+j;
                    same=itemKind[x]==itemKind[y];
                    if(same && itemKind[x]) same=nodeTmpl[itemRef[x]]==nodeTmpl[itemRef[y]] && itemSeg[x]==itemSeg[y];
                    if(same && !itemKind[x]) same=at[x+1]-at[x]==at[y+1]-at[y];
                }
                if(same && sameShape(&runs[i],&runs[candidate],blob,holes)) break;
                slot=(slot+1)&(cap-1);
            }
            if(!table[slot]) {
                table[slot]=i+1; canonical[i]=blobBytes; distinct++;
                memcpy(packed+blobBytes,blob+runs[i].src,runs[i].len); blobBytes+=runs[i].len;
            } else canonical[i]=canonical[table[slot]-1];
        }
        for(u32 i=0;i<nrun;i++)runs[i].src=canonical[i];
        free(blob); blob=packed; free(canonical); free(table);
    }
    double prepEnd=now();
    u64 inputBytes=blobBytes+(u64)nrun*sizeof(Run)+(u64)nh*sizeof(Hole)
        +2*((u64)nfn+1)*sizeof(u32)+((u64)nitem+1)*sizeof(u64)
        +((u64)nfn+1)*4+(u64)metaGet("nmark")*(4+2+4+8);
    printf("prep %c: construct %.3f ms, dedupe %.3f ms, total %.3f ms; runs %u distinct %u blob_bytes %llu holes %u bytes_per_run %.3f pieces_per_MB %.3f input_bytes %llu\n",
        mode,(constructEnd-prepStart)*1e3,(prepEnd-constructEnd)*1e3,(prepEnd-prepStart)*1e3,nrun,distinct,
        (unsigned long long)blobBytes,nh,(double)ncode/nrun,(double)nrun*1e6/ncode,(unsigned long long)inputBytes);
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
                            switch(x->w) {
                            case 1: code[hp]=(u8)x->v;break;
                            case 2: {u16 v=x->v;memcpy(code+hp,&v,2);break;}
                            case 4: {u32 v=x->v;memcpy(code+hp,&v,4);break;}
                            case 8: memcpy(code+hp,&x->v,8);break;
                            default:nerr++;
                            } break;
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
    printf("p4runs %c (%d threads): emit %.3f ms; GB/s %.6f; reps %d total %.3f s\n",mode,T,emitBest*1e3,ncode/emitBest/1e9,reps,total);
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
