# codex-h: separate dense emission from partition ownership

Two intermediate implementations, with baseline and compact unchanged:

- **onlyA:** one dense emission pass, compact member scatter, then all tables built before two global candidate probes from the unpartitioned chunk lists. Retains CAS and separate member readback.
- **onlyB:** baseline dense count and dense scatter, extended to partition candidates. Retains 24-byte member records plus separate slot IDs. Each partition owns build, candidate max, candidate readback and member readback, with no CAS.

B necessarily gathers candidates during count/scatter and removes the two old dense candidate scans. Thus these are the two implementation packages, not an orthogonal experiment on individual instructions: A includes 32-byte records and eager candidate rank loads; B includes table ownership, fused member readback, and fewer barriers. Effects need not add. [Implementation/review details](codex-h-onlyB-notes.md).

## Reproduce

Run from the checkout (GCC 13.3.0, `-O2 -march=native -fopenmp`, AMD EPYC 9554P, eight visible CPUs):

```sh
prlimit --as=8589934592 bash <<'SH'
mkdir -p build-h
for v in p1par p1onlyA p1onlyB p1compact; do
  gcc -O2 -march=native -fopenmp -o build-h/$v ref/flat/$v.c
done
python3 results/codex-h-phases.py
for v in p1par p1compact; do
  gcc -O2 -march=native -fopenmp -Iref/flat -o build-h/$v-phases build-h/$v-phases.c
done
python3 results/codex-h-bench.py
SH
```

Every child inherits the 8 GiB address-space cap. Runner explicitly sets `OMP_WAIT_POLICY=passive`, threads 1 or 8, and executes five interleaved processes per configuration, rotating the first variant each round. Each process runs 64 kernel repetitions; reported join is the mean over repetitions, including first-touch/allocation on iteration one. Relocations and 1c run and are checked but are excluded from join times. No affinity pinning, no wait for quiet, no perf counters. Phase copies run separately after the main comparison, five interleaved processes per dataset at one thread.

All 100 recorded processes printed **P1 OK**, with zero differences for name canon, builtins, relocation targets and content canon. The main runner independently reran both new implementations against the stock oracles at both thread counts. `check.sh` is unchanged.

Window: 2026-11-16T16:41:51.324551+00:00 to 2026-11-16T16:55:09.775595+00:00.

Before: `16:41:51 up  1:26,  0 user,  load average: 2.30, 3.75, 5.29`. After: `16:55:09 up  1:39,  0 user,  load average: 3.98, 4.39, 4.64`.

Main kernel durations: 1.009–11.507 s; all >=1 s. Raw stdout and [machine-readable measurements](codex-h-raw/measurements.json) are retained.

## Join results

Milliseconds per iteration; min / median / max of five process means.

| Dataset | Threads | Variant | Join ms |
|---|---:|---|---:|
| minio | 1 | p1par | 43.216 / 46.085 / 47.891 |
| minio | 1 | p1onlyA | 36.283 / 36.849 / 38.945 |
| minio | 1 | p1onlyB | 29.006 / 29.528 / 30.475 |
| minio | 1 | p1compact | 28.135 / 28.917 / 29.674 |
| minio | 8 | p1par | 12.220 / 13.968 / 14.040 |
| minio | 8 | p1onlyA | 10.054 / 10.922 / 11.207 |
| minio | 8 | p1onlyB | 8.567 / 9.804 / 17.904 |
| minio | 8 | p1compact | 7.680 / 8.466 / 9.001 |
| kube-apiserver | 1 | p1par | 100.398 / 101.934 / 112.930 |
| kube-apiserver | 1 | p1onlyA | 81.936 / 84.405 / 86.077 |
| kube-apiserver | 1 | p1onlyB | 67.020 / 73.838 / 75.691 |
| kube-apiserver | 1 | p1compact | 61.101 / 63.428 / 69.003 |
| kube-apiserver | 8 | p1par | 28.142 / 28.357 / 31.966 |
| kube-apiserver | 8 | p1onlyA | 20.505 / 23.317 / 25.766 |
| kube-apiserver | 8 | p1onlyB | 18.171 / 20.421 / 22.083 |
| kube-apiserver | 8 | p1compact | 20.153 / 20.435 / 20.693 |

## Per-phase diagnostics, one thread

Instrumented copies preserve the original loop order, including compact's fused per-partition table work. The compact timers sum each phase across 256 partitions; they do not split the join into separate whole-table passes. Build includes allocation and zeroing. Scatter includes prefix sums; member readback includes builtins. Baseline candidate times include their dense scans. Phase clock/dispatch gaps are excluded; clock overhead and the later measurement window mean these figures are diagnostic, not a subtraction from the main comparison.

Each cell is min / median / max ms over five process means (64 iterations each).

| Dataset | Variant | Count / emit | Prefix + scatter | Build | Candidate max | Candidate readback | Member readback + builtins |
|---|---|---:|---:|---:|---:|---:|---:|
| minio | p1par | 6.350 / 6.479 / 7.187 | 7.273 / 7.815 / 8.440 | 5.509 / 6.311 / 7.125 | 7.526 / 7.935 / 9.123 | 6.955 / 7.275 / 8.878 | 8.016 / 8.484 / 9.970 |
| minio | p1compact | 7.714 / 7.967 / 8.415 | 7.394 / 7.830 / 8.148 | 5.867 / 6.568 / 6.917 | 0.919 / 0.928 / 0.940 | 0.789 / 0.806 / 0.830 | 2.990 / 3.159 / 3.388 |
| kube-apiserver | p1par | 16.424 / 16.840 / 18.406 | 19.148 / 20.394 / 21.860 | 12.188 / 13.942 / 15.514 | 13.546 / 14.439 / 16.547 | 13.297 / 13.990 / 15.766 | 18.560 / 20.724 / 22.293 |
| kube-apiserver | p1compact | 18.612 / 19.849 / 21.365 | 17.067 / 18.005 / 19.648 | 13.850 / 14.870 / 16.348 | 0.806 / 0.835 / 0.973 | 0.655 / 0.663 / 0.770 | 9.292 / 9.936 / 11.801 |

## Verdict

**B (partition ownership) carries most of the observed ~1.5x improvement. A helps, but eliminating dense passes alone does not explain it.** At one thread, full compact is 1.59x faster on MinIO and 1.61x on kube. A-only is 1.25x / 1.21x; B-only is 1.56x / 1.38x. B-only beats A-only with non-overlapping ranges on both datasets, and each beats baseline. B-only captures 96% / 73% of the baseline-to-compact median reduction; A-only captures 54% / 46%. These are descriptive fractions, not additive causal shares: their sum exceeds 100% because the changes overlap in removing dense candidate scans and interact.

B-only versus compact is **within noise** on both datasets at one thread: their ranges overlap. The additional kube median reduction from 73.8 to 63.4 ms is suggestive, but does not clear the charter's range rule. At eight threads, A-only and compact beat baseline on both datasets; B-only beats baseline on kube but is **within noise** on MinIO due to the 17.9 ms run. A-only versus B-only, and B-only versus compact, are **within noise** at eight threads. No outlier was discarded.

The phase evidence explains why the old 18–21% stream-byte model underpredicts the gain:

- **Count/emission + scatter does not get faster:** the median of each process's combined phases is 14.17 → 15.81 ms on MinIO and 37.34 → 37.85 ms on kube. A single dense pass trades rereads for emitting and scattering larger records.
- **The two candidate phases collapse:** their combined median falls 15.18 → 1.74 ms on MinIO and 28.43 → 1.49 ms on kube. Compact avoids dense candidate scans and probes the just-built local table twice, instead of visiting tables across the global working set.
- **Member readback also benefits:** median 8.48 → 3.16 ms on MinIO and 20.72 → 9.94 ms on kube. Compact reads members immediately after its own partition's candidate work, whereas baseline waits until all tables and global candidates have been processed. This effect was missing from a candidate-only explanation of B.
- **Build itself is within noise:** its phase ranges overlap on both datasets. The big diagnostic changes are in probing/readback, not faster table construction.

The phase sums differ from instrumented full join times by only 0.002–0.119 ms across the 20 diagnostic processes. All diagnostic kernel durations also exceed one second. These phase measurements and the intermediates support locality/ownership plus avoided scans as the mechanism; they do not measure cache misses, DRAM traffic, or isolate the cost of CAS. The prior byte model explicitly excluded data-dependent table probes and cache reuse, so a larger timing gain is not a contradiction.

Implementation and results are committed/pushed only to `codex-h`, using the requested scratch-clone fallback for the worktree's read-only git metadata. Build products remain under `build-h/` and are ignored; no changes to `check.sh`, p1par or p1compact.
