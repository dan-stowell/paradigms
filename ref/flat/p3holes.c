// Maintained package/bucket layout experiment. See results/c-relayout.md.
#include "flat.h"
#include <assert.h>
#include <limits.h>
#define CK(x)                                                                                      \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "check failed %s:%d: %s\n", __FILE__, __LINE__, #x);                   \
            exit(2);                                                                               \
        }                                                                                          \
    } while (0)
typedef struct {
    u32 off, tgt;
    i64 add;
    u8 size, kind;
} Rel;
typedef struct {
    u32 id, group, unit, size, occ, align, addr, oldaddr, nr;
    const u8 *data;
    u32 len;
    Rel *r;
    u32 first;
    const u8 *str;
    u32 slen;
} Sym;
typedef struct {
    Sym *s;
    u32 n;
} Group;
typedef struct {
    u32 L, N, ng, nobj;
    Sym *s;
    Group *g;
    u32 *index, *fixedid, *fixedaddr, nfix;
    u32 sect[8], fallback[2], start[16], end[16], bmin[16], base;
    const u8 *oracle;
    size_t nimage;
} Data;
typedef struct {
    u32 base, cap, align;
} Arena;
typedef struct {
    u32 src, r, prev, next, tgt;
} Node;
typedef struct { u32 lo, len, next; } Hole;
typedef struct {
    Data *a, *b;
    Arena *ar;
    u32 *address, *head;
    Node *node;
    u32 nn, U;
    u32 *alloccap, *holehead, *tail;
    Hole *holes; u32 nh, freehole;
    u32 spare[16], spareend[16], bend[16];
    u32 *table, tablecap, tablen; u64 tablewrites;
    u8 *img, *dirty, *moved, *changed;
    u32 *groups, ng, *ids, ni, *todo, nt;
    size_t bytes;
    u32 sect[8];
    u64 copied, cleared, patched, written, visited, laid, patch_bucket[16];
} State;
static u32 up(u32 x, u32 a) { return (x + a - 1) / a * a; }
static u32 uid(const u32 *map, u32 x) { return map ? map[x] : x; }
static int order(const void *aa, const void *bb) {
    const Sym *a = aa, *b = bb;
    if (a->group != b->group)
        return a->group < b->group ? -1 : 1;
    if (a->group == 0) {
        int c = memcmp(a->str, b->str, a->slen < b->slen ? a->slen : b->slen);
        if (c)
            return c;
        if (a->slen != b->slen)
            return a->slen < b->slen ? -1 : 1;
    } else if (a->align != b->align)
        return a->align > b->align ? -1 : 1;
    return a->id < b->id ? -1 : a->id > b->id;
}
static Data load(const char *path, int edit, u32 universe) {
    flatDir = path;
    Data d = {0};
    d.L = metaGet("p3_placed");
    d.N = metaGet("nids");
    d.nobj = metaGet("nobjs");
    d.ng = 1 + 16 * d.nobj;
    d.base = metaGet("imageBase");
    const u32 *map = edit ? FMAP(u32, "diff/idmap_b.u32", NULL) : NULL;
    if (!universe) {
        universe = d.N;
        if (map)
            for (u32 i = 0; i < d.N; i++)
                if (map[i] >= universe)
                    universe = map[i] + 1;
    }
    d.index = calloc(universe, 4);
    d.s = calloc(d.L, sizeof(Sym));
    d.g = calloc(d.ng, sizeof(Group));
    const u32 *sym = FMAP(u32, "p3_sym.u32", NULL), *sz = FMAP(u32, "p3_size.u32", NULL),
              *unit = FMAP(u32, "p3_unit.u32", NULL), *obj = FMAP(u32, "slot_obj.u32", NULL),
              *adr = FMAP(u32, "oracle/p3_addr.u32", NULL),
              *canon = FMAP(u32, "oracle/p1c_canon.u32", NULL);
    const u8 *bucket = FMAP(u8, "p3_bucket.u8", NULL), *cl = FMAP(u8, "p3_class.u8", NULL),
             *bytes = FMAP(u8, "p3_data.bin", NULL);
    const u64 *off = FMAP(u64, "p3_data.off.u64", NULL),
              *str_off = FMAP(u64, "p3_str.off.u64", NULL);
    const u8 *str = FMAP(u8, "p3_str.bin", NULL);
    const u32 *tl = FMAP(u32, "p3_tlstr.u32", NULL);
    const u32 *rs = FMAP(u32, "p3_rel_start.u32", NULL), *ro = FMAP(u32, "p3_rel_off.u32", NULL),
              *rt = FMAP(u32, "p3_rel_tgt.u32", NULL);
    const u8 *rk = FMAP(u8, "p3_rel_kind.u8", NULL), *rz = FMAP(u8, "p3_rel_siz.u8", NULL);
    const i64 *ra = FMAP(i64, "p3_rel_add.i64", NULL);
    u8 *present = calloc(d.N, 1);
    for (u32 i = 0; i < d.L; i++)
        present[sym[i]] = 1;
    size_t nf0;
    const u32 *fi0 = FMAP(u32, "p3_fixed_id.u32", &nf0);
    for (u32 i = 0; i < nf0 / 4; i++)
        present[fi0[i]] = 1;
    u32 slots = metaGet("nslots"),
        fall[2] = {metaGet("addr_unreachableMethod"), metaGet("addr_mapinitnoop")};
    const char *keys[] = {"lay_text",      "lay_rodata", "lay_pclntab", "lay_types",
                          "lay_noptrdata", "lay_data",   "lay_bss",     "lay_noptrbss"};
    for (int i = 0; i < 8; i++)
        d.sect[i] = metaGet(keys[i]);
    memcpy(d.start, FMAP(u32, "p3_bucket_start.u32", NULL), 64);
    memcpy(d.end, d.start, 64);
    memcpy(d.bmin, FMAP(u32, "p3_bucket_minalign.u32", NULL), 64);
    for (u32 i = 0; i < d.L; i++) {
        Sym *s = d.s + i;
        u32 b = bucket[i];
        s->id = uid(map, sym[i]);
        s->group = b == 6 ? 0 : 1 + b * d.nobj + obj[sym[i]];
        s->unit = unit[i];
        s->size = sz[i];
        s->align = 1u << cl[i];
        s->occ = up(b <= 2 && sz[i] < 16 ? 16 : sz[i], s->align);
        s->oldaddr = adr[i];
        s->data = bytes + off[i];
        s->len = off[i + 1] - off[i];
        s->str = str + str_off[tl[i]];
        s->slen = str_off[tl[i] + 1] - str_off[tl[i]];
        s->nr = rs[i + 1] - rs[i];
        s->r = calloc(s->nr, sizeof(Rel));
        CK(s->len <= s->size);
        if (adr[i] + sz[i] > d.end[b])
            d.end[b] = adr[i] + sz[i];
        for (int k = 0; k < 2; k++)
            if (adr[i] == fall[k])
                d.fallback[k] = s->id;
        for (u32 j = 0; j < s->nr; j++) {
            u32 r = rs[i] + j, t = rt[r];
            Rel *q = s->r + j;
            q->off = ro[r];
            q->size = rz[r];
            q->kind = rk[r];
            q->add = ra[r];
            u32 canonical = t <= slots ? canon[t] : t;
            q->tgt = present[canonical] ? uid(map, canonical) : 0;
            if (!q->tgt && t && (q->kind == 1 || q->kind == 5 || q->kind == 7))
                CK(0);
            if (!t && (q->kind == 2 || q->kind == 6))
                q->kind = q->kind == 2 ? 1 : 5;
            CK(q->off + q->size <= s->size);
        }
    }
    // Materialize weak fallbacks as real target dependencies, so the reverse index sees them.
    for (u32 i = 0; i < d.L; i++)
        for (u32 j = 0; j < d.s[i].nr; j++) {
            Rel *r = d.s[i].r + j;
            if (!r->tgt && (r->kind == 2 || r->kind == 6))
                r->tgt = d.fallback[r->kind == 6];
        }
    size_t nf;
    const u32 *fi = FMAP(u32, "p3_fixed_id.u32", &nf), *fa = FMAP(u32, "p3_fixed_addr.u32", NULL);
    d.nfix = nf / 4;
    d.fixedid = calloc(d.nfix, 4);
    d.fixedaddr = calloc(d.nfix, 4);
    for (u32 i = 0; i < d.nfix; i++) {
        d.fixedid[i] = uid(map, fi[i]);
        d.fixedaddr[i] = fa[i];
    }
    qsort(d.s, d.L, sizeof(Sym), order);
    for (u32 i = 0; i < d.L; i++) {
        Sym *s = d.s + i;
        CK(s->id < universe);
        CK(!d.index[s->id]);
        d.index[s->id] = i + 1;
        if (!d.g[s->group].n)
            d.g[s->group].s = s;
        d.g[s->group].n++;
    }
    d.oracle = FMAP(u8, "oracle/p3_image.bin", &d.nimage);
    return d;
}
static u32 bucket(Data *d, u32 g) { return g ? (g - 1) / d->nobj : 6; }
static u32 need(Group *g) {
    u32 x = 0;
    for (u32 i = 0; i < g->n; i++) {
        Sym *s = g->s + i;
        x = up(x, s->align) + s->occ;
    }
    return x;
}
static u32 galign(Group *g) {
    u32 a = 1;
    for (u32 i = 0; i < g->n; i++)
        if (g->s[i].align > a)
            a = g->s[i].align;
    return a;
}
static int same(Sym *a, Sym *b) {
    if (a->group != b->group || a->size != b->size || a->align != b->align || a->len != b->len ||
        a->nr != b->nr || a->slen != b->slen)
        return 0;
    if (memcmp(a->data, b->data, a->len) || memcmp(a->str, b->str, a->slen))
        return 0;
    for (u32 j = 0; j < a->nr; j++) {
        Rel *x = a->r + j, *y = b->r + j;
        if (x->off != y->off || x->tgt != y->tgt || x->add != y->add || x->size != y->size ||
            x->kind != y->kind)
            return 0;
    }
    return 1;
}
static void layout(Group *g, Arena *a) {
    u32 x = a->base;
    for (u32 i = 0; i < g->n; i++) {
        Sym *s = g->s + i;
        x = up(x, s->align);
        s->addr = x;
        x += s->occ;
    }
    CK(x <= a->base + a->cap);
}
static u32 translate(State *s, u32 x) {
    Data *a = s->a;
    for (int b = 15; b >= 1; b--) {
        if (x >= a->end[b]) {
            u32 end = 0;
            for (u32 g = 0; g < a->ng; g++)
                if (bucket(a, g) == (u32)b && s->ar[g].base + s->ar[g].cap > end)
                    end = s->ar[g].base + s->ar[g].cap;
            return s->bend[b] + x - a->end[b];
        }
        if (x == a->start[b]) {
            u32 begin = UINT_MAX;
            for (u32 g = 0; g < a->ng; g++)
                if (bucket(a, g) == (u32)b && s->ar[g].cap && s->ar[g].base < begin)
                    begin = s->ar[g].base;
            return begin;
        }
        if (x >= a->start[b]) {
            fprintf(stderr, "external anchor inside occupied bucket %d\n", b);
            exit(2);
        }
    }
    return x;
}
static void patch(State *s, Sym *y, u32 j) {
    y->addr = s->address[y->id];
    Rel *r = y->r + j;
    u32 t = s->address[r->tgt];
    i64 v = 0;
    switch (r->kind) {
    case 1:
    case 2:
        CK(t || !r->tgt);
        v = (i64)t + r->add;
        break;
    case 4:
        if (!t) {
            v = -1;
            break;
        } /*fallthrough*/
    case 3: {
        if (!t)
            return;
        int k = 7;
        while (k && t < s->sect[k])
            k--;
        v = (i64)t - s->sect[k] + r->add;
        break;
    }
    case 5:
    case 6:
    case 7:
        CK(t || !r->tgt);
        v = (i64)t + r->add - (y->addr + r->off + r->size);
        break;
    case 8:
        v = r->add;
        break;
    default:
        CK(0);
    }
    memcpy(s->img + y->addr - s->a->base + r->off, &v, r->size);
    s->patched++;
    s->written += r->size;
    s->patch_bucket[bucket(s->a, y->group)]++;
}
static void copy(State *s, Data *d, Sym *y) {
    if (bucket(d, y->group) >= 13)
        return;
    y->addr = s->address[y->id];
    memcpy(s->img + y->addr - d->base, y->data, y->len);
    s->copied += y->len;
    for (u32 j = 0; j < y->nr; j++)
        patch(s, y, j);
}
static void unlink_node(State *s, u32 n) {
    Node *x = s->node + n;
    if (x->prev)
        s->node[x->prev].next = x->next;
    else
        s->head[x->tgt] = x->next;
    if (x->next)
        s->node[x->next].prev = x->prev;
}
static void link_node(State *s, u32 n) {
    Node *x = s->node + n;
    x->next = s->head[x->tgt];
    x->prev = 0;
    if (x->next)
        s->node[x->next].prev = n;
    s->head[x->tgt] = n;
}
static void nodes(State *s, Data *d) {
    for (u32 i = 0; i < d->L; i++) {
        Sym *y = d->s + i;
        y->first = s->nn;
        for (u32 j = 0; j < y->nr; j++) {
            Node *x = s->node + s->nn++;
            x->src = y->id;
            x->r = j;
            x->tgt = y->r[j].tgt;
        }
    }
}
static void mark(State *s, u32 id) {
    if (!s->dirty[id]) {
        s->dirty[id] = 1;
        s->todo[s->nt++] = id;
    }
}

// Address-ordered, coalescing free lists owned by the original arena even for
// overflow extents. No live allocation is compacted. Capacity is retained on shrink.
static u32 hnew(State *s) {
    u32 h=s->freehole;
    if(h) s->freehole=s->holes[h].next;
    else { h=++s->nh; CK(h < s->U + s->a->ng + 100); }
    return h;
}
static void hdrop(State*s,u32 h) { s->holes[h].next=s->freehole; s->freehole=h; }
static void release(State*s,u32 g,u32 lo,u32 len) {
    if(!len) return;
    u32 *p=&s->holehead[g];
    while(*p && s->holes[*p].lo < lo) p=&s->holes[*p].next;
    u32 h=hnew(s); s->holes[h]=(Hole){lo,len,*p}; *p=h;
    p=&s->holehead[g];
    while(*p) {
        Hole*x=&s->holes[*p]; u32 n=x->next;
        if(n && x->lo+x->len==s->holes[n].lo) {
            x->len+=s->holes[n].len; x->next=s->holes[n].next; hdrop(s,n);
        } else p=&x->next;
    }
}
static u32 reserve(State*s,Sym*y) {
    u32 g=y->group, n=y->occ, al=y->align;
    if(!n) n=al; // Zero-sized symbols still have a unique retained allocation.
    u32 *p=&s->holehead[g];
    while(*p) {
        u32 h=*p, lo=s->holes[h].lo, end=lo+s->holes[h].len, x=up(lo,al);
        if(x+n<=end) {
            *p=s->holes[h].next; hdrop(s,h);
            release(s,g,lo,x-lo); release(s,g,x+n,end-x-n);
            s->alloccap[y->id]=n; return x;
        }
        p=&s->holes[h].next;
    }
    u32 x=up(s->tail[g],al), end=s->ar[g].base+s->ar[g].cap;
    if(x+n<=end) { release(s,g,s->tail[g],x-s->tail[g]); s->tail[g]=x+n; }
    else {
        u32 b=bucket(s->a,g); x=up(s->spare[b],al);
        CK(x+n<=s->spareend[b]); release(s,g,s->spare[b],x-s->spare[b]); s->spare[b]=x+n;
    }
    s->alloccap[y->id]=n; return x;
}
static int retain(State*s,Sym*x,Sym*y) {
    return x && y && x->group==y->group && y->occ<=s->alloccap[y->id]
        && !(s->address[y->id] % y->align);
}
static void allocate_delta(State*s,Data*from,Data*to,u32*ids,u32 n) {
    // All frees precede all allocations; both lists are ascending unified ID.
    for(u32 k=0;k<n;k++) {
        u32 id=ids[k]; Sym*x=from->index[id]?from->s+from->index[id]-1:NULL;
        Sym*y=to->index[id]?to->s+to->index[id]-1:NULL;
        if(x && !retain(s,x,y)) {
            release(s,x->group,s->address[id],s->alloccap[id]);
            s->alloccap[id]=0;
        }
    }
    for(u32 k=0;k<n;k++) {
        u32 id=ids[k]; Sym*y=to->index[id]?to->s+to->index[id]-1:NULL;
        if(y) { Sym*x=from->index[id]?from->s+from->index[id]-1:NULL; y->addr=retain(s,x,y)?s->address[id]:reserve(s,y); s->laid++; }
    }
}
static void table_update(State*s,Data*d) {
    Group*g=&d->g[0]; CK(g->n<=s->tablecap);
    int change=g->n!=s->tablen;
    for(u32 i=0;i<g->n && !change;i++) change=s->table[i]!=s->address[g->s[i].id]-s->sect[3];
    s->tablewrites=0;
    if(change) {
        u32 count=g->n>s->tablen?g->n:s->tablen;
        if(s->tablen>g->n)memset(s->table+g->n,0,(s->tablen-g->n)*4ull);
        for(u32 i=0;i<g->n;i++) s->table[i]=s->address[g->s[i].id]-s->sect[3];
        s->tablewrites=4ull*count; s->tablen=g->n;
    }
}
// The diff is symbol replacements + membership lists of changed groups. Reverse lists
// are persistent and updated here; no precomputed moved-target or patch-site list.
static void update(State *s, Data *from, Data *to) {
    s->copied = s->cleared = s->patched = s->written = s->visited = s->laid = 0;
    memset(s->patch_bucket, 0, sizeof s->patch_bucket);
    s->nt = 0;
    for (u32 k = 0; k < s->ni; k++) {
        u32 id = s->ids[k];
        if (from->index[id]) {
            Sym *y = from->s + from->index[id] - 1;
            for (u32 j = 0; j < y->nr; j++)
                unlink_node(s, y->first + j);
        }
        if (to->index[id]) {
            Sym *y = to->s + to->index[id] - 1;
            for (u32 j = 0; j < y->nr; j++)
                link_node(s, y->first + j);
        }
        mark(s, id);
    }
    allocate_delta(s, from, to, s->ids, s->ni);
    // Clear all retired locations before copying: overlapping source moves are safe.
    for (u32 k = 0; k < s->nt; k++) {
        u32 id = s->todo[k];
        u32 old = s->address[id], v = to->index[id] ? to->s[to->index[id] - 1].addr : 0;
        s->moved[id] = old != v;
        if (from->index[id]) {
            Sym *y = from->s + from->index[id] - 1;
            if (bucket(from, y->group) < 13) {
                memset(s->img + old - from->base, 0, y->occ);
                s->cleared += y->occ;
            }
        }
        s->address[id] = v;
    }
    for (u32 k = 0; k < s->nt; k++) {
        u32 id = s->todo[k];
        if (to->index[id])
            copy(s, to, to->s + to->index[id] - 1);
    }
    for (u32 k = 0; k < s->nt; k++) {
        u32 id = s->todo[k];
        if (s->moved[id])
            for (u32 n = s->head[id]; n; n = s->node[n].next) {
                Node *x = s->node + n;
                s->visited++;
                if (!s->dirty[x->src]) {
                    CK(to->index[x->src]);
                    Sym *y = to->s + to->index[x->src] - 1;
                    if (bucket(to, y->group) < 13)
                        patch(s, y, x->r);
                }
            }
    }
    s->tablewrites=0;
    for(u32 k=0;k<s->ni;k++){u32 id=s->ids[k];
        if((from->index[id] && from->s[from->index[id]-1].group==0) || (to->index[id] && to->s[to->index[id]-1].group==0)){table_update(s,to);break;}}
    for (u32 k = 0; k < s->nt; k++) {
        u32 id = s->todo[k];
        s->dirty[id] = s->moved[id] = 0;
    }
}
static u8 *full(State *s, Data *d) {
    u8 *save = s->img;
    s->img = calloc(s->bytes, 1);


    for (u32 i = 0; i < d->L; i++)
        copy(s, d, d->s + i);
    u8 *out = s->img;
    s->img = save;
    return out;
}
static void diff(State*s,Data*f,Data*t) {
    s->ni=s->ng=0;
    memset(s->changed,0,s->U);
    for(u32 id=1;id<s->U;id++) {
        Sym*x=f->index[id]?f->s+f->index[id]-1:NULL;
        Sym*y=t->index[id]?t->s+t->index[id]-1:NULL;
        if((x||y)&&(!x||!y||!same(x,y))) s->ids[s->ni++]=id;
    }
}
static State initial(Data*a,u32 U,Data**ds,int nd,int index) {
    State s={.a=a,.U=U};
    s.ar=calloc(a->ng,sizeof(Arena)); s.address=calloc(U,4);
    s.alloccap=calloc(U,4); s.holehead=calloc(a->ng,4); s.tail=calloc(a->ng,4);
    s.holes=calloc(U+a->ng+100,sizeof(Hole));
    s.ids=calloc(U,4); s.todo=calloc(U,4); s.changed=calloc(U,1);
    s.dirty=calloc(U,1); s.moved=calloc(U,1); s.groups=calloc(a->ng,4);
    u32 cursor=a->start[1]; u64 slack=0,prodslack=0;
    for(u32 b=1;b<16;b++) {
        for(u32 g=0;g<a->ng;g++) if(bucket(a,g)==b && !(b==6&&g)) {
            u32 al=galign(a->g+g); if(al<a->bmin[b])al=a->bmin[b];
            cursor=up(cursor,al); Arena*r=s.ar+g;
            *r=(Arena){cursor,up(need(a->g+g)+64,al),al};
            layout(a->g+g,r);
            u32 end=cursor;
            for(u32 j=0;j<a->g[g].n;j++) {
                Sym*y=a->g[g].s+j; s.address[y->id]=y->addr;
                s.alloccap[y->id]=y->occ; if(y->addr+y->occ>end)end=y->addr+y->occ;
            }
            s.tail[g]=end; cursor+=r->cap;
            slack+=cursor-end; if(b<13)prodslack+=cursor-end;
        }
        // Fixed 64 KiB per bucket shared reserve, independent of either edit.
        s.spare[b]=cursor; cursor+=65536; s.spareend[b]=s.bend[b]=cursor;
        slack+=65536; if(b<13)prodslack+=65536;
        if(b<15) { CK(a->start[b+1]>=a->end[b]); cursor+=a->start[b+1]-a->end[b]; }
    }
    s.bytes=cursor-a->base;
    for(int k=0;k<8;k++)s.sect[k]=translate(&s,a->sect[k]);
    for(u32 i=0;i<a->nfix;i++)s.address[a->fixedid[i]]=translate(&s,a->fixedaddr[i]);
    s.tablecap=a->g[0].n; s.table=calloc(s.tablecap,4); table_update(&s,a);
    s.img=full(&s,a);
    if(index) {
        s.head=calloc(U,4); u32 nr=1;
        for(int k=0;k<nd;k++)for(u32 i=0;i<ds[k]->L;i++)nr+=ds[k]->s[i].nr;
        s.node=calloc(nr,sizeof(Node)); s.nn=1;
        for(int k=0;k<nd;k++)nodes(&s,ds[k]);
        for(u32 i=0;i<a->L;i++)for(u32 j=0;j<a->s[i].nr;j++)link_node(&s,a->s[i].first+j);
        printf("reservation total_slack=%llu produced_slack=%llu image=%zu original_image=%zu\n",(unsigned long long)slack,(unsigned long long)prodslack,s.bytes,a->nimage);
    }
    return s;
}
static void dispose(State*s) {
    free(s->ar);free(s->address);free(s->alloccap);free(s->holehead);free(s->tail);free(s->holes);
    free(s->ids);free(s->todo);free(s->changed);free(s->dirty);free(s->moved);free(s->groups);
    free(s->table);free(s->img);free(s->head);free(s->node);
}
// Reference discovers decisions by scanning the entire current and next versions,
// reconstructs allocator state from A, and renders every symbol into a zero image.
static void reference_step(State*s,Data*f,Data*t) {
    u32 n=0;
    for(u32 id=1;id<s->U;id++) {
        Sym*x=f->index[id]?f->s+f->index[id]-1:NULL;
        Sym*y=t->index[id]?t->s+t->index[id]-1:NULL;
        if((x||y)&&(!x||!y||!same(x,y)))s->ids[n++]=id;
    }
    allocate_delta(s,f,t,s->ids,n);
    for(u32 k=0;k<n;k++) {u32 id=s->ids[k];s->address[id]=t->index[id]?t->s[t->index[id]-1].addr:0;}
    table_update(s,t);
}
static void verify(State*s,Data**history,int n) {
    State r=initial(history[0],s->U,history,0,0);
    for(int k=1;k<n;k++)reference_step(&r,history[k-1],history[k]);
    CK(!memcmp(s->address,r.address,s->U*4ull));
    CK(!memcmp(s->alloccap,r.alloccap,s->U*4ull));
    for(u32 g=0;g<s->a->ng;g++){
        CK(s->tail[g]==r.tail[g]); u32 x=s->holehead[g],y=r.holehead[g];
        while(x&&y){CK(s->holes[x].lo==r.holes[y].lo && s->holes[x].len==r.holes[y].len);x=s->holes[x].next;y=r.holes[y].next;}CK(!x&&!y);
    }
    CK(!memcmp(s->spare,r.spare,sizeof s->spare));
    u8*expected=full(&r,history[n-1]);
    CK(!memcmp(s->img,expected,s->bytes)); free(expected);
    CK(s->tablen==r.tablen && !memcmp(s->table,r.table,s->tablecap*4ull));
    dispose(&r);
    // Reference uses separate allocator/address state but shared immutable symbol
    // records; restore their mutable address caches to the maintained truth.
    Data*d=history[n-1];for(u32 i=0;i<d->L;i++)d->s[i].addr=s->address[d->s[i].id];
}
static void export_one(State*s,Data*d,const char*dir,char tag) {
    char path[4096];FILE*f;
    snprintf(path,sizeof path,"%s/%c.addr",dir,tag);f=fopen(path,"wb");CK(f);
    for(u32 i=0;i<d->L;i++){u32 row[]={d->s[i].id,s->address[d->s[i].id]};fwrite(row,4,2,f);}fclose(f);
    snprintf(path,sizeof path,"%s/%c.img",dir,tag);f=fopen(path,"wb");CK(f);fwrite(s->img,1,s->bytes,f);fclose(f);
    snprintf(path,sizeof path,"%s/%c.table",dir,tag);f=fopen(path,"wb");CK(f);fwrite(s->table,4,s->tablecap,f);fclose(f);
    snprintf(path,sizeof path,"%s/anchors",dir);f=fopen(path,"w");CK(f);
    for(int k=0;k<8;k++)fprintf(f,"sect %u\n",s->sect[k]);
    for(u32 i=0;i<s->a->nfix;i++)fprintf(f,"fixed %u %u\n",s->a->fixedid[i],translate(s,s->a->fixedaddr[i]));fclose(f);
}
static u64 allocation_hash(State*s) {
    u64 h=1469598103934665603ull;
    for(u32 g=0;g<s->a->ng;g++) {
        h=(h^s->tail[g])*1099511628211ull;
        for(u32 n=s->holehead[g];n;n=s->holes[n].next) {
            h=(h^s->holes[n].lo)*1099511628211ull;
            h=(h^s->holes[n].len)*1099511628211ull;
        }
    }
    for(int b=1;b<16;b++)h=(h^s->spare[b])*1099511628211ull;
    return h;
}
static void report(State*s,Data*f,Data*t,u8*old,u32*oldaddr,const char*label) {
    u64 moved=0,bytes=0,holebytes=0,holes=0,tail=0,spare=0,internal=0;
    for(u32 i=0;i<t->L;i++) internal+=s->alloccap[t->s[i].id]-t->s[i].occ;
    for(u32 i=0;i<t->L;i++)if(f->index[t->s[i].id]&&oldaddr[t->s[i].id]!=s->address[t->s[i].id])moved++;
    for(size_t i=0;i<s->bytes;i++)bytes+=old[i]!=s->img[i];
    for(u32 g=0;g<s->a->ng;g++) {
        for(u32 h=s->holehead[g];h;h=s->holes[h].next){holes++;holebytes+=s->holes[h].len;}
        if(s->ar[g].cap)tail+=s->ar[g].base+s->ar[g].cap-s->tail[g];
    }
    for(int b=1;b<16;b++)spare+=s->spareend[b]-s->spare[b];
    printf("%s moved=%llu repatched=%llu bytes_changed=%llu copied=%llu cleared=%llu patch_bytes=%llu reverse_visits=%llu table_bytes=%llu written=%llu holes=%llu hole_bytes=%llu tail_free=%llu spare_free=%llu retained_unused=%llu\n",label,(unsigned long long)moved,(unsigned long long)s->patched,(unsigned long long)bytes,(unsigned long long)s->copied,(unsigned long long)s->cleared,(unsigned long long)s->written,(unsigned long long)s->visited,(unsigned long long)s->tablewrites,(unsigned long long)(s->copied+s->cleared+s->written+s->tablewrites),(unsigned long long)holes,(unsigned long long)holebytes,(unsigned long long)tail,(unsigned long long)spare,(unsigned long long)internal);
}
int main(int argc,char**argv) {
    CK(argc>=3);int nd=argc>4?3:2;u32 U=0;
    flatDir=argv[1];U=metaGet("nids");
    for(int k=1;k<nd;k++){flatDir=argv[k==1?2:4];u32 n=metaGet("nids");const u32*m=FMAP(u32,"diff/idmap_b.u32",NULL);for(u32 i=0;i<n;i++)if(m[i]>=U)U=m[i]+1;}
    Data a=load(argv[1],0,U), b=load(argv[2],1,U), c={0};if(nd==3)c=load(argv[4],1,U);
    Data*ds[]={&a,&b,&c};
    for(int k=1;k<nd;k++) {
        CK(ds[k]->ng==a.ng && ds[k]->nobj==a.nobj && ds[k]->base==a.base);
        CK(ds[k]->nfix==a.nfix);
        for(u32 i=0;i<ds[k]->nfix;i++) {
            u32 j=0;while(j<a.nfix&&a.fixedid[j]!=ds[k]->fixedid[i])j++;
            CK(j<a.nfix);
        }
    }
    State s=initial(&a,U,ds,nd,1);
    Data*hist[24]={&a};int hn=1;Data*seq[]={&b,&a,&c,&a};int steps=nd==3?4:2;
    u8*old=malloc(s.bytes);u32*oa=malloc(U*4ull);const char*exp=getenv("P3INC_EXPORT");
    if(exp)export_one(&s,&a,exp,'a');
    for(int k=0;k<steps;k++) {
        Data*f=hist[hn-1],*t=seq[k];diff(&s,f,t);memcpy(old,s.img,s.bytes);memcpy(oa,s.address,U*4ull);
        double start=now();update(&s,f,t);double ms=(now()-start)*1000;
        char label[80];snprintf(label,sizeof label,"step=%d",k+1);report(&s,f,t,old,oa,label);printf("first_step=%d ms=%.6f\n",k+1,ms);
        hist[hn++]=t;verify(&s,hist,hn);
        if(exp && k==0)export_one(&s,&b,exp,'b');
        if(exp&&nd==3){char dir[4096];snprintf(dir,sizeof dir,"%s/step%d",exp,k+1);export_one(&s,t,dir,t==&a?'a':'b');}
    }
    // Establish a repeated-cycle checkpoint; no assumption that returning A restores
    // its original compact addresses. Verify its first two cycles by full history replay.
    for(int cycle=0;cycle<2;cycle++)for(int k=0;k<steps;k++) {
        Data*f=k?seq[k-1]:&a,*t=seq[k];diff(&s,f,t);update(&s,f,t);hist[hn++]=t;verify(&s,hist,hn);
    }
    u8*checkpoint=malloc(s.bytes);memcpy(checkpoint,s.img,s.bytes);
    u32*ac=malloc(U*4ull);memcpy(ac,s.alloccap,U*4ull);memcpy(oa,s.address,U*4ull);
    u64 stablehash=allocation_hash(&s);
    double seconds=argc>3?atof(argv[3]):1, ts[4]={0};u32 reps=0;
    // Diffs are precomputed inputs, like p3inc; swap prepared lists outside timing.
    u32*lists[4];u32 lens[4];
    for(int k=0;k<steps;k++){diff(&s,k?seq[k-1]:&a,seq[k]);lens[k]=s.ni;lists[k]=malloc(s.ni*4ull);memcpy(lists[k],s.ids,s.ni*4ull);}
    u32*ownids=s.ids;
    do {
        for(int k=0;k<steps;k++){s.ids=lists[k];s.ni=lens[k];double t=now();update(&s,k?seq[k-1]:&a,seq[k]);ts[k]+=now()-t;}reps++;
    }while(steps==4 ? ts[0]+ts[1]+ts[2]+ts[3]<seconds : ts[0]<seconds||ts[1]<seconds);
    s.ids=ownids;
    CK(stablehash==allocation_hash(&s));
    CK(!memcmp(checkpoint,s.img,s.bytes));CK(!memcmp(oa,s.address,U*4ull));CK(!memcmp(ac,s.alloccap,U*4ull));
    u8*render=full(&s,&a);CK(!memcmp(render,s.img,s.bytes));free(render);
    // The ending allocation state equals the checked checkpoint. Check every
    // intermediate image of one further cycle against full history replay.
    for(int k=0;k<steps;k++) {
        Data*f=k?seq[k-1]:&a,*t=seq[k];
        diff(&s,f,t);memcpy(old,s.img,s.bytes);memcpy(oa,s.address,U*4ull);update(&s,f,t);
        char label[80];snprintf(label,sizeof label,"warm_step=%d",k+1);report(&s,f,t,old,oa,label);
        hist[hn++]=seq[k];verify(&s,hist,hn);
    }
    printf("P3HOLES OK reps=%u forward_ms=%.6f reverse_ms=%.6f timed=%.3f/%.3f\n",reps,ts[0]*1000/reps,ts[1]*1000/reps,ts[0],ts[1]);
    if(steps==4)for(int k=0;k<4;k++)printf("chain_step=%d mean_ms=%.6f timed=%.3f\n",k+1,ts[k]*1000/reps,ts[k]);
    return 0;
}
