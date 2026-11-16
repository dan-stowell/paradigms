# CP7: fewer copies, with a large prepared input

Data: `/home/exedev/paradigms/data/minio-p4`, read-only. Implementation:
`ref/p4/p4runs.c`; branch `codex-cp7`. This tests the requested run-fusion
experiment, not CP7's original top-4,096 parent/child-pair library selection.
No oracle bytes or offsets are used in preparation.

## Representation and counted work

Preparation is performed once per process before warmup, and timed separately like
an extraction. It maps the original input, scans item lengths, copies bytes into
zeroed-hole blobs, and flattens hole records into run order with run-relative positions.
Operand values remain per occurrence. The emit kernel still writes every integer
and computes every rel32 from its original target-item offset and auxiliary offset;
symbol holes and glue relocations append relocation records. Markers remain per function.

- **D, prepared unmerged control:** one run per original item, including empty items.
  This isolates the benefit of preparing lookup results from merging copies.
- **A, hole-free neighbours:** a run contains at most one original item with holes
  or glue relocations. Leading hole-free items join the first sensitive item;
  following hole-free items join that run until the next sensitive item. Never
  crosses a function boundary. Functions containing only hole-free items form one run.
- **B, one run per function:** precompute each entire function with all holes zeroed.
  **This trades the original template table for 60.7 MB of per-function blobs.**
  It is replay of a prepared function template, not emission from the small original
  node-template library. A and D also materialize 60.7 MB in this implementation.
- **C, deduplicated B:** require identical original item-kind and node-template-ID /
  segment sequences, glue lengths, normalized bytes, and ordered hole descriptors
  (position, width, encoding, relocation type). Glue IDs and operand values are not
  part of the key. Hash matches undergo full equality checks. Each function still
  has its own operand list and one memcpy; only source storage is shared.

| Representation | Emit runs | Distinct stored runs | Blob bytes | Bytes copied/run | Pieces/decimal MB | Holes incl. glue relocations | Logical emit input bytes |
|---|---:|---:|---:|---:|---:|---:|---:|
| Original p4 / p4par | 2,381,252 | template/glue library | 28,062,510 | 25.505 | 39,207.949 | 8,018,921 | 216,965,412 mapped columns |
| D | 2,381,252 | 2,381,252 | 60,733,909 | 25.505 | 39,207.949 | 8,018,921 | 415,942,599 |
| A | 1,033,547 | 1,033,547 | 60,733,909 | 58.763 | 17,017.627 | 8,018,921 | 372,816,039 |
| B | 136,634 | 136,634 | 60,733,909 | 444.501 | 2,249.715 | 8,018,921 | 344,114,823 |
| C | 136,634 | 55,389 | 48,985,021 | 444.501 | 2,249.715 | 8,018,921 | 332,365,935 |

Every iteration copies 60,733,909 bytes. The 8,018,921 records comprise 7,680,481
original template holes and 338,440 glue relocations; 95,195 marker relocations are
additional. Run records are 32 bytes and flattened hole records are 32 bytes each
(256,605,472 bytes for holes alone). Logical input includes these records, used blob
bytes, function-to-run and function-to-item indexes, original-item offsets for jump
targets, and marker columns. It excludes output and relocation work buffers.
Original mapped columns exclude the derived item-offset array and output buffers.
These are representation sizes, **not RSS**: original file mappings remain present,
run arrays have spare capacity, and C's packed buffer retains 60.7 MB allocation capacity.

A removes **56.6%** of copies; B removes **94.3%** (17.43× fewer). C removes **59.5%**
of B's distinct stored runs but only **19.3%** of its blob bytes (1.24× compression):
small functions repeat more than large ones. There is some whole-function shape
reuse, but it does not recover a compact template library. Total logical input
shrinks just **3.4%** from B to C because operands dominate.

## Correctness and timing scope

`ref/p4/check-runs.sh` builds all programs locally with GCC `-O2 -march=native`
and OpenMP for p4par/p4runs. A/B/C/D pass the full oracle at 1 and 8 threads;
A/B/C also pass with eight requested threads constrained to two and forced anonymous
output fallback. Checks compare all bytes, **all 136,635 function offsets unconditionally**,
and all 1,780,281 relocations after sorting. The baseline sources are unchanged;
their inherited checks only inspect function offsets when code bytes differ.

Independent review is in `codex-cp7-review.md`. Its separate UBSan build passes A/C,
and a deliberately changed oracle function offset causes failure despite unchanged
code bytes and relocations (`cp7-review-check.sh`, `codex-cp7-review-check.log`).

`ref/p4/bench-runs.sh` runs five interleaved rounds, each in the order p4,
p4par/1, D/1, A/1, B/1, C/1, p4par/8, D/8, A/8, B/8, C/8.
All use `prlimit --as=12884901888` (12 GiB), `OMP_WAIT_POLICY=passive`,
`FLAT_HUGE=0`, no pinning, one excluded warmup and **P4_REPS=128** iterations.
Preparation includes original-file mapping and allocation, layout, flattening and
C's hash/compare/packing. It is outside emit. This prototype prepares in memory;
there is no serialized prepared-input load or disk-write timing.

Emit includes copying, patches, marker and ordinary relocation collection, and
concatenation of reusable worker vectors, exactly as p4par does. It uses the same
original-item-count function partitions as p4par. Output requests 2 MB huge pages
with the same fallback implementation; p4 retains its original malloc output.
Output allocation, worker partition setup, warmup, final relocation sort and checks
are excluded. Original p4/p4par still time their separate layout phase each iteration;
p4runs moves layout into preparation. No layout speedup is claimed.

The reported emit value from each process is its minimum over 128 warm iterations,
matching the existing p4/p4par protocol. Tables give min / median / max of those
five process minima, not individual-iteration percentiles. GB/s is decimal output
bytes divided by median emit time, not total memory traffic. Raw timestamps,
uptime/load, timings and oracle outcomes are in `codex-cp7-runs.log`;
`python3 results/summarize-cp7.py` recomputes the tables.

## Measurements

AMD EPYC 9554P, 8 visible CPUs, GCC 13.3.0. Window:
**2026-11-16T18:25:19Z–18:31:29Z**. Recorded one-minute uptime load:
**2.53–4.85**. All **55** benchmark processes pass the oracle; all **50**
p4par/p4runs processes obtain 2 MB MAP_HUGETLB output. Every process performs
more than one second of measured kernels (minimum 1.503 seconds).
Preparation statistics pool the ten independent preparations per variant
(five processes at each thread count); preparation itself is serial.

| Variant | Emit min / median / max ms | GB/s at median | Measured seconds min–max |
|---|---:|---:|---:|
| p4/1 | 88.892 / 98.410 / 98.552 | 0.617 | 13.905–16.375 |
| p4par/1 | 89.908 / 94.695 / 95.876 | 0.641 | 13.803–14.985 |
| D/1 | 48.270 / 48.474 / 48.881 | 1.253 | 6.410–7.227 |
| A/1 | 43.826 / 43.981 / 44.061 | 1.381 | 5.798–6.134 |
| B/1 | 43.913 / 44.689 / 44.904 | 1.359 | 5.912–7.086 |
| C/1 | 46.455 / 46.589 / 46.656 | 1.304 | 6.173–7.166 |
| p4par/8 | 16.055 / 18.418 / 22.204 | 3.298 | 2.788–5.019 |
| D/8 | 11.421 / 13.675 / 15.438 | 4.441 | 1.859–3.390 |
| A/8 | 11.148 / 11.861 / 12.760 | 5.120 | 1.801–1.931 |
| B/8 | 9.773 / 12.258 / 12.672 | 4.955 | 1.658–2.735 |
| C/8 | 8.265 / 12.159 / 13.196 | 4.995 | 1.503–2.811 |

| Variant | Preparation total min / median / max ms | Construction incl. input mapping | Dedupe ms |
|---|---:|---:|---:|
| D | 345.915 / 369.551 / 447.977 | 345.915 / 369.550 / 447.977 | 0.000 / 0.000 / 0.000 |
| A | 334.365 / 345.803 / 391.239 | 334.365 / 345.803 / 391.239 | 0.000 / 0.000 / 0.000 |
| B | 312.047 / 325.465 / 447.974 | 312.047 / 325.464 / 447.974 | 0.000 / 0.000 / 0.001 |
| C | 513.653 / 536.751 / 584.040 | 321.349 / 342.000 / 354.137 | 190.799 / 196.203 / 229.903 |


## Verdict

**Fewer pieces alone do not explain the original slow emit. There is no
proportional pieces-to-time relationship.** Preparing metadata and hole traversal
is the large win; after that, whole-function fusion has diminishing returns.

1. **Preparation without fusion already wins:** D keeps all 2,381,252 copies but
   emits in 48.474 ms at one thread, versus p4par's 94.695 ms (**1.95×**), with
   nonoverlapping ranges. The source layout also changes to a sequential expanded
   blob pool, so this combines hoisted lookup/search work and improved input locality.
   It is not evidence that memcpy calls alone cost the original 95 ms.
2. **A reduces pieces 2.30× versus D but time only 1.10×** (48.474 → 43.981 ms).
   Their ranges do not overlap, but the benefit is modest. **B reduces pieces 17.43×
   versus D but time only 1.08×** (48.474 → 44.689 ms). A and B overlap: their
   additional 7.56× piece-count difference is **within noise** in emit time.
   If time were proportional to copies, B would take about **2.78 ms**, not 44.689 ms.
   A fixed-cost-plus-small-piece-cost model is more plausible than proportional
   scaling; this experiment does not rule out every affine approximation or
   establish a universal per-copy slope.
3. **At eight threads**, A/B/C improve on p4par with nonoverlapping ranges:
   medians 11.861 / 12.258 / 12.159 ms versus 18.418 ms (**1.55× / 1.50× / 1.51×**).
   D also improves (13.675 ms). But D/A/B/C ranges overlap each other:
   benefits of additional fusion or deduplication are **within noise** there.
4. **C compresses storage, not work.** It retains B's copy count and all patches.
   Its one-thread median is 46.589 ms versus B's 44.689 ms: a small ~4% difference,
   below CHARTER's useful performance scale despite nonoverlapping minima ranges.
   It establishes no speed benefit; eight-thread B/C ranges overlap.

All variants retain 8.02 million hole records, 1.78 million output relocations,
and their vector-concatenation traffic. That substantial unchanged work explains
why removing nearly all copy boundaries need not remove nearly all emit time.
The old 7.6% no-patch diagnostic skipped integer/jump stores, not the hole traversal,
lookup or relocation work; it did not prove that this remaining work disappears
when copy count falls. Run metadata traffic also falls as pieces merge, so even
D-versus-A is not a measurement of memcpy-call overhead in isolation.

**Tradeoff:** B needs 60.7 MB of function blobs and 344.1 MB of logical prepared input;
C still needs 49.0 MB of distinct blobs and 332.4 MB total. Median preparation costs
325 ms for B and 537 ms for C, excluded from the 44.7 / 46.6 ms warm emit figures.
This is an extraction-time/reuse tradeoff, **not a demonstrated end-to-end compiler
speedup**. For one emission, preparation alone exceeds the original warm emit time.
CP7's original top-pair/60,000-extra-template kill test remains untested; this
requested experiment establishes that indiscriminately removing pieces is not
sufficient to predict emit speed.

## Reproduce

```sh
ref/p4/check-runs.sh /home/exedev/paradigms/data/minio-p4
ref/p4/bench-runs.sh /home/exedev/paradigms/data/minio-p4 > results/codex-cp7-runs.log
python3 results/summarize-cp7.py
results/cp7-review-check.sh /home/exedev/paradigms/data/minio-p4
```
