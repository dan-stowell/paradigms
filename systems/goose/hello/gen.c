// gen N out.bin : N xorshift64* u32 values (little-endian) written to out.bin.
// Prints the oracle: total, prefix-sum checksum (sum of inclusive prefix sums, wrapping u64),
// histogram checksum (256 bins on the low 8 bits, h = h*31 + count*(bin+1), wrapping u64).
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
int main(int c, char **v) {
    long n = atol(v[1]);
    FILE *f = fopen(v[2], "wb");
    uint64_t s = 88172645463325252ULL;
    uint32_t *b = malloc(n * 4);
    for (long i = 0; i < n; i++) {
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        b[i] = (uint32_t)((s * 2685821657736338717ULL) >> 32);
    }
    fwrite(b, 4, n, f);
    fclose(f);
    uint64_t p = 0, ps = 0, hist[256] = {0};
    for (long i = 0; i < n; i++) { p += b[i]; ps += p; hist[b[i] & 255]++; }
    uint64_t hs = 0;
    for (int i = 0; i < 256; i++) hs = hs * 31 + hist[i] * (i + 1);
    printf("oracle n=%ld total=%llu prefix_checksum=%llu hist_checksum=%llu\n", n,
           (unsigned long long)p, (unsigned long long)ps, (unsigned long long)hs);
    return 0;
}
