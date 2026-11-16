# IDEAS

Each idea has a source and a kill test in its source file; this is the index and the status. An idea from a paper
is a hypothesis until a cell measures it. Status: open / measuring (cell) / alive (number) / killed (number).

Sources (written 2026-11-16 by Sonnet prior-art readers and the four installers; several entries rest on
abstracts or memory and say so: check before citing):

- `ideas/resolve-reach.md` (19), `ideas/layout-codegen.md` (17), `ideas/wildcard.md` (16 systems, top 3)
- `ideas/copy-and-patch.md` (8, problem 4; Sonnet reader 17:30Z, C&P and Deegen papers read in full): top CP1 width-relaxed
  templates (claims disp8/disp32 is a pure function of the operand, 0 exceptions in 4.86 M holes: **lead-verified 17:33Z**, `python3 -I results/cp1check.py data/minio-p4`; the 66,592-template bound is not yet), CP4 a
  generated straight-line emitter per hot template (Deegen 1.62 GiB/s vs our 0.42 GB/s), CP2 always-disp32 (new oracle)
- `systems/futhark/IDEAS.md` (13), `systems/halide/IDEAS.md` (13), `systems/souffle/IDEAS.md` (15),
  `systems/goose/IDEAS.md` (13)

## Being measured by the cells now

| idea | source | test | status |
|---|---|---|---|
| Winner rule as one max-reduce over a packed key `rank<<32 \| ~id` | resolve-reach 4, futhark I5 | every 1a cell uses it; the plain C reference does | alive: P1 OK in plain C |
| Method rule as a monotone Datalog program evaluated as one rule set | resolve-reach 11, souffle | Soufflé P2 cell: same live set, time vs 41-46 ms plain C | **measured** (GRID): correct, 1227-1467 / 431 ms (1/8 thr), ~30x C; methods forced into stratified rounds |
| Frontier BFS with level synchronisation: cost tied to level count (110 levels in hlink) | resolve-reach 9, futhark I1 | Futhark/Halide P2: time per level x levels | **killed for Futhark** (results/levelfree.md: 45 -> 15 levels within noise; passes per level are the cost); Halide batched-level test running (results/halide-levelfree.md) |
| Layout as a scan with an associative "round up then add" operator | layout-codegen L2, futhark I8 | Futhark 3a: scan form vs sequential walk | **killed**: scan 22 / 10 vs loop 14 / 7.3 ms (1/8; GRID) |
| Halide algorithm/schedule split on gather/scatter (relocate) | halide I4-I6 | Halide 3b: schedules tried, ms each | **measured** (systems/halide/CELLS.md): 24.9-27 on 8, 4-5x C; byte-at-a-time copy is the cost |
| Share-nothing threads: a flood partitioned by id range, cross edges as messages | goose 10 | Goose P2 at 8 workers vs 1 | **measured**: 1.7x at 8 (supersteps); async 1.3-1.5x better (ATTACKS 3) |

| Per-source edge dedup before the flood (25% of read edges repeat) | lead, reachfacts.py | p2par on a deduped CSR, 1 thread | killed: ~8% (23.9-29.1 vs 26.2-32.5 ms), 16:09Z |
| Reach over the eager CSR is the wrong shape; decode in the flood | lead, ATTACKS 1 | p2lazy vs p2pipe | alive: 52.6 / 13.6 vs 108.7 / 30.1 ms |
| Output on huge pages for relocate | lead, ATTACKS 2 | P3_THP=1 build/p3 | alive: 24 vs 49 ms 1 thread, 5.4-6.8 vs 14 on 8 (at the bandwidth bound), 16:13Z |
| Flat inputs on 2 MB pages (TLB reach for the flood's random walk) | lead, after the hugetlb tunable sped the join 1.5x | `FLAT_HUGE=1` (flat.h copies files >= 2 MB onto MAP_HUGETLB) vs the page-cache mapping, 1 thread, 3 interleaved | **killed: 9-12%** (p2par 24.7-25.9 vs 27.6-29.4; p2lazy 60.7-62.0 vs 66.7-70.2; p1compact within noise), 16:41Z |
| A barrier-free flood (async worklist, no level sync) is robust where a level-synchronous one collapses under a shared machine | lead, 16:20Z: p2par 8 threads 14 ms -> 900-1300 ms under builder load with spinning OMP barriers (111 levels), 14-55 passive | p2async (codex-g) vs p2par | **alive under overload**: 81-93 vs passive 106-156 vs active 2079-3510 ms (8 threads + 8 busy loops); ambient within noise of passive; quiet untested. ATTACKS 3 |
| Resolve's join is bound by passes over the dense slot arrays (15% keyed) | lead, 16:17Z | codex-f: p1compact (one dense pass, partition-local candidates) vs p1par | **alive: 1.5x on 1 thread** (lead rerun 16:34Z, load 3-5, 5 interleaved, mean of 30 reps: join MinIO 28.1-30.7 vs 41.7-48.7 ms, kube 68.6-81.9 vs 103-123); 8 threads medians 7.3 vs 9.8 (MinIO), 22.2 vs 33.8 (kube), ranges touch. Mechanism not settled: modeled streamed bytes fall only 18-21% (results/codex-f.md: p1par covers 203 MB of keys, not 333, since kind-1 probes skip lines); the partition-local candidate join (no random probes into 256 tables) is the other change. **Split (codex-h, 16:42-16:55Z, 5 interleaved, medians, 1 thread MinIO/kube): p1par 46.1/101.9, dense-pass-only 36.8/84.4, partition-local-candidates-only 29.5/73.8, both 28.9/63.4.** The bandwidth hypothesis is killed: the cost was 162 K candidates each probing a random one of 256 tables twice (cache + TLB misses), which also fits the hugetlb tunable's 1.5x on this join. Lesson: in a partitioned join, partition the probe side too. |

## New columns suggested

| system | why | source | status |
|---|---|---|---|
| SuiteSparse:GraphBLAS | reach as masked BFS over a boolean semiring; the natural sparse-linear-algebra form | wildcard top 1, resolve-reach 10 | **added M3** (P1, P2) |
| GraphIt | a schedule language for graph algorithms: Halide's axis on the problem Halide strains at | halide headline | open: after M2 |
| DuckDB | already on the VM; resolve as SQL join + group-by, layout as window functions | wildcard top 2 | **added 15:58Z** (P1-P3) |
| Differential Dataflow | incremental relink as a new axis | wildcard top 3 | **installing** (2026-11-16, systems/ddflow; Rust via rustup) |

## Cheap measurements to run first (from the readers)

| test | source | status |
|---|---|---|
| BFS level count and frontier-size histogram on the flat graph | resolve-reach 9 | **done 17:35Z** (results/levels.md): 111 levels; 81 have frontier < 4,096 and hold 5.9% of pops; the 8 widest hold 54%. Suggests a hybrid: parallel while wide, sequential drain for the tail |
| CP1 width-relaxed templates (ideas/copy-and-patch.md) | Sonnet reader, verified premise | codex-cp (`ref/p4/check-cp.sh`, lead rerun 17:52Z P4 OK) | **split**: count alive (66,046 templates vs 108,833; top 4,096 cover 82.3% of nodes vs 74.6%); layout killed: deriving lengths from operands makes the scan 6.3x slower (reads 7.68 M operands instead of 2.38 M lengths). Merge the table, keep per-node lengths precomputed |
| CP4 generated emitter for the top 4,096 templates | Deegen | codex-cp, P4 OK | **killed at its threshold**: covered-segment emit 1.67x faster (median) but ranges overlap; 26% fewer instructions; hot templates are only 36% of emitted bytes (glue 29%, cold templates 35%), so whole-emit gain is capped; 2.9 MB of generated C, 52-63 s to compile |
| relocation-kind histogram of the 2.05 M live relocations | layout-codegen | **done 17:40Z** (results/relkinds.txt, `python3 -I results/relkinds.py data/minio`): 8 (kind, width) pairs only; call+pcrel rel32 56%, addr64 19%, addroff/methodoff 24%; 454 K distinct targets but the top 100 take 30% of relocations, 92% have addend 0. So 3b's T-gather is mostly cache-hot, and per-kind branches are few: a branch-free table of 8 formulas fits SIMD |
| shape-coverage recount for problem 4 | layout-codegen | **done** (ref/p4/NOTES.md): typed shape does not fix the bytes; top 4,096 byte templates cover 74.6% of nodes (82.3% width-relaxed) |
