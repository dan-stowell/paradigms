# ATTACKS

New attacks on the grid's problems (CHARTER.md). Each entry: the problem, the old shape, the new
attack and where it came from, the evidence, what it would take. Before, observation, fix, number.

## 1. Reach: don't build the graph; decode each reference when the flood reaches it, from columns

**Problem.** Reach (2): the live set from the roots, over relocations plus the method rules.

**Before.** Two shapes. hlink floods straight over the raw objects, decoding each live symbol's relocation records
((package index, symbol index) -> canonical id) on the spot: 98-103 ms on 1 core, 22-23 ms on 8 (MinIO). The textbook
data-parallel shape, and what the flat files hand every paradigm, is "decode everything into a canonical CSR, then
flood": the flood alone is fast (p2par 26-30 / 8.6-17 ms), which made it look as if the flat layout had won.

**Observation.** It hadn't: the flood reads only **28.7%** of the CSR's edges on MinIO (2.39 M of 8.32 M) and **21.0%**
on kube-apiserver (3.27 M of 15.6 M), and a quarter of those repeat a (source, target) pair (580,425 on MinIO counting (kind, target), 821,056 on kube)
(`python3 ref/flat/reachfacts.py data/<dataset>`). Building the graph is 3.5-5x more decoding than reach needs, and
measured end to end (Codex, `ref/flat/p2pipe.c`: decode 9.4 M targets + count/prefix/fill + flood) it loses:
**108.7 ms on 1 thread, 30.1 ms on 8** on MinIO (results/codex-d.md; my rerun 16:04Z: 33.7 ms on 8), slower than hlink.
The CSR build alone (11.7 ms on 8) costs as much as the flood.

**Attack.** Keep the relocations as columns (`rel_pkg`, `rel_idx`, `rel_type`, one contiguous range per symbol,
`slot_rel`) and decode inside the flood: when a symbol becomes live, walk its range, map (pkg, idx) through its
object's package-base table and the canon array, mark. That is hlink's lazy shape on a columnar layout, parallel
by level (`ref/flat/p2lazy.c`, the flood loop of p2par with ~15 lines changed).

**Number.** MinIO: **52.6-53.3 ms on 1 thread, 13.6-15.4 ms on 8** (P2 OK; 16:02-16:04Z, load 0.8, interleaved with
p2par). kube-apiserver: 72.7 / 19.9 ms. vs the eager pipeline 108.7 / 30.1: **2x, reproduced independently** by Codex
(16:04-16:05Z, interleaved, medians of minima: MinIO 53.5 vs 110.8 / 18.2 vs 32.8; kube 75.3 vs 196.7 / 23.5 vs 58.5,
i.e. 2.1-2.6x; reviews/done/20261116T1603-codex-attack1.md). What the 2x covers: the same flood, eager decode+CSR vs decode in
the flood; both exclude file mapping, 1a, aux and kinds 1-3 decode and method tables. **Not a claim about hlink**:
hlink's flood timer does work the flat kernels don't (aux and funcdata/pcdata marking, inline-tree parsing, flRef
marking, itab and method-table parsing; dead.c:301-580) and excludes work they include (frontier merging, methods),
so no fair ratio to hlink follows from these numbers (an earlier draft said 1.9x / 1.5x; withdrawn 16:20Z).

**Caveats.** Both flat pipelines take the funcdata aux edges (704 K on MinIO), the interface kinds 1-3 and the
method tables precomputed from the p2 files, and the 1a canon from its oracle; hlink decodes those inside its
flood too, so the hlink ratio overstates the win by the cost of that work (dumpflat's untuned eager version of
all of it is 133-139 ms; a lazy one should be a fraction). The columnar relocation layout is what the extractor
produced, i.e. a cost hlink does not pay (it reads the object files in place); what the measurement shows is that
the *layout* of the references matters more than pre-decoding them.

**What it would take / next.** (a) Done for aux (codex-e, results/codex-e.md; `build/p2lazy <data> N --aux` decodes the
1.48 M raw funcdata aux records from new `aux_*` columns inside the flood): +6.9 ms MinIO / +9.3 ms kube on 1 thread
(59.9 -> 66.8, 77.2 -> 86.5 best of 5, 16:12Z), so the lazy flood *with its own aux decode* still beats the eager pipeline
*without* one by 1.6x. Kinds 1-3 and method tables remain precomputed; the hlink comparison stays withdrawn. (b) Dedup per source: measured 16:09Z, a deduped CSR (8.32 M -> 5.28 M edges) floods in 23.9-29.1 ms vs 26.2-32.5 on 1 thread (~8%; 8-thread runs swamped at load 6): duplicates hit cache lines already loaded, so not worth a pass. (c) Ask the paradigms: a lazy decode inside a flood is
a gather inside a fixpoint. Futhark/Halide want the edge list materialized (the eager shape that loses here);
Soufflé's rule `live(t) <- live(s), rel(s, r), target(r, t)` evaluated semi-naively is exactly the lazy shape,
if its join on `rel` runs over the columns rather than a B-tree. That is the kill test for each paradigm's P2 cell:
does it beat 108.7 / 30.1 ms (the eager pipeline) when its own input decode is counted?

## 2. Relocate: the output's page faults are half the work (2x on 1 core; 8 threads at 1.1-1.4x the floor)

**Problem.** Relocate (3b): copy 76 MB of symbol bytes into a 109 MB image and patch 2.05 M relocations (MinIO).

**Before.** The plain C reference callocs the image and writes it: 48-68 ms on 1 core (85-115 in noisy runs).
The FLOOR.md floor for hlink's write phase is 4.92 ms on 8 cores.

**Observation.** First-touching the image (memset outside the timed region, `P3_PREFAULT=1 build/p3`) costs 45-64 ms,
and the copy + patch that remains is 31-35 ms: on 4 KB pages, **the kernel zeroing and mapping 26,700 pages is
about half of relocate.** (2026-11-16T16:05Z, load ~1-2.)

**Attack.** Put the output on 2 MB huge pages (53 faults): `P3_THP=1 build/p3` maps the image MAP_HUGETLB (with
`vm.nr_hugepages=512` reserved; this kernel has no transparent huge pages), faults included in the timed region.

**Number.** `P3_REPS=20` (a fresh output mapping per rep, its faults inside the timed region; input warm), three
interleaved runs each, 2026-11-16T16:13Z, load 2.4-3.0, P3 OK:

| | 4 KB pages | 2 MB pages |
|---|---|---|
| 1 thread (`build/p3`) | min 48.6, median 49.3-50.3, max 54-56 ms; 19,109 faults/rep | min 23.9-24.2, median 24.4-24.8, max 25-30 ms; 39 faults/rep |
| 8 threads (`build/p3par`) | min 13.4-14.1, median 14.3-15.1, max 21-29 ms; 19,137 faults | min 5.4-6.0, median 6.1-6.8, max 7-12 ms; 155 faults |

On 8 threads the huge-page relocate is **1.1-1.4x the FLOOR.md floor of 4.92 ms**; on 4 KB pages it is 3x. The ranges
don't overlap at either thread count. The attack is not a new loop: it is the output's page size. Against this VM's measured bandwidth (36 GB/s read, 27 write on 8
threads: 109 MB read + 76 MB written -> ~3-6 ms) the 8-thread huge-page relocate is at the memory bound; nothing is left in the loop.

**No code needed.** The Futhark builder found that `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` (glibc's malloc on the reserved
2 MB pages) gives Futhark 1.2-1.7x across most cells. On the plain C relocate, unchanged `calloc`, 3 interleaved runs
(16:40Z, load 2.6): **24.2-24.7 min / 24.4-26.6 median ms vs 55.0-55.8 / 55.5-58.9 on 4 KB, faults 39 vs 19,109 per
rep**: the same 2x as the explicit MAP_HUGETLB, from an environment variable. (p3's "4 KB pages" label under the
tunable is wrong: it reports the allocation path, not the page size.) Any paradigm whose runtime allocates through
glibc malloc gets the attack for free, given reserved huge pages (`vm.nr_hugepages`).
The tunable also speeds the hash join: p1compact's join on 1 thread 20.5-21.2 vs 30.1-32.3 ms (3 interleaved, 16:40Z)
-- its tables and records are malloc'd and probed at random, so this is TLB reach, not faults. The floods (p2par, p2lazy)
don't move (within noise): their inputs are mmap'd files, which the tunable doesn't touch.

**Note for the grid.** hlink with `vm.nr_hugepages=512` vs `0`, 6 interleaved rounds on 8 cores (16:15Z, load 1.9-2.5):
total 193-267 ms (median ~226) vs 205-249 (~225): **within noise**; write phase 17-56 vs 17-23. The earlier noisy
215-229 vs 250-287 did not reproduce, so GRID's hlink numbers (taken with 0 reserved pages) stand. hlink reuses its
previous output file in place ("overwrote the old output in place, zeroed": create 11 ms), so its output faults are
already mostly paid outside write.

**What it would take.** Quiet 8-thread numbers; then the paradigms' 3b cells should report whether their runtime lets
the output live on huge pages (Futhark's and Halide's buffers, Goose's arenas, DuckDB's blobs), because on this
problem that matters as much as the loop.

## 3. Reach: drop level synchronization where waiting is the cost (C under load, Goose), not where each level is many passes (Futhark)

**Problem.** Reach (2), the same flood as entry 1.

**Before.** Every data-parallel shape of the flood is level-synchronous: expand the frontier, barrier, swap. MinIO's
reach has **111 levels in 7 rounds** (kube 103). p2par (OpenMP) pays one team barrier per level; Futhark pays ~26
whole-array passes per level (45 levels; its lazy flood, 165 / 90 ms, ties its eager flood once eager's own decode is
counted, so it is not a falls-down; corrected after the M3 review); Goose pays a message exchange per superstep and scales only 1.7x on 8 workers; DuckDB iterates 45 SQL rounds.

**Observation.** On this shared 8-core VM the barrier is not a constant: with OpenMP's default spinning wait, p2par's
8-thread flood takes **593-1071 ms instead of 15-27** (MinIO, ambient load from the builders; kube 856-1064 vs 28-103),
because 111 times per solve all threads wait on whichever one was descheduled (codex-g, results/codex-g.md, 16:25-16:28Z;
lead saw the same at 16:20Z). A passive wait policy removes most of it, but not under oversubscription.

**Attack.** An asynchronous worklist flood (`ref/flat/p2async.c`, codex-g): per-thread LIFO stacks, chunk stealing,
termination by credits; the rules are monotone (flags only go 0 -> 1, a push on each transition) so level order is not
needed for the live set. One join per round: **7 phases instead of 111 barriers.**

**Number.** 5 interleaved processes, ms per solve, min / median / max (codex-g):

| MinIO, 8 threads | p2par active | p2par passive | p2async |
|---|---|---|---|
| ambient load | 593 / 953 / 1071 | 15.2 / 21.4 / 26.6 | 16.7 / 19.9 / 30.3 |
| + 8 busy loops | 2079 / 2290 / 3510 | 105.9 / 152.7 / 156.1 | **81.3 / 89.4 / 93.4** |

1 thread: within noise of p2par (34-38 vs 37-44 ms). Under deliberate overload async beats passive by 1.7x with ranges
apart; ambient, within noise. P2 OK on 120/120 separate runs (both datasets, 1/2/8 threads) and in check.sh.

**Scope (after the M2 review).** Measured: in C, overload robustness (async vs passive within noise when lightly loaded,
1.7x under overload). Not measured: that the level count is *the* bottleneck in Futhark, Halide, Goose and DuckDB; the
builders' notes point there (passes, pipeline calls, supersteps, statements per level), but the test is a batched-level or
level-free version of each, counted (passes, barriers, queue operations) and timed against its original. Owed in M3.

**Measured in the paradigms (M3, 17:20-17:55Z, load 17-26: times provisional; [results/levelfree.md](results/levelfree.md)).**
The premise was half wrong. Futhark's flood has 45 levels, not 111 (it checks methods every level), at ~26 parallel
operations each; Goose's p2par already floods each partition to a local fixpoint, so it has 10 supersteps. In
**Futhark the level count is not the cost**: a hybrid (parallel while the frontier is wide, sequential drain when
narrow) cuts 45 levels / 1,211 parallel ops to 15 / 437 and is within noise of the original at 8 threads; a plain
sequential worklist written in Futhark beats both (8 threads 67/102/130 vs 156/201/258 ms). The cost is the passes
inside each level. In **Goose** an async flood (no supersteps, termination by message counts) is 1.3-1.5x the
bulk-synchronous one at 8 workers (33/39/44 vs 48/53/63 ms, ranges apart in one window; lead one-round rerun 32 vs
61); the limit after that is the busiest partition and ~1.04 M ids through the queues. In tuned C the same hybrid
(ref/flat/p2hyb.c) drops 111 barriers to 12 but moves ~44% of pops to one thread (results/levels.md). So:
In **Halide** (M4 review 5; [results/halide-levelfree.md](results/halide-levelfree.md); lead rebuilt and reran 19:30Z, P2 OK every variant): k = 2/4/8 levels per pipeline call cut calls 98 -> 17-58 and are **1.0-1.2x slower** (1 thread 294 vs 348 ms k=4; 8 threads 64 vs 84 ms k=8). A call costs ~0.02 ms (98 calls ~2 ms of 300); each level's passes cost 0.63 ms (1) / 0.23 ms (8) even when the frontier is empty, ~21-30% of the flood, whatever the grouping. Batching had to be forced (a Func cannot read its own state across levels, so an input aliased to the state buffer; loop bounds cannot come from data computed in the same call, so later levels run to a cap). Same verdict as Futhark. dropping level sync pays where waiting on a descheduled worker is the cost (C under load, Goose), not where each
level is many whole-array passes (Futhark); there the fix is fewer passes per level, which no variant here achieved.

**What it says about the paradigms.** None of the grid's data-parallel systems can say "a fixpoint without levels":
Halide has no worklist; Futhark has one only as a sequential loop (in-place updates, no data parallelism), and that loop beats its own parallel flood (8 threads 67/102/130 vs 156/201/258 ms), so in Futhark the fastest flood is the one that drops the paradigm; Soufflé's semi-naive evaluation is level-synchronous by construction; DuckDB's
recursive CTE iterates rounds; Goose has the queues but no shared visited set. The C worklist is ~200 lines of
mechanism (stealing, credits) for ~15 lines of rules. A build farm's machines are shared like this VM, so robustness
under load is part of the speed. **Next:** a quiet-machine run (does async lose anything with nothing else running?),
and whether a level-free flood also cuts Futhark's/Goose's cost (fewer, larger rounds: e.g. expand several levels per
superstep, a "delta-stepping" of the flood).

## 4. Code generation by shape: the emit costs indirection per piece, not pieces or bytes; flatten the stream, merge templates by width

**Problem.** Code generation by shape (4): emit fc's 60.7 MB of MinIO function bodies from 108,833 byte templates,
7.68 M operands and 17.6 MB of glue (ref/p4/README.md).

**Before.** The obvious model, from copy-and-patch compilers (Xu and Kjolstad 2021; ideas/copy-and-patch.md): a
template table plus a patch pass; speed is bandwidth, and the table should be keyed by typed shape (SHAPES.md: 4,096
shapes cover 96.7% of nodes).

**Observation.** (a) The plain C emit runs at **0.42-0.62 GB/s** (1 core, 98-145 ms medians across windows), 40-60x
below this VM's read bandwidth, and patching holes is only **7.6%** of it (codex-p4par, P4_NOPATCH split). (b) It is
not the number of pieces either (codex-cp7, results/codex-cp7.md, 5 interleaved runs, quiet machine, all P4 OK, lead
rerun): precomputing each of the 2.38 M items as a flat blob with its holes rebased (control D, same piece count)
emits in **48.3/48.5/48.9 ms vs p4's 88.9/98.4/98.6** (2x); merging pieces 2.3x (A: hole-free neighbours) or 17.4x
(B: one run per function) then adds only 1.08-1.10x (A 44.0, B 44.7 ms). On 8 threads: p4par 16.1/18.4/22.2, D 13.7,
A 11.9, B 12.3 ms median. **The cost is the indirection per piece** (item -> template -> segment -> hole range), not
the copies. The paradigm cells' top-K curves give the same split from the other side (Halide, Goose: keep
the commonest K templates, the rest as glue): copying all 60.7 MB as 136 K hole-free runs takes 12.8 ms (1 thread) /
4.8 (8); each hole costs ~5.8 ns (8.0 M holes ~ 46 ms) and each template piece ~40 ns. So once the stream is flat, the
**holes** are most of what is left: C's B variant (one run per function) at 44.7 ms is almost exactly its 8 M holes. (c) The typed shape does not fix the bytes (74.6% of nodes in the top 4,096 *byte* templates); one
splitter is purely mechanical: disp8 exactly when the operand fits in int8, **0 exceptions in 4,864,669 holes**.

**Attack.** Two independent steps. (1) Flatten the emit stream: resolve item -> template -> segment -> holes once
(an extraction-time "decode", like ATTACKS 1's lazy decode in reverse: here the eager form wins because the emit is
the hot loop), so the emit is a sequential read of (blob, holes) runs: 2x on 1 core, 1.3x on 8 over p4par. The price
is input size (416 MB of columns vs 217 MB; dedupe of per-function runs, C, finds 55,389 distinct of 136,634, 49 MB).
(2) Key templates with the displacement width relaxed: **108,833 -> 66,046 templates; the top 4,096 cover 82.3% of
nodes instead of 74.6%**, oracle reproduced byte for byte (codex-cp, `ref/p4/check-cp.sh`, lead rerun), but keep each
node's length precomputed (deriving it from operands made the scan 6.3x slower). Specialising hot templates into
straight-line C (Deegen's method) cut instructions 26% and the covered segments' emit 1.67x (median), but ranges
overlap; 2.9 MB of generated C.

**What it says about the paradigms.** Futhark expresses the whole problem in 159 lines (lengths by map, places by
scan, holes as a flat map, rel32 by gather, relocations by expand) at 3.2x C on 1 core; what it had to force is the
copy itself (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x
cheaper than scattering bytes, and the same trick made its 3b relocate 2.7-3.6x faster). The flattened stream (1) is
exactly the shape a data-parallel language wants (no nested lookups), so it should help Futhark and Halide more than C:
**next**, a Futhark or Halide cell reading D's representation.


**Hole follow-up (codex-holes).** Before: B's 8.02 M records emit in 44.495 ms at one core.
Observation: 5.63 M are integers, 0.70 M rel32, and 1.69 M relocations that write no code.
Skipping integer stores while retaining their records takes 41.559 ms. Fix: pre-bake integers
into each function blob and drop their records (H1): **28.757 ms, 1.55x**, nonoverlapping
one-core ranges, but preparation rises from 343.769 to 401.802 ms median. This moves work into
preparation; it is not an end-to-end compiler speedup. H2 merges 3.53 M integer holes into
1.76 M masked stores: 28% fewer patch stores, **41.275 ms**, overlapping B's range (within
noise). H1 removes 89% of patch stores but only 35% of emit time: no proportional scaling.
Eight-core H1/B ranges overlap under load 3.43–13.35. Full bytes/offsets/relocations pass,
three planted faults fail, independent recount/UBSan pass. User-mode perf and the checked
no-store control point to metadata traversal/dispatch and memory traffic, not a 5.8 ns
scalar-store cost. Counts, five interleaved rounds, preparation and counter scope:
[results/codex-holes.md](results/codex-holes.md).

## 5. Relink: after a small edit, maintain the answer instead of recomputing it; first make symbol identity stable

**Problem.** Resolve (1) and reach (2) after a small source edit: every build today relinks from scratch.

**Old shape.** A from-scratch join and flood over ~10 M facts on every link (C: p2 52 ms 1 core, p2par 17-20 ms on 8;
p1 287 / p1par 22 ms).

**Where the attack came from.** The Differential Dataflow column (added 2026-11-16, IDEAS wildcard 3), built to ask a
question no other column can: how much of a relink's work does a small edit change? Two real edits of MinIO, rebuilt
with the frozen toolchain and re-extracted ([results/ddflow-incremental.md](results/ddflow-incremental.md)).

**Observation 1: the edit is tiny, the ids are not.** Edit 1 (a 5-line fallback added in one function) changes the
facts of **31 symbols out of 9.7 M facts** (3.2e-6) and **3 live ids** of 663,630; edit 2 (16 lines deleted: the console
UI start-up) changes 141 symbols and 234 edges but kills **12,320 live ids** (21.5 MB of image). But slot ids are
positional (object base + index), so 3 added slots in object 109 renumber every slot in objects 110-1420: a raw diff
is ~all facts. Recovering identity (pair objects, match by unique name/content key, Myers-align the unnamed DWARF
remainder) takes **0.7-1.1 s**, outside the update times below. A "lowest id" tie-break churns 34 K resolve facts in
edit 2 for 73 real changes.

**Observation 2: an additive edit updates 35-400x faster than recompute; a retracting one does not.** DD, lead rebuilt
and reran 19:55Z (edit 1, 1 worker: update 1.33-1.40 ms, P2 OK on both versions and from scratch). Bench 19:40Z, load
4.4-5.6, medians: reach update edit 1 **1.4 ms (1 worker) vs C p2 52 / p2par 31 (1 thread), 9.0 (8 workers) vs p2par
20.5**; resolve update edit 1 **0.7 ms vs p1 287 / p1par 22**. Edit 2: reach update **65 ms (1) / 555 ms (8): slower than
C recompute**; retracting 12 K reachable ids withdraws each derivation round by round through 45 loop rounds. DD's own
full run is 60-100x C (2.7 s / 1.6 s) and holds 2.7 GB of arrangements.

**Observation 3: in plain C, maintenance wins both edits, retractions included, if deletions use derivation
stamps** ([results/c-incremental.md](results/c-incremental.md), `ref/flat/p2inc.c`, 1 core, reading the stored diff
`data/minio-edit{,2}/diff/`). Bench 20:12Z, load 0.7-1.7, 5 interleaved rounds, medians: edit 1 update **3 µs** (DD
1.4 ms, p2 45 ms, p2par 17 ms on 8); edit 2 update **9.5 ms A->B / 5.2 ms B->A** (p2 43, p2par 14 on 8, DD 59 on 1
worker / 29 on 8). Deletion is DRed (delete, rederive) where a fact survives the delete step if it has a derivation
from a true fact with a smaller stamp (the clock when it was set): edit 2 retracts 32 K ids for 12 K real deaths;
textbook DRed retracts 594 K and takes 123 ms (3x recompute). Checked on both edits and directions and by 300
random diffs per edit against a from-scratch flood (the fuzzer found two bugs the edits did not: a method kept by a
sig count whose remaining sources were its own targets, and a lost re-check across removals in one update). The
same quiet window corrects DD's 8-worker edit-2 update: 29 ms, not 555 (that was load).

**Attack.** (1) Give symbols a stable identity at extraction: (package path, name) or content hash, per-object
order only as a last resort, and tie-breaks by name hash, not position; then the input diff of an edit is the edit's
size (31 symbols), and nothing pays 0.7-1.1 s to find it. (2) Keep reach and resolve as maintained collections
(semi-naive deltas, a grouped reduce per name key): additions propagate in µs (C) to ~1 ms (DD). (3) For deletions,
DRed with derivation stamps (Observation 3): exact on cycles, where per-symbol support counts are not, and its cone
stays near the real deaths. The hybrid "maintain on additions, recompute on removals" is unnecessary, and would
recompute on every real edit (each removes the edited function's old GC-locals edges).

**What it would take.** A stable-id extraction (hlink -dumpflat keyed by name, or a mapping table written once; today
`ddflow writediff` writes the mapping and diff once as flat files). The maintained flood is done (`p2inc.c`, ~700
lines with checks and fuzzer); its edit-2 cost is now 3 M reverse-list re-checks of popular targets (a per-id count
of smaller-stamp live predecessors would make each O(1)). Not measured yet: resolve maintained in C, 1c, layout and
relocate after an edit (layout shifts every address after the edited function, so 3a/3b are mostly recompute unless
sections are padded per package).

**What it says about the paradigms.** None of the five original columns can state "the answer after this change";
Datalog engines have the same semi-naive deltas inside one evaluation but discard them. DD expresses the whole
reach as 71 lines and the update as two `feed` calls, but it is not a from-scratch engine (60-100x C), its updates get
slower with 8 workers on a loaded machine (on a quiet one, edit 2: 29 ms on 8 vs 59 on 1), and identity is something it assumes, not something it provides.
