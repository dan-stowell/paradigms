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

## Hybrid flood in tuned C (ref/flat/p2hyb.c, lead 17:23Z)

p2par plus a sequential LIFO drain while the frontier is below P2_TAIL (the drain runs until the stack passes
4 x P2_TAIL). P2 OK. Counts (deterministic up to scheduling, MinIO): tail 0 = p2par: 111 barriers, 678 K pops in
parallel; tail 256: 64 barriers, 676 K parallel; tail 1,024: 19 barriers, 395 K parallel (42% of pops sequential);
tail 4,096: 12 barriers, 379 K parallel (44% sequential). So the tiny levels are only 6% of the work, but they are
*interleaved* with growth: a drain that starts on a small frontier soon reaches wide parts of the graph, and the
hysteresis that saves the barriers puts ~40% of the work on one thread. Times at load 10-19 (seven helpers
running) are noise: 8 threads slower than 1 for both; p2hyb 8 threads 56-145 vs p2par 96-258 best-of-20, in the
same three rounds (ranges overlap). **To time on a quiet machine**; the shape to test next is a drain capped by a
budget of pops (hand the stack back to the parallel loop after, say, 2 K pops) instead of by stack size.
