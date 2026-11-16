// Driver: mmap raw LE u32 files (idx: 50M, table: 16M), run AOT Halide pipelines, verify, time.
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <functional>
#include "HalideBuffer.h"
#include "gather.h"
#include "hist0.h"
#include "hist1.h"
#include "hist2.h"
#include "scan0.h"
#include "scan1.h"
using Halide::Runtime::Buffer;

static const uint32_t *map_file(const char *p, size_t *n) {
    int fd = open(p, O_RDONLY);
    if (fd < 0) { perror(p); exit(1); }
    struct stat st;
    fstat(fd, &st);
    void *m = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (m == MAP_FAILED) { perror("mmap"); exit(1); }
    *n = st.st_size / 4;
    return (const uint32_t *)m;
}
static double ms(std::function<void()> f, int reps = 5) {
    double best = 1e30;
    for (int i = 0; i < reps; i++) {
        auto t = std::chrono::steady_clock::now();
        f();
        double d = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
        if (d < best) best = d;
    }
    return best;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s idx.u32 table.u32\n", argv[0]); return 2; }
    size_t N, T;
    const uint32_t *idx = map_file(argv[1], &N), *table = map_file(argv[2], &T);
    const int NB = 1 << 20;
    Buffer<uint32_t, 1> bidx((uint32_t *)idx, (int)N), btab((uint32_t *)table, (int)T);
    Buffer<uint32_t, 1> out((int)N), hist(NB), ps((int)N);
    std::vector<uint32_t> rg(N), rh(NB, 0), rs(N);
    auto cgather = [&] { for (size_t i = 0; i < N; i++) rg[i] = table[idx[i] < T ? idx[i] : T - 1]; };
    auto chist = [&] { std::fill(rh.begin(), rh.end(), 0); for (size_t i = 0; i < N; i++) rh[idx[i] % NB]++; };
    auto cscan = [&] { uint32_t a = 0; for (size_t i = 0; i < N; i++) { a += idx[i]; rs[i] = a; } };
    cgather(); chist(); cscan();
    auto chk = [&](const char *nm, const uint32_t *got, const std::vector<uint32_t> &ref) {
        for (size_t i = 0; i < ref.size(); i++)
            if (got[i] != ref[i]) { printf("MISMATCH %s at %zu\n", nm, i); exit(1); }
    };
    const char *th = getenv("HL_NUM_THREADS");
    printf("N=%zu table=%zu bins=%d HL_NUM_THREADS=%s\n", N, T, NB, th ? th : "(default)");
    printf("%-22s %10.2f ms\n", "gather", ms([&] { gather(bidx, btab, out); })); chk("gather", out.data(), rg);
    printf("%-22s %10.2f ms\n", "hist serial", ms([&] { hist0(bidx, hist); })); chk("hist0", hist.data(), rh);
    printf("%-22s %10.2f ms\n", "hist atomic.parallel", ms([&] { hist1(bidx, hist); })); chk("hist1", hist.data(), rh);
    printf("%-22s %10.2f ms\n", "hist rfactor", ms([&] { hist2(bidx, hist); })); chk("hist2", hist.data(), rh);
    printf("%-22s %10.2f ms\n", "scan serial", ms([&] { scan0(bidx, ps); })); chk("scan0", ps.data(), rs);
    printf("%-22s %10.2f ms\n", "scan blocked", ms([&] { scan1(bidx, ps); })); chk("scan1", ps.data(), rs);
    printf("all Halide outputs verified against plain C++\n");
    printf("%-22s %10.2f ms\n", "[c++] gather 1thr", ms(cgather));
    printf("%-22s %10.2f ms\n", "[c++] hist 1thr", ms(chist));
    printf("%-22s %10.2f ms\n", "[c++] scan 1thr", ms(cscan));
    return 0;
}
