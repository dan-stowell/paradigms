# MORNING, 2026-11-16T18:40Z (M3; M4 half done)

## The grid (MinIO, ms, 1 / 8 threads, warm; load 7-26 during M3, so 8-thread numbers are upper bounds)

| problem | tuned C (flat) | Goose | Futhark | Halide | Soufflé | DuckDB | GraphBLAS (new) |
|---|---|---|---|---|---|---|---|
| 1a join by name | p1compact 28 / 7-10 | 79-115 / 37-57 | 79 / 24 | 81-93 / 20-27 | 1928-2034 / 636 | - / 103-113 | - / 268-339 |
| 1b targets | 43 / 5-6 | 39-57 / 42-67 | 47 / 18 | 43-46 / 10-11 | 5220-5426 / 1080 | - / 272-375 | - / 162-203 |
| 1c content join | 37 / 7-9 | 97-118 / 39-47 | 111 / 38 | 122-133 / 33-37 | 1625-1865 / 394-587 | - / 128-190 | - / 279-392 |
| 2 reach | 27-30 / 13-17 | 41-49 / 22-32; async 33-44 | 97 / 58; seq worklist 67-130 | 312-332 / 70-127 | 1227-1467 / 431 | ~600 + 1438-2234: **falls down** (3 attempts) | 192-203 / 179-213 (no scaling) |
| 3a lay out | p3a_par 5.0 / 3.9 | 15-26 / 17-31 | 14 / 7.3 | - / 20-23 | 1930 / 1725-3541 (**no longer falls down**) | - / 217-263 | not attempted (no scan) |
| 3b relocate (2 MB) | 24 / 5.4-6.8 | 41-42 / 17-24 | 107 / 41 | - / 27-31 | - / 320 | - / 309-332 | - |
| 4 emit by shape | p4 116-174 / **p4par 51-68** | - | **326-439 / 161-216** | running | - | - | - |

Every cell above is correct against its oracle; the lead reran every new check (rebuilding from source where the
cell compiles). **kube-apiserver:** every Goose/Futhark/Halide/Soufflé/DuckDB cell of P1-P3 also reproduces kube's
oracles (results/codex-kube.md); 1-thread times grow 1.5-2.5x for ~1.4x the data.

## Surprises
- **Down (for the premise):** the level count is *not* Futhark's cost: its flood has 45 levels (not 111), and halving
  them saves nothing; a plain sequential worklist written in Futhark beats its data-parallel flood. The ~26 whole-array
  passes inside each level are the cost.
- **Down:** GraphBLAS's flood is the shortest legible reach, but it runs on one core: frontiers (~7 K) sit below
  SuiteSparse's per-call threading threshold, 379 of 382 vxm calls single-threaded; 9x tuned C.
- **Up:** code generation's emit runs at 0.42 GB/s, and patching is only 7.6% of it: the cost is 2.38 M small pieces.
- **Up:** disp8 vs disp32 is a pure function of the operand (0 exceptions in 4.86 M holes): templates merge 108,833 -> 66,046.
- **Up:** Futhark *can* bulk-copy: a sequential slice-update loop inside a map over 64 blocks compiles to memcpy
  (3-5x the byte scatter its 3b used; not yet applied to 3b).
- **Mixed:** reach's frontier: 81 of 111 levels are under 4,096 symbols and hold 6% of the work, but a hybrid flood in
  C that drains them sequentially moves ~44% of the work to one thread (the small levels lead into wide ones).

## ATTACKS.md
- **New 4. Code generation by shape: the emit costs pieces, not bytes; merge templates by width** (above numbers;
  generated per-template emitters 1.67x on covered segments but within noise).
- **3 extended:** measured in Futhark and Goose (level sync costs Goose 1.3-1.5x, not Futhark), and in C as p2hyb.

## Ideas tried and killed
CP1's operand-derived lengths (layout 6.3x slower; keep lengths precomputed); CP4 at its 1.5x threshold (ranges
overlap); Futhark batched levels (1.5-2.5x slower); Futhark hybrid (within noise); GraphBLAS pull direction (40x slower);
DuckDB recursive CTE as a flood (each step rescans 2.7 M edges).

## Review disagreements worth your attention
- M3 skeptic fired 18:38Z; triage in REVIEWS.md when it lands.

## Decisions for you
None.
