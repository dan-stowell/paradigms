# P4: parallel C layout and emit

Implementation: `ref/p4/p4par.c` (264 lines including file mapping and oracle checks). Reproduce correctness with
`ref/p4/check.sh /home/exedev/paradigms/data/minio-p4`; reproduce measurements with
`ref/p4/bench-par.sh /home/exedev/paradigms/data/minio-p4`.
Both scripts build/run locally; benchmark requires check.sh's build first. Every dataset
mapping is read-only. Runs use `prlimit --as=12884901888` (12 GiB address-space cap).

## Algorithm and timed scope

Layout splits the 2,381,252 items into contiguous equal-count chunks, computes local
exclusive prefix sums, scans the worker totals, and adds each chunk's base. All three
steps and OpenMP synchronization are timed. Emit assigns contiguous function ranges
whose boundaries approximate equal item counts. Functions aren't split, so markers and
item output have one owner. Per-worker relocation vectors are reusable; concatenation
in increasing range order preserves the serial traversal order, including markers.
Concatenation is included in emit; no atomics or sorting are needed in that kernel.

The per-item template selection, memcpy, binary search for segment holes, and integer /
rel32 / symbol cases remain explicit, following p4.c. Output requests 2 MB MAP_HUGETLB
pages, falling back to anonymous mmap plus MADV_HUGEPAGE advice. Every measured parallel
run obtained MAP_HUGETLB pages. `P4_HUGE=0` exercises the fallback; transparent huge pages
are not guaranteed there. Inputs retain the default file mappings (`FLAT_HUGE=0`).

Both programs execute one untimed warmup then `P4_REPS=64` measured layout+emit iterations
in one process, reuse buffers, and report separate phase minima. These minima need not
come from the same iteration. File mapping, allocation, parallel function partition
setup, the warmup, sorting, and oracle comparisons are outside the phase times. Private
vector growth occurs during warmup for this fixed workload. The initial OpenMP team size is sampled before timing and used for partitioning/reporting.
The small serial p4.c change
adds the same repetition protocol without changing its scan or per-item emit algorithm.
These are warm kernel times, not end-to-end compiler or file-loading times.

## Correctness

Every normal run compares all 60,733,909 code bytes and the sorted list of 1,780,281
relocations against the supplied oracle. Sorting is never charged to emit. The independent
review (`codex-p4par-review.md`) rebuilt separately and checked one thread, a constrained
OpenMP team, and forced page fallback. `codex-p4par-check.log` records the normal build/check.
`codex-p4par-final-check.log` repeats checks on the final source and exercises a two-thread
limit together with forced fallback. After the benchmark, the only code adjustment was
sampling the initial actual OpenMP team size for reporting/partitioning (outside timing),
instead of using its requested maximum; the timed kernels are unchanged. This corrects
the review's reporting caveat; unconstrained benchmark teams remain 1 and 8.
A no-patch diagnostic is explicitly labeled and never prints P4 OK or exports code.

## Measurement

VM: 8 visible CPUs, AMD EPYC 9554P, GCC 13.3.0, `-O2 -march=native` plus `-fopenmp`
for p4par. `OMP_WAIT_POLICY=passive`; no CPU pinning; 1,536 configured 2 MB huge pages.
Five rounds run serial p4, p4par/1, p4par/8 in that order, interleaved in the same window.
`codex-p4par-runs.log` retains timestamps, uptime/load, every per-process minimum, total
measured duration, page allocation status and oracle outcomes.

Window: **2026-11-16T17:22:36Z through the final diagnostic starting 2026-11-16T17:28:24Z**.
One-minute load at recorded starts ranged **14.67–22.45**.

Each table entry is min / median / max across the five per-process phase minima, in ms.

| Variant | Layout ms | Emit ms | Measured seconds per process (min–max) |
|---|---:|---:|---:|
| p4, 1 core | 6.496 / 6.945 / 9.777 | 115.811 / 145.367 / 174.484 | 19.171–43.884 |
| p4par, 1 thread | 7.762 / 7.908 / 10.427 | 106.864 / 117.722 / 150.179 | 18.269–33.304 |
| p4par, 8 threads | 2.894 / 4.283 / 7.756 | 51.034 / 65.553 / 68.189 | 5.996–7.745 |

Per-run layout / emit minima (ms), preserving the interleaving order:

| Round | p4/1 | p4par/1 | p4par/8 |
|---|---:|---:|---:|
| 1 | 9.777 / 174.484 | 8.032 / 117.722 | 2.894 / 51.034 |
| 2 | 6.697 / 145.367 | 7.908 / 150.179 | 3.626 / 67.551 |
| 3 | 6.496 / 115.811 | 7.762 / 109.339 | 4.283 / 68.189 |
| 4 | 7.883 / 168.314 | 10.427 / 137.601 | 6.662 / 54.387 |
| 5 | 6.945 / 117.947 | 7.763 / 106.864 | 7.756 / 65.553 |

Eight-thread layout: **1.62×** serial by ratio of medians. Ranges overlap: within noise.
Eight-thread emit: **2.22×** serial by ratio of medians. Ranges do not overlap.

The one-thread implementation and serial reference have overlapping ranges: **within noise**;
this experiment does not isolate a huge-page benefit. The eight-thread result combines parallelism,
huge-page output and private relocation buffering. Host contention is substantial; no claim
of ideal eightfold scaling or stable unloaded latency follows from these runs.


## Counts and bandwidth estimate

Rerun `prlimit --as=12884901888 python3 ref/p4/count-par.py`; exact results are in
`codex-p4par-counts.log`. The counter reads the flat files independently of the emitter.

| Work per iteration | Count |
|---|---:|
| Items / functions | 2,381,252 / 136,634 |
| Bytes copied | 60,733,909 |
| Template / glue bytes | 43,160,255 / 17,573,654 |
| Holes visited | 7,680,481 |
| Integer / rel32 patches | 5,633,606 / 700,229 |
| Actual patch bytes written | 13,969,635 |
| Symbol holes producing relocations | 1,346,646 |
| Glue / marker relocations | 338,440 / 95,195 |
| Total relocation records | 1,780,281 |

At the supplied 8-thread rates of **36 GB/s read, 27 GB/s write** (decimal units), copying
60.734 MB alone needs 1.687 ms read service and 2.249 ms write service: 2.249 ms with
perfect overlap, or 3.936 ms if the services serialize. This ignores item lookup and small
copy overhead; it is not a prediction for 2.38 million short, indexed copies.

A wider logical-traffic estimate includes copied bytes, operand values, jump/symbol
auxiliaries, jump target offsets, glue/marker relocation inputs, patch stores, and the
private+final relocation buffers. `sizeof(Rel)=24`: collection writes 42.727 MB, and
concatenation reads and writes another 42.727 MB each. This gives **195,025,203 bytes read
and 160,157,032 bytes written**: 5.417 ms read service, 5.932 ms write service,
**5.932 ms with perfect overlap / 11.349 ms serialized**.

This is an optimistic bandwidth scale, not measured DRAM traffic or a strict physical
lower bound. It omits item/template/hole index columns, binary-search reads, cache-line
fetches, write allocation, and synchronization; repeated templates and patch stores may
hit cache. The VM bandwidth numbers are given calibration, not remeasured during this
noisy window. The scan additionally writes each item offset twice and reads it once
(57.150 MB total), besides reading lengths and their index columns.

At the eight-thread median of 65.553 ms, the modeled 355.182 MB corresponds to
5.42 GB/s of logical traffic, about 5.8–11.1 times the idealized service-time scale.
This supports looking beyond bulk memcpy bandwidth: indexed metadata, short copies,
hole decoding, load balance and host scheduling all remain in the measured kernel.
No hardware counter evidence isolates their individual contributions here.

## Patch diagnostic

One additional warm process with patching disabled at each thread count, each preceded
by a patched control; same 64 repetitions and minimum reporting:

| Threads | Patched emit ms | No-patch emit ms | Difference ms | Fraction of patched emit |
|---|---:|---:|---:|---:|
| 1 | 104.641 | 96.642 | 7.999 | 7.6% |
| 8 | 52.333 | 70.795 | -18.462 | -35.3% |


At one thread, the observed incremental patch cost is 7.999 ms (7.6%); the other
96.642 ms is the residual work. At eight threads, disabling patching was **slower** by
18.462 ms: the pair is overwhelmed by timing variation, so **no meaningful patch/copy
fraction can be assigned at eight threads** from this diagnostic. This is not evidence
that patching accelerates emission. Per CHARTER, retain the negative/noisy observation
instead of chasing a quiet machine.

The residual includes copies, template/hole lookup, branches, relocation collection and
concatenation. The delta is the incremental integer/jump patch cost, not the entire hole
traversal cost, and the residual must not be described as a pure memcpy benchmark. The
single diagnostic pair per thread count is descriptive, not a separately repeated speed
claim. No-patch output is intentionally incorrect and is not counted as a result.
