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

## 3. Reach: the level count is the cost; drop level synchronization

**Problem.** Reach (2), the same flood as entry 1.

**Before.** Every data-parallel shape of the flood is level-synchronous: expand the frontier, barrier, swap. MinIO's
reach has **111 levels in 7 rounds** (kube 103). p2par (OpenMP) pays one team barrier per level; Futhark pays ~25
whole-array passes per level (its lazy flood "falls down": 165 / 90 ms, slower than its own eager flood, NOTES.md);
Goose pays a message exchange per superstep and scales only 1.7x on 8 workers; DuckDB iterates 45 SQL rounds.

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

**What it says about the paradigms.** None of the grid's data-parallel systems can say "a fixpoint without levels":
Futhark and Halide have no worklist; Soufflé's semi-naive evaluation is level-synchronous by construction; DuckDB's
recursive CTE iterates rounds; Goose has the queues but no shared visited set. The C worklist is ~200 lines of
mechanism (stealing, credits) for ~15 lines of rules. A build farm's machines are shared like this VM, so robustness
under load is part of the speed. **Next:** a quiet-machine run (does async lose anything with nothing else running?),
and whether a level-free flood also cuts Futhark's/Goose's cost (fewer, larger rounds: e.g. expand several levels per
superstep, a "delta-stepping" of the flood).
