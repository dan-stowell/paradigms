# IDEAS

Each idea has a source and a kill test in its source file; this is the index and the status. An idea from a paper
is a hypothesis until a cell measures it. Status: open / measuring (cell) / alive (number) / killed (number).

Sources (written 2026-11-16 by Sonnet prior-art readers and the four installers; several entries rest on
abstracts or memory and say so: check before citing):

- `ideas/resolve-reach.md` (19), `ideas/layout-codegen.md` (17), `ideas/wildcard.md` (16 systems, top 3)
- `systems/futhark/IDEAS.md` (13), `systems/halide/IDEAS.md` (13), `systems/souffle/IDEAS.md` (15),
  `systems/goose/IDEAS.md` (13)

## Being measured by the cells now

| idea | source | test | status |
|---|---|---|---|
| Winner rule as one max-reduce over a packed key `rank<<32 \| ~id` | resolve-reach 4, futhark I5 | every 1a cell uses it; the plain C reference does | alive: P1 OK in plain C |
| Method rule as a monotone Datalog program evaluated as one rule set | resolve-reach 11, souffle | Soufflé P2 cell: same live set, time vs 41-46 ms plain C | open |
| Frontier BFS with level synchronisation: cost tied to level count (110 levels in hlink) | resolve-reach 9, futhark I1 | Futhark/Halide P2: time per level x levels | open |
| Layout as a scan with an associative "round up then add" operator | layout-codegen L2, futhark I8 | Futhark 3a: scan form vs sequential walk | open |
| Halide algorithm/schedule split on gather/scatter (relocate) | halide I4-I6 | Halide 3b: schedules tried, ms each | open |
| Share-nothing threads: a flood partitioned by id range, cross edges as messages | goose 10 | Goose P2 at 8 workers vs 1 | open |

| Per-source edge dedup before the flood (25% of read edges repeat) | lead, reachfacts.py | p2par on a deduped CSR, 1 thread | killed: ~8% (23.9-29.1 vs 26.2-32.5 ms), 16:09Z |
| Reach over the eager CSR is the wrong shape; decode in the flood | lead, ATTACKS 1 | p2lazy vs p2pipe | alive: 52.6 / 13.6 vs 108.7 / 30.1 ms |
| Output on huge pages for relocate | lead, ATTACKS 2 | P3_THP=1 build/p3 | alive: 25-29 vs 48-68 ms on 1 core |

## New columns suggested

| system | why | source | status |
|---|---|---|---|
| SuiteSparse:GraphBLAS | reach as masked BFS over a boolean semiring; the natural sparse-linear-algebra form | wildcard top 1, resolve-reach 10 | open: after M2 |
| GraphIt | a schedule language for graph algorithms: Halide's axis on the problem Halide strains at | halide headline | open: after M2 |
| DuckDB | already on the VM; resolve as SQL join + group-by, layout as window functions | wildcard top 2 | open: after M2 |
| Differential Dataflow | incremental relink as a new axis | wildcard top 3 | open: needs Rust |

## Cheap measurements to run first (from the readers)

| test | source | status |
|---|---|---|
| BFS level count and frontier-size histogram on the flat graph | resolve-reach 9 | open |
| relocation-kind histogram of the 2.05 M live relocations | layout-codegen | open |
| shape-coverage recount for problem 4 | layout-codegen | open (M4) |
