# Soufflé column: problems 1-3 on MinIO

Soufflé 2.5, compiled mode (`souffle -g` + g++ -O3), every cell correct against its oracle. Inputs come in through
`flatio.h` (C++, I/O only): build.sh rewrites the generated reader/writer calls so binary flat-file columns are
mmap'd, turned into tuples in parallel, sorted per index and bulk-loaded into Soufflé's B-trees (`btree_set::load`).
The rules are plain Datalog; nothing in C computes an answer, except the 3b image assembly (copying bytes; Soufflé
has no bytes) and the untimed P2-lazy setup that p2lazy.c also does untimed.

## Surprises

- **Lazy decode is expressible and wins against Soufflé's own eager shape, but not against C (ATTACKS 1).** The rule
  `plain(s, t) :- live(s), source(s), rel(s, p, i, ty), plainty(ty), slotobj(s, o), objpkg(o, p, b), canon(b + i, t)`
  is evaluated semi-naively, so only live sources' relocations are decoded (1.73 M `plain` tuples of 9.4 M
  relocations). Warm: 2.9-3.2 s on 1 thread, 0.96-1.6 s on 8, vs 1b's eager decode 5.2-5.4 / 1.08-1.30 s plus the
  flood over the pre-decoded CSR 1.23-1.47 / 0.43-0.58 s. The C pipelines: eager 108.7 / 30.1 ms, lazy 52.6 /
  13.6-15.4 ms. So Soufflé is ~30x / ~35-50x the *eager* C pipeline and does not approach the lazy one. And the
  laziness stops at the rules: the engine joins only over B-trees, so the 9.4 M relocation columns are still loaded
  into a B-tree index first (6.15 M distinct tuples, ~1.5 s of the 2.2 s load at 1 thread): Soufflé materializes the
  *input* even when the rules don't materialize the graph. e2e: lazy 4.6-5.0 s / 2.3-3.1 s vs eager 1b + 2 =
  7.4 + 2.6 s / 2.9 + 1.3 s.
- **The output's page size is irrelevant to the Soufflé 3b cell (ATTACKS 2).** The image lives in flatio's writer
  (C), where MAP_HUGETLB works: assembly 53.5-96.9 ms (19,108 minor faults) on 4 KB pages vs 20.0-34.7 ms (39 faults)
  on 2 MB, 1 thread; 18.3-31.6 vs 7.8-16.2 ms (150-172 faults) on 8 (FLATIO_IMG_REPS=20, three interleaved runs each,
  16:29-16:31Z, load 8.7-11). But the rules computing the 2.05 M patch values take 1.05-1.12 s / 0.32-0.45 s: the
  faults are 2-5% of the cell. Soufflé's own runtime (B-trees from `new`) has no page-size knob; this kernel has no
  THP.
- **8 threads with OpenMP's default (spinning) wait policy lost 10-25x under load**: reach_lazy -j8 took 38 s vs
  4.2 s at -j1 (16:17Z, load ~11). With `OMP_WAIT_POLICY=passive` it is 1.5 s. Every 8-thread number below is passive.
- **A sum over "the earlier members of my group" is fast when the group key is an index prefix**: Soufflé turns
  `y = sum o : { occ(b, c, u, j, o), j < i }` into a range scan of a (b, c, u, j) index; layout's 327 K non-typelink
  addresses (Σ n²/2 = 108 M tuples visited) take 56 ms on 8 threads. Comparing *symbols* (strings) in the same shape
  is 80x slower per pair: 4.6 s for the 33 K typelinks.

## Timings

`bench.sh` (5 interleaved rounds, min / median / max ms), 2026-11-16T16:37Z, load 2.9-6.2; full output in
`bench-20261116T1637Z.txt`. warm = flatio's "compute" (rules only; inputs already in the B-trees, output writing
excluded). e2e = process start to exit. 8 threads: `OMP_WAIT_POLICY=passive`. Each warm run is one evaluation (the
generated main runs once); the warm times are >= 0.3 s, most >= 1 s.

| cell | 1 thread warm | 1 thread e2e | 8 threads warm | 8 threads e2e | plain C 1 core / tuned C 8 |
|---|---|---|---|---|---|
| 1a join by name (resolve_a) | 1928 / 1996 / 2034 | 3136 / 3206 / 3295 | 636 / 680 / 754 | 1722 / 1736 / 1888 | 194-203 / 8.6-10.5 |
| 1b relocation targets (resolve_b) | 5220 / 5263 / 5426 | 7371 / 7542 / 7657 | 1080 / 1206 / 1301 | 2669 / 2875 / 2972 | 43-44 / 5.3-6.3 |
| 1c content-hash join (resolve_c) | 1625 / 1664 / 1865 | 2805 / 2879 / 3127 | 587 / 615 / 718 | 1566 / 1610 / 1761 | 57-61 / 7.2-8.9 |
| 1c, rules over the slot columns (resolve_c2) | 1013 / 1055 / 1069 | 3086 / 3184 / 3256 | 394 / 408 / 437 | 1936 / 2015 / 2107 | |
| 2 reach (reach, pre-decoded CSR) | 1227 / 1317 / 1467 | 2571 / 2713 / 2921 | 431 / 462 / 579 | 1242 / 1294 / 1429 | 39-43 / 13-17 |
| 2' reach, lazy decode (reach_lazy) | 2905 / 3124 / 3214 | 4637 / 4797 / 4982 | 960 / 1069 / 1610 | 2312 / 2478 / 3081 | 52.6 / 13.6-15.4 (p2lazy) |
| 3a layout (layout) | 7222 / 7337 / 7589 | 7367 / 7480 / 7717 | 1339 / 1625 / 1784 | 1474 / 1774 / 1933 | 8-19 / - |
| 3b relocate (relocate; rules) | 1054 / 1079 / 1115 | 2086 / 2109 / 2139 | 320 / 366 / 451 | 1205 / 1302 / 1374 | 59-67 / 13.4-29 |
| 3b image assembly (flatio, C, 4 KB pages) | 54.2 / 54.4 / 55.9 | | 18.0 / 26.1 / 28.5 | | |

Orders of magnitude: resolve 10-120x the plain C on 1 core (1b worst) and 70-200x the tuned C on 8; reach 30x / 30x; layout
~500x / ~100x the plain C; relocate's rules ~17x / ~25x (only the C assembly is near C).

## Checks (each must print nothing / P3 OK)

```
cd systems/souffle; D=/home/exedev/paradigms/data/minio; for p in resolve_a resolve_b resolve_c resolve_c2 reach layout relocate reach_lazy; do ./build.sh $p.dl; done
gcc -O2 -o build/check_p3 check_p3.c; gcc -O2 -o build/prep_lazy prep_lazy.c; build/prep_lazy $D build/lazyfacts
build/resolve_a -j8 -F $D -D out && cmp out/p1_canon.u32 $D/oracle/p1_canon.u32 && cmp out/p1_builtin.u32 $D/oracle/p1_builtin.u32
build/resolve_b -j8 -F $D -D out && cmp out/p1_reltarget.u32 $D/oracle/p1_reltarget.u32
build/resolve_c -j8 -F $D -D out && cmp out/p1c_canon.u32 $D/oracle/p1c_canon.u32      # also resolve_c2
build/reach -j8 -F $D -D out && cmp out/p2_live.u32 $D/oracle/p2_live.u32
build/reach_lazy -j8 -F build/lazyfacts -D out && cmp out/p2_live.u32 $D/oracle/p2_live.u32
build/layout -j8 -F $D -D out && build/check_p3 $D $D/oracle/p3_image.bin out/p3_addr.u32   # 3a
build/relocate -j8 -F $D -D out && build/check_p3 $D out/p3_image.out                       # 3b
```
`bench.sh` runs all of these each round and complains on stderr if one fails.

## Per cell

Lines: rules + declarations, without comments (the file's total in brackets). Shared: flatio.h 385 lines of C++
(I/O only), build.sh 19.

**1a join by name** (resolve_a.dl, 37 [51]). Easy: groups, "a kind-1 candidate joins only an existing group"
(a join with `group`), winner = `max rank` then `min id` aggregates, the debug-reference rule as negation.
Forced: none. Impossible: none. The 128-bit key is two columns.

**1b relocation targets** (resolve_b.dl, 25 [35]). Easy: the package-table gather is a join on (object, entry);
the special package indices are arithmetic. Forced: `npkg` is a count aggregate per object; "no target -> 0" needs
a helper relation and negation. Slow: 9.4 M relocations x 4 joins, each a B-tree probe and an insert (5.2 s / 1.1 s).

**1c content-hash join** (resolve_c.dl 26 [42]; resolve_c2.dl 23 [31]). Easy: the winner order as aggregates
(`max size` among reached copies, then `min id`). Forced: aggregates run once per tuple of the body before them, so
each group must first be its own relation (`group2(k1) :- ...` then `size2(k1, s) :- group2(k1), s = max ...`);
written as `copy2(k1, _, 1, _), s = max ...` it ran once per copy. Two shapes: copy relations (resolve_c), or the
rules straight over one 5-column `slot` relation (resolve_c2: 37% less compute, but more indexes to bulk-load, so
e2e is a wash).

**2 reach** (reach.dl 47 [60]; reach_literal.dl 39 [50], README's rules written literally). Easy: every rule of
deadcode.go is one Datalog rule, including the conditional method rule; mutual recursion of live/iface/sig/cand is
just a stratum. Forced: joining the static parts (type-to-type edges, a type's methods) once before the flood.
Semi-naive evaluation inserts every live tuple into `live`, `delta_live` and `new_live` B-trees per iteration.

**2' reach, lazy decode** (reach_lazy.dl 65 [84], plus prep_lazy.c 67, untimed, as p2lazy's setup). See Surprises.
Easy: the decode is three rules (ordinary package / special package / builtin), body-joined with `live(s)`.
Found and checked: hlink's walk skips the two relocations after an R_METHODOFF (`r += 2`); on MinIO all 225,760
skipped ones are themselves R_METHODOFF (prep_lazy checks this), so "plain" is a filter on the type and needs no
walk. Forced: the aux edges (funcdata) come pre-split from p2_edge, as in p2lazy.

**3a layout** (layout.dl 45 [66]). Datalog has no scan or sort. Forced: (1) every running sum is an aggregate over
the earlier members of a group (`sum o : { occ(b, c, u, j, o), j < i }`, a range scan, O(n²) per group but fast,
56 ms); (2) the walk over units is a recursion, one unit per iteration (1,763 iterations, ~0.25 s); (3) typelinks:
the sort by type string is "the sum of the sizes of the typelinks before me", O(n²) comparisons. Attempt 1 compared
symbols (Soufflé compares symbols as strings): 28 s / 5.0 s. Attempt 2 adds an 8-byte big-endian prefix of each
type string as a number (loader column `p3_str.key8`) so most pairs are a numeric range: 7.2 s / 1.3 s, still 95% of
the cell. Also forced: the typelink round-up only commutes with the sum because every MinIO typelink is class 3 with
a size that is a multiple of 8 (checked by the rule `tlcheck`, empty). Verdict: correct but ~500x C: **falls down on
speed** (two attempts; the O(n²) "rank by counting" is what Datalog without sort has).

**3b relocate** (relocate.dl 31 [40]). Easy: each relocation kind is one or two rules; the section base is a `max`
aggregate over 8 values; errors are a relation. Forced: the image itself (bytes) is assembled by flatio's writer in
C (`image=` directive): copy each symbol's bytes, write each patch's low bytes. Attempt 1 materialized `site`
(rel x addr x target address): 5.2 s / 1.5 s; attempt 2 inlines it (`.decl site(...) inline`): 2.5 s / 0.74 s at
load 11 (1.05 / 0.32 s at load 3).

## Code excerpts

1c, the content-hash join straight over the slot columns (resolve_c2.dl):
```
group2(k1) :- slot(_, 2, 1, k1, _).
size2(k1, s) :- group2(k1), s = max ss : { slot(_, 2, 1, k1, ss) }.
win2(k1, w, s) :- size2(k1, s), w = min g : { slot(g, 2, 1, k1, s) }.
moved(g, w) :- win2(k1, w, _), slot(g, 2, 1, k1, _), g != w.
moved(g, w) :- win2(k1, w, ws), slot(g, 2, 2, k1, s), s <= ws.
final(g, c) :- canon(g, c), !moved(g, _).
```
2', decoding a relocation when the flood reaches its symbol (reach_lazy.dl):
```
plain(s, t) :- live(s), source(s), rel(s, p, i, ty), plainty(ty), p < 2147483643,
               slotobj(s, o), objpkg(o, p, b), b != 0xFFFFFFFF, canon(b + i, t), t != 0.
live(t)    :- plain(_, t).
iface(t)   :- iface(s), gotype(s), plain(s, t), gotype(t).
```
3a, a symbol's address (layout.dl):
```
va(b, u + 1, x + n) :- ubase(b, u, x), ub(u, b, _, n), meta("nunits", nu), u + 1 < nu.
cbase(b, c, u, x + y) :- ubc(u, b, c, _), ubase(b, u, x), y = sum z : { ubc(u, b, c2, z), c2 > c }.
addr(i, x + y) :- occ(b, c, u, i, _), cbase(b, c, u, x), y = sum o : { occ(b, c, u, j, o), j < i }.
```

## Files

`*.dl` the cells; `flatio.h` the binary column reader/writer (inputs `IO=flat cols=... [csr=] [index=|pos=]`,
`.str`/`.key8` string-table columns, `meta=true`; outputs `cols=`, `image=`, `exitafter=`); `build.sh`;
`bench.sh`; `check_p3.c`; `prep_lazy.c`; `best.sh` (older single-cell timer).

## Install, hello-world, timings, gotchas (installer notes)

Measured 2026-11-16 15:27 UTC (`date -u`), Ubuntu 24.04.5, 8 cores, g++ 13.3. Everything reproducible with
`hello/build.sh` then `hello/run.sh`.

### Install (done)

```
mkdir -p ~/tools/souffle-dl && cd ~/tools/souffle-dl
curl -sLO https://github.com/souffle-lang/souffle/releases/download/2.5/x86_64-ubuntu-2404-souffle-2.5-Linux.deb
sudo apt-get install -y ./x86_64-ubuntu-2404-souffle-2.5-Linux.deb     # deps: g++, libffi-dev, libncurses5-dev, libsqlite3-dev, mcpp, zlib1g-dev
```
`souffle --version`: **2.5** (latest release as of today; options enabled: ffi openmp ncurses sqlite zlib).
Headers in /usr/include/souffle (used for the embedding build). Installed system-wide in /usr (not ~/tools/souffle).
Verified: interpreter (`souffle -j8 reach.dl`), compiled (`souffle -o prog -j8`, then `./prog -j1|-j8`),
and `souffle -g x.cpp` code generation + own `main` (embedding).

### Hello world: reach from 100 roots

`hello/gen.cpp`: 2 M nodes, 10 M uniformly random edges (seed 42), 100 random roots. Writes `edge.bin`
(80 MB, u32 pairs), `edge.facts` (149 MB TSV), `root.facts`. Rules (`reach.dl`):
`reach(x):-root(x). reach(y):-reach(x),edge(x,y).` + count. Result **1,985,956** reached (all variants agree).
After set-dedup the relation has 9,999,989 distinct edges.

Timings, wall clock, ms (one run each, noise ~5%):

| path | load | eval | total |
|---|---|---|---|
| interpreter, TSV, -j8 | | | 10,117 |
| compiled, TSV `.input IO=file`, -j1 | 9,527 (load-only program) | ~4,340 (diff) | 13,863 |
| compiled, TSV, -j8 | 9,365 | ~1,310 (diff) | 10,679 |
| embedded, `Relation::insert(tuple)` public API, serial | 4,812 | 2,717 | 7,529 |
| embedded, btree insert w/ per-thread hint ctx, 8 thr | 919 | 2,999 | 3,918 |
| embedded, sort pairs + `btree_set::load` bulk, 1 thr | 943 | 2,507 | 3,450 |
| **embedded, parallel sort + `btree_set::load`, 8 thr** | **446** | 2,636 | **3,082** |

"diff" = reach program minus load-only program; includes the extra index build and is only indicative.
Embedded numbers are in-process phase timers (mmap of edge.bin is ~5 ms). Hardware-style context: the raw
80 MB read is ~1 ms at DRAM speed, so even the best path is ~450x its read floor on load and a 10 M-edge
BFS taking 2.5 s is ~4 M edges/s, orders of magnitude under the 0.74 G random loads/s the brief quotes.

#### Fastest fact-input path
1. TSV `.input IO=file` is single-threaded text parsing: ~9.4 s for 10 M rows (~1 us/row), and the same at -j8.
   Dominates everything. (Not tried: IO=sqlite, gz, splitting TSV into N relations. SQLite is strictly more work.)
2. C++ embedding API: `souffle -g prog.cpp prog.dl`, then own main with `#include "prog.cpp"` and
   `-D__EMBEDDED_SOUFFLE__` (skips the generated main). Public `Relation::insert` = 4.8 s (virtual call + tuple
   copy + cold btree insert), not much better than text.
3. Fastest: bypass the wrapper. `#define private public` before including the generated file exposes
   `rel_edge_<hash>->ind_0` (a `btree_set<Tuple<u32,2>>`); build a sorted unique `vector<Tuple>` from the
   mmap'd binary (parallel `__gnu_parallel::sort` on packed u64 keys) and `ind_0 = btree_set::load(begin,end)`
   (linear-time bulk build, BTree.h). 446 ms at 8 threads, and the resulting tree is dense, so eval is faster too
   (2.3-2.6 s vs 2.7-3.1 s for random-order insertion). Needs a one-index relation; with several indexes
   (auto-index selection) each `ind_k` must be loaded in its own sort order. Fragile: hash suffixes in member
   names (`rel_edge_04d25f5060e9043b`) change if the .dl changes; grep after `souffle -g`.
4. Not tried: a custom IO via functor / `IO=` plugin; and a functor-based edge lookup (`@edges(x)`) would
   skip the relation entirely but gives up Soufflé's joins.

### Gotchas
- `souffle -g` drops input relations with no `.input` and no rules (warning "No rules/facts defined"); keep a
  dummy `.input ... IO=file` and call `runAll("", "", /*performIO=*/false, false)` to skip file IO.
- `count` yields signed `number`; declaring the result `unsigned` is a type error.
- `.input` without `-F` looks in cwd; `-D-` prints outputs to stdout.
- **-j8 barely helps reachability**: semi-naive BFS runs one round per frontier level; small frontiers do not
  parallelise, and the embedded eval is 2.6 s at both -j1 and -j8. Loading (parallel insert) is what scales.
  GDlog paper observes the same: tuple insertion is 77.8 % of Soufflé runtime on REACH, 16->32 threads gives 7 %.
- Semi-naive keeps `reach`, `delta_reach`, `new_reach` as separate btrees and re-inserts every discovered
  node; a bitmap flood would touch each edge once. See IDEAS.md.
- Compile time: ~10 s per generated program with g++; interpreter has no compile step but is similar here (10.1 s,
  all of it load).
- Output relation `total` must be read via `getRelation("total")` iterator when performIO is off.
