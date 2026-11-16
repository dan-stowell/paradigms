# Second attempts owed by the M2 review (REVIEWS.md 20261116T1713): DuckDB P2, Soufflé 3a

Branch `worktree-agent-aa77583b9c2460e4b`. Both new cells match their oracles exactly. Times are ms,
min-median-max over 5 interleaved runs, warm unless marked e2e. The machine was shared (load 6-22 during the work,
noted per bench).

## Soufflé 3a (lay out): attempt 3, `systems/souffle/layout_rank.dl`. No longer falls down

Check: `build/layout_rank -j8 -F $D -D out && build/check_p3 $D $D/oracle/p3_image.bin out/p3_addr.u32` gives P3 OK
(0 of 360,315 addresses differ). Rerun: `ONLY='3a|3a-rank' systems/souffle/bench.sh 5`.

Shape: the typelink order with no sort, no pair counting and no C functor.
- The loader shows each type string as big-endian 8-byte words (`words8="p3_str"`, like the existing `key8`: bytes as
  numbers, no comparison done in C).
- A string's prefix of c words is a Soufflé record `[prefix, word]`. The record table interns records, so equal
  prefixes are the same value, and a prefix can be a group key (`node`, 164 K records).
- rank(s) = the sum, over the prefix groups s shares with other strings, of the members whose next word is smaller.
  A string that ends inside a group has next word 0, so shorter strings sort first.
- "Members with a smaller word" uses a radix-256 trie of group-by counts. For each byte L of the word, it counts the
  members that agree above L and have a smaller byte at L. Each sum ranges over at most 255 distinct bytes, never
  over members. That is 871 K tuples, against about 10^9 pair visits before.
- A typelink's address = the sizes of the typelinks with a smaller rank (the same trie over the rank's bytes), plus
  the typelinks with the same string and a smaller id (a range of an index).

| 17:56Z, load 11-13 | 1 thread warm | 1 thread e2e | 8 threads warm | 8 threads e2e |
|---|---|---|---|---|
| attempt 3 (layout_rank) | 1712-1930-3086 | 1893-2061-3409 | **1725-1937-3541** | 1922-2128-3790 |
| attempt 2 (layout), same window | 7647-8270-9097 | 7800-8427-9310 | 2860-3388-7055 | 3006-3604-7302 |

What this shows:
- The typelink part dropped from ~95% of the cell (~7 s on 1 thread) to ~1.1 s on 1 thread and ~0.65 s on 8
  (souffleprof).
- The cell is still ~100x the plain C (8-19 ms) and ~400x the tuned C (p3a_par 3.9-5.0 ms). The sort stays in
  Datalog. Verdict: **slow, not falls down.**
- 8 threads do not help. The 1,763-iteration unit-walk recursion costs ~0.6 s of per-iteration overhead at -j8.

Traps found on the way:
1. A binary 64-level trie produced 7 M tuples and lost to the old O(n²) version on 8 threads.
2. An expression inside an aggregate's body atom, `cnt(p, j, (w bshr j) - 1, k)`, makes Soufflé materialize the body
   with only a prefix of the key bound. The run went past 100 s, so every key is now spelled out as its own relation.
3. `tl(_, _, _, _, c2), c2 != c` in `tlcheck` scanned all of tl for each typelink (also present in attempt 2). It now
   goes through a class relation.

## DuckDB P2 (reach): attempt 3, `systems/duckdb/sql/p2rec.sql`. Falls down

Check: `./dk $D 8 sql/load2.sql sql/p2rec.sql && cmp out/p2_live.u32 $D/oracle/p2_live.u32` matches (663,630 live).
Rerun: `ONLY=p2 systems/duckdb/bench.sh`.

Shape: the C reference's outer loop, 7 method rounds. Each round runs two WITH RECURSIVE floods: the plain closure,
and the used-in-interface closure over gotype->gotype edges plus type->method type. Between floods, the
sig/named/reflect sets and the callable methods are computed. This replaces 45 BFS rounds x ~25 statements.

Tuning, each step measured:
- Edges deduplicated (8.15 M -> 5.18 M) and split by target into inner edges (2.73 M, walked by the recursion) and
  leaf edges (added by one join after it).
- The working edge sets are pruned as they are consumed, so no recursion anti-joins against the growing live set.
  The first version did, rebuilding live's hash table every iteration, and took 3.7 s.
- Reflect is tested through a small relation instead of a join with `sym`.
- Seeds are not re-filtered against live.
- Sorting the edges did not help (within noise), so it was dropped.

| 18:00Z, load 6.6-10.9 | 8 thr prep + flood | 8 thr e2e | 1 thr prep + flood | 1 thr e2e |
|---|---|---|---|---|
| attempt 3 (p2rec) | 577-643-871 + **1438-1666-2234** | 2168-2336-3401 | 2058-2104-2220 + 2761-2821-3134 | 5060-5249-7048 |
| attempt 2 (p2loop), same window | 112-128-175 + 2362-2698-3159 | 2502-2709-3104 | 201-221-250 + 2879-3375-3804 | 3708-4503-5067 |

On 8 threads the flood is 1.6x faster (the ranges do not overlap) and the whole cell is ~1.2x faster (2.3 vs 2.8 s,
~140x the tuned C). On 1 thread the cell is slower because the prep grows to 2.1 s.

Verdict: **falls down** after three attempts. Both reasons are measured:
1. **Every iteration of a recursive CTE re-runs its join with the whole edge table as the probe side.** It scans
   2.7 M rows however small the frontier is. Round 1 alone is 35 BFS levels, with frontiers of 16 to 24 K
   (`systems/duckdb/p2levels.py`). So the cost is levels x edges, the same as attempts 1-2.
2. **DuckDB has no lookup for a frontier-sized probe that avoids a scan or a hash build.**
   - An ART index on `src` is used in a join only when the build side is tiny. On 5 M synthetic edges
     (`systems/duckdb/x_artjoin.sql`), a 40-row frontier takes 2 ms with the index vs 9-19 ms without. A 2,000-row
     frontier takes 12 vs 15 ms, even with `dynamic_or_filter_threshold` and `index_scan_max_count` raised.
   - A dense array held as a `getvariable` LIST is copied on every call: 1,000 probes take 7.7 s.

   That rules out the persistent-index/frontier shape. Batching several levels per statement would need the E^k
   edge relation.

`dk.c` gained `DK_ALLROUNDS=1`, which prints every `.repeat` round.
