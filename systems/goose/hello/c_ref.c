// C reference for hello1: fread whole file, prefix sum into u64 array, histogram, checksums.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
static double now() { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
int main(int c, char **v) {
    double t0 = now();
    FILE *f = fopen(v[1], "rb"); fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    uint32_t *b = malloc(sz);
    if (fread(b, 1, sz, f) != (size_t)sz) return 1;
    fclose(f);
    long n = sz / 4;
    double t1 = now();
    uint64_t *pre = malloc(n * 8), p = 0, hist[256] = {0};
    for (long i = 0; i < n; i++) { p += b[i]; pre[i] = p; hist[b[i] & 255]++; }
    uint64_t ps = 0;
    for (long i = 0; i < n; i++) ps += pre[i];
    uint64_t hs = 0;
    for (int i = 0; i < 256; i++) hs = hs * 31 + hist[i] * (i + 1);
    double t2 = now();
    printf("c n=%ld total=%llu prefix_checksum=%llu hist_checksum=%llu\n", n,
           (unsigned long long)p, (unsigned long long)ps, (unsigned long long)hs);
    fprintf(stderr, "c: read %.1f ms, scan+hist+checksum %.1f ms, total %.1f ms\n", t1 - t0, t2 - t1, t2 - t0);
    return 0;
}
