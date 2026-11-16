# Fewer holes in prepared function blobs

Input: `/home/exedev/paradigms/data/minio-p4`, read-only. Implementation:
`ref/p4/p4holes.c`, derived from `p4runs.c` B. No oracle is read until checking.
All variants copy the same 60,733,909 bytes in 136,634 function runs.

## Count first

`P4_COUNTS=1` produced `codex-holes-counts.log`. An independent implementation
reading the original flat columns (`holes-review.py`) reproduced every count;
see `codex-holes-independent-counts.log` and `codex-holes-review.md`.

| B record kind | Count | Code writes |
|---|---:|---|
| Integer | 5,633,606 | 1, 2, 4 or 8 bytes |
| rel32 | 700,229 | 4 bytes |
| Relocation | 1,685,086 | None |
| **Total** | **8,018,921** | **6,333,835 writing holes** |

The total includes 338,440 glue relocations; original template holes number
7,680,481. Another 95,195 marker relocations are outside this record count.
All variants produce 1,780,281 relocation records.

| Integer reuse criterion | Count |
|---|---:|
| Same signed operand value as previous occurrence of this template hole in this function | 1,135,878 |
| Low-width operand bytes already equal the supplied constant template bytes | 456,215 |
| Union (no double counting) | 1,452,746 |

The previous value is tracked by global template-hole ID, reset at function
boundaries, in emission order. The first occurrence has no predecessor. The
second criterion compares the actual supplied template bytes (zero at holes),
not a hypothetical template containing the most frequent operand. Repeated
values at different destinations still require writes unless baked into a blob.

| Width in bytes | Records (including relocation holes) |
|---|---:|
| 1 | 3,792,009 |
| 2 | 373 |
| 4 | 4,223,772 |
| 8 | 2,767 |

Widths 0, 3, 5, 6, 7 have zero records. Markers are counted separately.

Sort writable holes by position inside each function, skipping relocations:
6,245,406 consecutive pairs exist, 177,552 touch, and 5,328,335 have a gap of
at most eight bytes (including touching). Only **3,454,252 pairs** have their
*entire span* within eight bytes and room for an eight-byte store starting at
the first hole. Of these, **3,044,116** are integer/integer pairs. A small gap
alone does not imply an eight-byte store can cover both holes. These overlapping
pair counts describe opportunities, not disjoint groups or saved stores.

## Representations and scope

**H1:** copy B's zeroed function blob during preparation, then write every integer
operand into it. Compact away those integer records; emit only patches rel32 and
collects relocations. This explicitly **moves integer patching into preparation**;
it does not discover a smaller universal template library. Layout remains in
preparation as in B, but rel32 is still calculated during every emit.

**H2:** retain integers in emit. Greedily combine consecutive integer records
while their whole span fits within eight bytes, never across a function boundary
or a noninteger record. Precompute the mask and positioned integer bits from
operands. Emit performs an unaligned-safe load, `(old & ~mask) | value`, and one
eight-byte store via `memcpy`; untouched bytes survive, including any intervening
non-hole bytes. Singletons retain B's width-specific store. No oracle values
enter the masks. This also hoists bit packing into preparation, but performs the
integer writes at emit time.

**H0 diagnostic:** pre-bake integers, retain every B record, and skip stores in
the integer dispatch case. The full oracle still passes. This contrasts retaining
traversal with removing it, but is not an instruction-identical B with stores
removed: p4holes also has a mode test and masked-store dispatch case.

| Variant | Records traversed | Integer stores | rel32 stores | Hole record bytes |
|---|---:|---:|---:|---:|
| B | 8,018,921 | 5,633,606 | 700,229 | 256,605,472 |
| H1 | 2,385,315 | 0 | 700,229 | 76,330,080 |
| H2 | 6,247,428 | 3,862,113 | 700,229 | 199,917,696 |
| H0 | 8,018,921 | 0 | 700,229 | 256,605,472 |

H2 combines **3,527,790 integer holes into 1,756,297 groups**, removing
1,771,493 stores and records. It saves 31.45% of integer stores, 27.97% of all
patch stores, and 22.09% of all B records. H1 removes 88.94% of patch stores and
70.25% of records. Per output byte, patch-store counts are B **0.10429**, H2
**0.07512**, H1 **0.01153**. All still copy 60.7 MB and collect the same relocations.
Logical active record bytes shrink, but the prototype retains B-sized allocation
capacity and original mappings; these are not RSS savings.

## Correctness

`ref/p4/check-holes.sh` extends the checks used by `check-runs.sh` and is invoked
by it. H0/H1/H2 pass byte-for-byte comparison, **all 136,635 function offsets**
unconditionally, and all sorted relocations at 1/8 threads. They also pass with
eight requested threads constrained to two and anonymous output fallback.
Three explicit post-emit faults independently flip a code byte, change a function
offset, and change a relocation addend. Each produces `P4 FAIL` and nonzero exit;
no oracle file is modified. The final check also runs B and p4par at 1/8 threads;
p4par's inherited conditional offset test is now unconditional. This check-only
addition follows the timing run and does not change its emit kernel. Log: `codex-holes-check.log`.

Independent UBSan builds pass all three variants at 1/8 threads and reject each
fault (`codex-holes-ubsan.log`). The review found no correctness issue.

## Measurement protocol

Local GCC 13.3.0, `-O2 -march=native -fopenmp`; AMD EPYC 9554P, eight visible
CPUs. Five interleaved rounds of p4par, B, H1, H2 and H0 at 1/8 threads,
rotating variant order each round. `P4_REPS=256`, one excluded warmup,
`OMP_WAIT_POLICY=passive`, `OMP_PROC_BIND=true`, `OMP_PLACES=cores`,
`FLAT_HUGE=0`, `P4_HUGE=1`, address-space cap 12 GiB. Single-thread processes
are pinned with `taskset -c 6`; eight-thread processes use `taskset -c 0-7`.
Other builders share the VM: affinity prevents migration, not competing work.
The independent review briefly overlapped the first p4par process; no review
work ran during the rest of the benchmark.

Each process reports its minimum over 256 warm emissions, matching B/p4par's
existing protocol. The table gives min/median/max of **five process minima**,
not iteration percentiles. Preparation is serial, measured independently and
pooled over ten processes per variant. It includes original mapping, allocation,
layout, B blob/record construction and H1/H2 transformation; no serialization
or prepared-file loading is measured. Counting is disabled. Emit includes blob
copies, patching and relocation collection/concatenation. Allocation, partition
setup, warmup, final sort and checking are excluded. p4par also times its separate
layout, which these prepared representations move to preparation; its emit
column excludes layout. Worker partitions are B's original-item quantiles.

Raw timestamps, each process's neighboring `uptime`, oracle outcomes, and timings
are in `codex-holes-runs.log`. `summarize-holes.py` checks that every process
passed and ran measured kernels for at least one second before printing tables.

## Results

Window **2026-11-16T18:53:09Z–19:05:26Z**. All 50 processes pass their oracle
checks and receive 2 MB MAP_HUGETLB output. Minimum accumulated measured kernel
time is 3.975 s; even p4par's minimum emit multiplied by 256 exceeds one second
without including its layout. One-minute load ranges below are the `uptime`
samples immediately preceding each process, not idle-machine measurements.

| Variant | Threads | Emit min / median / max ms | One-minute load range |
|---|---:|---:|---:|
| p4par | 1 | 90.510 / 94.942 / 115.164 | 4.72–11.99 |
| B | 1 | 44.173 / 44.495 / 49.599 | 4.20–11.70 |
| H1 | 1 | 28.344 / 28.757 / 31.321 | 3.79–10.22 |
| H2 | 1 | 41.128 / 41.275 / 44.688 | 3.51–12.38 |
| H0 | 1 | 39.980 / 41.559 / 43.529 | 3.43–11.89 |
| p4par | 8 | 16.470 / 28.034 / 32.403 | 3.73–13.21 |
| B | 8 | 9.041 / 12.890 / 18.086 | 4.56–12.11 |
| H1 | 8 | 5.571 / 8.183 / 12.583 | 4.35–13.35 |
| H2 | 8 | 10.912 / 13.827 / 22.201 | 5.55–12.76 |
| H0 | 8 | 12.626 / 13.566 / 24.020 | 4.99–12.78 |

| Variant | Preparation min / median / max ms (10 processes) |
|---|---:|
| B | 318.962 / 343.769 / 802.348 |
| H1 | 367.111 / 401.802 / 908.347 |
| H2 | 364.795 / 396.145 / 1014.625 |
| H0 | 378.338 / 410.909 / 1086.152 |

Load range 3.43–13.35; kernel seconds 3.975–56.799; 50 oracle passes.

## Verdict: remove records, not just stores

**Before:** B copies one prepared blob per function, then traverses 8.02 million
32-byte hole records. It emits in **44.495 ms** at one thread. Calling that
roughly 5.8 ns per hole obscures that 1.69 million records do not write code at
all, and the measured time also includes copying and relocation vectors.

**Observation:** H0 eliminates every integer store but keeps the record stream.
It takes **41.559 ms**, only **6.6%** below B. H1 removes those records too and
takes **28.757 ms**, **1.55× faster than B**, with nonoverlapping one-thread
ranges. Removing the metadata traversal adds a much larger gain than skipping
integer stores while retaining it. B-to-H0 is a diagnostic, not a calibrated
store-latency measurement: width dispatch and operand access change too.

**Fix:** when prepared functions will be reused, bake layout-independent
integers into their blobs and omit their patch records. This removes **89% of
patch stores** and **70% of traversed records**, but only **35% of emit time**.
Time does **not** fall in proportion: patch-store proportional scaling would
predict **4.92 ms**, or record-count scaling **13.24 ms**, versus the measured
**28.76 ms**. Copying, jump work, 1.78 million relocation outputs and their
concatenation remain. H1's median preparation is **401.802 ms**, versus B's
**343.769 ms**; this is work moved into preparation, not an end-to-end compiler
speedup. Preparation alone exceeds p4par's 94.942 ms warm emission.

**Nearby-store alternative:** H2 cuts patch stores **28%**, from 6.33 million to
4.56 million, but its median is **41.275 ms** versus B's **44.495 ms** (1.08×).
Their ranges overlap: **within noise**. Patch-count proportional scaling would
predict 32.05 ms. A masked store adds a read and mask operations while retaining
most metadata; fewer store instructions alone are not a sufficient fix.

At eight threads, H1's 8.183 ms median is lower than B's 12.890 ms, but ranges
overlap: **within noise**. H2 and B also overlap. H1 versus p4par (28.034 ms)
has nonoverlapping ranges even in this busy window. The experiment supports
preparation and metadata reduction; it does not establish precise eight-core
scaling or an isolated branch-versus-cache-miss cost.

## What the counters say

`perf` was absent from PATH. Downloaded Ubuntu's `linux-tools-6.8.0-149` package
into this clone's ignored `build/` directory and extracted it there (no system
installation). Its `perf stat` works with user-mode hardware counters.
`perf-holes.sh` ran B/H0/H1/H2 serially after the five-round experiment, pinned
to CPU 6 with 256 repetitions. All four pass the oracle. Raw results:
`codex-holes-perf.log`; table reproduced by `summarize-holes-perf.py`.

These are **whole-process user counters divided by 256**, including preparation,
one extra warmup, sort and checking; they are not instrumented emit-only counts.
Each is one counter run, not another five-round timing result. Cache misses
are the generic hardware event, not measured DRAM bytes or isolated store misses.

| Variant | M cycles / rep | M instructions / rep | M branches / rep | M branch misses / rep | M cache misses / rep |
|---|---:|---:|---:|---:|---:|
| B | 202.211 | 244.795 | 58.897 | 1.712 | 0.822 |
| H0 | 200.457 | 215.645 | 47.451 | 1.171 | 0.975 |
| H1 | 129.751 | 130.729 | 19.202 | 0.601 | 0.693 |
| H2 | 198.436 | 209.381 | 42.610 | 1.839 | 0.879 |

H1 roughly halves instructions and cuts branches by two thirds. H2 cuts
instructions about 14% yet has slightly more branch misses in this sample;
its masked load/store and remaining record traffic do not disappear. Counters
support the traversal/dispatch explanation but do not isolate misprediction
penalties from memory stalls. B is already flattened: the original nested
template lookups are absent. The defensible answer is **record traversal,
dispatch and memory traffic dominate the nominal per-hole cost; it is not a
5.8 ns scalar-store cost**. H0/H1 provide the stronger controlled evidence;
there is no claim that all remaining time is branches or that cache bandwidth
alone has been measured.

## Reproduce

```sh
ref/p4/check-holes.sh /home/exedev/paradigms/data/minio-p4
# check-runs.sh also runs the above after its existing A/B/C/D checks.
OMP_NUM_THREADS=1 P4_COUNTS=1 build/p4holes /home/exedev/paradigms/data/minio-p4 H2
python3 results/holes-review.py /home/exedev/paradigms/data/minio-p4
ref/p4/bench-holes.sh /home/exedev/paradigms/data/minio-p4 > results/codex-holes-runs.log
python3 results/summarize-holes.py
# Use an installed perf via PERF=/path/to/perf, or extract the tested package locally:
(cd build && apt-get download linux-tools-6.8.0-149)
dpkg-deb -x build/linux-tools-6.8.0-149_6.8.0-149.149_amd64.deb build/holes-perf
ref/p4/perf-holes.sh /home/exedev/paradigms/data/minio-p4 > results/codex-holes-perf.log 2>&1
python3 results/summarize-holes-perf.py
```

This is a prepared-input reuse tradeoff. It changes neither fc nor the original
flat dataset, and leaves CP7's compact parent/child template-library experiment
untested.
