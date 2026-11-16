# CP1 and CP4: width relaxation and generated emitters

Data: `/home/exedev/paradigms/data/minio-p4`, read-only. Branch: `codex-cp`.
The experiments are independent: CP4 specializes the original top-4,096 templates,
not CP1's merged table. The thresholds are CP1 and CP4 in `ideas/copy-and-patch.md`.

## CP1: compression works; operand-dependent layout fails its kill test

`ref/p4/gen_cp1.py` decodes each original template segment from its instruction boundary
using **Capstone 5.0.6**, checking complete consumption. For kind 1/2/3 holes it verifies
the decoded displacement position, finds the actual ModRM byte, changes mod=01 to 10,
and widens disp8 to disp32. SIB bytes are preserved. RIP-relative and no-base SIB
symbol displacements remain fixed; a kind 1/2/3 displacement with mod=00 is rejected,
not silently rewritten into a different addressing mode. No such rejection occurs
on this data. Capstone reports disp_size=2 for some 66-prefixed disp32 stores;
ModRM and instruction length correctly establish the architectural 4-byte width.

The key is `(shape, mode, normalized bytes, ordered hole list)`, where each hole has
normalized offset, width, encoding, kind and relocation type. Width and ModRM are
not taken from the original node template at emission. All input templates decode;
1,209,921 non-SIB and 34,435 SIB displacement fields are normalized in the original
template library. Counts refer to template fields, not dynamic node occurrences.

| Key | Distinct | Top 256 nodes / node bytes | Top 1,024 nodes / node bytes | Top 4,096 nodes / node bytes |
|---|---:|---:|---:|---:|
| Original byte templates (NOTES) | 108,833 | 43.3% / 19.5% | 59.9% / 33.2% | 74.6% / 50.3% |
| Relaxed templates | **66,046** | **47.1677% / 26.4238%** | **66.5435% / 44.9163%** | **82.2516% / 64.6577%** |

Top K is ranked by node count, with deterministic first-seen template-ID ties;
byte coverage uses those same K keys and original emitted node lengths, not widened
lengths. Exact counts are in `codex-cp1-count.log`: top-4,096 covers 923,117 nodes and
27,906,439 original node bytes. The whole population is 1,122,309 nodes / 43,160,255 bytes.
Compression is **39.31% fewer keys**, 546 fewer than the preliminary 66,592 estimate.

Child-segment boundaries are not part of the requested key, and can differ between
otherwise identical keys. They remain separate metadata: original segmentation IDs
per node (4,489,236 bytes), 108,834 segment offsets (435,336 bytes), and normalized
segment ends (830,788 bytes). Thus 66,046 is the byte/hole-key count, not a count
of unique template-plus-segmentation layouts. The emitter uses original template IDs
only to select boundaries; every byte and hole comes from the deduplicated relaxed table.
The relaxed template byte pool is 8,400,870 bytes versus 10,488,856 original; the generated
overlay totals 35,110,037 bytes, including node IDs, retained segmentation and decoded
ModRM offsets. This prototype does not claim overall metadata compression.

`ref/p4/p4cp1.c` computes every item length at runtime as normalized segment length
minus three times its fitting displacement holes. It binary-searches the segment's
hole start, visits the holes, and tests actual operands in [-128,127]. Emission copies
runs from the relaxed pool, restores mod=01 where fitting, writes shortened fields,
and adjusts all subsequent hole/relocation positions. The original widths are not
used to decide the output width. Layout and emission each do the fit decisions;
no precomputed item-length cache hides that cost. The dynamic input has 4,863,909
displacement holes and 7,680,481 total holes; the length pass visits the latter to
find the former, in addition to ordinary item metadata and the offset scan.

**Correctness passes:** all 60,733,909 bytes, all 136,635 function boundaries, and all
1,780,281 sorted relocations match the oracle. Both new emitters check function offsets
unconditionally, closing an inherited limitation of p4.c's check. The independent
review (`codex-cp-review.md`) also exercises BP/SIB width pairs, RIP, no-base SIB and
66-prefixed displacements using `cp-review-check.py`. Changing one oracle function
offset makes the checker fail. Logs and the synthetic regression are committed.

**Count verdict: PASS**, 66,046 ≤66,592 and 82.2516% ≥82%; also comfortably below
the fallback kill boundary of 85,000. **Layout verdict: KILL**; see measurements below.
The compression finding survives; this runtime relaxation implementation does not.

## CP4: generated hot-template emission

`ref/p4/gen_cp4.py` emits one C function for each of the original top-4,096 templates,
dispatched by a 108,833-entry function-pointer table. Each function's segment arm
contains constant-length memcpy and constant-offset operand stores / relocation appends.
Multi-segment templates need a segment switch; each arm is straight-line, with no
hole loop. Cold templates use the unchanged generic loop. Constant-size memcpy stores
avoid unaligned access and aliasing UB. The generated header stays under `build/`.

| Route per full iteration | Items | Bytes copied |
|---|---:|---:|
| Generated hot node segments | 1,141,371 | 21,718,974 |
| Generic cold node segments | 499,045 | 21,441,281 |
| Generic glue | 740,836 | 17,573,654 |
| Total | 2,381,252 | 60,733,909 |

The functions cover 837,365 nodes (74.61%) and 50.32% of node bytes, but only 35.76%
of all emitted bytes. Zero-length segments count as items. Routing is counted
independently by the generator in `cp4-generation.log`.

Generated C size: **2,916,263 bytes**. Initial GCC compile: **52.50 s wall**, 458,844 KiB
peak RSS. Final rebuild after the unconditional function-offset check: **63.35 s wall**,
22.94 s user, 0.51 s system, 459,568 KiB peak RSS (`cp4-final-compile.log`). Final binary
ELF text/data/bss: **1,226,048 / 871,556 / 32 bytes**; p4par is 15,795 / 876 / 32.
Compiler: GCC 13.3.0, `-O2 -march=native -fopenmp`; build contention is included in wall time.

All full normal runs check code, function offsets and relocations against the oracle.
Independent checks also pass with forced anonymous output pages and with 8 requested
threads constrained to an actual team of 2. Normal measured parallel runs use 2 MB
MAP_HUGETLB output, like p4par; p4 and CP1 retain p4.c's malloc output.

## Measurement protocol

`ref/p4/bench-cp.sh` runs five interleaved rounds: p4, CP1, p4par/1, CP4/1,
CP4-generic/1, p4par/8, CP4/8. All 1-thread runs are pinned to CPU 5 as CP4 specifies;
8-thread runs use the visible CPUs without pinning. Every process has a 12 GiB address
space limit and `OMP_WAIT_POLICY=passive`, one excluded warmup, then **P4_REPS=32**
in-process iterations with reusable buffers. Mapping, allocation, final relocation
sorting and oracle checks are outside phase timing; private-vector concatenation is
inside full emit, as in p4par. Per-process phase minima are summarized below as
min / median / max across the five processes, matching the earlier p4par protocol.
Layout and emit minima need not be from the same iteration. Raw output and uptime
snapshots are in `codex-cp-runs.log`; `python3 results/summarize-cp.py` recomputes tables.

The covered-node kill test uses `P4_CP4_COVERED=1`, paired with
`P4_CP4_GENERIC=1`. After the full oracle passes, each mode traverses the same hot-item
list in input order, including selection, copying, patching and private relocation
appends. Gathering that list, cold/glue/marker work, final relocation concatenation,
and sorting are excluded in both modes. Code is checked again afterward. The diagnostic
is not a whole-emit measurement or a separate full relocation-oracle check; that proof
comes from the complete emission immediately preceding it.

## Results and kill-test verdicts

VM: 8 visible CPUs, AMD EPYC 9554P, GCC 13.3.0. Measurement starts run from
**2026-11-16T17:36:39Z to 17:41:59Z** (last process completes afterward).
Recorded 1-minute load: **14.30–20.16**. Every measured process performs more than
one second of kernel work, including each covered-only diagnostic.

| Variant | Layout min / median / max ms | Emit min / median / max ms | Measured full-kernel seconds min–max |
|---|---:|---:|---:|
| p4, 1 core | 6.575 / 6.805 / 9.037 | 106.172 / 109.463 / 156.210 | 4.556–12.276 |
| CP1, 1 core | 39.838 / 42.708 / 45.301 | 150.879 / 165.210 / 211.920 | 7.070–22.128 |
| p4par, 1 thread | 7.595 / 7.997 / 8.262 | 107.001 / 118.870 / 176.608 | 4.317–18.528 |
| CP4, 1 thread | 7.296 / 8.009 / 12.744 | 93.640 / 110.532 / 187.381 | 4.010–9.074 |
| CP4 generic control, 1 thread | 7.877 / 7.935 / 8.350 | 104.719 / 108.921 / 219.275 | 4.567–13.414 |
| p4par, 8 threads | 3.275 / 3.946 / 8.264 | 42.131 / 51.635 / 76.882 | 1.929–4.087 |
| CP4, 8 threads | 2.792 / 6.347 / 10.305 | 30.697 / 45.317 / 63.775 | 1.765–3.873 |

**CP1 layout fails decisively:** median **6.28×** p4, with nonoverlapping ranges.
The 1.3× contemporaneous median budget is **8.847 ms**. Even the historical
12.6–16.6 ms reference gives only **16.38–21.58 ms** at 1.3×; every CP1 process minimum
is slower. Dynamic lengths require a new operand/hole traversal where the old scan
only gathered segment lengths. CP1's correctness and table count pass; the combined
idea is **killed by its layout threshold** for this implementation.

CP4 full emit is **within noise** of both p4 and same-thread p4par: their ranges
overlap. Median ratios p4/CP4-1 = 0.99×, p4par-1/CP4-1 = 1.08× and
p4par-8/CP4-8 = 1.14× are descriptive, not established speedups. The one-core
**≤80 ms whole-emit target fails**: even the best CP4 process minimum is 93.640 ms,
and the median is 110.532 ms. Eight-thread CP4 is below 80 ms, but that is not the
one-core target and does not establish a specialization benefit.

| Covered-only 1-core emit | Min / median / max ms | Measured seconds per process min–max |
|---|---:|---:|
| Generic loop | 41.336 / 46.611 / 65.650 | 1.825–4.216 |
| Generated functions | 26.553 / 27.865 / 43.779 | 1.068–2.885 |

**CP4's ≥1.5× covered-node requirement is not established:** the ratio of medians is
**1.67×**, numerically above the threshold, but the ranges overlap (41.336–43.779 ms).
Under CHARTER this is **within noise**, so the timing kill test is **inconclusive,
not a demonstrated pass**. It is also not evidence of a measured speedup below 1.5×;
labeling that literal kill condition true would overstate the result. Do not count
CP4 as a validated performance survivor. The full-emit target is a definite miss.
No additional quiet-machine rerun is needed to record this result.

For audit, covered generic / specialized minima by round, in ms:

| Round | Generic | Specialized |
|---|---:|---:|
| 1 | 44.837 | 29.910 |
| 2 | 52.130 | 26.908 |
| 3 | 41.336 | 27.865 |
| 4 | 65.650 | 43.779 |
| 5 | 46.611 | 26.553 |

## Hardware counters

`perf` was absent from PATH. Downloading Ubuntu's `linux-tools-6.8.0-149` package
and extracting it under `build/perf-tools` made the actual binary available without
system installation. `ref/p4/perf-cp.sh` records one additional full process per
configuration after the timing campaign; raw results are in `codex-cp-perf.log`.
These are **user-mode whole-process counts**, including setup, one warmup + 32
layout/emit repetitions, and final sorting/checking. They are not isolated emit
counts or an additional five-run timing study. All five processes pass the oracle.

| Variant | Instructions | Branch misses |
|---|---:|---:|
| p4, 1 core | 24,951,617,716 | 112,957,693 |
| p4par, 1 thread | 21,893,138,287 | 111,520,807 |
| CP4, 1 thread | 16,184,319,547 | 112,770,374 |
| p4par, 8 threads | 21,804,042,029 | 111,338,205 |
| CP4, 8 threads | 16,095,233,021 | 113,951,244 |

CP4 executes about **26% fewer instructions than p4par** at either thread count,
while branch misses do not decrease. That supports the intended mechanism—removing
hot-template metadata/hole-loop work—but does not turn overlapping timing ranges
into a speed claim. Glue and cold items still process 39,014,935 bytes per iteration,
plus the unchanged layout and full relocation-vector concatenation.

## Reproduce

From this checkout, with Python 3, GCC/OpenMP, and Capstone installed locally:

```sh
python3 -m pip install --target build/cpdeps --no-cache-dir capstone==5.0.6
ref/p4/check-cp.sh /home/exedev/paradigms/data/minio-p4
ref/p4/bench-cp.sh /home/exedev/paradigms/data/minio-p4 > results/codex-cp-runs.log
python3 results/summarize-cp.py
```

`check-cp.sh` regenerates both tables, builds all four executables and checks both
new emitters. Normalization requires no oracle input. The oracle is read only by
the emitters' final checks. CP4's generated header is dataset-specific; regenerate
for a different dataset. `P4_CP1_TABLE` can override the default `build/cp1` location.
No generated C/header or binary/table is committed.
