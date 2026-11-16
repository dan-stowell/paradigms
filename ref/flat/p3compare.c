// Current-layout change census. Symbol identities use the supplied B->A id map.
// Relocation sites are paired by (unified source id, offset, width, occurrence).
// Values are the actual oracle bytes at those sites, not address arithmetic.
// Images contain ONLY 3b's produced symbol ranges, zero elsewhere. Image byte
// differences are counted over the union of A/B produced ranges, per bucket.
#include "flat.h"
#include <assert.h>
typedef struct {
    u32 id, off, width, order, sym;
} Site;
typedef struct {
    u32 L, ids, base;
    size_t image_n;
    const u32 *sym, *addr, *size, *rs, *ro, *map, *unit;
    const u8 *bucket, *rw, *oracle;
    u32 *index;
    u8 *img, *mask;
    Site *sites;
    u32 nr;
} Data;
static int cmp(const void *aa, const void *bb) {
    const Site *a = aa, *b = bb;
#define C(f)                                                                                       \
    if (a->f != b->f)                                                                              \
    return a->f < b->f ? -1 : 1
    C(id);
    C(off);
    C(width);
    C(order);
    return 0;
#undef C
}
static int keycmp(const Site *a, const Site *b) {
    if (a->id != b->id)
        return a->id < b->id ? -1 : 1;
    if (a->off != b->off)
        return a->off < b->off ? -1 : 1;
    if (a->width != b->width)
        return a->width < b->width ? -1 : 1;
    return 0;
}
static u32 uid(Data *d, u32 id) { return d->map ? d->map[id] : id; }
static Data load(const char *path, int mapped) {
    flatDir = path;
    Data d = {0};
    d.L = metaGet("p3_placed");
    d.ids = metaGet("nids");
    d.base = metaGet("imageBase");
    d.sym = FMAP(u32, "p3_sym.u32", NULL);
    d.unit = FMAP(u32, "p3_unit.u32", NULL);
    d.addr = FMAP(u32, "oracle/p3_addr.u32", NULL);
    d.size = FMAP(u32, "p3_size.u32", NULL);
    d.bucket = FMAP(u8, "p3_bucket.u8", NULL);
    d.rs = FMAP(u32, "p3_rel_start.u32", NULL);
    d.ro = FMAP(u32, "p3_rel_off.u32", NULL);
    d.rw = FMAP(u8, "p3_rel_siz.u8", NULL);
    d.oracle = FMAP(u8, "oracle/p3_image.bin", &d.image_n);
    if (mapped)
        d.map = FMAP(u32, "diff/idmap_b.u32", NULL);
    return d;
}
static void prepare(Data *d, u32 n, size_t bytes) {
    d->index = calloc(n, 4);
    d->img = calloc(bytes, 1);
    d->mask = calloc(bytes, 1);
    d->sites = malloc((size_t)d->rs[d->L] * sizeof(Site));
    assert(d->index && d->img && d->mask && d->sites);
    for (u32 i = 0; i < d->L; i++) {
        u32 id = uid(d, d->sym[i]);
        assert(id < n && !d->index[id]);
        d->index[id] = i + 1;
        if (d->bucket[i] >= 13)
            continue;
        size_t off = d->addr[i] - d->base, sz = d->size[i];
        assert(off + sz <= d->image_n);
        memcpy(d->img + off, d->oracle + off, sz);
        for (size_t k = off; k < off + sz; k++) {
            assert(!d->mask[k]);
            d->mask[k] = d->bucket[i];
        }
        for (u32 r = d->rs[i]; r < d->rs[i + 1]; r++) {
            assert((u64)d->ro[r] + d->rw[r] <= sz);
            d->sites[d->nr++] = (Site){id, d->ro[r], d->rw[r], r, i};
        }
    }
    qsort(d->sites, d->nr, sizeof(Site), cmp);
}
int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s A B\n", argv[0]);
        return 2;
    }
    Data a = load(argv[1], 0), b = load(argv[2], 1);
    assert(a.base == b.base);
    u32 n = a.ids;
    for (u32 i = 0; i < b.ids; i++)
        if (b.map[i] >= n)
            n = b.map[i] + 1;
    size_t bytes = a.image_n > b.image_n ? a.image_n : b.image_n;
    prepare(&a, n, bytes);
    prepare(&b, n, bytes);
    u64 add[16] = {0}, del[16] = {0}, common[16] = {0}, move[16] = {0}, ra[16] = {0}, rd[16] = {0},
        rc[16] = {0}, rv[16] = {0}, bc[16] = {0}, bu[16] = {0};
    u32 unit_changes = 0;
    for (u32 id = 0; id < n; id++) {
        u32 x = a.index[id], y = b.index[id];
        if (x && y) {
            x--;
            y--;
            assert(a.bucket[x] == b.bucket[y]);
            common[b.bucket[y]]++;
            unit_changes += a.unit[x] != b.unit[y];
            move[b.bucket[y]] += a.addr[x] != b.addr[y];
        } else if (x)
            del[a.bucket[x - 1]]++;
        else if (y)
            add[b.bucket[y - 1]]++;
    }
    u32 x = 0, y = 0;
    while (x < a.nr || y < b.nr) {
        int c = x == a.nr ? 1 : y == b.nr ? -1 : keycmp(&a.sites[x], &b.sites[y]);
        if (c < 0) {
            rd[a.bucket[a.sites[x++].sym]]++;
            continue;
        }
        if (c > 0) {
            ra[b.bucket[b.sites[y++].sym]]++;
            continue;
        }
        Site *s = &a.sites[x++], *t = &b.sites[y++];
        u32 bucket = b.bucket[t->sym];
        rc[bucket]++;
        rv[bucket] += memcmp(a.oracle + a.addr[s->sym] - a.base + s->off,
                             b.oracle + b.addr[t->sym] - b.base + t->off, s->width) != 0;
    }
    u64 global_union = 0, global_changed = 0;
    for (size_t k = 0; k < bytes; k++) {
        u8 x = a.mask[k], y = b.mask[k];
        int diff = a.img[k] != b.img[k];
        if (x || y) {
            global_union++;
            global_changed += diff;
        }
        if (x) {
            bu[x]++;
            bc[x] += diff;
        }
        if (y && y != x) {
            bu[y]++;
            bc[y] += diff;
        }
    }
    puts("bucket added removed common moved rel_added rel_removed rel_common rel_value_changed "
         "image_union image_changed");
    for (int i = 1; i < 16; i++)
        printf("%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu\n", i,
               (unsigned long long)add[i], (unsigned long long)del[i],
               (unsigned long long)common[i], (unsigned long long)move[i],
               (unsigned long long)ra[i], (unsigned long long)rd[i], (unsigned long long)rc[i],
               (unsigned long long)rv[i], (unsigned long long)bu[i], (unsigned long long)bc[i]);
    printf("global_image_union %llu global_image_changed %llu\n", (unsigned long long)global_union,
           (unsigned long long)global_changed);
    printf("common_extractor_unit_changes %u\n", unit_changes);
    puts("P3COMPARE OK");
    return 0;
}
