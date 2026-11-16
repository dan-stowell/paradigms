# Tuned C layout (3a), 2026-11-16

`p3a_par.c` matches all 360,315 MinIO and 430,349 kube-apiserver oracle addresses
at both 1 and 8 threads. The full `check.sh` suite also passed on both datasets
with `BUILD_DIR=build-i`, 8 threads, and the 8 GiB address-space limit. It never opens `p3_addr_all.u32`. An independent helper
rebuilt it and repeated all four checks; details in [the profile review](codex-i-profile.md).

Each unit independently accumulates bucket/class sizes, turns them into local
class offsets, and scatters local addresses in list order. Fourteen independent
bucket scans add the required unit alignment and compute unit bases; a parallel
pass adds those bases. Typelinks use MSD byte radix sorting, cached string pointers
and lengths, insertion sorting for small leaves, and symbol IDs to break ties.
Large radix partitions become OpenMP tasks. The final typelink address walk is serial.
All classification, pointer preparation, sorting, and address generation repeat
inside the timer; no sorted order or computed layout is cached across repetitions.

## Reproduce

```sh
mkdir -p build-i
gcc -O2 -march=native -o build-i/p3 ref/flat/p3.c
gcc -O3 -march=native -fopenmp -o build-i/p3a_par ref/flat/p3a_par.c
python3 ref/flat/bench-3a.py
# Full reference suite, with the new cell included:
BUILD_DIR="$PWD/build-i" OMP_NUM_THREADS=8 prlimit --as=8589934592 ref/flat/check.sh data/minio
```

The script runs five rounds, interleaving baseline, tuned 1/8 threads, and tuned
1/8 with `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, on both datasets. Every process
uses `prlimit --as=8589934592`, `OMP_WAIT_POLICY=passive`, and >=1 second of timed
kernel repetitions. Plain p3's layout is serial regardless of OMP_NUM_THREADS.
Means below are per-process kernel means, then min/median/max over five processes;
input mmap/loading and oracle verification are excluded. Tuned scratch allocations
are outside timing, while baseline preserves its original allocations inside
(~0.07 ms); tuned scratch clearing and initial scratch faults remain inside.
These are warm kernel measurements, not end-to-end link times. No CPU affinity.
Raw outputs and uptime before/after **every process**: [codex-i-runs.json](codex-i-runs.json).

Window: `17:01:05 up  1:45,  0 user,  load average: 4.14, 3.42, 4.07` through `17:01:56 up  1:46,  0 user,  load average: 4.37, 3.63, 4.11`.

## Per-phase baseline profile

Medians of the same five >=1 s baseline runs; logical bytes per iteration are
array-access counts, not hardware DRAM counters. Sort counters run separately
outside timing, preserving the original timed comparator.

| Phase | MinIO ms | kube ms | MinIO read / write bytes | kube read / write bytes |
|---|---:|---:|---:|---:|
| allocate | 0.047 | 0.064 | 0 / 2,908,076 | 0 / 4,298,100 |
| classify | 1.565 | 2.231 | 4,778,322 / 2,749,476 | 5,661,086 / 3,260,892 |
| bucket_prefix | 0.354 | 0.947 | 2,464,632 / 1,283,464 | 4,351,320 / 2,254,616 |
| scatter | 0.704 | 0.933 | 4,612,017 / 2,749,476 | 5,433,711 / 3,260,892 |
| sort | 4.663 | 7.408 | 33,039,706 / 3,713,576 | 50,286,632 / 5,272,824 |
| typelink_walk | 0.093 | 0.215 | 299,349 / 133,044 | 409,275 / 181,900 |

Sort comparisons: **441,528 / 630,658** (MinIO / kube); distinguishing string
prefix bytes: 8,120,114 / 14,726,816. The review document specifies the traffic
formulas and glibc merge-movement model, including omissions and fallback limits.

## Five-run comparison

| Dataset | Kernel | Threads | malloc hugepage request | min / median / max ms |
|---|---|---:|---|---:|
| minio | p3 | 1 | default | 7.248 / 7.373 / 7.588 |
| minio | p3a_par | 1 | default | 4.897 / 4.998 / 5.411 |
| minio | p3a_par | 8 | default | 3.657 / 3.888 / 7.562 |
| minio | p3a_par | 1 | 2 MB | 4.782 / 4.807 / 5.060 |
| minio | p3a_par | 8 | 2 MB | 4.004 / 4.335 / 5.963 |
| kube-apiserver | p3 | 1 | default | 11.347 / 11.812 / 12.781 |
| kube-apiserver | p3a_par | 1 | default | 7.622 / 7.760 / 8.086 |
| kube-apiserver | p3a_par | 8 | default | 5.410 / 6.078 / 7.616 |
| kube-apiserver | p3a_par | 1 | 2 MB | 7.459 / 7.604 / 8.106 |
| kube-apiserver | p3a_par | 8 | 2 MB | 5.407 / 5.467 / 6.813 |

One-thread tuned vs baseline is a clear ~1.5x improvement on both datasets (ranges
do not overlap). MinIO 8-thread default vs baseline overlaps: **within noise** by
the charter's criterion, despite the lower median. Kube's 8-thread default range
is below baseline, but the 1-vs-8-thread tuned gap is too small to call an order-of-
magnitude result. Hugepage/default ranges overlap in all tuned comparisons:
**within noise**. The tunable is a request; this run does not prove which malloc
allocations were backed by reserved hugepages. Input file mappings remain normal.

Initial radix code reread string offsets on every digit and made many small task
constructs; caching pointers and avoiding small task creation gave the final version.
Exploratory measurements are not used to claim a separate optimization speedup.

## Floor verdict

A useful one-pass compulsory-column model is 10L input bytes (bucket, class, size,
unit), 4L output bytes, 8T typelink symbol/string IDs, distinct referenced string
offset entries, the referenced string bytes, and 128 bytes of bucket metadata.
MinIO: 32,136 distinct strings, 1,201,312 string bytes, 32,137 offset entries;
kube: 44,144 strings, 2,041,702 string bytes, 44,145 offset entries. Total useful
traffic: **6,769,034 / 8,783,676 bytes**. It assumes reading each whole referenced
string once; a comparison sort can skip unique suffixes, so this is a bandwidth
model rather than a strict information-theoretic minimum. The non-string compulsory
columns alone are 5.568 / 6.742 MB. It excludes scratch passes and cache-line effects.

At this VM's 25 / 36 GB/s bandwidth, the one-pass model is **0.271 / 0.188 ms**
(MinIO, 1 / 8 threads), **0.351 / 0.244 ms** (kube). Thus the best default MinIO
8-thread median **3.888 ms** is **6.5x FLOOR.md's 0.60 ms**, and ~21x its own flat
kernel bandwidth model. This cell does **not** reach the floor. Radix sorting still
costs multiple dependent string passes; unit scans, repeated scratch traffic, task
scheduling and passive-team wakeups also prevent streaming bandwidth. The hlink
85 / 23–25 ms layout phase includes additional work and is not a like-for-like speedup.

Implementation: 81 lines of C (including comments/pragmas).

Easy: independent units and buckets. Forced: radix task decomposition and cached
string metadata. Remaining limits: deep common-prefix recursion and quadratic
ID insertion sorting for huge equal-string groups; neither blocks these datasets.
