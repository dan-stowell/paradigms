// writes idx.u32 (50M random < 16M) and table.u32 (16M random), raw little-endian
#include <cstdio>
#include <cstdint>
#include <vector>
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    uint64_t s = 88172645463325252ull;
    auto rnd = [&] { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); };
    const size_t N = 50000000, T = 1 << 24;
    std::vector<uint32_t> v(N);
    for (auto &x : v) x = rnd() & (T - 1);
    char p[512];
    snprintf(p, sizeof p, "%s/idx.u32", dir);
    FILE *f = fopen(p, "wb"); fwrite(v.data(), 4, N, f); fclose(f);
    v.resize(T);
    for (auto &x : v) x = rnd();
    snprintf(p, sizeof p, "%s/table.u32", dir);
    f = fopen(p, "wb"); fwrite(v.data(), 4, T, f); fclose(f);
}
