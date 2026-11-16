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
typedef struct {
    Data *a, *b;
    Arena *ar;
    u32 *address, *head;
    Node *node;
    u32 nn;
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
            return end + x - a->end[b];
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
    // Keep old addresses until all changed groups have been laid out.
    for (u32 k = 0; k < s->ng; k++) {
        u32 g = s->groups[k];
        layout(to->g + g, s->ar + g);
        s->laid += to->g[g].n;
        Group *f = from->g + g, *t = to->g + g;
        for (u32 i = 0; i < f->n; i++) {
            Sym *y = f->s + i;
            if (!to->index[y->id] || s->address[y->id] != to->s[to->index[y->id] - 1].addr)
                mark(s, y->id);
        }
        for (u32 i = 0; i < t->n; i++) {
            Sym *y = t->s + i;
            if (s->address[y->id] != y->addr)
                mark(s, y->id);
        }
    }
    // Clear all retired locations before copying: overlapping source moves are safe.
    for (u32 k = 0; k < s->nt; k++) {
        u32 id = s->todo[k];
        u32 old = s->address[id], v = to->index[id] ? to->s[to->index[id] - 1].addr : 0;
        s->moved[id] = old != v;
        if (from->index[id]) {
            Sym *y = from->s + from->index[id] - 1;
            if (bucket(from, y->group) < 13) {
                memset(s->img + old - from->base, 0, y->size);
                s->cleared += y->size;
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
    for (u32 k = 0; k < s->nt; k++) {
        u32 id = s->todo[k];
        s->dirty[id] = s->moved[id] = 0;
    }
}
static u8 *full(State *s, Data *d) {
    u8 *save = s->img;
    s->img = calloc(s->bytes, 1);
    for (u32 g = 0; g < d->ng; g++)
        layout(d->g + g, s->ar + g);
    for (u32 i = 0; i < d->L; i++)
        s->address[d->s[i].id] = d->s[i].addr;
    for (u32 i = 0; i < d->L; i++)
        copy(s, d, d->s + i);
    u8 *out = s->img;
    s->img = save;
    return out;
}
int main(int argc, char **argv) {
    CK(argc >= 3);
    flatDir = argv[2];
    const u32 *map = FMAP(u32, "diff/idmap_b.u32", NULL);
    u32 nb = metaGet("nids"), U = 0;
    for (u32 i = 0; i < nb; i++)
        if (map[i] >= U)
            U = map[i] + 1;
    flatDir = argv[1];
    if (metaGet("nids") > U)
        U = metaGet("nids");
    double setup = now();
    Data a = load(argv[1], 0, U), b = load(argv[2], 1, U);
    CK(a.ng == b.ng);
    State s = {.a = &a, .b = &b};
    s.ar = calloc(a.ng, sizeof(Arena));
    // Test capacity policies independent of B. Include empty package/bucket slots.
    u32 quantum[] = {64, 128, 256, 512, 1024, 4096, 64, 64, 64},
        percent[] = {0, 0, 0, 0, 0, 0, 1, 5, 10};
    int chosen = -1;
    for (int p = 0; p < 9; p++) {
        u64 cap = 0, used = 0;
        u32 spill = 0;
        for (u32 g = 0; g < a.ng; g++) {
            if (bucket(&a, g) == 0 || (bucket(&a, g) == 6 && g))
                continue;
            u32 n = need(a.g + g), q = quantum[p], al = galign(a.g + g);
            if (al > q)
                q = al;
            u32 c = up(n + n * percent[p] / 100 + 1, q);
            cap += c;
            used += n;
            if (need(b.g + g) > c || galign(b.g + g) > q)
                spill++;
        }
        printf("policy quantum=%u percent=%u capacity=%llu payload=%llu slack=%llu spills=%u\n",
               quantum[p], percent[p], (unsigned long long)cap, (unsigned long long)used,
               (unsigned long long)(cap - used), spill);
        if (!spill && chosen < 0)
            chosen = p;
    }
    CK(chosen >= 0);
    if (argc > 4)
        chosen = atoi(argv[4]);
    CK(chosen >= 0 && chosen < 9);
    u32 q0 = quantum[chosen], pc = percent[chosen], cursor = a.start[1];
    u64 capacity = 0;
    for (u32 bucketno = 1; bucketno < 16; bucketno++) {
        for (u32 g = 0; g < a.ng; g++)
            if (bucket(&a, g) == bucketno && !(bucketno == 6 && g)) {
                u32 al = galign(a.g + g);
                if (al < a.bmin[bucketno])
                    al = a.bmin[bucketno];
                u32 q = q0 > al ? q0 : al;
                s.ar[g].align = al;
                cursor = up(cursor, al);
                s.ar[g].base = cursor;
                s.ar[g].cap = up(need(a.g + g) + need(a.g + g) * pc / 100 + 1, q);
                CK(need(b.g + g) <= s.ar[g].cap);
                cursor += s.ar[g].cap;
                capacity += s.ar[g].cap;
            }
        if (bucketno < 15) {
            CK(a.start[bucketno + 1] >= a.end[bucketno]);
            cursor += a.start[bucketno + 1] - a.end[bucketno];
        }
    }
    u64 prodcap = 0, prodpayload = 0, payload = 0, required = 0;
    u32 largest = 0, prodend = 0;
    for (u32 g = 0; g < a.ng; g++)
        if (s.ar[g].cap) {
            u32 n = need(a.g + g), m = need(b.g + g);
            payload += n;
            if (m > n) {
                required += m - n;
                if (m - n > largest)
                    largest = m - n;
            }
            if (bucket(&a, g) < 13) {
                prodcap += s.ar[g].cap;
                prodpayload += n;
                if (s.ar[g].base + s.ar[g].cap > prodend)
                    prodend = s.ar[g].base + s.ar[g].cap;
            }
        }
    printf("arena_total capacity=%llu payload=%llu slack=%llu produced_capacity=%llu "
           "produced_payload=%llu produced_slack=%llu produced_extent=%u retrospective_growth=%llu "
           "max_group_growth=%u\n",
           (unsigned long long)capacity, (unsigned long long)payload,
           (unsigned long long)(capacity - payload), (unsigned long long)prodcap,
           (unsigned long long)prodpayload, (unsigned long long)(prodcap - prodpayload),
           prodend - a.base, (unsigned long long)required, largest);
    CK(a.nfix == b.nfix);
    for (u32 i = 0; i < b.nfix; i++) {
        u32 j = 0;
        while (j < a.nfix && a.fixedid[j] != b.fixedid[i])
            j++;
        CK(j < a.nfix);
    }
    s.bytes = cursor - a.base;
    s.address = calloc(U, 4);
    s.head = calloc(U, 4);
    s.dirty = calloc(U, 1);
    s.moved = calloc(U, 1);
    s.changed = calloc(U, 1);
    s.groups = calloc(a.ng, 4);
    s.ids = calloc(U, 4);
    s.todo = calloc(U, 4);
    u8 *cg = calloc(a.ng, 1);
    for (int k = 0; k < 8; k++)
        s.sect[k] = translate(&s, a.sect[k]);
    for (u32 i = 0; i < a.nfix; i++)
        s.address[a.fixedid[i]] = translate(&s, a.fixedaddr[i]);
    for (u32 id = 1; id < U; id++) {
        Sym *x = a.index[id] ? a.s + a.index[id] - 1 : NULL,
            *y = b.index[id] ? b.s + b.index[id] - 1 : NULL;
        if ((x || y) && (!x || !y || !same(x, y))) {
            s.ids[s.ni++] = id;
            s.changed[id] = 1;
            if (x)
                cg[x->group] = 1;
            if (y)
                cg[y->group] = 1;
        }
    }
    for (u32 g = 0; g < a.ng; g++)
        if (cg[g])
            s.groups[s.ng++] = g;
    u32 nr = 1;
    for (u32 i = 0; i < a.L; i++)
        nr += a.s[i].nr;
    for (u32 i = 0; i < b.L; i++)
        nr += b.s[i].nr;
    s.node = calloc(nr, sizeof(Node));
    s.nn = 1;
    nodes(&s, &a);
    nodes(&s, &b);
    for (u32 i = 0; i < a.L; i++)
        for (u32 j = 0; j < a.s[i].nr; j++)
            link_node(&s, a.s[i].first + j);
    u8 *ia = full(&s, &a);
    memset(s.address, 0, 4ull * U);
    for (u32 i = 0; i < a.nfix; i++)
        s.address[a.fixedid[i]] = translate(&s, a.fixedaddr[i]);
    u8 *ib = full(&s, &b);
    memset(s.address, 0, 4ull * U);
    for (u32 i = 0; i < a.nfix; i++)
        s.address[a.fixedid[i]] = translate(&s, a.fixedaddr[i]);
    for (u32 i = 0; i < a.L; i++)
        s.address[a.s[i].id] = a.s[i].addr;
    // The expected B image is now sealed. The update may inherit only A's
    // addresses for unchanged groups, never the scratch computation for dirty ones.
    for (u32 i = 0; i < b.L; i++) {
        Sym *y = b.s + i;
        if (cg[y->group])
            y->addr = 0; // the first update must really run changed-group layout
        else {
            CK(a.index[y->id]);
            CK(y->addr == a.s[a.index[y->id] - 1].addr);
        }
    }
    s.img = malloc(s.bytes);
    memcpy(s.img, ia, s.bytes);
    printf("chosen=%d image=%zu capacity=%llu changed_symbols=%u changed_groups=%u setup_ms=%.3f\n",
           chosen, s.bytes, (unsigned long long)capacity, s.ni, s.ng, (now() - setup) * 1000);
    update(&s, &a, &b);
    CK(!memcmp(s.img, ib, s.bytes));
    printf("forward copied=%llu cleared=%llu repatched=%llu patch_bytes=%llu reverse_visits=%llu "
           "laid_symbols=%llu\n",
           (unsigned long long)s.copied, (unsigned long long)s.cleared,
           (unsigned long long)s.patched, (unsigned long long)s.written,
           (unsigned long long)s.visited, (unsigned long long)s.laid);
    for (u32 bn = 1; bn < 16; bn++) {
        u64 moved = 0, added = 0, removed = 0, diff = 0;
        for (u32 i = 0; i < a.L; i++) {
            Sym *x = a.s + i;
            if (bucket(&a, x->group) != bn)
                continue;
            if (!b.index[x->id])
                removed++;
            else if (x->addr != b.s[b.index[x->id] - 1].addr)
                moved++;
        }
        for (u32 i = 0; i < b.L; i++)
            if (bucket(&b, b.s[i].group) == bn && !a.index[b.s[i].id])
                added++;
        for (u32 g = 0; g < a.ng; g++)
            if (bucket(&a, g) == bn && bn < 13)
                for (u32 j = 0; j < s.ar[g].cap; j++) {
                    size_t o = s.ar[g].base - a.base + j;
                    diff += ia[o] != ib[o];
                }
        printf("slack_bucket %u added=%llu removed=%llu moved=%llu bytes_changed=%llu "
               "repatched=%llu\n",
               bn, (unsigned long long)added, (unsigned long long)removed,
               (unsigned long long)moved, (unsigned long long)diff,
               (unsigned long long)s.patch_bucket[bn]);
    }
    update(&s, &b, &a);
    CK(!memcmp(s.img, ia, s.bytes));
    // Export optional independent-check inputs (all paths explicitly supplied inside clone).
    if (getenv("P3INC_EXPORT")) {
        const char *dir = getenv("P3INC_EXPORT");
        for (int k = 0; k < 2; k++) {
            Data *d = k ? &b : &a;
            char path[4096];
            snprintf(path, sizeof path, "%s/%c.addr", dir, 'a' + k);
            FILE *f = fopen(path, "wb");
            CK(f);
            for (u32 i = 0; i < d->L; i++) {
                u32 row[] = {d->s[i].id, d->s[i].addr};
                fwrite(row, 4, 2, f);
            }
            fclose(f);
            snprintf(path, sizeof path, "%s/%c.img", dir, 'a' + k);
            f = fopen(path, "wb");
            CK(f);
            fwrite(k ? ib : ia, 1, s.bytes, f);
            fclose(f);
        }
        char path[4096];
        snprintf(path, sizeof path, "%s/anchors", dir);
        FILE *f = fopen(path, "w");
        CK(f);
        for (int k = 0; k < 8; k++)
            fprintf(f, "sect %u\n", s.sect[k]);
        for (u32 i = 0; i < a.nfix; i++)
            fprintf(f, "fixed %u %u\n", a.fixedid[i], translate(&s, a.fixedaddr[i]));
        fclose(f);
    }
    double seconds = argc > 3 ? atof(argv[3]) : 1, ta = 0, tb = 0;
    u32 reps = 0;
    do {
        double t = now();
        update(&s, &a, &b);
        ta += now() - t;
        t = now();
        update(&s, &b, &a);
        tb += now() - t;
        reps++;
    } while (ta < seconds || tb < seconds);
    CK(!memcmp(s.img, ia, s.bytes));
    update(&s, &a, &b);
    CK(!memcmp(s.img, ib, s.bytes));
    printf("P3INC OK reps=%u forward_ms=%.6f reverse_ms=%.6f timed=%.3f/%.3f\n", reps,
           ta * 1000 / reps, tb * 1000 / reps, ta, tb);
    return 0;
}
