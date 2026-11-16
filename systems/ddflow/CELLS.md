# Differential Dataflow cells (for GRID.md; column: incremental relink)

Times: ms, min / median / max of 5 interleaved rounds, 2026-11-16T19:39-19:49Z, load 4.4-5.6
(`systems/ddflow/bench.sh`, output `bench-20261116T1940Z.txt`). warm = facts in memory, arrangements built inside
the timing; e2e = process. Updates: 25 per cell (P1: 50). Checks are built in (exit 1 on mismatch); commands in
NOTES.md. Edits: edit 1 = `ISO8601Parse` gains an `http.ParseTime` fallback (amztime), edit 2 = the console UI
start-up deleted from `serverMain` (cmd); both rebuilt with freeze.sh and re-extracted (`data/minio-edit{,2}`).
Same window C: p2 52.4 median (1 core), p2par 17.2 (8); p1 287 (1), p1par 22.0 (8).

**1a+1b resolve** — **P1 OK** on MinIO and both edits (`ddflow p1inc $D/minio $D/minio-edit 5 -w 8` -> canon and
targets 0 extra / 0 missing on both versions) · full: 1 wkr 8659 / 9310 / 10823 · 8 wkr 1285 / 2023 / 3203 ·
**update edit 1: 0.6 / 0.7 / 1.5 (1 wkr), 0.5 / 29 / 195 (8)**; edit 2: 70 / 87 / 122, 44 / 64 / 237 · 23 lines
of dataflow · easy: winner = `reduce` max per name key, candidates = semijoin/antijoin on groups, 1b = a join;
forced: "lowest id" tie-break as (object, index in object) so other objects' growth does not churn it (still
churns 34 K facts inside an edited object), package-table decode at load. ~30x C full; update 400x C.

**2 reach** — **P2 OK** MinIO and kube-apiserver (`ddflow p2 $D/minio -w 8` -> extra 0, missing 0) · MinIO 1 wkr
warm 3077 / 3343 / 3591, e2e 3468-4103 · 8 wkr warm 1491 / 1814 / 2439, e2e 1903-2907 (index facts 751 + flood
1257) · kube 1 wkr 5605 / 5945 / 6787, 8 wkr 2608 / 2762 / 2948 · 71 lines · easy: all deadcode rules as two
mutually recursive Variables (`live`, `iface`) in one iterative scope, the method rule as three semijoins;
forced: static joins hoisted out of the loop, sig ids decoded at load. **60x / 100x C**: not a from-scratch engine.

**2 incremental relink** — **OK**: after the update the live set = B's oracle (mapped) = a from-scratch run on B
(`ddflow inc $D/minio $D/minio-edit 5 -w 1`) · diff edit 1: 27 edges + 30 flags, 31 symbols with changed facts of
9.7 M facts, live -1 +2 · **update 1.3 / 1.4 / 3.4 (1 wkr) vs C recompute 52 / p2par 31 (1 thr)**; 8 wkr
1.8 / 9.0 / 82 vs p2par 20.5 · diff edit 2: 234 edges, 141 symbols, **live -12,320 +10** · update 55 / 65 / 113
(1), 28 / 555 / 688 (8): **slower than recomputing in C** (retraction through 45 rounds; 8 workers 8x worse) ·
code: the full run plus `feed(minus, -1); feed(plus, +1)` · forced: a stable id space (positional ids shift on
any edit): name/hash keys + Myers alignment, `facts.rs`, outside DD.

1c, 3a, 3b: not attempted (outside this column's question).
