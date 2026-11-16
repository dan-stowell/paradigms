# Reach: frontier sizes per level (lead, 2026-11-16T17:35Z)

`gcc -O2 -I ref/flat -o build/p2levels ref/flat/p2levels.c && build/p2levels data/minio 2>results/levels-minio.txt`
(p2.c processed level by level; P2 OK on both datasets). Raw per-level lines: results/levels-{minio,kube}.txt.

| | MinIO | kube-apiserver |
|---|---|---|
| levels / rounds | 111 / 7 | 103 / 7 |
| levels per round | 38 23 22 7 13 6 2 | 34 19 15 13 11 8 3 |
| symbols popped / edges scanned | 678,450 / 2.53 M | 945,245 / 3.44 M |
| frontier median / max | 460 / 121,950 | 1,362 / 215,257 |
| levels with frontier < 1,024 (their share of pops) | 67 (1.9%) | 49 (1.2%) |
| levels with frontier < 4,096 (their share) | 81 (5.9%) | 62 (4.2%) |
| share of pops in the 8 largest levels | 54% | 57% |

Reading: the level-synchronous flood is a few wide levels and a long tail of tiny ones. 81 of 111 levels carry 6% of
the work but cost a full barrier (or ~25 whole-array passes in Futhark, a superstep in Goose, a statement round in
DuckDB) each. A hybrid kills the tail without a worklist: data-parallel levels while the frontier is wide, then one
sequential worklist drain (~40 K symbols, well under 1 ms) when it falls below a few thousand. Kill test: Futhark or
Goose flood with that switch vs its original, passes counted.
