// bench.h: timing on a noisy machine (CHARTER.md): repeat the kernel in-process until one run takes >= 1 s,
// five runs, report per-iteration min / median / max in ms.
#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>
#include <ctime>
struct Stat { double mn, md, mx; int k; };
// BENCH_CPU=1: process CPU time instead of wall time (on a loaded machine, a 1-thread run's CPU time leaves out the
// time it was descheduled; it still includes cache and SMT interference)
static double benchNow() {
    static int cpu = getenv("BENCH_CPU") ? atoi(getenv("BENCH_CPU")) : 0;
    if (!cpu) return now();
    struct timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
static Stat timeRuns(std::function<void()> f, int runs = 5, double minRun = 1.0) {
    double t = benchNow();
    f();
    double one = benchNow() - t;
    int k = std::max(1, (int)std::ceil(minRun / std::max(one, 1e-6)));
    std::vector<double> v;
    for (int r = 0; r < runs; r++) {
        t = benchNow();
        for (int i = 0; i < k; i++) f();
        v.push_back((benchNow() - t) / k * 1e3);
    }
    std::sort(v.begin(), v.end());
    return {v[0], v[runs / 2], v[runs - 1], k};
}
#define STAT_FMT "min %8.2f  med %8.2f  max %8.2f ms (x%d)"
#define STAT_ARGS(s) (s).mn, (s).md, (s).mx, (s).k
