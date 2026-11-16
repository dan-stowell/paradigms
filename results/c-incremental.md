# A maintained flood in plain C (ATTACKS 5, "what it would take")

**Original repeated-toggle result (qualified by the follow-up below).** A 1-core C program that keeps the previous link's reach state and applies a stored diff updates
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
process. The first update of each direction is listed apart. B->A here follows A->B; it is **not** an update from a freshly flooded B. The follow-up below separates stamp history from cache displacement.

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


## Follow-up (skeptic-inc), 2026-11-16

**Fresh edits are not repeated toggles.** In five new interleaved rounds, edit 2's first A->B update
was **23.581 ms median**, versus **13.550 ms** on repeated toggles. Freshly flooded B->A was
**105.433 ms**, slower than recomputing A (**45.700 ms**). These are reach kernels with indexes,
previous reach, and stored diffs available, not complete incremental relinks. They exclude identity/diff
construction, initialization, oracle checks and cache-displacement work. The earlier blanket claim that
maintenance beats recompute in both directions does not hold for fresh B.

### Chained correctness and fixes

`CHAIN=1` starts from A (`REPS=0`) and keeps the same parent state across **240 random updates per seed**,
for seeds **4, 5, 6**. Each update changes up to 199 edges in each direction, one flag value (toggling
Def/Reflect/GoType/Iface), and one root. It removes both original and overlay edges and can restore removed
original edges. Half the root candidates come from A's original roots, so root removals are exercised.
Every 20 random updates it also applies and reverses a real edit, alternating B1 and B2: **24 real steps per
seed**, **792 checked steps total**. Random changes accumulate around those real patches; no reset or inverse
random update occurs. Randomly edited sources/flags/roots exclude the real patches' fact owners, keeping
both real directions valid set diffs. The two diffs share A's IDs; their added-ID slots are reused only after
the preceding real patch has been reversed, and random edges use A IDs only.

After **every** step a forked child clears derived facts and floods the current graph/flags/roots from scratch,
then compares every live-set slot, including missing and extra IDs. Copy-on-write keeps the parent's stamps,
support cursors, counts and worklists untouched. All **720 random + 72 real updates passed**. This checks
maintenance against a fresh flood sharing its rule implementation; it does not independently prove those
rules. The real A/B oracle checks and separate method-table comparison provide additional independent checks.
Raw progress, coverage counts, timestamps and load are in [c-incremental-chain.txt](c-incremental-chain.txt).

No live-set discrepancy was found. Review of persistent storage did find two real bugs: the fixed 65,536-slot
removed-edge hash retained restored keys and could eventually probe forever; the fixed added-edge pool
appended on every re-add and could overflow. Both now grow dynamically. The minimal regression uses a
four-slot hash with eight distinct remove/restore keys and a two-entry pool with eight add/remove cycles.
The original hash times out (exit 124); the original pool produces an AddressSanitizer heap-buffer-overflow.
Both fixed cases pass, including ordinary builds under the 8 GiB cap. See
[test-p2inc-storage.c](../ref/flat/test-p2inc-storage.c) and [review evidence](c-incremental-review.txt).
The original single-diff `FUZZ` mode remains available; only `CHAIN` establishes maintained multi-edit history.

### Fresh versus repeat measurements

Window **2026-11-16T20:30:29Z–20:31:20Z**, observed one-minute `uptime` load **3.14–4.18**.
All benchmark child processes use `prlimit --as=8589934592` (8 GiB). Five rounds interleave both edits,
fresh-A/fresh-B processes, normal toggles, displaced-cache toggles, and recomputes of each destination.
Each fresh treatment has **five independent processes**; repeat treatments have **20 samples** (four later
round trips in each of five processes). Every update passes its full destination oracle.

| ms, min / median / max | edit 1 A->B | edit 1 B->A | edit 2 A->B | edit 2 B->A |
|---|---|---|---|---|
| First update, fresh derivation state | 0.035 / 0.038 / 0.049 | 0.026 / 0.031 / 0.041 | 20.979 / 23.581 / 33.811 | 99.367 / 105.433 / 116.229 |
| Repeated toggles | 0.002 / 0.009 / 0.020 | 0.004 / 0.012 / 0.022 | 10.582 / 13.550 / 47.584 | 5.737 / 8.101 / 23.076 |
| Repeats after touching 1 GiB | 0.011 / 0.013 / 0.015 | 0.016 / 0.019 / 0.046 | 12.327 / 14.119 / 20.225 | 7.321 / 8.118 / 11.150 |
| p2, destination recompute | 40.300 / 49.300 / 63.000 | 40.600 / 44.100 / 57.900 | 40.400 / 45.200 / 56.400 | 40.100 / 45.700 / 55.600 |
| p2par, 8 threads, best of five floods | 17.300 / 18.000 / 20.600 | 18.200 / 20.800 / 22.700 | 16.700 / 21.500 / 26.200 | 15.800 / 17.000 / 23.600 |

`FRESH_B=1` installs B's edge/flag/root facts directly into A's indexed representation, clears derived state,
and floods B before the first timed B->A. No untimed update primes its retraction buffers. This is fresh
**derivation state over A's CSR plus B's overlay**, not newly indexed compact B CSR. The scratch flood seeds
roots in ascending ID order; A's initial flood uses file order. Stamps depend on traversal order as well as
edit history, so the fresh-B result is specific to this documented initialization. Building A's indexes and
initial A flood precedes this setup, outside the update timer.

| Work per update | fresh retractions / reverse reads | repeat retractions / reverse reads |
|---|---:|---:|
| edit 1 A->B | 1 / 16 | 1 / 14 |
| edit 1 B->A | 6 / 31 | 2 / 7 |
| edit 2 A->B | 32,238 / 3,049,918 | 20,412 / 2,253,023–2,257,483 |
| edit 2 B->A | 35,378 / 49,171,996 | 8,102 / 1,086,132–1,086,145 |

Fresh counts are identical across all five runs. Repeats have the larger reverse-read count on repetition 1
and the smaller count thereafter. Touching one byte per cache line of a **1 GiB** private allocation immediately
before each timed update leaves these work counts **identical** to normal toggles. Thus stamps/support history
unambiguously reduce work; the first/repeat difference cannot be attributed solely to cache warming. The cache
experiment displaces caches rather than flushing them with a hardware instruction. Normal/displaced repeat
time ranges overlap in every direction in this loaded window: their timing difference is **within noise**.
Fresh edit-2 B->A versus serial recompute A has nonoverlapping ranges and loses clearly. Fresh A->B versus
8-thread recompute overlaps; repeated edit-2 comparisons also overlap. No general speed advantage over parallel
recompute follows from this run.

`bench-p2inc.sh` now measures B for A->B and **A for B->A**, for both p2 and p2par, and uses an 8 GiB cap.
The focused runner avoids rebuilding DD and preserves all stdout, setup/flood times, oracle checks and counts
in [raw output](c-incremental-followup-raw.txt); [summary TSV](c-incremental-followup-summary.tsv) also contains
first toggles and single-thread p2par. The old DD results were not remeasured here.

### Method tables

**Neither edit changes method-table rows.** An independent Python comparison reads all six raw method columns,
maps B IDs and names into A's namespace, and compares complete row sets: **106,079 rows** in A, B1 and B2,
**zero removals and additions**. All twelve `diff/meth_*` files per edit are zero bytes. p2inc therefore correctly
keeps A's method indexes for these edits. A fixture with a nonempty method-row diff exits **3** with the explicit
unsupported diagnostic; p2inc does not silently ignore it or return a purported correct result. General method
updates still require support or recomputation, but no method support is needed for these two edits.
See [checker](c-incremental-method-check.py) and [output](c-incremental-method-check.txt).

### Reproduce

Run from `/home/exedev/paradigms-w`; data paths are read-only. All build/test artifacts are local.

```sh
mkdir -p build
export TMPDIR="$PWD/build"
bash ref/flat/check-p2inc-chain.sh > results/c-incremental-chain.txt
# Script runs CHAIN=1 CHAIN_EDIT=/home/exedev/paradigms/data/minio-edit2
# FUZZ=240 SEED={4,5,6} prlimit --as=8589934592 build/p2inc \
#   /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit 0
python3 ref/flat/bench-p2inc-followup.py 5
prlimit --as=8589934592 python3 results/c-incremental-method-check.py build/p2inc
# Minimal storage regressions (old-failure commands are in c-incremental-review.txt):
gcc -O2 -o build/test-p2inc-storage ref/flat/test-p2inc-storage.c
prlimit --as=8589934592 build/test-p2inc-storage hash
prlimit --as=8589934592 build/test-p2inc-storage pool
```
