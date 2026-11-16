# Halide cells (for GRID.md), MinIO, 2026-11-16T17:00-17:10Z, load 1.8-7.3. ms: per-iteration min-max of 5 runs >= 1 s.
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
lazy is slower than eager in Halide.

**3a. lay out** — correct: `$B/p3 $D 3a` (addr 0 differ). 53-58 / **20-23** · in e2e 22-31 · 154 lines.
Easy: sizes, per-unit sums, alignment walk. Forced: two-site cursor bump as a phase RVar; merge sort with merge-path
binary search + extern comparator (no sort). Impossible: none.

**3b. relocate** — correct: `$B/p3 $D 3b` (image 0 differ, 0 errors). warm 78-82 / **24.9-27**; fresh output 4 KB
pages 113-119 (19,109 faults) / 42.7-45 (19,187); **2 MB pages 84.6-88 (39 faults) / 27-30.8 (170)** · e2e (3a+3b)
0.21-0.25 / 0.10-0.12 s · 106 lines. Easy: ragged copy, 8-kind select chain. Forced: values stored byte by byte with
scratch bytes for unwritten ones (a non-innermost where is not a loop bound); chunked parallel. ATTACKS 2: yes, the
caller maps the output MAP_HUGETLB; faults 19 K -> 39, 1.3-1.6x faster, but 4-5x C on 2 MB pages.
