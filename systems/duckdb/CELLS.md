# DuckDB cells (for GRID.md)

MinIO. bench.sh 2026-11-16T16:49Z-17:09Z, load 3-6, five interleaved rounds; ranges min-median-max ms. Default
scheduler (no spin/passive knob). Check commands: NOTES.md "Oracle checks".

**1a. join by name** — correct (`cmp out/p1_canon.u32`, `p1_builtin.u32` vs oracle) · warm 1 thr 319-337-355 /
8 thr **103-107-113**; e2e (all of P1) 4188-4228 / 1112-1122 · 18 lines SQL · easy: GROUP BY max over a UNION ALL,
SEMI JOIN for candidates; the 128-bit key is a HUGEINT via Arrow decimal128 · forced: nothing · impossible: nothing.

**1b. relocation targets** — correct (`cmp out/p1_reltarget.u32`) · warm 1405-1449-1470 / **272-314-375** · 17 lines ·
easy: the CSRs as Arrow list columns, `unnest` · forced: an equi-join written as `ON bc.i = CASE ... END` (the
natural `ON pkg = B AND i = idx` LEFT JOIN plans as a nested-loop join, 6.8 s CPU); an as-of join version was 6.4 s
· impossible: an array lookup by dense id (every lookup is a hash join, ~50x C).

**1c. content-hash join** — correct (`cmp out/p1c_canon.u32`) · warm 563-595-632 / **128-165-190** · 16 lines ·
easy: `arg_min(struct, (mark, -size, id))` is the winner rule · forced/impossible: nothing.

**2. reach** — correct (`cmp out/p2_live.u32`; also from relocations, eager and lazy) · warm prep + loop
207 + 2915-3206-3492 / **103 + 1965-2273-2354**; e2e 3463-3625 / 2367-2554 · 48 + 8 lines · easy: each rule is one
INSERT ... ANTI JOIN · forced: the loop (45 rounds) is driven from outside (dk `.repeat/.until`); flag tests as
small relations · impossible: a fast round (no index persists across statements: ~25 statements x 1-10 ms per
round); a recursive CTE (the method rules need global sets). ~150x tuned C. ATTACKS 1: eager decode+flood ~3.56 s,
lazy 3.44-3.54-3.69 s on 8 threads (tie; 1.2x on 1 thread): SQL expresses decode-on-reach but each round's decode
is a hash join against all of canon, so it does not pay; ~100x the eager C pipeline (30.1 ms).
**Attempt 3** (sql/p2rec.sql, 54 lines; `./dk $D 8 sql/load2.sql sql/p2rec.sql && cmp out/p2_live.u32
$D/oracle/p2_live.u32` -> P2 ok): one WITH RECURSIVE per flood (plain closure; used-in-interface closure) inside the
C reference's 7 method rounds, edges deduplicated and split inner/leaf, pruned as they are consumed · 18:00Z load
6.6-10.9, 5 interleaved rounds: 8 thr warm prep + flood **577-643-871 + 1438-1666-2234** (attempt 2 same window:
112-128-175 + 2362-2698-3159), e2e 2168-2336-3401 (2502-2709-3104); 1 thr 2058-2104-2220 + 2761-2821-3134 (201-221-250 +
2879-3375-3804), e2e 5060-5249-7048 (3708-4503-5067) · flood 1.6x faster on 8 threads, cell total ~1.2x (8) / 0.7x (1)
· **falls down** (three attempts): a recursive CTE's every iteration rescans its edge table (2.7 M inner edges; 35 BFS
levels in round 1 alone, frontiers 16-24 K), and DuckDB has no index join for frontier-sized probes (ART used only
for <=50-row build sides) and no O(1) array lookup (`getvariable` list indexing copies the list: 1,000 probes 7.7 s).
~140x tuned C.

**3a. lay out** — correct (`cmp out/p3_addr.u32`) · warm 304-315-322 / **217-246-263**; e2e 408-415 / 325-335 ·
35 lines · easy: occupied sizes and in-class positions are window sums · forced: the unit chain
(round up, then add) is a fold: `list_reduce` with a list accumulator (65-95 ms; a recursive CTE was 340-620 ms, a
padding fixpoint 1,121 rounds); typelinks by the padding fixpoint (1 round on MinIO) · impossible: nothing.

**3b. relocate** — correct (`check3b.py` -> P3b OK; also as P1 -> 3a -> 3b pipeline from DuckDB's own canon and
addresses, e2e 5962-6138 / 2017-2090) · warm 789-827-1142 / **339-362-368** on 4 KB pages, 813-832 / **309-321-332**
on 2 MB pages; e2e 1583-1637 / 687-750 · 32 lines · easy: every write as rows (pos, bytes) / (pos, width, value),
one CASE over the kinds · forced: the store at byte offset is dk's (a memcpy per row) · impossible: DuckDB's own
buffers on huge pages; ATTACKS 2: the image's faults drop 21,870 -> 4,101, ~10% at 8 threads, because the
target-address hash join, not the faults, is the cost.
