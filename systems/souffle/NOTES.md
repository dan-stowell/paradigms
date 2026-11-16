# Soufflé column: install, hello-world, timings, gotchas

Measured 2026-11-16 15:27 UTC (`date -u`), Ubuntu 24.04.5, 8 cores, g++ 13.3. Everything reproducible with
`hello/build.sh` then `hello/run.sh`.

## Install (done)

```
mkdir -p ~/tools/souffle-dl && cd ~/tools/souffle-dl
curl -sLO https://github.com/souffle-lang/souffle/releases/download/2.5/x86_64-ubuntu-2404-souffle-2.5-Linux.deb
sudo apt-get install -y ./x86_64-ubuntu-2404-souffle-2.5-Linux.deb     # deps: g++, libffi-dev, libncurses5-dev, libsqlite3-dev, mcpp, zlib1g-dev
```
`souffle --version`: **2.5** (latest release as of today; options enabled: ffi openmp ncurses sqlite zlib).
Headers in /usr/include/souffle (used for the embedding build). Installed system-wide in /usr (not ~/tools/souffle).
Verified: interpreter (`souffle -j8 reach.dl`), compiled (`souffle -o prog -j8`, then `./prog -j1|-j8`),
and `souffle -g x.cpp` code generation + own `main` (embedding).

## Hello world: reach from 100 roots

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

### Fastest fact-input path
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

## Gotchas
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
