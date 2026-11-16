// Full-update regressions for witness loss, alternative supports, and cycles.
// gcc -O2 -o build/test-p2inc-witness ref/flat/test-p2inc-witness.c
#define main p2inc_main
#include "p2inc.c"
#undef main
#include <assert.h>

typedef struct { u32 src; u8 kind; u64 val; } testEdge;
static void fixture(const testEdge *edges, size_t ne, int method) {
    N=nslA=12; nstrU=1; nmeth=method;
    flags=calloc(N+1,1); live=calloc(N+1,1); iface=calloc(N+1,1);
    typeDone=calloc(N+1,1); isRoot=calloc(N+1,1);
    ordL=calloc(N+1,4); ordI=calloc(N+1,4); ordT=calloc(N+1,4);
    posL=calloc(N+1,4); posI=calloc(N+1,4); expanded=calloc(N+1,4);
    witL=calloc(N+1,sizeof(witness)); witI=calloc(N+1,sizeof(witness));
    ordM=calloc(nmeth+1,4); posM=calloc(nmeth+1,4);
    witM=calloc(nmeth+1,sizeof(witness)); called=calloc(nmeth+1,1);
    namedCnt=calloc(nstrU+1,4); eoff=calloc(N+2,4); roff=calloc(N+2,4);
    ekind=calloc(ne+1,1); eval=calloc(ne+1,8); rsrc=calloc(ne+1,4); rkind=calloc(ne+1,1);
    revoff=calloc(N+2,4); revm=calloc(4,4);
    moff=calloc(N+2,4); mown=calloc(1,4); mname=calloc(1,4);
    mtyp=calloc(1,4); mifn=calloc(1,4); mtfn=calloc(1,4); mexp=calloc(1,1);
    nameoff=calloc(nstrU+2,4); namem=calloc(1,4); expm=calloc(1,4);
    nsoff=calloc(nstrU+2,4); namesrc=calloc(ne+1,4); sigsrc=calloc(ne+1,4);
    reflsrc=calloc(N+1,4); bysig=calloc(1,4);
    addHead=calloc(N+1,4); raddHead=calloc(N+1,4); maybeRem=calloc(N+1,1);
    poolcap=16; pool=calloc(poolcap,sizeof(*pool));
    rmask=15; rem=malloc((rmask+1)*sizeof(*rem));
    for(u64 h=0;h<=rmask;h++) rem[h].alive=0xff;
    smask=15; sigs=calloc(smask+1,sizeof(*sigs));
    for(u32 x=1;x<=N;x++) flags[x]=fDef;
    u32 ei=0,ri=0;
    for(u32 x=0;x<=N;x++) {
        eoff[x]=ei; roff[x]=ri;
        for(size_t j=0;j<ne;j++) if(edges[j].src==x) { ekind[ei]=edges[j].kind; eval[ei++]=edges[j].val; }
        for(size_t j=0;j<ne;j++) if(edges[j].kind<=1 && edges[j].val==x) { rsrc[ri]=edges[j].src; rkind[ri++]=edges[j].kind; }
    }
    eoff[N+1]=ei; roff[N+1]=ri;
    if(method) {
        mown[0]=5; mname[0]=1; mtyp[0]=6; mifn[0]=7;
        for(u32 x=6;x<=N+1;x++) moff[x]=1;
        revoff[7]=1; for(u32 x=8;x<=N+1;x++) revoff[x]=2;
        nameoff[2]=1;
        sigent *se=sig((u64)1<<32|6); se->mlen=1;
        for(size_t j=0;j<ne;j++) if(edges[j].kind==2) sigsrc[se->slen++]=edges[j].src;
        flags[5]|=fGoType|fIface; flags[6]|=fGoType;
        isRoot[5]=1;
    }
    epoch=1; witnesses=1;
    counts=getenv("COUNTS")!=NULL; if(counts) smallL=calloc(N+1,4);
    breadthFirst=getenv("LIFO")==NULL;
}
static void assertCounts(void) {
    if(!counts) return;
    for(u32 target=1;target<=N;target++) if(live[target]) {
        u32 expected=0;
        for(u32 source=1;source<=N;source++) if(live[source] && (flags[source]&fDef) && ordL[source]<ordL[target]) {
            for(u32 e=eoff[source];e<eoff[source+1];e++)
                expected+=ekind[e]==0 && eval[e]==target && !removed(source,0,target);
            for(u32 a=addHead[source];a;a=pool[a].next)
                expected+=pool[a].alive && pool[a].kind==0 && pool[a].val==target;
        }
        assert(smallL[target]==expected);
    }
}
static void start(void) {
    for(u32 x=1;x<=N;x++) if(flags[x]&fIface) setIface(x);
    for(u32 x=1;x<=N;x++) if(isRoot[x]) setLive(x);
    drain(); assertCounts();
}
static void removeEdge(u32 src,u8 kind,u64 val) {
    side minus={.ne=1,.esrc=&src,.ekind=&kind,.eval=&val}, plus={0};
    update(&minus,&plus);
    assertCounts(); assert(!scratchCheck(0));
}
static void cycle(void) {
    const testEdge e[]={{1,0,2},{2,0,3},{3,0,2},{4,0,2}};
    fixture(e,4,0); isRoot[1]=isRoot[4]=1; start();
    assert(live[2] && live[3]);
    u32 first=witL[2].id, other=first==1?4:1;
    assert(first==1 || first==4);
    removeEdge(first,0,2); assert(live[2] && live[3]);
    removeEdge(other,0,2); assert(!live[2] && !live[3]);
    puts("witness alternative and dead plain cycle OK");
}
static void overlay(void) {
    fixture(NULL,0,0); isRoot[1]=isRoot[4]=1;
    edgeAdd(1,0,2); edgeAdd(4,0,2); edgeAdd(2,0,3); edgeAdd(3,0,2); start();
    u32 first=witL[2].id, other=first==1?4:1;
    removeEdge(first,0,2); assert(live[2] && live[3]);
    removeEdge(other,0,2); assert(!live[2] && !live[3]);
    puts("witness overlay edge removal and dead cycle OK");
}
static void methodCycle(void) {
    const u64 key=(u64)1<<32|6;
    const testEdge e[]={{1,0,2},{2,2,key},{7,2,key}};
    fixture(e,3,1); isRoot[1]=1; start();
    assert(called[0] && live[6] && live[7]);
    assert(ordL[2]<ordM[0] && ordL[7]>ordM[0]);
    removeEdge(1,0,2);
    assert(!called[0] && !live[2] && !live[6] && !live[7]);
    assert(iface[6]); // Owner 5 still supplies its method's interface type.
    puts("witness method condition cannot self-support through its target OK");
}
static void defLoss(void) {
    const testEdge e[]={{1,0,2},{2,0,3},{3,0,2}};
    fixture(e,3,0); isRoot[1]=1; start();
    u32 owner=1; u8 before=fDef,after=0;
    side mi={.nf=1,.fid=&owner,.fval=&before},pl={.nf=1,.fid=&owner,.fval=&after};
    update(&mi,&pl); assertCounts(); assert(!scratchCheck(0));
    assert(live[1] && !live[2] && !live[3]);
    puts("witness source definition loss OK");
}
static void ifaceCycle(void) {
    const testEdge e[]={{1,0,2},{2,0,3},{3,0,2},{4,0,2}};
    fixture(e,4,0); isRoot[1]=isRoot[4]=1;
    flags[1]|=fGoType|fIface; flags[4]|=fGoType|fIface;
    flags[2]|=fGoType; flags[3]|=fGoType; start();
    assert(live[2] && live[3] && iface[2] && iface[3]);
    assert(witI[2].kind==W_TYPE);
    u32 first=witI[2].id, other=first==1?4:1;
    removeEdge(first,0,2); assert(live[2] && live[3] && iface[2] && iface[3]);
    removeEdge(other,0,2); assert(!live[2] && !live[3] && !iface[2] && !iface[3]);
    puts("witness interface type alternative and dead type cycle OK");
}
static void overlayCondition(void) {
    fixture(NULL,0,1); isRoot[1]=isRoot[4]=1;
    u64 key=(u64)1<<32|6;
    edgeAdd(1,2,key); edgeAdd(4,2,key); start();
    assert(called[0] && live[6] && live[7] && witM[0].kind==W_OVERLAY);
    u32 first=pool[witM[0].id].src, other=first==1?4:1;
    removeEdge(first,2,key); assert(called[0] && live[6] && live[7]);
    // Re-add appends a new overlay slot. A saved slot must never alias it.
    u8 kind=2;
    side mi={0},pl={.ne=1,.esrc=&first,.ekind=&kind,.eval=&key};
    update(&mi,&pl); assertCounts(); assert(!scratchCheck(0));
    removeEdge(other,2,key); assert(called[0] && live[6] && live[7]);
    removeEdge(first,2,key); assert(!called[0] && !live[6] && !live[7]);
    assert(live[1] && live[4]); // Removing the edge alone invalidates its witness.
    puts("witness overlay method condition removal and re-add OK");
}
int main(int argc,char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"cycle")) cycle();
    else if(!strcmp(argv[1],"overlay")) overlay();
    else if(!strcmp(argv[1],"method")) methodCycle();
    else if(!strcmp(argv[1],"flags")) defLoss();
    else if(!strcmp(argv[1],"iface")) ifaceCycle();
    else if(!strcmp(argv[1],"condition-overlay")) overlayCondition();
    else return 2;
    return 0;
}
