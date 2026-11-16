# A maintained flood in plain C (ATTACKS 5, "what it would take")

**Result.** A 1-core C program that keeps the previous link's reach state and applies a stored diff updates
MinIO's live set in **3 µs for edit 1** (add a call) and **5-10 ms for edit 2** (delete a call: 12,320 ids die),
against 43-45 ms for recomputing with `ref/flat/p2`, 14-17 ms for `p2par` on 8 threads, and 1.4 ms / 29-58 ms for
Differential Dataflow's update. Every update was checked: B's oracle after A->B, A's after B->A, five times per
process, plus 300 random diffs per edit checked against a from-scratch flood. The deletion strategy matters more
than anything else: textbook DRed takes 120-130 ms for edit 2, about 3x a recompute.

## (1) The diff as flat files

`ddflow writediff /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit` (and `minio-edit2`)
writes `data/<edit>/diff/` with its own README.md: `idmap_b.u32` (B id -> A's id space, new slots after `nids_a`),
`strmap_b.u32`, edges by kind (`edge_{minus,plus}_{src.u32,kind.u8,val.u64}`), flags
(`flag_{minus,plus}_{id.u32,val.u8}`), roots, method-table rows (empty for both edits), `live_b.u32` (B's oracle in
unified ids) and `meta.txt`. Edit 1: edges -11 +16, flags -14 +16. Edit 2: edges -135 +99, flags -73 +44. The
mapping is described in results/ddflow-incremental.md. Any system can apply these files; nothing here depends on
Rust.

## (2) `ref/flat/p2inc.c`

    gcc -O2 -march=native -o build/p2inc ref/flat/p2inc.c
    prlimit --as=12884901888 build/p2inc /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit2 5          # -> P2INC OK
    FUZZ=300 prlimit --as=12884901888 build/p2inc /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit2 1 # -> fuzz: 300 random diffs, 0 failed
    ref/flat/bench-p2inc.sh 5                                                                                                  # the table below

What the program does:

- **State kept between links.** Setup takes 365 ms and is not timed: a deduplicated CSR, the reverse CSR of
  kind-0/1 edges, and method indexes by (name, mtyp), by name, by target and by exported. Then a flood of A with
  the same worklist rules (60-90 ms, checked against A's oracle) produces this state:
  - live, iface, typeDone (live + iface + type descriptor, with its type edges and methods applied) and called,
    per method;
  - counts of live sources for sig, named and reflect;
  - a **stamp** per fact: the clock value when the fact was set. Every fact is therefore derived from facts with
    smaller stamps.
- **Overlay graph.** A's CSR stays as it is. A hash set holds the A edges that were removed, and per-source and
  per-target lists hold the added edges. Updates are applied to the overlay.
- **Additions.** These use the same worklist as the flood, so only new targets are pushed. The method and iface
  rules are part of it: a sig count going 0 -> 1 wakes the methods keyed by that sig, and a type becoming
  live + iface activates its type edges and methods.
- **Deletions: DRed (delete and rederive), refined by stamps.**
  1. All removed edges leave the graph first. The fuzzer found that interleaving removals and retractions loses a
     re-check.
  2. Their consequences are retracted transitively. A fact is **kept** if it still has a derivation from a true
     fact with a smaller stamp: a plain or useiface predecessor, a root, a flag, a type edge from an active type,
     or a called method. A call is kept if some live source of its condition has a smaller stamp than the call.
  3. Every retraction re-checks what it supported. So whatever is kept has a chain of decreasing stamps down to
     roots and flags, which means it is derivable. That is the induction the proof rests on.
  4. Everything retracted is then **rederived** if it has any derivation from true facts (one reverse lookup
     each), and the worklist restores what follows.
  5. A per-id resume pointer keeps re-checks cheap. Predecessors before the last support found can never support
     that id again while it keeps its stamp, because a retracted predecessor comes back only with a larger stamp.
- **Why not the alternatives:**
  - Per-id support counts are wrong on cycles. Live code is full of them: a function and its closure keep each
    other alive.
  - The aggregate rule has the same trap, and the fuzzer found it. A sig's remaining live sources can be the
    method's own targets. So "sig count > 0" does not justify keeping a call; a source with a smaller stamp does.
  - "Recompute whenever a live edge is removed" always recomputes. Every real edit removes live edges, because the
    edited function's GC-locals symbols are renamed (edit 1 removes 2).
  - Textbook DRed (`plain`) over-deletes 594,148 of 663,630 live ids for edit 2: everything downstream of a
    retracted sig, named or reflect source. That is 120-130 ms.
- **OpenMP: not attempted.** The edit-1 update is 3 µs. Edit 2's is 5-10 ms of dependent support checks on a
  worklist of ~30 K retractions, already below p2par's 8-thread recompute (14 ms). A level-synchronous parallel
  version pays a barrier per level; p2par needs 111 of them.

## Times (ms, min / median / max, 5 interleaved rounds, 2026-11-16T20:12:47-20:14:57Z, load 0.7-1.7: a quiet window)

`ref/flat/bench-p2inc.sh`, raw output in `results/c-incremental-bench.txt`. p2inc runs 5 updates per direction per
process. The first update of a process is listed apart: it touches the state's pages for the first time.

| | edit 1 A->B | edit 1 B->A | edit 2 A->B | edit 2 B->A |
|---|---|---|---|---|
| **p2inc** (stamps), first update | 0.036 / 0.038 / 0.047 | 0.005 / 0.005 / 0.006 | 18.0 / 18.7 / 21.1 | 5.2 / 5.5 / 7.4 |
| **p2inc** (stamps), later updates | **0.002 / 0.003 / 0.004** | 0.003 / 0.004 / 0.006 | **9.2 / 9.5 / 16.6** | **5.0 / 5.2 / 10.1** |
| p2inc plain DRed | 0.002 / 0.003 / 0.011 | 0.004 / 0.005 / 0.011 | 116 / 123 / 130 | 115 / 121 / 137 |
| DD update, 1 worker | 1.36 / 1.43 / 1.81 | 1.34 / 1.38 / 2.73 | 54.2 / 58.6 / 83.9 | 50.4 / 57.0 / 79.3 |
| DD update, 8 workers | 1.79 / 2.10 / 3.62 | 1.72 / 2.06 / 3.17 | 25.4 / 29.1 / 80.5 | 23.5 / 30.3 / 56.0 |
| C p2 recompute (1 core) | 40.7 / 44.9 / 48.7 | | 35.7 / 42.6 / 51.1 | |
| C p2par recompute, 1 thread | 26.5 / 27.6 / 29.7 | | 25.6 / 27.6 / 28.1 | |
| C p2par recompute, 8 threads | 15.5 / 17.3 / 19.8 | | 13.0 / 14.2 / 18.3 | |

Medians:
- **Edit 1:** p2inc is ~15,000x faster than p2 and ~500x faster than DD.
- **Edit 2:** p2inc is 4.5x faster than p2 (A->B) to 8x (B->A), 1.5-2.7x faster than p2par on 8 threads, and 3-6x
  faster than DD on 8 workers.

**Correction to the DD column:** on this quiet machine DD's 8-worker update for edit 2 is 29 ms, not the 526-555 ms
measured at load 4.4-5.6 (bench-20261116T1940Z). The slowdown on 8 workers is a symptom of a loaded machine
(most likely workers waiting for descheduled peers; not profiled), not a property of DD on an idle machine.

## The mechanism, counted (one update, first round)

| | over-deleted live ids | rederived directly | ids set again or newly | out-edges read | reverse entries read | support checks |
|---|---|---|---|---|---|---|
| edit 1 A->B (stamps) | 1 | 2 | 2 | 0 | 16 | 2 |
| edit 1 B->A (stamps) | 2 | 1 | 1 | 0 | 7 | 3 |
| edit 2 A->B (stamps) | 32,238 | 5,050 | 19,928 | 176,298 | 3,049,918 | 369,484 |
| edit 2 B->A (stamps) | 8,112 | 13 | 20,422 | 100,280 | 1,088,061 | 98,502 |
| edit 2 A->B (plain DRed) | 594,148 | 137,584 | 581,838 | 3,890,503 | 4,621,812 | 0 |
| recompute (p2inc's own flood of A) | | | 663,630 | 2,197,091 | | |

Edit 2 A->B retracts 32 K ids for 12,320 real deaths: the stamps cut textbook DRed's cone 18x. Reverse reads
(3.0 M) are now the cost. They are re-checks of popular targets, such as runtime functions with tens of thousands
of predecessors, each time one of their predecessors dies. A per-id count of "live predecessors with a smaller
stamp" would make a re-check O(1). That is the next step if 9.5 ms matters.

## Checks

- Each p2inc run checks the live set after every update against B's oracle (A->B) or A's (B->A): 5 + 5 per process,
  20 processes in the bench. The bench aborts on `P2INC OK` missing.
- `FUZZ=300` on each edit, stamp mode: 300 random diffs on top of B, each with up to 200 removed edges from live
  sources and up to 200 added edges of all four kinds with random or existing targets. Each is checked against a
  from-scratch flood of the same overlay graph, then reverted and checked against B's oracle: 0 failures. Plain
  mode: `FUZZ=30`, 0 failures.
- The fuzzer found two real bugs that the edits alone did not show, both fixed before these numbers. One: a call
  kept by a sig count whose remaining sources were the call's own targets. Two: a removed edge whose source was
  retracted by an earlier removal in the same update, so its target was never re-checked.

## What it changes in ATTACKS 5

- Maintenance in plain C beats recompute for both edits, including the retracting one, once deletions use
  derivation stamps. The "hybrid: maintain on additions, recompute on removals" policy is unnecessary, and would
  recompute on every real edit anyway.
- DD's engine is not needed for the speed. Its value is the 71-line statement of the rules.
- The 0.7-1.1 s id unification remains the dominant cost of an incremental relink. Stable ids at extraction are
  still step one.
