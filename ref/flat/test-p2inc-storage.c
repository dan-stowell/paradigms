// Minimal persistent-overlay regressions. See results/c-incremental-review.txt.
#ifndef P2INC_SOURCE
#define P2INC_SOURCE "p2inc.c"
#endif
#define main p2inc_main
#include P2INC_SOURCE
#undef main
#include <assert.h>

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "hash")) {
        rmask = 3;
        rem = malloc(4 * sizeof(*rem));
        for (u32 i = 0; i < 4; i++) rem[i].alive = 0xff;
        for (u32 i = 1; i <= 5; i++) {
            remFind(i, 0, i, 1)->alive = 1;
            remFind(i, 0, i, 1)->alive = 0; // restored facts still occupy keys
        }
        for (u32 i = 1; i <= 5; i++) assert(remFind(i, 0, i, 0)->alive == 0);
        assert(remFind(99, 0, 99, 0) == NULL);
        puts("storage hash OK");
    } else {
        nslA = 0;
        addHead = calloc(3, sizeof(*addHead));
        raddHead = calloc(3, sizeof(*raddHead));
        pool = calloc(2, sizeof(*pool));
#ifndef OLD_STORAGE
        poolcap = 2;
#endif
        // Each re-add appends a new record; two cycles exceed the old capacity.
        for (u32 i = 0; i < 2; i++) {
            edgeAdd(1, 0, 2);
            assert(pool[addHead[1]].alive && pool[raddHead[2]].alive);
            edgeRemove(1, 0, 2);
            assert(!pool[addHead[1]].alive);
        }
        assert(npool == 2);
        puts("storage pool OK");
    }
    return 0;
}
