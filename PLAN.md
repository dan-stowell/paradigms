# PLAN

The user writes the campaign; the agent keeps the Milestones log and the Now line.

## Campaign 1 (2026-11-16): the paradigm grid on MinIO's link

Fill CHARTER.md's grid: four paradigms (Goose, Futhark, Halide, Soufflé) against the C
reference, on problems 1-3, then the stretch problem 4. Chase surprises first.

## Oracle number (report at every milestone)

Cells filled in GRID.md (correct, or a written-up "falls down" after two attempts), out of 20
(5 systems x 4 problems), and the count of entries in ATTACKS.md backed by a measurement.

## Setup

- Reference repos under `/home/exedev/targets/`: `dan-stowell/bottom-up` (branch
  `link-hardware-speed`: hlink, `link/`, `hw/`, FLOOR.md, the floor script, the frozen-object
  scripts) and `dan-stowell/simd-lang` (fclink with `-dumpmap`, SHAPES.md). Clone them through
  the integration; read, don't modify.
- MinIO and kube-apiserver objects built by the stock toolchain, frozen as bottom-up's
  PLAN.md and JOURNAL.md describe. bottom-up's VM is gone, so rebuild them here.
- The flat files: one directory per dataset, columns as raw little-endian arrays plus a
  string table, with a README that says what each file is. Every system reads these; none
  parses Go objects. Write the extractor once (C or Go), and write the oracles (live list,
  resolved table, image checksum per section) next to the data.

## Milestones

M0. Data and oracles: MinIO's flat files, the three oracles, hlink's own time on each phase
    from the same files (or from its stats, mapped to the problems), and the floors. GRID.md's C
    column filled.
M1. Every paradigm installed, with a first correct cell (any problem) on MinIO's data.
M2. Every paradigm has attempted problems 1-3 on MinIO: correct, or "falls down" with a reason.
    The first ATTACKS.md entries.
M3. Second attempts where a cell fell down or was slow; prior-art ideas that survived their kill
    tests measured in a cell; kube-apiserver as the second dataset.
M4. Problem 4 (code generation by shape) in at least two paradigms.

Milestones log:

- 2026-11-16T15:40Z M0: MinIO frozen; flat files + oracles for problems 1-3 (ref/flat/README.md) extracted by hlink -dumpflat; plain 1-core C references reproduce every oracle exactly (ref/flat/check.sh); hlink's phase times on this VM in GRID.md (149 ms whole link, 8 cores); floors from bottom-up FLOOR.md. Oracle number: 0/20 paradigm cells (C column filled), 0 ATTACKS backed.

- 2026-11-16T17:20Z M1 + M2: every paradigm (Goose, Futhark, Halide, Soufflé, and the added DuckDB) is correct on problems
  1-3 against the oracles (lead rebuilt each from source and reran every check); falls-downs written with two attempts
  (Soufflé 3a: no sort; DuckDB P2: 45 rounds x 25 statements; Futhark and Halide: lazy reach slower than eager; Goose:
  one-array 8-worker 3b). Oracle number: **16/20** (4 paradigms x P1-P3 + the C column's 4; P4 0/4 paradigms) **+ DuckDB 3**;
  **3 ATTACKS entries backed by measurement** (lazy decode in the flood; huge-page output; drop level synchronization).

- 2026-11-16T18:40Z M3: second attempts (Soufflé 3a ranked chain: no longer falls down; DuckDB P2 recursive query:
  falls down after 3), level-free floods measured in Futhark and Goose (ATTACKS 3 corrected), prior-art CP1/CP4 measured
  in C, GraphBLAS column (P1, P2), kube-apiserver for every paradigm's P1-P3 cells (all correct). M4: Futhark P4 correct
  (3.2x / ~5x C), tuned C P4 (p4par). Oracle number: **17/20** (+ DuckDB 3, GraphBLAS 2); **4 ATTACKS backed**.

- 2026-11-16T18:55Z M4: problem 4 correct in Futhark (4bf, flat 4D), Halide and Goose, plus tuned C (p4par) and C
  kill tests (CP1 split, CP4 killed, CP7: indirection not pieces). Oracle number: **19/20** (+ DuckDB 3, GraphBLAS 2);
  **4 ATTACKS backed**.

Now (2026-11-16T19:06Z): M4 review triaged. Running: Opus builders (worktrees) Soufflé P4 (systems/souffle/P4-REPORT.md)
and Halide batched-level flood (results/halide-levelfree.md); Codex in /home/exedev/paradigms-c (branch codex-holes:
ref/p4/p4holes.c, results/codex-holes.md, log codex-holes.log) and /home/exedev/paradigms-g (branch codex-goose-fnoff:
Goose P4 checks function offsets, log codex-gfn.log); each log ends with "EXIT n". On each: rebuild + rerun its check
in main, merge, GRID/ATTACKS. Then: (5) a Halide cell on the flattened stream (send to the Halide builder when its
flood is done); (4) quiet reruns of the 8-thread numbers (systems/*/bench.sh one at a time) once no builder runs;
(6) problem 4 on kube-apiserver (ref/p4/extract.sh on kube) as its second dataset.
