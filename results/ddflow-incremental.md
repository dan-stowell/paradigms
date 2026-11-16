# Differential Dataflow: incremental relink of a real edit (MinIO)

The question this column adds to the grid: **after a small source edit, how much of a relink's work must be
redone?** Answered here for problem 2 (reach) and problem 1a+1b (resolve), with two real edits of MinIO rebuilt
with the frozen toolchain, and Differential Dataflow (DD 0.26.0 / timely 0.32.0) keeping the result up to date.
Code: `systems/ddflow/` (`src/main.rs` reach + driver, `src/p1.rs` resolve, `src/facts.rs` loading, id unification,
diff). Timings: `systems/ddflow/bench.sh` (interleaved with ref/flat/p2, p2par, p1, p1par),
`bench-20261116T1940Z.txt` (the numbers below; load 4.4-5.6) and `bench-20261116T1930Z.txt` (earlier run, load
2.8-5.3, P2 only, same picture).

## The two edits (real source edits, rebuilt, re-extracted)

Both start from a `cp -r` of `/home/exedev/targets/minio` (unchanged), are built by bottom-up's
`link/freeze.sh` (`GOTOOLCHAIN=local` go1.27.4, `go build -a -trimpath`, the stock procedure) into
`/home/exedev/targets/frozen-edit/<name>`, and dumped by `ref/flat/extract.sh` (hlink `-dumpflat`) to
`/home/exedev/paradigms/data/<name>`. `ref/flat/p2` and `p1` pass on both new dumps against their own oracles.

| | edit 1: `minio-edit` (add a call) | edit 2: `minio-edit2` (delete a call) |
|---|---|---|
| source change | `internal/amztime/iso8601_time.go`: `ISO8601Parse` falls back to `http.ParseTime` (5 lines, `systems/ddflow/edit.patch`) | `cmd/server-main.go`: the console UI start-up block deleted (16 lines, `edit2.patch`) |
| rebuild | 2 min 3 s (`freeze.sh`, nice 15) | 1 min 7 s |
| archives that changed bytes | 4 of 1,311 + main.a (amztime, its importers cmd, lifecycle, object/lock: export fingerprints) | cmd + main.a |
| objects whose symbol lists differ | 1 (amztime) | 1 (cmd) |
| nslots / P2 edges / live | +3 / +30 / **+1** (663,631) | -37 / -48 / **-12,310** (651,320) |
| image | +128 B of types | **-21.5 MB** (the console, go-openapi, swagger, websocket... die) |

`http.ParseTime` was inlined into `ISO8601Parse`, so edit 1 adds no call edge to it: its new plain edges are to
`net/http.timeFormats` (already live) and new GC-locals symbols.

## Ids: why a unification step is needed first

Slot ids are positional (`object base + index`). Edit 1 adds 3 slots to object 109 of 1,421, so every slot in
objects 110..1420 changes id; a raw fact diff would be ~all facts. `facts::unify` maps version B's ids into A's:

1. objects pair by index (both dumps are in importcfg order, sorted by package path; counts must agree);
2. an object whose slot keys (kind, name, 128-bit name key, 128-bit content hash) are identical maps by position
   (1,420 of 1,421 objects in both edits);
3. in a differing object, a key that occurs exactly once on each side (a named symbol, a content hash) is the
   same symbol wherever it moved; the rest (unnamed DWARF range/location symbols, duplicate names) is aligned in
   order with a Myers diff, with the size/rank bits as part of the key for unnamed slots;
4. linker-made ids map by position; method-name strings by content; unmatched B slots get fresh ids.

Unify takes 0.7-1.1 s (reading the name and key columns of both dumps). The mapping choice changes the *fact*
diff a little (edit 1: 9 plain edges with ordinal keys, 27 with the final rank-keyed alignment) but not the live
diff. A first try that keyed unnamed slots by ordinal inside the object gave edit 2 a spurious 81,882-edge diff
(every unnamed DWARF symbol after the deleted block shifted); the alignment brings it to 234.

## Diff sizes (the finding in itself)

Problem 2 facts (edges by kind, flag bytes, method tables, roots), A has 9,741,684:

| | edit 1 | edit 2 |
|---|---|---|
| slots only in A / only in B | 14 / 17 | 81 / 44 |
| edges - / + | 11 / 16 | 135 / 99 |
| flag facts - / + | 14 / 16 | 73 / 44 |
| method-table facts, roots | 0, 0 | 0, 0 |
| **symbols with changed facts** | **31** (3.2e-6 of facts) | **141** |
| **live ids changed** | **-1 +2** of 663,630 | **-12,320 +10** (1.9%) |

Edit 1's live change is one GC-locals symbol replaced by two. Edit 2's 12,320 dead ids are the console
(github.com/minio/console/api 869, madmin-go 846, models 457, go-openapi 1,000+, websocket 331, type name data
2,237, ...); its +10 are 7 closures that the compiler renumbered (`serverMain.func15.2` was `func15.3`: a rename
looks like delete + add to name-keyed identity) and 3 etcd closures whose inlining changed. Full lists:
`results/ddflow-inc-minio-edit.log`, `results/ddflow-inc-minio-edit2.log`.

Problem 1a+1b facts (named and candidate slots with their keys and ranks, other slots, relocations with raw
targets), A has 14,609,633:

| | edit 1 | edit 2 |
|---|---|---|
| named-slot facts - / + | 18 / 19 | **34,499 / 34,497** |
| other slots, candidates | 14/16, 1/1 | 79/44, 0/0 |
| relocations - / + | 19 / 52 | 192 / 148 |
| canonical ids changed | -14 +17 | -73 +44 |
| relocation targets changed | -19 +52 | -171 +127 |

The 34,497 in edit 2 is churn from one representation choice: the winner's tie-break "lowest id" is kept exact by
ordering on (object, index in object) instead of the global id, so other objects growing does not matter, but
every named slot in `cmd` after the deleted code shifts its index. Only 73 canonical ids actually change
(IDEAS.md 2: a tie-break that does not use the position would remove the churn).

## Checks (every run)

`ddflow inc A B` checks: the live set after full A equals A's oracle; after the update A->B it equals B's oracle
mapped through the id map; after B->A it equals A's again; a second dataflow run from scratch on B's facts (in
unified ids, same process) equals the updated set exactly and equals B's oracle. `ddflow p1inc A B` checks
canon and every relocation target, A and after the update, against both versions' p1 oracles (5.2 M canon entries,
9.3 M nonzero targets). All pass at 1 and 8 workers for both edits (`grep -c 'P2 OK\|P1 OK'` in the bench output;
the bench aborts on any failure).

## Times (ms, min / median / max, 5 interleaved rounds, 2026-11-16T19:39-19:49Z, load 4.4-5.6)

Updates are 5 per process in each direction (25 per cell; P1: 10 per process, 50 per cell); full runs are one per
process. "Recompute" for C is the warm flood only (inputs mapped, CSR given), DD's full is warm too (facts in
memory, arrangements built inside the timing).

### Problem 2

| | 1 core / worker | 8 |
|---|---|---|
| C p2 recompute (flood, warm) | 41.7 / 56.5 / 92.6 (edit 1), 46.1 / 48.8 / 52.1 (edit 2) | |
| C p2par recompute (best of 5) | 27.7 / 30.8 / 118.4; 24.8 / 31.2 / 34.2 | 12.7 / 20.5 / 30.1; 16.2 / 19.4 / 34.9 |
| DD full A | 2447 / 2703 / 3839 | 1389 / 1621 / 2118 |
| DD full B from scratch | 2105 / 2143 / 3310 | 491 / 1628 / 1943 |
| **DD update, edit 1** (A->B; B->A) | **1.3 / 1.4 / 3.4; 1.3 / 1.4 / 1.9** | 1.8 / 9.0 / 81.9; 1.8 / 4.3 / 71.6 |
| **DD update, edit 2** | **54.9 / 65.3 / 113; 50.0 / 64.7 / 118** | 28.0 / 555 / 688; 28.2 / 526 / 607 |

- Edit 1: the update is **~1,900x** DD's own full run, **35-40x faster than the single-core C recompute** and ~20x
  p2par on 1 thread; on 8 cores it is a tie to ~2x faster than p2par's 8-thread recompute (medians 4.3-9.0 vs
  20.5) with a long tail.
- Edit 2 (12 K ids die): the update is ~40x DD's full but **slower than recomputing in C** (65 vs 49 single
  core, 19 on 8 threads). Retracting reachability is DD's expensive case: every dead id's derivation is withdrawn
  round by round through the loop (45 rounds), with the arrangements' history of every round.
- **Corrected 20:12Z (results/c-incremental.md): on a quiet machine (load 0.7-1.7) edit 2 updates in 29 ms on 8
  workers vs 59 on 1, edit 1 in 2.1 vs 1.4; the slowdown below is load.** At load 4.4-5.6, 8 workers made updates *slower* (edit 2: 526-555 median vs 65 on 1 worker): per-round coordination of a
  small, many-round update. The 28 ms minimum shows what it costs when the machine is quiet for a moment.

### Problem 1a+1b

| | 1 | 8 |
|---|---|---|
| C p1 (join 1a + targets 1b, warm) | 260.9 / 287.0 / 563.3 | p1par: 19.9 / 22.0 / 43.7 |
| DD full A (1a+1b) | 8659 / 9310 / 10823 | 1285 / 2023 / 3203 |
| **DD update, edit 1** | **0.6 / 0.7 / 1.5** | 0.5 / 29.3 / 195 |
| **DD update, edit 2** | **70.2 / 87.2 / 122** | 43.9 / 64.2 / 237 |

Edit 1 resolve update is **~400x faster than single-core C recompute and ~30x faster than p1par on 8 threads**.
Edit 2's update (35 K changed facts, mostly the tie-break churn) is ~3x faster than C single-core, ~3-4x slower than
p1par.

## What P1 (resolve) needs to be incremental

Measured above, so this is what had to be true for it to work:

1. **Symbol identity that does not move.** Positional ids shift every later object on any slot-count change.
   The flat files give enough (name, name key, content hash, per-object order) to rebuild identity by matching;
   a linker designed for incrementality would key symbols by (package, name) or content from the start and
   would use the importcfg's package path for objects.
2. **The by-name join keyed by strings, as a grouped reduce.** 1a is `reduce` over the 128-bit name key: a group
   is recomputed only if one of its members changed (edit 1: ~20 groups). DD does this directly
   (`members.reduce(max)`, 23 lines of dataflow for 1a+1b in `src/p1.rs`).
3. **Tie-breaks that are not positional.** "Lowest id" had to become "lowest (object, index in object)" to stay
   exact; that still churns inside the edited object (edit 2: 34 K facts). Ordering ties by something stable
   (name hash) would make the update proportional to the edit.
4. **Relocation targets as (package, index) or name, not base + index.** 1b reads `obj_pkg` bases; the raw
   target `base + idx` was mapped into the stable id space at load. Expressed as a join of (relocation, raw
   target) with canon, 1b updates only changed relocations and relocations whose target's canon changed
   (edit 1: 71 of 9.3 M).
5. **1c (content-hash join) depends on P2's marks**, so an incremental 1c needs the flood's output as an input
   collection, which DD can wire as one dataflow (not done here).

## Memory

Peak RSS (`/usr/bin/time %M`): `p2` 1.35-1.43 GB, `inc` (two versions' facts + two dataflows) 2.7 GB, `p1inc`
3.5 GB, all under `prlimit --as=12G`.
