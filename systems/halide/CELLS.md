# Halide cells (for GRID.md), MinIO. P1-P3: 2026-11-16T17:00-17:10Z, load 1.8-7.3; P4: 18:31Z, load 4.1-4.9. ms: per-iteration min-max of 5 runs >= 1 s.
# Check: build with systems/halide/cells/build.sh; B=systems/halide/cells/build; D=data/minio (absolute path).

**1. resolve** — correct: P1 OK (`HL_NUM_THREADS=8 $B/p1 $D e2e 1`: canon, builtin, reltarget, p1c canon, all 0 differ).
1a join 81-93 / **19.8-27** (1 / 8 thr, warm) · 1b targets 42.7-46 / **9.9-10.9** · 1c content join 122-133 / **33-36.5**
· counts 10 / 2 · e2e process 0.35-0.40 s / 0.13-0.17 s · ~390 lines (gen_p1 219 + join.h 60 + driver 80 + counts).
Easy: winners as atomic max updates, targets as a ragged gather. Forced: no hash table, so the join is rounds of
scatter-min + gather (join.h); scans are serial, the parallel compaction is hand-blocked; sizes need a separate
pipeline. Impossible: none. Surprise: 1a beats plain C 2.5-3x on 1 core (compact first, small tables), not tuned C.

**2. reach** — correct: P2 OK (`HL_NUM_THREADS=8 $B/p2 $D e2e 1`; 30/30 parallel runs). lazy decode in the flood
417-465 / **79-90** · eager over p2_edge 312-332 / **70-127** · e2e 0.55-0.59 / 0.24-0.25 s (incl. 115 ms untimed C
edge split) · gen_p2 241 + driver 125 lines. Easy: each rule one select. Forced: level loop in C++ (no fixpoint),
implicit frontier + block lists (no worklist), 3 iterations per edge (one site per iteration), a byte per fact with
racy constant stores (atomic() on such an update = a mutex array per call, 3.5 s, and lost symbols). Impossible:
worklist, fixpoint, test-and-set. ATTACKS 1: lazy is expressible but does not beat the eager C pipeline (30.1 ms);
lazy is slower than eager in Halide. ATTACKS 3 (results/halide-levelfree.md, 19:16Z, load 6-10): the ~100 calls are
not the cost. One call is 0.02 ms. An empty level is 0.63 / 0.23 ms (3 passes over the 325 K block summary), so
62 / 23 ms of the flood. k levels per call (`flood_*_eK`: k = 2, 4, 8, lists built in-call from an aliased `stIn`,
capped later lists) cut the calls 98 -> 17-58 but are 1.0-1.2x slower (1 thr 299-344 vs 294 min; 8 thr 73-83 vs
66; p2par 26.7 / 10.3 in the same window). Forced: RDom bounds cannot come from data, so later levels in a call
loop to a cap. gen_p2 274 + driver 188 lines with the variants.

**3a. lay out** — correct: `$B/p3 $D 3a` (addr 0 differ). 53-58 / **20-23** · in e2e 22-31 · 154 lines.
Easy: sizes, per-unit sums, alignment walk. Forced: two-site cursor bump as a phase RVar; merge sort with merge-path
binary search + extern comparator (no sort). Impossible: none.

**3b. relocate** — correct: `$B/p3 $D 3b` (image 0 differ, 0 errors). warm 78-82 / **24.9-27**; fresh output 4 KB
pages 113-119 (19,109 faults) / 42.7-45 (19,187); **2 MB pages 84.6-88 (39 faults) / 27-30.8 (170)** · e2e (3a+3b)
0.21-0.25 / 0.10-0.12 s · 106 lines. Easy: ragged copy, 8-kind select chain. Forced: values stored byte by byte with
scratch bytes for unwritten ones (a non-innermost where is not a loop bound); chunked parallel. ATTACKS 2: yes, the
caller maps the output MAP_HUGETLB; faults 19 K -> 39, 1.3-1.6x faster, but 4-5x C on 2 MB pages.

**4. codegen by shape** — correct: P4 OK (`HL_NUM_THREADS=8 $B/p4 $D4 e2e lay_t_lf emit_t_i2_v32_p1`, D4=data/minio-p4:
code == oracle/p4_code.bin, fn offsets == p4_fn_off.u64, 1,780,281 relocations sorted == p4_rel_*; every variant and
P4_TOPK=0..all checked). 2026-11-16T18:31Z, load 4.1-4.9, min-max of 5; 1 thread = CPU time. templates (template-only
index) 9.4-9.6 / **6.8-7.7** · layout 17.9-18.7 / **4.9-5.6** (p4.c 6.5-7.0, p4par 8 thr 1.3-2.0) · emit 157-162 /
**23.0-25.1** (p4.c 103-112, p4par 97-99 / 16.4-20.1) · gather-form emit 237-251 / 56-72 · e2e process 0.21-0.24 /
0.07-0.10 s (2 MB pages) · gen_p4 345 + driver 288 lines. Easy: lengths + two-column scan (at, rat), rel32 as a gather
of at, the relocation list as a scatter to rat[i]+k (no C host needed), chunked parallel, caller-owned 2 MB output.
Forced: the template index (CSR inverse scatter, binary search as an RDom update); the vector copy only as a 32-byte
read-modify-write in parity passes (the obvious split+vectorize: 26 s); the patch as an 8-lane read-modify-write;
entry-0 indices on untaken select sides (Halide evaluates both: 200 -> 160 ms). Impossible: fusing copy and patch
(two updates of one Func; compute_with refused); exact-width stores in a vector loop. Top K templates (rest as glue):
emit is per piece (~42 ns) and per hole (~5.8 ns), not per byte (all-glue copy of 60.7 MB: 12.8 / 4.8 ms).


**4f. codegen from the flattened stream** (ATTACKS 4 step 1; results/halide-p4flat.md). Correct: P4 OK on
minio-p4 and kube-p4, 1 and 8 threads (`HL_NUM_THREADS=8 $B/p4f $D4 all`: code, all fn offsets, sorted relocations).
Timed 19:36-19:45Z, load 3.8-9.1, 5 interleaved rounds, emit ms min / median / max:
- MinIO, D stream: **41.1 / 41.9 / 43.1** (1 thread) and **13.2 / 15.8 / 25.5** (8). The template emit was 157-162 /
  23-25, so flattening gives 3.8x / 1.5x.
- MinIO, H1 stream (integers baked in): **12.2 / 12.8 / 14.1** (1) and **5.3 / 6.0 / 8.4** (8).
- Same window: Futhark 4D 67.4 / 22.7 median, p4runs D 46.3 / 13.8, p4runs B 41.1 / 11.8, p4holes H1 27.6 / 7.7.
- kube: D 48.4 / 18.3, H1 17.1 / 8.3 median.
- Prep (serial C, apart): 250-336 ms.

gen_p4f 59 + driver/prep 213 lines. Easy: a pure copy, one flat scatter of records, a relocation copy (D and B are
the same input here). Forced: scratch bytes for records narrower than 4. Impossible: nothing. H1 cuts the emit
3.0-3.3x (1 thread) and 2.3-2.6x (8 threads), against C's 1.55x, because the per-record patch (about 5 ns) was 80%
of Halide's flat emit.
