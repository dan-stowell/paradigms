# Problem 4 on kube-apiserver and MinIO

Built from source at `811146582a385e7e942d28dfb7049f3c029abf59` in
`/home/exedev/paradigms-k`, branch `codex-kube-p4`, on 2026-11-16. No paradigm
code, driver, runtime, or size constant was changed. Inputs were read from
`/home/exedev/paradigms/data/{kube,minio}-p4`; all output was written inside this
clone. VM: eight vCPUs, AMD EPYC 9554P; GCC 13.3, Futhark 0.29.1, Halide 21,
Goose v2026.4 (runtime dce889723a23).

## Correctness and the Goose failure

C `p4`, C `p4par`, Halide's documented `lay_t_lf` / `emit_t_i2_v32_p1`
schedule, and Futhark `4bf` and `4D` passed on **both datasets**, at 1 and 8
threads (`p4` is single-threaded). C and Halide print `P4 OK`; Futhark spells
its variant-specific successes `P4bf OK` and `P4D OK`. All four implementations compare
emitted bytes and sorted relocation records with the extraction oracle.
Futhark and Halide also check every function offset unconditionally. The C
references only inspect function offsets in a diagnostic branch after a code
memcmp fails (`p4.c:150`, `p4par.c:236`); their successful `P4 OK` does **not**
independently establish offset equality. This existing limitation was found
by source review and left unchanged. Checks remained enabled during every
timed invocation.
The separate Futhark `4lay` timing entry has no checker; the complete emission
checks validate the function offsets. Halide's warm driver also checks the two
chunk-size schedules selected by the same emit-name prefix.

**Goose built successfully but cannot run under the requested
`prlimit --as=8589934592` (8 GiB) cap.** It exits 1 with
`goose runtime error: cannot reserve data stack address space`, for both
datasets and both worker counts, including every attempted timing round.
The syscall trace shows three successful 2,148,532,224-byte anonymous
`MAP_NORESERVE` mappings, followed by `ENOMEM`. The runtime retries with
smaller reservations until even a 2,097,152-byte mapping fails. No dataset
file is opened. This is a virtual-address reservation limit, not a kube
input-size limit or an oracle mismatch. `systems/goose/bench-p4.sh:22` and
`systems/goose/NOTES.md:9` explicitly warn that Goose's roughly 2 GiB per
stack reservations are incompatible with `prlimit --as`, and use a physical
memory cgroup limit instead. This run retained the user's address-space cap;
no Goose timing or kube/MinIO ratio can be claimed.

## Dataset sizes

Kube emits 74,522,346 bytes versus MinIO’s 60,733,909 (**1.227×**), from
2,919,022 versus 2,381,252 items (**1.226×**) and 9,323,635 versus 7,680,481
operands (**1.214×**). Relocations grow 1.242× and functions 1.486×. But
distinct templates shrink from 108,833 to 93,112 (**0.856×**), template holes
from 1,863,292 to 1,603,190 (**0.860×**), and template segments from 207,697
to 179,471 (**0.864×**). Thus ~1.2× is a useful emission baseline, not the
expected ratio for every preparation pass. Counts come from each `meta.txt`.

## Measurement

Five sequential, interleaved outer rounds, alternating dataset order and thread
order each round. Each system's MinIO/kube pair runs next to the others and the
C references; no benchmarks in this run execute concurrently. Other users'
VM jobs were active. All times are wall-clock milliseconds, without CPU affinity.
`uptime` is recorded before every process.

Each outer-round sample is that process's minimum: C uses 16 repetitions at
one thread and 64 at eight, after a warmup; Futhark uses 16 for `4bf`, 64 for
`4D`, 256 for standalone layout, and 8 for preparation; Halide uses its
unchanged five-batch adaptive benchmark (nominally >=1 second per batch).
The table reports **min / median / max of those five outer-round samples**,
not the fastest single run from another window. Raw output preserves the
within-process statistics as well. Futhark includes its first iteration
and result allocations; unlike C and Halide, its driver has no untimed warmup.

`GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, `P4_HUGE=1`, and
`OMP_WAIT_POLICY=passive` were identical for both datasets. Futhark uses the
stock **multicore backend at both 1 and 8 threads**, without the optional
yield-runtime patch. Full templates and patching are enabled throughout.
Halide uses the same parallel emit schedule at both thread counts. Its warm
emit driver initializes addresses with layout variant 0 (`lay_f_lf`) outside
the emit timer; the reported `lay_t_lf` layout is measured separately. The
explicit e2e checks use `lay_t_lf` plus `emit_t_i2_v32_p1`.

Timing scopes differ across systems, but are identical between datasets:
C and Halide report layout and emission separately. Futhark `4bf`'s emit
column includes layout and relocation expansion; its layout column is a
separate `4lay` measurement, **not** a time to subtract from or add to emit.
Futhark `4D` prepares the flattened stream separately, then emits from it;
preparation includes layout. Halide's preparation is its template-only index
(the sample from the emit process). Loading, oracle checks, sorting for the
check, and Futhark's host copy-out are outside these kernel times.

Measurement window: **18:56:56–19:13:17 UTC**, 2026-11-16 (first and last
process starts). The one-minute
`uptime` load ranged **4.09 / 9.11 / 23.75** (min / median / max across
process starts). First `uptime`: `load average: 11.43, 6.38, 4.47`; last:
`8.86, 9.13, 7.85`. All 150 round records are present (30 per round).
Ratios below divide the kube median by the MinIO median for the same system,
thread count, and phase. Plain C has no eight-thread mode.

| System | Dataset | Threads | Emit ms min / med / max | Layout ms min / med / max | Preparation ms min / med / max | Kube/MinIO median: emit; layout; prep |
|---|---|---:|---:|---:|---:|---|
| C p4 | minio | 1 | 90.89 / 98.81 / 109.42 | 5.99 / 6.18 / 6.45 | — | — |
| C p4 | kube | 1 | 105.96 / 112.49 / 122.67 | 6.50 / 6.85 / 7.11 | — | 1.14×; 1.11×; — |
| C p4par | minio | 1 | 96.08 / 97.80 / 101.54 | 6.98 / 7.15 / 11.31 | — | — |
| C p4par | kube | 1 | 105.08 / 109.46 / 122.20 | 7.66 / 7.91 / 8.37 | — | 1.12×; 1.11×; — |
| C p4par | minio | 8 | 23.21 / 26.93 / 45.47 | 1.93 / 2.41 / 2.74 | — | — |
| C p4par | kube | 8 | 24.09 / 31.03 / 48.79 | 2.14 / 2.93 / 3.29 | — | 1.15×; 1.22×; — |
| Futhark 4bf | minio | 1 | 286.84 / 300.08 / 368.54 | 8.39 / 8.47 / 8.66 | — | — |
| Futhark 4bf | kube | 1 | 302.53 / 304.60 / 326.48 | 8.97 / 9.18 / 10.01 | — | 1.02×; 1.08×; — |
| Futhark 4bf | minio | 8 | 107.70 / 136.73 / 206.46 | 1.85 / 2.53 / 2.78 | — | — |
| Futhark 4bf | kube | 8 | 96.51 / 147.52 / 302.77 | 2.30 / 2.73 / 2.88 | — | 1.08×; 1.08×; — |
| Futhark 4D | minio | 1 | 64.97 / 65.40 / 73.09 | — | 322.52 / 393.08 / 435.58 | — |
| Futhark 4D | kube | 1 | 75.41 / 75.95 / 84.94 | — | 335.42 / 340.68 / 424.32 | 1.16×; —; 0.87× |
| Futhark 4D | minio | 8 | 18.11 / 25.18 / 26.18 | — | 126.42 / 154.01 / 212.69 | — |
| Futhark 4D | kube | 8 | 20.52 / 29.79 / 31.03 | — | 110.72 / 135.05 / 216.26 | 1.18×; —; 0.88× |
| Halide scatter | minio | 1 | 155.36 / 160.80 / 186.40 | 23.82 / 25.89 / 35.64 | 9.62 / 9.65 / 15.21 | — |
| Halide scatter | kube | 1 | 177.94 / 231.16 / 279.76 | 28.61 / 30.33 / 38.88 | 8.44 / 9.71 / 11.48 | 1.44×; 1.17×; 1.01× |
| Halide scatter | minio | 8 | 29.75 / 42.96 / 105.60 | 6.49 / 11.98 / 29.64 | 6.14 / 6.38 / 8.59 | — |
| Halide scatter | kube | 8 | 28.97 / 38.04 / 56.16 | 6.42 / 8.60 / 10.97 | 5.36 / 5.68 / 8.96 | 0.89×; 0.72×; 0.89× |
| Goose | minio | 1 | failed before load | — | — | — |
| Goose | kube | 1 | failed before load | — | — | — |
| Goose | minio | 8 | failed before load | — | — | — |
| Goose | kube | 8 | failed before load | — | — | — |

## Ratios and interpretation

- **C references:** emit ratios are 1.14× for `p4`, 1.12× for one-thread
  `p4par`, and 1.15× for eight-thread `p4par`. These are reasonably near the
  ~1.2× workload baseline on this machine.
- **Futhark `4D`:** emission is 1.16× / 1.18× (1 / 8 threads), also near
  baseline. Preparation is only **0.87× / 0.88×**, a conspicuous departure.
  A plausible reason is the 14% smaller template-hole/segment tables:
  preparation resolves those tables and constructs the expanded streams,
  whereas emission walks the already flattened output. This is a hypothesis;
  the preparation ranges overlap.
- **Futhark `4bf`:** emission is only **1.02× / 1.08×**, and separate layout
  is 1.08× at both thread counts. Unlike `4D` emission, `4bf` repeatedly
  classifies template holes and expands relocations; less template-table
  work and better reuse may offset the extra items/operands. The emit ranges
  overlap, so the apparent near-flat scaling is **within noise**, not proof
  that the larger dataset is free. The stock spinning multicore runtime is
  also sensitive to descheduling.
- **Halide:** emission is **1.44× at one thread but 0.89× at eight**, and
  eight-thread layout is only **0.72×**. These depart from ~1.2× in opposite
  directions. The strongest immediate explanation is contention and scheduling
  noise: eight-thread MinIO emission spans 29.75–105.60 ms, and layout spans
  6.49–29.64 ms. Even the one-thread emission ranges overlap
  (155.36–186.40 versus 177.94–279.76 ms), so neither departure establishes
  a dataset-specific performance mechanism. Different per-item lengths/hole
  distributions and cache locality are additional possible causes. The
  template-index ratios, 1.01× / 0.89×, should not track code bytes: their
  input tables are smaller on kube.
- **Goose:** no ratio; startup fails before either workload under the required
  address-space cap.

The ranges distinguish observations from explanations. No additional runs
were selected or discarded to obtain a quiet window.

## Reproduction

From the clone root (the temporary-directory setting also keeps compiler
scratch files inside the clone):

```sh
mkdir -p build/kube-p4-tmp systems/goose/bin
export TMPDIR="$PWD/build/kube-p4-tmp"
systems/futhark/build.sh p4
systems/halide/cells/build.sh p4
(cd systems/goose && /home/exedev/tools/goose/rel/goose-v2026.4-linux-x64/goose build p4.goose -o bin/p4)
ref/p4/check.sh /home/exedev/paradigms/data/kube-p4
python3 results/codex-kube-p4-run.py check
python3 results/codex-kube-p4-run.py bench
python3 results/codex-kube-p4-summarize.py
```

The runner records the exact command, return code, UTC start, `uptime`, and
complete output of every invocation in `results/codex-kube-p4-raw.json`.
The timeout, address-space cap, and common environment are specified in the
runner; build logs, source hashes, and the Goose syscall trace are also
retained in the raw archive.

## Lead addendum (2026-11-16T19:16Z): Goose under its own cap

Goose's runtime reserves ~2 GiB per data stack, so it needs a cgroup cap, not `prlimit --as` (systems/goose/NOTES.md).
Lead ran `P4_REPS=5 systemd-run --user --scope -q -p MemoryMax=8G bin/p4 data/{minio,kube}-p4 {1,8}`: **P4 OK on both,
every function offset equal** (136,635 / 202,981). Rep wall min/med/max ms, load 5-7: 1 worker MinIO 99.3/104.8/106.8,
kube 111.6/155.8/165.0 (emit 89.6/94.8/95.2 vs 101.4/141.6/147.5); 8 workers MinIO 31.6/32.4/39.4, kube 35.7/37.7/41.2
(emit 23.3/25.7/32.0 vs 29.0/33.1/38.3). One window, one process each: kube/MinIO emit ~1.3x (8), 1.1-1.5x (1, noisy).
Also: ref/p4/p4.c now checks every function offset unconditionally (p4par did since codex-holes); P4 OK on both datasets.
