# DuckDB (vectorized, morsel-parallel SQL)

DuckDB v1.5.5 (libduckdb from the GitHub release, `~/tools/duckdb/{duckdb.h,libduckdb.so}`; the CLI is
`/usr/local/bin/duckdb`, same version). Every problem's logic is SQL in `sql/`; `dk.c` is a C host that only does I/O:
it shows the flat files to DuckDB as Arrow arrays (zero-copy over the mmaps), runs the scripts with a clock per
statement and per `.cell`, and writes results out as raw arrays or into an image.

## Surprises

- **The CSRs are Arrow lists, for free.** `slot_rel.u32` + `rel_*.u32` is exactly Arrow's list layout (offsets +
  child arrays), so dk hands DuckDB `srel.rels`, `sym.out`, `sym.meths`, `obj.bases`, `sym3b.rels` as
  `LIST(STRUCT(...))` columns with no copy, and `unnest(rels, recursive := true)` turns a symbol into its
  relocations without a join. That replaced an as-of join (1b: 6.4 s -> 2.1 s on 1 thread) and a bucketed range join
  (P2 prep: 16.9 s -> 0.2 s on 1 thread). The first version of both was "SQL over flat tables"; the nested type is
  what made them reasonable.
- **Decoding in the flood does not win in SQL (ATTACKS 1).** In C the lazy flood is 2x the eager pipeline. In DuckDB
  the lazy loop (2loop-lazy) costs about as much more than the eager loop as the eager decode costs: each round's
  decode is a hash join against the 5.2 M-row `canon` table, rebuilt or rescanned every round (no persistent index
  on a dense id), so 45 rounds of "decode only the frontier" read `canon` 45 times. See ATTACKS below.
- **A dense-id lookup is a hash join.** SQL has no `a[i]`. 9 M probes into a 5 M-row table cost 1.3 s of CPU
  (~140 ns per probe, 0.22-0.29 s on 8 threads, CLI test 16:20Z at load 12); the C gather is a few ns per element.
  This one fact explains most of the distance to C in every problem. A `LIST` lookup (`l[i+1]` on a 5 M-element
  list cross-joined in) is worse: 5.3 s.
- **`prlimit --as` segfaults DuckDB at any limit** (6G, 12G, 32G, 100G): it reserves address space up front. Cap
  memory with `systemd-run --user --scope -p MemoryMax=8G` instead (bench.sh does).

## Layout

| file | what |
|---|---|
| `dk.c` (build: `./build.sh`) | the host: `.table NAME ROWS col=file ...` (Arrow view + CREATE TABLE), `col=OFF.u32[f=file,...]` a list column over a CSR, `col=#` a row index, `.cell`, `.repeat/.until` (a loop driven from outside: SQL has no loop), `.dump`, `.image/.blob/.int/.imgout` (the 3b scatter), `.show`; `DK_REPS` (repeat the scripts after `--`), `DK_THP` (image on 2 MB pages), `DK_STMTS` (time per statement over all rounds) |
| `sql/load1.sql p1.sql` | P1: 1a, 1b, 1c |
| `sql/load2.sql p2prep.sql p2loop.sql` | P2 from the p2_* files |
| `sql/p2aux.sql p2eager.sql p2lazy.sql` | ATTACKS 1: P2 from the relocation columns (after P1), eager or lazy decode |
| `sql/load3.sql p3a.sql` | 3a |
| `sql/load3b.sql load3b_iso.sql p3b.sql` | 3b isolated (oracle addresses, `p3_addr_all`) |
| `sql/p3link.sql` | 3a -> 3b pipeline: addresses from DuckDB's own 3a through its own 1c canon |
| `check3b.py` | the 3b image check (every placed symbol's bytes vs `oracle/p3_image.bin`) |
| `bench.sh` | every timing, one line per (cell, threads, warm/e2e) |

## Oracle checks

    cd systems/duckdb && ./build.sh && D=/home/exedev/paradigms/data/minio
    ./dk $D 8 sql/load1.sql sql/p1.sql && for f in p1_canon p1_builtin p1_reltarget p1c_canon; do cmp out/$f.u32 $D/oracle/$f.u32 && echo ok $f; done
    ./dk $D 8 sql/load2.sql sql/p2prep.sql sql/p2loop.sql && cmp out/p2_live.u32 $D/oracle/p2_live.u32 && echo P2 ok
    ./dk $D 8 sql/load1.sql sql/p1.sql sql/load2.sql sql/p2prep.sql sql/p2aux.sql sql/p2lazy.sql sql/p2loop.sql && cmp out/p2_live.u32 $D/oracle/p2_live.u32 && echo P2 lazy ok   # (p2eager.sql likewise)
    ./dk $D 8 sql/load3.sql sql/p3a.sql && cmp out/p3_addr.u32 $D/oracle/p3_addr.u32 && echo 3a ok
    ./dk $D 8 sql/load3b.sql sql/load3b_iso.sql sql/p3b.sql && python3 -I check3b.py $D out/p3_image.bin       # P3b OK
    ./dk $D 8 sql/load1.sql sql/p1.sql sql/load3.sql sql/p3a.sql sql/load3b.sql sql/p3link.sql sql/p3b.sql && python3 -I check3b.py $D out/p3_image.bin   # pipeline

All pass (2026-11-16T16:40Z-16:47Z): 1a, 1b, 1c, P2 (three ways), 3a, 3b isolated, and the P1 -> 3a -> 3b pipeline
(DuckDB's own canon and layout, `p3_fixed_*` given) reproduce the oracles exactly.

## Timings

`bench.sh` (2026-11-16T16:49Z-17:09Z, load 3.0-6.0, five interleaved rounds of 1 and 8 threads; warm = median of
5 in-process reps per round, then min/median/max over the rounds; e2e = the whole process; raw output in
`results-bench-20261116T1649Z.txt`). ms. C references from GRID.md (plain C 1 core warm; tuned C 8 threads).

| cell | 1 thread warm | 8 threads warm | 1 thread e2e | 8 threads e2e | plain C 1 core | tuned C 8 thr | DuckDB / tuned C (8) |
|---|---|---|---|---|---|---|---|
| 1a | 319-337-355 | **103-107-113** | P1: 4188-4228-4386 | P1: 1112-1122-1278 | 194-203 | 8.6-10.5 | ~11x |
| 1b | 1405-1449-1470 | **272-314-375** | (in P1) | (in P1) | 43-44 | 5.3-6.3 | ~50x |
| 1c | 563-595-632 | **128-165-190** | (in P1) | (in P1) | 57-61 | 7.2-8.9 | ~18x |
| 2 (prep + loop) | 207-212-251 + 2915-3206-3492 | **103-103-116 + 1965-2273-2354** | 3463-3625-3875 | 2367-2554-2567 | 39-43 | 13.0-17.0 | ~150x |
| 3a | 304-315-322 | **217-246-263** | 408-415-439 | 325-335-448 | 8-19 | (none) | ~15-30x plain C |
| 3b (4 KB pages) | 789-827-1142 | **339-362-368** | 1583-1637-1786 | 687-750-956 | 48.6-56 | 13.4-29 | ~25x |
| 3b (2 MB pages) | 813-832-937 | **309-321-332** | | | 23.9-30 | 5.4-12 | ~50x |
| P1 -> 3a -> 3b pipeline | | | 5962-6138-6436 | 2017-2090-2765 | | | |

e2e includes DuckDB start, the Arrow views and the copy into DuckDB tables (P1 load ~0.3-0.6 s), and writing the
output arrays / image. Load (`CREATE TABLE AS FROM arrow view`) is not in the warm numbers.

Lines (non-blank, non-comment SQL): 1a 18, 1b 17, 1c 16 (p1.sql 56 + load 5); P2 48 + prep 8 + load 3;
ATTACKS 1 aux 16, eager 12, lazy 20; 3a 35 + load 3; 3b 32 + load 3; pipeline link 9. Host: dk.c 628 lines of C
(I/O only: Arrow views, the statement runner, the scatter of 3b's writes).

## Per cell

**1a (join by name).** Easy: two GROUP BY max over a UNION ALL, a SEMI JOIN for "a candidate joins only an existing
group", a LEFT JOIN for the canonical ids. The 128-bit key is read as Arrow `decimal128` = DuckDB HUGEINT, so
equality and hashing are on the raw bits. Forced: nothing. 18 lines.

**1b (relocation targets).** Easy once the relocations are a list per slot (`unnest`), the package table a list
per object (`generate_subscripts` gives the index). Forced: a LEFT JOIN `ON rel.pkg = BUILTIN AND bc.i = rel.idx`
is planned as a block-wise nested-loop join (6.8 s of CPU); `ON bc.i = CASE WHEN rel.pkg = BUILTIN THEN rel.idx END`
is a hash join. Two hash joins against 5 M and 9 M rows are the whole cost. 17 lines.

**1c (content-hash join).** Easy: `arg_min(struct, (mark, -size, id))` per group is the winner rule as written. 16 lines.

**2 (reach).** Forced. SQL has no loop: dk's `.repeat/.until` reruns a block of statements until the frontier is
empty (45 rounds), i.e. semi-naive evaluation driven from outside, one temp table per relation. 48 lines for the
rules + 8 prep. Every round rebuilds hash tables for `live`, `iface`, `uii` and the frontier joins; there is no
persistent index, so a round costs ~25 statements x (1-10 ms) regardless of how small the frontier is. Two tuned
attempts: (1) edges by a bucketed range join, 16.9 s prep; (2) edges as lists + the rules' flag tests as small
relations (`gt`, `rf`) instead of joins with the 5.2 M symbols. A recursive CTE cannot express it (the method
rules need the global sig/named/reflect sets, and a recursive CTE may reference itself once).

**3a (layout).** Mostly easy: occupied sizes, per-(unit, bucket, class) sums and every in-class position are window
sums. Forced: "va = round_up(va, align); va += n" over the units is a fold, not a prefix sum (a round-up does not
compose). Tried three ways: a Jacobi fixpoint over the paddings (prefix-sum with guessed paddings, recompute,
repeat): exact but 1,121 rounds for the units; a recursive CTE over the unit index: 1,322 iterations, 340 ms on 1
thread and 620 ms on 8 (the scheduler's per-iteration cost); `list_reduce` with a list accumulator (a fold in
DuckDB's lambdas, O(n^2) list copies but n = 1,322): 65-95 ms. Typelinks (33 K) use the fixpoint: 1 round on MinIO
because every typelink size is a multiple of its alignment, exact in general. 35 lines.

**3b (relocate).** SQL computes every write: the symbols' bytes as (pos, blob) rows and every relocation as
(pos, width, value) from one CASE over the kinds; dk copies them in (`.blob`, `.int`: a memcpy per row, the only
thing SQL can't say, "store at byte offset"). The section base is a CASE ladder. 32 lines. Isolated and pipeline
both exact.

## ATTACKS

**ATTACKS 1 (reach, counting the decode).** Both shapes expressed and exact (`p2eager.sql`, `p2lazy.sql`, P2 OK).
Medians, 8 threads: eager = 1b decode 314 + build the adjacency lists 813 + flood 2437 = **~3.56 s**; lazy = the
flood with the decode inside **3536** (range 3438-3689). 1 thread: eager 1449 + 3081 + 3161 = ~7.7 s; lazy
6420-6445-7116. Setup excluded from both, as in the C: 1a's canon and the aux / kind 1-3 edges (2aux: ~1.4 s, it
needs a full decode to tell aux edges from relocation edges in the p2 CSR). So: DuckDB does not beat the eager C
pipeline (108.7 / 30.1 ms) by any shape; it is **~100x slower than it** at 8 threads. Can SQL express "decode a
reference when the flood reaches it"? Yes, naturally: a view that unnests only the frontier's relocation lists and
joins them through the package table and canon (`rel_dl`, `decoded()`). But it does not pay: lazy ties eager on 8
threads and wins only 1.2x on 1. The reason is the paradigm's, not the attack's: a round's decode is a hash join
against `canon` (5.2 M rows), whose hash table is rebuilt (or `canon` rescanned) every round, so 45 small decodes
cost about one big one. The attack's premise (a decode is an O(1) array read, so decoding only 29% of the edges
saves 71% of the work) fails in an engine where every lookup by id is a join with a per-statement setup cost.
The flood itself (2loop, ~2.3 s) dominates either way: ~25 statements per round x 45 rounds, each 1-10 ms.

**ATTACKS 2 (relocate on huge pages).** dk's image can live on 2 MB pages (`DK_THP=1`: MAP_HUGETLB). The scatter's
minor faults drop from 21,870 to 4,101 (16:36Z; the 4,101 are DuckDB's own result buffers), but DuckDB's buffers
(its intermediates, ~30 K faults per rep) stay on 4 KB pages: it has no huge-page knob for its allocator. Time:
8 threads 309-321-332 vs 339-362-368 ms (ranges just apart, ~10%), 1 thread 813-832-937 vs 789-827-1142 (within
noise). The page faults that are half of the C relocate are ~5% of DuckDB's: the query (the target-address hash
join, 2 M rows against 5.2 M) is the cost.

## Excerpts

The P2 round (sql/p2loop.sql), what a rule looks like as a statement:

```sql
-- new live symbols: plain targets of new live code, and what called methods keep
CREATE OR REPLACE TEMP TABLE nl AS
SELECT DISTINCT id FROM (SELECT dst AS id FROM out_l WHERE kind = 0 UNION ALL SELECT unnest([mtyp, ifn, tfn]) FROM call)
ANTI JOIN live USING (id) WHERE id <> 0;
INSERT INTO live FROM nl;
```

The lazy decode (sql/p2lazy.sql): only the frontier's relocation lists are unnested and decoded:

```sql
CREATE OR REPLACE TEMP VIEW rel_dl AS SELECT id, o, unnest(rels, recursive := true) FROM rsrc SEMI JOIN dl USING (id);
CREATE OR REPLACE TEMP VIEW out_dl AS
SELECT dst, 0::UTINYINT AS kind FROM decoded('rel_dl')
UNION ALL SELECT unnest(out, recursive := true) FROM adjx JOIN dl ON adjx.src = dl.id;
```

3a's fold (sql/p3a.sql):

```sql
SELECT b, list_reduce(list({'a': align, 'n': n, 'e': []::UBIGINT[]} ORDER BY j),
                      lambda acc, x: {'a': 0, 'n': 0, 'e': list_append(acc.e, (acc.e[-1] + x.a - 1) // x.a * x.a + x.n)},
                      {'a': 0, 'n': 0, 'e': [any_value(start)::UBIGINT]}).e AS e
FROM blk JOIN bucket USING (b) GROUP BY b
```

## Gotchas

- An Arrow stream view is consumed by one scan: dk always does `CREATE TABLE x AS FROM x_v` (`.table`); the copy
  into DuckDB's storage is in "load" (e2e), not in the cells.
- A view is bound when it is created: create the tables it reads first (empty), then `CREATE OR REPLACE` them freely.
- `list_reduce`'s initial value must have the element type, so the accumulator rides in a struct field.
- `POSITIONAL JOIN` works but runs on one thread (567-642 ms for 9.4 M rows at 1 and 8 threads).
- Threads: DuckDB's scheduler has no passive/spin setting (workers spin briefly, then park on a semaphore). The lead's
  load warning (OpenMP spinning barriers losing ~100x) does not show here: 8 threads are faster than 1 in every
  cell (1.3x for 3a, 1.4x for the P2 loop, 3-5x for the big joins of 1a/1b/1c/3b), except the abandoned recursive CTE
  (1,322 tiny iterations: 620 ms on 8 threads vs 340 on 1). The default scheduler was used throughout.
