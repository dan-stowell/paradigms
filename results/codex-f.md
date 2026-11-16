# codex-f: compact the name join

Implemented `ref/flat/p1compact.c`: one parallel dense pass initializes canon and emits per-chunk members and candidates, each with (k1, k2, val, slot). Separate per-(chunk, partition) histograms scatter both lists. Each partition owns its table through build, candidate max, candidate readback and member readback. Max is deterministic and candidates never create groups. Relocation pass 4 and the oracle/1c tail are unchanged. `check.sh` builds and runs the new reference.

All 40 measured processes must print `P1 OK`; all four exact oracle comparisons report zero differences. Independent review also verified disjoint scratch regions, scatter destinations and partition ownership, and independently checked kube-apiserver on one thread. Details: [review](codex-f-raw/review.md). The review’s record-count observation was applied: baseline build charges 24B, and readback charges 4B+24M, excluding the separate slotOf stream during build.

## Measurement

GCC 13.3.0, AMD EPYC 9554P VM, 8 visible CPUs. Commands, run from the checkout:

```sh
mkdir -p build-f
gcc -O2 -march=native -fopenmp -o build-f/p1par ref/flat/p1par.c
gcc -O2 -march=native -fopenmp -o build-f/p1compact ref/flat/p1compact.c
python3 results/codex-f-bench.py
prlimit --as=8589934592 python3 results/codex-f-bytes.py
```

The runner executes `OMP_NUM_THREADS={1,8} prlimit --as=8589934592 build-f/{p1par,p1compact} data/{minio,kube-apiserver} 100`, five interleaved pairs per dataset/thread count, baseline first in each pair. No pinning or wait for quiet. Both binaries retain the original best-time output and add a mean over all 100 kernel repetitions. The table reports min/median/max across the five process means, in ms per iteration; **total means name join + relocation targets (1a+1b)**, matching p1par’s total, and excludes loading, checks and 1c. The unchanged 1c still runs 100 repetitions and is checked separately. First-touch/allocation costs in the first kernel iteration are included in the mean.

Window: 2026-11-16T16:18:49.703391+00:00 to 2026-11-16T16:30:05.950850+00:00. Before: `16:18:49 up  1:03,  0 user,  load average: 4.43, 3.96, 3.17`. After: `16:30:05 up  1:14,  0 user,  load average: 15.70, 11.64, 7.58`.

| Dataset | Threads | Version | Join min / median / max (ms) | Total min / median / max (ms) |
|---|---:|---|---:|---:|
| minio | 1 | p1par | 50.135 / 64.031 / 100.696 | 86.643 / 110.926 / 172.053 |
| minio | 1 | p1compact | 28.046 / 28.633 / 65.107 | 65.260 / 65.678 / 146.333 |
| minio | 8 | p1par | 18.566 / 46.334 / 58.922 | 27.880 / 62.882 / 79.212 |
| minio | 8 | p1compact | 13.332 / 23.449 / 41.940 | 24.420 / 40.195 / 65.697 |
| kube-apiserver | 1 | p1par | 110.245 / 124.077 / 140.846 | 175.032 / 199.877 / 227.230 |
| kube-apiserver | 1 | p1compact | 69.238 / 77.424 / 84.626 | 133.398 / 152.681 / 168.042 |
| kube-apiserver | 8 | p1par | 57.739 / 68.655 / 123.722 | 83.036 / 99.163 / 172.263 |
| kube-apiserver | 8 | p1compact | 34.315 / 38.339 / 61.452 | 62.158 / 67.513 / 103.013 |

Recorded runs: 40; kernel duration 2.442–22.723 s per process (all >=1 s). Raw stdout and machine-readable timings: [codex-f-raw](codex-f-raw/measurements.json). Load changed substantially during this window; short independent correctness/counting work also overlapped the early MinIO runs.

Attempted both binaries with `perf stat -e cycles,instructions,cache-misses env OMP_NUM_THREADS=8 prlimit --as=8589934592 build-f/NAME data/minio 100`. Both attempts failed before launch: `perf: command not found`. No hardware-counter claim is made.

## Count the mechanism

The lead’s “four whole name-key arrays” estimate is too high. In GCC’s generated assembly, count and scatter load k1 for every slot, but both candidate passes test `kind == 1` before loading k1. A 16-byte stride over k1 covers the whole array’s cache lines; candidate passes cover only cache lines containing kind-1 slots. Reproduce with `objdump -d -Mintel build-f/p1par`: inspect `main._omp_fn.0`, `.1`, `.3`, `.4`.

Use N=slots, D=all kind-1 slots, A=keyed kind-1 candidates, M=keyed kinds 4/5, K=M+A, W=wanted keys, B=M+W, L=K+W, H=candidates whose keys already have a group, nb=builtins, Q=distinct 64-byte name-key cache lines containing kind-1 slots. S is total hash-table capacity: sum over 256 partitions of the smallest power of two >= max(64, 2*partition members). These counts are printed by [codex-f-bytes.py](codex-f-bytes.py).

| Dataset | N | D | A | M | W | H | Q | S |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| minio | 5,205,427 | 1,124,269 | 162,385 | 627,018 | 326 | 3,652 | 282,108 | 1,839,104 |
| kube-apiserver | 13,270,144 | 2,754,106 | 168,132 | 1,664,902 | 325 | 2,815 | 690,134 | 4,595,712 |

**Streaming arithmetic, not measured DRAM bytes.** Charge name-key cache-line coverage per pass, whole record extents for each traversal (24+4 bytes for baseline rec/slotOf, 32 including padding for compact), and other fields at logical size. Ignore boundary-line rounding (under 128 bytes per dense pass). Include table zeroing, but exclude data-dependent hash probes/inserts/max updates/CAS, hot histogram/prefix metadata, allocator traffic, write allocation, cache reuse, and hardware prefetch. Sparse kind/rank/canon fields can fetch more cache-line bytes than their logical sizes. Record extent charges can exceed executed field loads. Thus this model compares explicit streams; it is not an exact total of all memory transactions.

```text
Z = 4*(N+1) + 4*(M+H) + 4*nb + 24*S   # canon + builtin stores and table zeroing
p1par reads  = 37*N + 128*Q + 4*(M+H) + 28*B + 24*M + 24*W + 16*nb
p1par writes = Z + 28*B
p1compact reads  = 17*N + 5*K + 96*L + 16*W + 16*nb
p1compact writes = Z + 64*L
```

Baseline reads: two full name-key streams (32N), two candidate key streams (128Q), four kind streams plus flags (5N), member and matched-candidate ranks (4(M+H)), table-build records (24B), slotOf readback (4B), member-record readback (24M), wants and builtin keys. Compact reads: one full name-key stream plus flags (17N), keyed kind/rank (5K), one scratch read and two partition-record traversals (96L), wants and builtin keys. Compact writes each record twice (scratch then scatter); baseline writes records/slotOf once. nb=299 in both datasets.

| Dataset | Version | Reads (bytes) | Writes (bytes) | Sum (decimal MB) |
|---|---|---:|---:|---:|
| minio | p1par | 263,859,975 | 85,049,716 | 348.910 |
| minio | p1compact | 168,263,258 | 118,026,740 | 286.290 |
| kube-apiserver | p1par | 672,599,936 | 216,676,088 | 889.276 |
| kube-apiserver | p1compact | 410,770,066 | 287,384,708 | 698.155 |

minio: name-key coverage 202.683 → 83.287 MB. Modeled read+write streams fall by 62.620 MB (17.9%, 1.22x ratio), while writes increase by 32.977 MB.
kube-apiserver: name-key coverage 512.982 → 212.322 MB. Modeled read+write streams fall by 191.121 MB (21.5%, 1.27x ratio), while writes increase by 70.709 MB.

## Verdict

Only kube-apiserver at one thread clears the charter’s range test: join median 124.077 → 77.424 ms (37.6% lower, 1.60x), with compact max 84.626 ms below baseline min 110.245 ms by 25.619 ms. Its total also improves beyond the range (199.877 → 152.681 ms median). MinIO at both thread counts and kube-apiserver at eight threads are **within noise**, despite lower medians. The requested compact implementation helps this larger workload at one thread; the stronger claim that dense traffic, rather than hash-table behavior, is the bottleneck remains unproven.

| Dataset | Threads | Join median reduction | Join baseline min minus compact max | Join ranges separate? | Total median reduction | Total ranges separate? |
|---|---:|---:|---:|---|---:|---|
| minio | 1 | 35.398 ms (55.3%) | -14.971 ms | no: within noise | 45.248 ms (40.8%) | no: within noise |
| minio | 8 | 22.884 ms (49.4%) | -23.374 ms | no: within noise | 22.687 ms (36.1%) | no: within noise |
| kube-apiserver | 1 | 46.653 ms (37.6%) | 25.619 ms | yes | 47.196 ms (23.6%) | yes |
| kube-apiserver | 8 | 30.316 ms (44.2%) | -3.713 ms | no: within noise | 31.650 ms (31.9%) | no: within noise |

The hypothesis’s MinIO 400 MB read estimate does not describe this code: name keys cover about 203 MB, and this explicit-stream model charges about 264 MB reads plus 85 MB writes. Compact trades rereads for record staging, and also keeps candidate probes under one partition owner, removes CAS, and reduces parallel barriers. Those effects are confounded; these measurements cannot isolate dense-array bandwidth from table locality or scheduling.

Using the lead’s quoted read-stream ceilings only as an illustration (not remeasured here), the MinIO key-coverage reduction alone is 119.397 MB / 36 GB/s = 3.32 ms at eight threads, or 4.78 ms at 25 GB/s on one thread. After charging the added record writes, the modeled net stream saving is about 63 MB, equivalent to only 1.74 ms at 36 GB/s (applying a read-only ceiling to mixed traffic is optimistic). The measured median changes must be judged against the reported ranges, not interpreted as confirmation of that bandwidth ceiling. The original 8.6–10.5 ms lead timing came from another window and is not a baseline for this noisy-window comparison.

Implementation and results are committed on `codex-f` using the requested scratch-clone fallback because `/home/exedev/paradigms-c/.git` points at read-only metadata outside the writable workspace; no push to main.
