// Independent scalar P3 relocation oracle. Does not include p3inc.c.
// p3inc_check A B EXPORT_DIR
#include "flat.h"
#include <assert.h>
typedef struct {
    u32 lo, hi;
} Range;
static int rangeorder(const void *a, const void *b) {
    u32 x = ((const Range *)a)->lo, y = ((const Range *)b)->lo;
    return x < y ? -1 : x > y;
}
static void fail(const char *s) {
    fprintf(stderr, "p3inc_check: %s\n", s);
    exit(2);
}
static const void *external(const char *dir, const char *name, size_t *n) {
    const char *save = flatDir;
    flatDir = dir;
    const void *p = fmap(name, n);
    flatDir = save;
    return p;
}
static void apply(u8 *p, u32 addr, u32 r, const u32 *off, const u8 *siz, const u8 *kind,
                  const i64 *add, const u32 *tgt, const u32 *all, const u32 *sect, u32 unreach,
                  u32 noop) {
    u32 t = all[tgt[r]], k = kind[r], w = siz[r];
    i64 value;
    if (k == 2 && !t && tgt[r])
        t = unreach;
    if (k == 6 && !t && tgt[r])
        t = noop;
    if (k == 1 || k == 2) {
        if (!t && tgt[r])
            fail("missing absolute target");
        value = (i64)t + add[r];
    } else if (k == 3 || k == 4) {
        if (!t) {
            if (k == 3)
                return;
            value = -1;
        } else {
            int section = 7;
            while (section > 0 && t < sect[section])
                section--;
            value = (i64)t - sect[section] + add[r];
        }
    } else if (k == 5 || k == 6 || k == 7) {
        if (!t && tgt[r])
            fail("missing relative target");
        value = (tgt[r] ? (i64)t : 0) + add[r] - (i64)(addr + off[r] + w);
    } else if (k == 8)
        value = add[r];
    else
        fail("unknown relocation kind");
    if (w != 1 && w != 2 && w != 4 && w != 8)
        fail("invalid width");
    for (u32 j = 0; j < w; j++)
        p[off[r] + j] = (u64)value >> (j * 8);
}
static int check(const char *path, const char *exp, int which) {
    flatDir = path;
    u32 L = metaGet("p3_placed"), N = metaGet("nids"), slots = metaGet("nslots"),
        base = metaGet("imageBase");
    const u32 *map = which ? FMAP(u32, "diff/idmap_b.u32", NULL) : NULL;
    const u32 *sym = FMAP(u32, "p3_sym.u32", NULL), *size = FMAP(u32, "p3_size.u32", NULL),
              *orig = FMAP(u32, "oracle/p3_addr.u32", NULL),
              *canon = FMAP(u32, "oracle/p1c_canon.u32", NULL),
              *origall = FMAP(u32, "p3_addr_all.u32", NULL);
    const u8 *cl = FMAP(u8, "p3_class.u8", NULL), *bucket = FMAP(u8, "p3_bucket.u8", NULL),
             *raw = FMAP(u8, "p3_data.bin", NULL), *oracle = FMAP(u8, "oracle/p3_image.bin", NULL);
    const u64 *dataoff = FMAP(u64, "p3_data.off.u64", NULL);
    const u32 *rs = FMAP(u32, "p3_rel_start.u32", NULL), *off = FMAP(u32, "p3_rel_off.u32", NULL),
              *tgt = FMAP(u32, "p3_rel_tgt.u32", NULL);
    const u8 *siz = FMAP(u8, "p3_rel_siz.u8", NULL), *kind = FMAP(u8, "p3_rel_kind.u8", NULL);
    const i64 *add = FMAP(i64, "p3_rel_add.i64", NULL);
    char file[32];
    snprintf(file, sizeof file, "%c.addr", 'a' + which);
    size_t nr;
    const u32 *pairs = external(exp, file, &nr);
    if (nr != L * 8ull)
        fail("address row count");
    u32 U = N;
    for (u32 i = 0; i < L; i++)
        if (pairs[2 * i] >= U)
            U = pairs[2 * i] + 1;
    u32 *placed = calloc(U, 4), *all = calloc(N, 4), *addr = calloc(L, 4);
    for (u32 i = 0; i < L; i++) {
        if (placed[pairs[2 * i]])
            fail("duplicate unified ID");
        placed[pairs[2 * i]] = pairs[2 * i + 1];
    }
    Range *ranges = calloc(L, sizeof(Range));
    u32 rn = 0;
    char ap[4096];
    snprintf(ap, sizeof ap, "%s/anchors", exp);
    FILE *f = fopen(ap, "r");
    if (!f)
        fail("anchors missing");
    u32 sect[8], os[8], x, y;
    for (int i = 0; i < 8; i++)
        if (fscanf(f, " sect %u", sect + i) != 1)
            fail("section anchor");
    while (fscanf(f, " fixed %u %u", &x, &y) == 2) {
        if (x >= U)
            fail("fixed ID outside universe");
        placed[x] = y;
    }
    fclose(f);
    const char *keys[] = {"lay_text",      "lay_rodata", "lay_pclntab", "lay_types",
                          "lay_noptrdata", "lay_data",   "lay_bss",     "lay_noptrbss"};
    for (int i = 0; i < 8; i++)
        os[i] = metaGet(keys[i]);
    u32 oldweak[2] = {metaGet("addr_unreachableMethod"), metaGet("addr_mapinitnoop")},
        weak[2] = {0};
    for (u32 i = 0; i < L; i++) {
        u32 id = map ? map[sym[i]] : sym[i];
        if (id >= U || !placed[id])
            fail("placed symbol missing");
        addr[i] = placed[id];
        if (addr[i] & ((1u << cl[i]) - 1))
            fail("misaligned symbol");
        if (size[i])
            ranges[rn++] = (Range){addr[i], addr[i] + size[i]};
        for (int j = 0; j < 2; j++)
            if (orig[i] == oldweak[j])
                weak[j] = addr[i];
    }
    if (!weak[0] || !weak[1])
        fail("weak fallback missing");
    qsort(ranges, rn, sizeof(Range), rangeorder);
    for (u32 i = 1; i < rn; i++)
        if (ranges[i].lo < ranges[i - 1].hi)
            fail("overlapping symbols");
    free(ranges);
    for (u32 i = 0; i < N; i++) {
        u32 c = i <= slots ? canon[i] : i, u = map ? map[c] : c;
        all[i] = u < U ? placed[u] : 0;
    }
    snprintf(file, sizeof file, "%c.img", 'a' + which);
    size_t nimg;
    const u8 *img = external(exp, file, &nimg);
    u8 *expect = calloc(nimg, 1);
    u64 bad_orig = 0, bad_slack = 0, literal = 0, nonrel = 0, relbytes = 0, relcount = 0, max = 0;
    for (u32 i = 0; i < L; i++)
        if (bucket[i] < 13 && size[i] > max)
            max = size[i];
    u8 *scratch = malloc(max), *mask = malloc(max);
    for (u32 i = 0; i < L; i++) {
        if (bucket[i] >= 13)
            continue;
        u32 n = size[i];
        if (addr[i] < base || (u64)(addr[i] - base) + n > nimg)
            fail("symbol outside slack image");
        u8 *p = expect + addr[i] - base;
        memset(scratch, 0, n);
        memset(mask, 0, n);
        u64 len = dataoff[i + 1] - dataoff[i];
        if (len > n)
            fail("raw data exceeds symbol");
        memcpy(scratch, raw + dataoff[i], len);
        memcpy(p, scratch, n);
        for (u32 r = rs[i]; r < rs[i + 1]; r++) {
            if (off[r] + siz[r] > n)
                fail("relocation outside symbol");
            apply(scratch, orig[i], r, off, siz, kind, add, tgt, origall, os, oldweak[0],
                  oldweak[1]);
            apply(p, addr[i], r, off, siz, kind, add, tgt, all, sect, weak[0], weak[1]);
            memset(mask + off[r], 1, siz[r]);
            relcount++;
        }
        bad_orig += memcmp(scratch, oracle + orig[i] - base, n) != 0;
        bad_slack += memcmp(p, img + addr[i] - base, n) != 0;
        literal += memcmp(oracle + orig[i] - base, img + addr[i] - base, n) != 0;
        for (u32 j = 0; j < n; j++) {
            relbytes += mask[j];
            if (!mask[j] && oracle[orig[i] - base + j] != img[addr[i] - base + j])
                nonrel++;
        }
    }
    u64 imagebad = 0;
    for (size_t i = 0; i < nimg; i++)
        imagebad += expect[i] != img[i];
    printf("CHECK %c original_symbols_bad=%llu slack_symbols_bad=%llu slack_image_bytes_bad=%llu "
           "literal_symbols_differ=%llu nonrel_bytes_differ=%llu relocation_bytes=%llu "
           "relocations=%llu\n",
           'A' + which, (unsigned long long)bad_orig, (unsigned long long)bad_slack,
           (unsigned long long)imagebad, (unsigned long long)literal, (unsigned long long)nonrel,
           (unsigned long long)relbytes, (unsigned long long)relcount);
    free(placed);
    free(all);
    free(addr);
    free(expect);
    free(scratch);
    free(mask);
    return bad_orig || bad_slack || imagebad || nonrel;
}
int main(int argc, char **argv) {
    if (argc != 4)
        fail("usage: p3inc_check A B EXPORT_DIR");
    int bad = check(argv[1], argv[3], 0);
    bad |= check(argv[2], argv[3], 1);
    puts(bad ? "P3INC INDEPENDENT FAIL" : "P3INC INDEPENDENT OK");
    return bad;
}
