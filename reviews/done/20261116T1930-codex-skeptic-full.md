Reading additional input from stdin...
OpenAI Codex v0.169.0
--------
workdir: /home/exedev/paradigms-w
model: gpt-6-astra
provider: exe-llm
approval: never
sandbox: read-only
reasoning effort: none
reasoning summaries: none
session id: 01a1e5d6-c67f-7c80-87ac-b68b4652ef7f
--------
user
You are reviewing the paradigms repository (current directory). Read CHARTER.md, PLAN.md,
STATUS.md, GRID.md, ATTACKS.md and the tail of JOURNAL.md. Do not edit anything.

Answer briefly and concretely, citing files and numbers:
1. Which cell in GRID.md is claimed correct, but its oracle doesn't actually check the output? Run
   the check command if you can.
2. Which "falls down" verdict is premature: the attempt was a toy, ignored the system's own idioms
   or tuning (schedules, indexes, backends), or blamed the paradigm for a plumbing problem?
3. Which timing compares unlike things (warm against cold, a different input, work left out)?
4. What surprise is being ignored: a result that doesn't fit the expected story and wasn't chased?
5. Which ATTACKS.md entry isn't backed by a measurement, and what would measure it?
6. Quote any recent JOURNAL.md passage that declines or shrinks work because of time, usage or a
   session ending.
Skip praise.
codex
I’ll read the review files, trace the claims to their checks and measurements, and run the relevant check if the environment permits.
exec
/bin/bash -lc "pwd && rg --files -g 'CHARTER.md' -g 'PLAN.md' -g 'STATUS.md' -g 'GRID.md' -g 'ATTACKS.md' -g 'JOURNAL.md' -g 'CLAUDE.md' -g 'AGENTS.md'" in /home/exedev/paradigms-w
 succeeded in 0ms:
/home/exedev/paradigms-w
ATTACKS.md
PLAN.md
JOURNAL.md
STATUS.md
GRID.md
CHARTER.md

exec
/bin/bash -lc 'cat CHARTER.md PLAN.md STATUS.md GRID.md ATTACKS.md; tail -n 220 JOURNAL.md' in /home/exedev/paradigms-w
 succeeded in 0ms:
# CHARTER

Only the user edits this file. Agents read it at the start of every session. The current
campaign is in `PLAN.md`; the procedure is in `LOOP.md`.

*Draft of 2026-11-16, written for the user to edit.*

## North star

Find new attacks on the compute-heavy parts of building a program, by taking paradigms that
already run at hardware speed elsewhere (graphics, databases, numerical computing) and making
them do a linker's and a code generator's work. The output is knowledge, not a tool: for each
paradigm, what it makes easy, what it makes hard, where it falls down, and what new way of
shaping these problems it suggests.

The sibling projects found what the work is. `dan-stowell/simd-lang` built a whole-program Go
compiler (`fc`) and wrote up what made it fast in `SHAPES.md`: read that first. The work is
symbols, types and references, a graph keyed by name; it is bound by memory, not
instructions; the wins were joins, floods, scans, deterministic reduce-by-key, arenas, and
storing answers on the thing instead of in maps. `dan-stowell/bottom-up` (branch
`link-hardware-speed`) built `hlink`, a C linker at 4-5% of its computed hardware floor, with
`FLOOR.md` and the frozen-object setup. Neither project is to be improved here; they are the
reference and the source of data.

## The grid

Paradigms (columns), each a different axis:

| system | axis |
|---|---|
| **Goose** (https://aardappel.github.io/goose/) | memory: data stacks freed at scope exit, narrow typed links, inline nested data, threads with no shared memory |
| **Futhark** | data-parallel by default: map, scan, reduce, scatter, no loops |
| **Halide** | the algorithm written apart from its schedule |
| **Soufflé** (Datalog) | dataflow: rules over relations, compiled to parallel joins |
| **C (hlink)** | the reference: hand-written, plus the hardware floor |

Problems (rows), all from linking MinIO with the stock Go toolchain's objects, so the data is
real, the oracles exist and the floor is computed:

1. **Resolve**: match every symbol reference to its definition (by name, by package index,
   by content hash for deduplicated symbols). A join.
2. **Reach**: the live set from the entry points, following relocations and the method rules.
   A flood. Oracle: fclink's live list (360,376 symbols).
3. **Lay out and relocate**: addresses for the live symbols, then every relocation patched in
   the output image. A scan, then a scatter. Oracle: hlink's image and `go tool link`'s.
4. **Stretch: code generation by shape.** Group MinIO's expressions by shape (4,096 node
   shapes cover 96.7% of 1.4 M; SHAPES.md), emit each shape from a template, patch operands.
   Oracle: fc's emitted bytes on a subset, or a program that runs.

Each problem's input is extracted once into flat files that every system reads, so the grid
compares paradigms, not object-file parsers. kube-apiserver is the second dataset, once MinIO
works.

## What a cell records

`GRID.md` holds one cell per (system, problem):

- **correct?** against the oracle, with the command that checks it
- **time**: 1 core and 8 cores, warm (data in memory) and end to end (reading the flat files),
  next to hlink's phase time and the floor from bottom-up's `FLOOR.md`
- **size**: lines of code
- **what was easy, what had to be forced, what was impossible**, one line each

`systems/<name>/NOTES.md` holds the long form, including an excerpt of the code so a reader
can judge how it reads. Legibility counts: the user wants to work directly with simple ideas,
not with templates, pragmas and intrinsics.

## Room for surprise

The grid is a starting frame, not a cage. Leave room to be surprised in both directions.

- **A surprising result jumps the queue.** A paradigm beating hlink, a paradigm failing for a
  reason nobody predicted, a problem that turns out to be the wrong question: chase it before
  filling routine cells, and put it at the top of STATUS.md.
- **Negative results are results**, and often the most useful ones. A cell is "falls down"
  only after two serious attempts, each using the system's own idioms and tuning (Halide's
  schedules, Soufflé's indexes and parallel mode, Futhark's backends), with the reason written
  down. A toy that proves nothing is not an attempt.
- **Add a column when the evidence points at one.** If prior-art reading or a cell's failure
  points at a system not listed (Differential Dataflow, BQN, DuckDB, ISPC, Taichi, a research
  prototype from a paper), add it and say why in JOURNAL.md.
- **Abuse a paradigm if that's where the idea is.** Using Halide on a graph or Datalog for
  layout is fair, and the place it strains is a finding.

## Measuring on a noisy machine

The VM shares its host with other VMs, and the run's own builders compile in the background,
so a single millisecond timing means little. Don't trust one, and don't stall on noise either.

- **Speed here is about orders of magnitude.** The grid asks whether a paradigm is at 2x, 10x or
  100x of the reference and the floor, and why. A 5% difference is not a finding; don't spend
  effort resolving it.
- **Make the work meaty.** Repeat a kernel inside one process until a run takes at least a
  second, and report time per iteration; or use the bigger dataset (kube-apiserver), or scale
  the data up (MinIO's data several times over, with renumbered IDs). Then report the per-unit
  number next to the floor.
- **Repeat and report the range.** At least five runs, interleaved with whatever it's compared
  against, in the same window. Report min, median and max. A difference counts only when the
  ranges don't overlap; otherwise write "within noise" and move on.
- **Count the mechanism when you can.** Bytes touched, passes over the data, cache misses
  (`perf stat` if it works here), instructions. A count explains a time and doesn't drift.
- **Never wait for a quiet machine.** Note the load (`uptime`) next to a measurement. Pin a
  benchmark to cores the builders aren't using (`taskset`) if the comparison needs it, or take
  the repeats. A measurement left as "to redo when quiet" goes in the Now line once, not as a
  blocker.

## ATTACKS.md

The deliverable that matters most. Each entry: the problem, the old shape, the new attack and
where it came from (a cell, a paper, a failure), the evidence, and what it would take. Written
for the user in SHAPES.md's concrete style: before, observation, fix, number. Also note when an
existing paradigm already offers what the user wants (static guarantees, no loops, no
allocations, data parallel by default) and when none does.

## Prior art

Read papers and research code, not only documentation: many ideas live in PDFs and
researchware. Sonnet subagents do the reading (one per paradigm, one per problem, one wildcard
that looks for systems not listed) and write each idea to `IDEAS.md` with a source link and a
kill test that the grid can run. An idea from a paper is a hypothesis until a cell measures it.

## How to work

- **Never stop to ask.** Pick the most ambitious step you can justify, write why in
  `JOURNAL.md`, and do it. Decisions that are the user's go in STATUS.md; keep working.
- **Horizon.** This runs for days. A session ending is not an event; the next one continues
  from `PLAN.md`. Time left, usage left and "the next session" are never reasons to decline or
  shrink a step. The only valid reason is a measurement.
- **Calibrate on your own record.** In bottom-up, an agent took a C linker from nothing to
  agreeing with the stock linker on all of the standard library and MinIO's tests in about an
  hour. Installing a toolchain and getting a first correct cell is an hour, not a day.
- **Commit and push often** with real messages. Report absolute numbers (ms, GB/s) and
  timestamps from `date -u`, measured as in "Measuring on a noisy machine".

## Anti-goals

- Improving `fc`, `fclink` or `hlink`.
- Benchmark-harness polish and frameworks.
- Toy implementations counted as results.
- Declining or shrinking work because of time left, usage, or a session ending.
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

- 2026-11-16T19:33Z Grid full: Soufflé P4 correct (emit2.dl, ~70x / ~130x C; lead rebuilt, P4 OK on MinIO and kube).
  Every P4 cell correct on kube-apiserver too. Halide batched-level flood measured (1.0-1.2x slower: ATTACKS 3 holds
  in Halide as in Futhark); hole cost split (baking integer holes 1.5x; store merging within noise). New column:
  Differential Dataflow (incremental relink, builder running). Oracle number: **20/20** (+ DuckDB 3, GraphBLAS 2);
  **4 ATTACKS backed** (3 and 4 extended).

Now (2026-11-16T19:00Z): Running: Opus builders (worktrees) Soufflé P4 (systems/souffle/P4-REPORT.md) and Halide
batched-level flood (results/halide-levelfree.md); Sonnet installer for a new column, Differential Dataflow
(systems/ddflow, incremental relink); Codex in /home/exedev/paradigms-c (branch codex-holes, results/codex-holes.md, log
codex-holes.log) and /home/exedev/paradigms-k (branch codex-kube-p4: every P4 cell on data/kube-p4, results/codex-kube-p4.md,
log codex-kp4.log); each log ends with "EXIT n". Done: Goose P4 offset check merged; kube-apiserver P4 data extracted
(ref/p4/README.md). On each finish: rebuild + rerun its check in main, merge, GRID/ATTACKS. Then: a Halide cell on
the flattened P4 stream (to the Halide builder after its flood); DD's incremental relink as a real cell (P1+P2 under
an edit: which symbols change between two MinIO builds); quiet reruns of the 8-thread numbers once no builder runs.
# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T18:55Z. **M4 done** (problem 4 correct in Futhark, Halide, Goose + tuned C). Oracle number: **19/20 cells**
(+ DuckDB 3, GraphBLAS 2), **4 ATTACKS backed**. Only Soufflé P4 is missing.
Problem 4 is the paradigms' best row: Goose ties OpenMP C at 8 workers, Halide 1.3x, Futhark 1.9x on a flattened stream.
ATTACKS 4 (corrected same hour): emit costs lookups per piece, not bytes or pieces; flattening = 2x C, 4.5x Futhark;
after that the 8 M holes are the cost. Width-relaxed templates: 108,833 -> 66,046.
Next: Soufflé P4 (20/20), a Halide batched-level flood (M3 review), hole-cost attack (fewer holes per byte), quiet reruns.

## Decisions for Dan
- None. (vm.nr_hugepages is 1536 now; not persistent across a reboot.)
# GRID

One cell per (system, problem): correct? (oracle command) · time 1 core / 8 cores, warm / end to end ·
lines · easy / forced / impossible. Dataset: MinIO unless marked. Data contract and rules: `ref/flat/README.md`.

All times are wall ms on this VM (AMD EPYC 9554P under KVM, 8 cores), best of >= 3 runs, load < 1 unless marked.

## Reference: C

Two references, measured 2026-11-16T15:33Z (load 0.4):

- **hlink** (bottom-up's tuned C linker, `ref/hlink` = bottom-up `774e37c`), from its own `-stats` and `HLINK_TSC=1`
  stage clocks, `ref/hlink/bench.sh minio` -> `results/minio.hlink.tsv`. It reads raw Go objects, so its phases
  include work the flat files have done in advance (name hashing, decoding references): it is the speed target,
  not a like-for-like row. Whole link: 537 ms on 1 core, **149 ms on 8 cores** (process, warm).
- **plain C on the flat files** (`ref/flat/p1.c p2.c p3.c`, single-threaded, untuned, one hash table or one loop
  per rule): the like-for-like baseline every system is compared with first. `ref/flat/check.sh` runs them and
  checks every oracle.

| problem | floor (8 cores, FLOOR.md model) | hlink 1 core / 8 cores | plain C, flat, 1 core warm | tuned C, flat (p1par/p2par), 1 / 8 threads | check |
|---|---|---|---|---|---|
| 1a+1b. resolve: join by name, relocation targets | 1.36 (resolve) | 125 / 24 (resolve phase, 15:56Z) | 194-203 (join) + 43-44 (9.4 M targets) | 74-77 / **13.9-16.8** (join 8.6-10.5 + targets 5.3-6.3); p1compact join 28.1-30.7 / 6.5-9.9 vs p1par 41.7-48.7 / 9.5-12.9 same window (16:34Z, load 3-5) | `build/p1 data/minio`, `build/p1par` -> P1 OK |
| 1c. content-hash join | (in resolve) | 37-39 / 8-9 (inside deadcode) | 57-61 (radix-partitioned, p1c.h; was 300-309 with a 400 MB table) | 36.9 / **7.2-8.9** | same |
| 2. reach | 2.78 | 98-103 / 22-23 floods + 7 / 1.5 methods | 39-43 | 27-30 / **13.0-17.0** | `build/p2 data/minio`, `build/p2par` -> P2 OK |
| 2'. reach incl. decoding relocations | | (= the row above: hlink decodes lazily) | dumpflat's untuned eager loop 133-139 + flood 39-43 | eager decode + CSR + flood (p2pipe): 108.7 / 30.1; **lazy decode in the flood (p2lazy): 52.6 / 13.6-15.4** | `build/p2pipe`, `build/p2lazy` -> P2 OK |
| 3a. lay out | 0.60 | 85 / 23-25 (layout phase, includes pclntab sizing and the typelink sort) | 8-19 | p3a_par **4.90–5.41 / 3.66–7.56** (medians 5.00 / 3.89; load 4.1–4.4); kube 7.62–8.09 / 5.41–7.62; [five-run report](results/codex-i.md) | `build-i/p3a_par data/minio` -> addr 0 differ; both datasets, 1/8 threads |
| 3b. relocate | 4.92 (write) | 102-104 / 15 (write phase, includes pclntab and GC bitmaps) | 59-67 (76 MB, 2.05 M relocations); P3_REPS=20 16:13Z: 48.6-56 on 4 KB pages, **23.9-30** on 2 MB pages | p3par 8 threads: 13.4-29 on 4 KB, **5.4-12 (median 6.1-6.8) on 2 MB** (ATTACKS 2) | same; `build/p3 data/minio --pipeline` runs 3a -> 3b from its own addresses -> P3 pipeline OK |
| 4. codegen by shape: emit 136,634 bodies (60.7 MB) from 108,833 byte templates + 7.68 M operands | | fc's tree walker (no separate phase time) | 13-17 (lay out 2.38 M pieces) + 139-147 (emit), load 3.5; lead rerun 16:05Z: 18.4 + 162.4 | p4par (codex 17:27Z, load ~20, 5 interleaved, min/med/max): 1 thread layout 7.8/7.9/10.4 + emit 107/118/150; **8 threads 2.9/4.3/7.8 + 51/66/68** (vs p4 116/145/174 same window). Patching is 7.6% of 1-thread emit: the copy of 2.38 M ~25-byte pieces at ~0.6 GB/s is the cost (per-item, not bandwidth). [results/codex-p4par.md](results/codex-p4par.md). Quiet rerun (codex-cp7, 18:30Z): p4 88.9/98.4/98.6, p4par 8 threads **16.1/18.4/22.2**; flattened stream (p4runs D) 48.5 (1) / 13.7 (8), one run per function (B) 44.7 / 12.3 ([results/codex-cp7.md](results/codex-cp7.md), ATTACKS 4) | `ref/p4/check.sh` -> P4 OK (`ref/p4/README.md`) |

**Floors vs this VM.** FLOOR.md's floors imply ~87 GB/s (minio resolve: 119 MB in 1.36 ms); this VM streams **25 GB/s read on
1 thread, 36 GB/s read / 27 GB/s write on 8** (16:16Z, load 5, a sum and a fill over 1 GB). Floors here are ~2.4x lower than the
column says. Per-kernel bandwidth bound for flat 3b: 76 MB symbol bytes + ~33 MB relocations read, 76 MB written -> ~3-6 ms on
8 threads, which p3par on 2 MB pages (5.4-6.8 median) meets.

Tuned and plain flat rows re-measured 2026-11-16T15:56Z at load 0.8, 3 interleaved rounds (`ref/flat/bench.sh`); hlink's resolve/deadcode from `COLD=0 ref/hlink/bench.sh minio build/hlink 3` right after.

**kube-apiserver** (second dataset, extracted 2026-11-16T15:55Z, 2.2 GB of flat files, 13.27 M slots): every plain and
tuned C reference reproduces its oracles (`ref/flat/check.sh data/kube-apiserver`, loaded machine): p1 434 + 88 ms,
p1c 550 ms, p2 66.5 ms (926,486 live), p3a 30.4 ms, p3b 67.8 ms (72.6 MB, 2.82 M relocations); p1par 26.2 ms and
p2par 19.6 ms on 8 threads. Its oracles come from hlink alone so far (MinIO's live list was also checked against fclink).

**Paradigm cells on kube-apiserver** (codex-kube, 17:21-18:16Z, merged 18:30Z; [results/codex-kube.md](results/codex-kube.md)): every P1-P3 cell of Goose, Futhark, Halide, Soufflé and DuckDB, unchanged, reproduces kube's oracles at 1 and 8 threads (lead spot-checked Halide P1 and Goose P2 at 18:28Z). Load was 17-26 for most of the window, so 8-thread kube/MinIO ratios (2-10x) mostly measure the load; the 1-thread ratios, 1.5-2.5x, are at or below the data's growth (kube has **2.55x** MinIO's slots, 2.65x its named symbols, 2.1x the relocations, 1.4x the live set and 1.2x the placed symbols; an earlier version of this line said "1.4x the ids": wrong). Halide 1a's 6.7x was the load: rerun interleaved at load 2.6 (18:33Z, 1 thread pinned, 3 rounds): MinIO 119-126 ms, kube 320-350, **2.6x, linear in slots** (M3 review finding 4).

Floors are bottom-up's FLOOR.md model (same CPU model, the 2026-11-13 VM); they count the raw object bytes hlink
reads, so for the flat files they are an upper bound on the floor.

## The paradigms

**Caveat (M3 review finding 3).** Warm numbers for Goose P1 (213 ms first run vs 79-115 later) and Futhark 3b reuse
buffers whose pages are already mapped, while the plain C references include their first-touch faults; those warm
cells flatter the paradigm by up to ~2x. The e2e numbers in each system's CELLS.md include the faults.

Added columns (M3): **GraphBLAS** (SuiteSparse 10.5.2, JIT on; lead rebuilt + reran 18:24Z, P1/P2 OK, all files `cmp`). 8 threads warm, min/med/max, load 7-10, tuned C same window: **P2 reach 179/186/213** (p2par 15/20/26: 9x); 1 thread 192-203, i.e. it does not scale (frontier ~7 K is below SuiteSparse's per-call threading threshold: 379 of 382 vxm calls on one thread); 178 synchronous steps (97 BFS + 81 iface levels) in 7 rounds; pull direction 40x slower. **1a** 268/306/339 (18x p1par; the 128-bit key must be hashed into a 60-bit index and checked), **1b** 162/194/203 (21x), **1c** 279/349/392 (24x). P3: not attempted (no scan; the round-up-then-add fold is no monoid). Easy: reach is ~the shortest legible form (masked vxm); forced: the iface rule as its own closure per round. `systems/graphblas/CELLS.md`.

| problem | Goose | Futhark | Halide | Soufflé | DuckDB (added 15:58Z) |
|---|---|---|---|---|---|
| 1. resolve | **P1 OK** (1a/1b/1c). 1a 79-115 / 37-57; 1b 39-57 / 42-67; 1c 97-118 / 39-47 (1 / 8 workers, load 8-13). Forced: shuffle join, one table per worker; impossible: a shared table. | **P1 OK** (1a/1b/1c). 2 MB pages: 1a 79 / **24**, 1b 47 / 18, 1c 111 / 38 (1 / 8, load 5-13; 4 KB 1.2-2.5x slower). Easy: winner as reduce_by_index max; forced: the hash table as rounds of probing. | **P1 OK** (1a/1b/1c). 1a 81-93 / **19.8-27**; 1b 42.7-46 / 9.9-10.9; 1c 122-133 / 33-36.5 (1 / 8; tuned C same window 12.1-20.9 / 7.5-8.9 / 11.7-14.8). Surprise: 1a beats plain C 2.5-3x on 1 core (compact first). Forced: no hash table, so join = rounds of scatter-min + gather. | **P1 OK** (1a/1b/1c x2). 1a 1928-2034 / 636; 1b 5220-5426 / 1080; 1c 1625-1865 / 394-587 (1 / 8 threads, load 3-6): 30-200x C. Easy: join + max aggregate are the rules; forced: B-tree loading of 5.2 M rows. | **P1 OK** (1a/1b/1c). 8 threads: 1a 103-113, 1b 272-375, 1c 128-190 (10-60x tuned C). Easy: the joins are SQL joins; forced: every lookup by dense id is a hash join (SQL has no a[i]). |
| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood over given edges 97 / 58; lazy (own decode) 165 / 90 vs eager with its own decode >= 97 + 47 decode + an unwritten CSR build: **a tie at best, not a win** for ATTACKS 1 (corrected after review: the earlier 'slower than eager' compared unlike work). Cost: 111 levels x ~25 passes each. | **P2 OK** (eager + lazy). 8 thr: lazy 79-90, eager 70-127 (tuned C 25.7-29.1 / 12.7-14.6). Lazy is 1.3-1.5x *slower* than eager in Halide. Forced: no fixpoint (one pipeline call per level, ~100 levels), no worklist; `atomic()` dropped (mutex per element, wrong live sets). | **P2 OK** (eager + lazy). Flood over given edges 1227-1467 / 431. With its own decode: **lazy 2.9-3.2 s vs eager ~6.5 s on 1 thread, 960 ms vs ~1.6 s on 8** (decodes 1.73 M of 9.4 M relocations, 3 rules): ATTACKS 1 holds inside Soufflé; ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: slow as implemented (45 rounds x ~25 statements, hash tables rebuilt each round); Third attempt (M3, one recursive query per method round, `p2rec.sql`; lead rerun 18:22Z P2 OK): 8 threads setup 577-871 + flood 1438-2234 ms, 1.2x the old cell overall. **Falls down after three attempts**: each recursive step rescans all 2.7 M edges; DuckDB has no fast lookup by id (an index pays below ~50 rows, frontiers are 16-24 K). Lazy ties eager. |
| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down: one image array on 8 workers (gather +142-171). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). **3b_blk** (M3: the copy as a sequential slice-update loop inside a map over 64 blocks = memcpy; lead rebuilt + reran P3b_blk OK 18:36Z): 60.4/62.0 (1) / **23.5/30.5 (8)** vs old 3b 159.7/166.7 / 96.4/110.9 same window, load 5: 2.7x / 3.6x, ranges apart. Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK**. 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a: second attempt `layout_rank.dl` (M3; lead rebuilt from source, P3 OK 18:21Z): the order by a ranked successor chain, no pair counting, no C functor: 1 thread 1930 (median, was 8270), 8 threads 1725/1937/3541 (was 2860/3388/7055 same runs), load 6-22. **No longer falls down**, but ~100x plain C: the 1,763-step unit walk is the limit. 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
| 4. codegen by shape (stretch) | **P4 OK** (lead rebuilt + reran 1/8 workers 18:47Z). Workers own ranges of whole functions, no messages while computing: layout+emit 97.1/98.3 (1) / **17.6/21.3 (8)** vs p4.c 105.6 / p4par 17.1, within noise; same 698 M instructions. **Not a tie end to end**: that time leaves out the join of the parts, which is Goose's cost (no shared memory): +120-260 ms through queues, +37-63 ms writing each range into the file (M4 review 3). 482 lines + 40 C | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). **4D** (flattened stream, ATTACKS 4; lead reran P4D OK both backends 18:49Z): emit 65.1/67.8 (1) / **23.4/26.9 (8)** vs 4bf 284/303 / 95/113 same window, 4.5x / 4.2x; the flattening (`prepare`, 50 lines) 324 / 138 ms, so it pays only if the stream is reused | **P4 OK** (lead rebuilt + reran 1/8 18:46Z). Emit **157-162 (1, CPU time; one thread, so ~wall) / 23.0-25.1 (8, wall)** vs p4.c 103-112 / p4par 16.4-20.1 (load 4): 1.5x / 1.3x; layout 18 / 5 (+ template index 9.5 / 7). Relocations by a scatter to the layout's own slot ranges (no C host). Forced: entry-0 indices on untaken select sides (Halide loads both: 200 -> 160 ms); the vector copy only as 32-byte read-modify-write in parity passes (obvious form 26 s). 345 + 288 lines | **P4 OK** (lead rebuilt emit2 + reran 19:29Z: bytes, 136,635 offsets, 1,780,281 relocations; also P4 OK on kube-apiserver). Attempt 2 `emit2.dl` warm 7246/8316/9046 (1) / **2722/3814/4741 (8)** vs p4.c 107/111 / p4par 21.6/27.0 same rounds, load 7-10: ~70x / ~130x; lead rerun 8 threads compute 2571 ms (load 2.7-4.5). 74 lines + a 42-line C writer (memcpy runs, store low bytes, at places the rules computed). Easy: lengths, holes to segments, rel32 as a join with `at`, relocations, offsets. Forced: the prefix sum as a radix-16 tree of block sums (11%); **set semantics silently dropped 37,703 exactly-repeated marker relocations** (each now carries its id). Cost: 8.65 M output tuples, a B-tree insert each | not attempted |

**P4 on kube-apiserver** (19:17Z, results/codex-kube-p4.md): every P4 cell (C p4/p4par, Futhark 4bf/4D, Halide, Goose) is P4 OK on the second dataset unchanged (1.23x the bytes, 0.86x the templates). Emit medians kube/MinIO: C 1.12-1.15x, Futhark 4D 1.16-1.18x, 4bf 1.02-1.08x, Halide 1.44x (1) / 0.89x (8), Goose ~1.3x (8); load 4-24, ranges overlap for Futhark 4bf and Halide. No cell changes rank.

Halide column merged 17:17Z (lead rebuilt every generator and driver in a copy; P1/P2/P3 OK; `systems/halide/CELLS.md`).
DuckDB column merged 17:13Z (lead reran every check in a copy, pipeline included; `systems/duckdb/CELLS.md`).
Soufflé column merged 16:58Z (lead rebuilt all 8 programs from source; every check OK; `systems/souffle/CELLS.md`).
Futhark column merged 16:42Z (lead rebuilt from source; every cell OK; `systems/futhark/CELLS.md`). Surprise: `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, no code change, makes most Futhark cells 1.2-1.7x faster.
Goose column merged 16:35Z; lead reran every check (P1/P2/P3/pipeline OK, a source rebuild of p2par passes). Long form and lines: `systems/goose/CELLS.md`, `NOTES.md`.
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
# JOURNAL (append-only: one line per commit, a paragraph at each milestone)
- 2026-11-16T15:31:42Z M0 data: MinIO frozen (1,311 archives + main.a), hlink copied to ref/hlink with -dumpflat, flat files + oracles for P1-P3 (ref/flat/README.md); plain 1-core C references reproduce every oracle exactly (ref/flat/check.sh). Installs merged: Futhark 0.29.1, Halide 21, Soufflé 2.5, Goose v2026.4 (systems/*/NOTES.md).
- 2026-11-16T15:34:24Z M0: GRID.md C column (hlink phases on this VM, plain flat C references), IDEAS.md index, four Opus cell builders launched (ideas/BUILDER.md).
- 2026-11-16T15:36:04Z ref/flat/p2par.c: level-synchronous OpenMP flood on the flat CSR, full rules, P2 OK: 33.4 ms 1 thread, 12.1 ms 8 threads (load 14: provisional), 111 levels in 7 rounds; hlink's flood is 98-103 / 22-23 ms on raw objects. Problem 4 data/oracle handed to an Opus agent.
- 2026-11-16T15:37:00Z ref/flat/p1par.c: partitioned join (256 partitions, per-partition tables) + parallel gather, P1 OK: 72.7 ms 1 thread, 19.2 ms 8 threads (load 7, provisional) vs plain 230 ms; hlink resolve 29-33 ms on raw objects.
- 2026-11-16T15:54:54Z triage M0 skeptic: flat README contract fixes (name offsets, p1c_mark meaning, rank bit 30, id 0, p3_addr_all leak); stats.c note
- 2026-11-16T15:57:39Z GRID: tuned flat C column at load 0.8 (p1par 13.9-16.8 ms, p2par 13.0-17.0 ms on 8 threads vs hlink resolve 24, floods 22-23); kube-apiserver flat files + all C refs OK; dumpflat decode-to-CSR timed 133-139 ms; Codex: p3 pipeline + 1c par (codex-c), p2pipe eager decode (codex-d)
- 2026-11-16T15:58:28Z new column DuckDB (Opus builder, same brief): the north star names databases; DuckDB is installed (zero install cost), wildcard's top-2, and is the only vectorized-SQL/morsel-parallel engine in the grid (Soufflé is compiled Datalog, a different axis). Started before M2 because 4 builders + 2 Codex jobs leave the lead idle.
- 2026-11-16T15:59:27Z ref/flat/reachfacts.py: the flood uses only 28.7% (MinIO, 2.39 M of 8.32 M) / 21.0% (kube, 3.27 M of 15.6 M) of the eagerly decoded CSR, and 25.6% / 26.6% of those are duplicate (source, target) pairs. Eager decode does 3.5-5x the decode lazy hlink does; ATTACKS 1 must compare against a frontier-batched lazy decode, not only eager.
- 2026-11-16T16:00:00Z fix timestamps: GRID/PLAN said 16:xxZ for measurements taken at 15:56-15:58Z (the results commit message 'hlink minio bench (16:56Z)' should read 15:56Z)

- 2026-11-16T15:59Z codex-c: partitioned content-hash join shared by plain C/OpenMP; all MinIO oracles and independent all-pairs cases pass; interleaved plain 302.9 -> 57.0 ms, warm par 1/8 threads 36.9/7.2 ms (results/codex-c-p1.md).
- 2026-11-16T15:59Z codex-c: added independent P3 pipeline diagnostic; all 3a addresses match, but missing special-symbol inputs cause 81 relocation errors / 40 differing ranges. Corrected flat contract; details and reproduction in results/codex-c.md. Passing pipeline blocked on extraction extension.
- 2026-11-16T16:02:18Z p3 --pipeline passes on both datasets: dumpflat now extracts the linker-placed relocation targets (p3_fixed_*, 7 on MinIO, 6 on kube) as given inputs; copied into data/* from a scratch re-extraction (other files byte-identical). Merged codex-c: 1c partitioned join 7.2-8.9 ms on 8 threads, 36.9 on 1 (hlink 8-9 / 37-39); plain p1 1c 303 -> 57 ms.
- 2026-11-16T16:03:00Z ref/flat/p2lazy.c: lazy-decode flood over the flat relocation columns (no pre-decoded CSR for relocation edges; aux/kinds 1-3/methods precomputed as in p2pipe): MinIO 52.6-53.3 ms 1 thread / 14.4-15.4 ms 8 (p2par on the prebuilt CSR 26.0 / 8.6-9.7, interleaved, load 0.8); kube 72.7 / 19.9 (p2par 38.0 / 12.4). P2 OK both.
- 2026-11-16T16:03:39Z merged codex-d (p2pipe: eager decode+CSR+flood 108.7 / 30.1 ms MinIO, loses to hlink); ATTACKS 1 written: lazy decode from columns inside the flood (p2lazy) 52.6 / 13.6-15.4 ms, 2x eager, 1.9x/1.5x hlink's flood with caveats (aux, kinds 1-3, methods precomputed). GRID rows 1c, 2', 3b updated.
- 2026-11-16T16:08:14Z ATTACKS 2 draft: relocate's output page faults are ~half of 3b (prefault 45-64 ms vs copy+patch 31-35); MAP_HUGETLB output 25-29 ms vs 48-68 on 1 core; build/p3par (omp) added; vm.nr_hugepages set to 512 (was 0: hlink's hugetlbfs output path was not in use for GRID's hlink numbers); check.sh runs p2pipe, p2lazy, p3par.
- 2026-11-16T16:10:08Z killed per-source dedup (flood ~8% faster on 1 thread for 37% fewer edges); IDEAS rows for ATTACKS 1/2
- 2026-11-16T16:13:40Z p3 P3_REPS (fresh output per rep): relocate on 2 MB pages 23.9-30 vs 48.6-56 ms (1 thread), 5.4-12 (median 6.1-6.8) vs 13.4-29 ms (8 threads) = 1.1-1.4x the 4.92 floor; load 2.4-3.0. ATTACKS 2 no longer a draft.
- 2026-11-16T16:15:20Z hlink huge pages 512 vs 0, 6 interleaved rounds: within noise (193-267 vs 205-249 ms total); earlier 'huge pages help hlink' withdrawn. Builders resumed (ideas/RESUME.md); Codex attack1 review triaged.
- 2026-11-16T16:17:12Z PLAN Now: builders resumed in paradigms-46; codex-f launched on the p1par dense-pass hypothesis (789K of 5.2M slots keyed, 4 passes over 83 MB keys).
- 2026-11-16T16:17:35Z PLAN.md: restored the user's campaign text (sections Campaign 1 through Milestones log), deleted by a Now-line rewrite after 40f4d4b; Now line kept.
- 2026-11-16T16:18:13Z GRID: measured VM bandwidth (25 / 36 GB/s read, 27 write) vs FLOOR.md's implied ~87 GB/s; 3b on huge pages meets the per-kernel bandwidth bound.
- 2026-11-16T16:21:00Z relaxed atomics for the shared flags in p2par/p2pipe flood (Codex review; P2 OK both datasets). Found: under the builders' load, spinning OMP barriers make p2par 8 threads 900-1300 ms instead of 14; OMP_WAIT_POLICY=passive 14-55 ms. check.sh defaults to passive; builders warned (ideas/RESUME.md).

2026-11-16T16:14:34Z codex-e: raw AuxFuncdata columns + opt-in lazy aux flood; exact per-source aux multisets and P2 live oracles on both datasets. Interleaved best-of-five and load in results/codex-e.md. All 67 existing files byte-identical; shared data publication blocked by read-only mount, new columns retained in /tmp/codex-e-data.
- 2026-11-16T16:23:34Z merged codex-e: aux_* columns (add-aux.sh, run by the lead: 67/67 old files byte-identical on both datasets), p2lazy --aux decodes funcdata aux in the flood, P2 OK both datasets 1/8 threads (lead rerun 16:23Z, load 11); +7-9 ms on 1 thread. check.sh runs --aux.
- 2026-11-16T16:25:07Z p2lazy: timed-decoder check (per-source edge hash vs oracle-derived, mutation-tested) and relaxed atomics; closes the attack1 review's accepted items.
- 2026-11-16T16:33:15Z merged Goose (P1/P2/P3 all OK, lead-rerun; 3b on 2 MB pages via C remap 41 vs 92 ms; 8-worker reach scales 1.7x; one-array 8-worker 3b falls down). GRID Goose column filled.

2026-11-16T16:22Z codex-f: test the dense-name-array traffic hypothesis with a single-pass compact member/candidate join; partition ownership replaces global candidate probes. MinIO 8-thread and independent kube-apiserver 1-thread checks agree with all four oracles. Added mean timing instrumentation to both versions; interleaved 100-repetition measurements in progress, build-f only, 8 GiB address-space cap.

2026-11-16T16:31Z codex-f: completed 40 interleaved oracle-checked runs (100 repetitions each, 1/8 threads, MinIO/kube-apiserver, 8 GiB cap). Kube one-thread join median 124.077 -> 77.424 ms, non-overlapping ranges; other join comparisons within noise. Counted name-key coverage corrects MinIO's four-full-pass premise to 202.683 -> 83.287 MB; compact staging leaves modeled total streams 348.910 -> 286.290 MB. perf unavailable. Full ranges, arithmetic, scripts and raw evidence in results/codex-f.md. The earlier implementation-entry timestamp was written ahead of the VM clock; its commit occurred before 16:21Z.
- 2026-11-16T16:37:02Z merged codex-f (p1compact, P1 OK both datasets); lead rerun: join 1.5x faster on 1 thread (ranges apart), ~1.35-1.5x medians on 8; modeled bytes only -18-21%, so mechanism open (IDEAS).
- 2026-11-16T16:38:40Z merged Futhark (every cell OK from a lead source rebuild; ATTACKS 1 falls down in Futhark: lazy 165 / 90 slower than its eager flood; surprise: glibc.malloc.hugetlb=2 tunable gives 1.2-1.7x across cells with no code change).
2026-11-16T16:30Z codex-g: implemented pthread asynchronous P2 flood (seven join phases, chunk stealing, outstanding-work credits); 120/120 oracle checks plus independent six-case review and full check.sh on both datasets passed. Five interleaved >=1s samples per case: ambient 8-thread MinIO active/passive/async medians 953.49/21.40/19.93 ms, kube 940.07/33.49/34.38 ms; with eight busy workers MinIO 2289.64/152.71/89.40 ms. Passive vs async ambient within noise; deliberate-load ranges separate. Quiet claim untested; identical live sets do not imply identical queue-pop counts. Raw load, timing, counts and commands in results/codex-g.md.
- 2026-11-16T16:39:31Z ATTACKS 3: the level count is reach's cost across paradigms; p2async (7 joins vs 111 barriers) 1.7x over passive OpenMP under overload, 25x over default active.
- 2026-11-16T16:40:19Z ATTACKS 2: glibc.malloc.hugetlb=2 tunable gives plain C relocate the same 2x (24 vs 55 ms, 39 vs 19,109 faults) with no code change.
- 2026-11-16T16:40:57Z hugetlb tunable: p1compact join 1.5x on 1 thread (20.5-21.2 vs 30.1-32.3), floods within noise (inputs are mmap'd files).
- 2026-11-16T16:41:52Z flat.h FLAT_HUGE=1 (inputs copied onto 2 MB pages): floods 9-12% faster, join unchanged: killed as an attack; vm.nr_hugepages raised 512 -> 1536 (builders use the tunable too).
- 2026-11-16T16:56:03Z merged Soufflé (all cells OK from a lead rebuild; 30-200x the C refs; 3a falls down: no sort; lazy reach is 3 rules, 2x its eager).

- 2026-11-16T16:43:09.060031+00:00: codex-h separates compact emission (onlyA) from partition-owned candidate joins (onlyB); both datasets pass all P1 oracles; interleaved measurements in progress.

- 2026-11-16T16:56:23.217995+00:00: codex-h completed 80 interleaved comparison and 20 phase processes, all P1 OK. One-thread medians baseline/A/B/compact: MinIO 46.085/36.849/29.528/28.917 ms; kube 101.934/84.405/73.838/63.428 ms. Partition ownership carries most of the gain; candidate/member readback dominates the phase savings. Full ranges and commands in results/codex-h.md; check.sh unchanged.
- 2026-11-16T16:57:49Z merged codex-h: p1compact's 1.5x is mostly partition-local candidates (random probes into 256 tables), not fewer dense passes; bandwidth hypothesis killed (IDEAS).
- 2026-11-16T17:10:46Z merged DuckDB (all cells OK, lead rerun; 10-150x tuned C: every dense-id lookup is a hash join; P2 falls down: 45 rounds x 25 statements).
- 2026-11-16T17:13:26Z merged Halide (P1/P2/P3 OK, lead rebuild; 1a beats plain C 2.5-3x on 1 core; lazy reach slower than eager in Halide; no fixpoint/worklist).

2026-11-16T17:03Z codex-i: profile 3a and add tuned unit-parallel/MSD radix C layout; both datasets match every address at 1/8 threads. Five interleaved >=1 s runs: MinIO medians 7.37 ms plain, 5.00 / 3.89 ms tuned; hugepage request within noise, floor not reached. Details/results: results/codex-i.md.
- 2026-11-16T17:14:11Z merged codex-i: p3a_par 3a 5.0 / 3.9 ms (plain 7.4); the typelink string sort is 4.7 of 7.4 ms; 6.5x FLOOR.md's 0.60. Lead rerun: addr 0 differ both datasets 1/8 threads.

**M1 + M2, 2026-11-16T17:20Z.** All five paradigms are correct on problems 1-3 on MinIO; the lead rebuilt every cell from
its source and reran its oracle. None of them beats tuned C on any cell; the distances are the finding. Halide is closest
(1a 20-27 vs 12-21 ms on 8 threads; its 1a beats *plain* C 2.5-3x on one core by compacting first), Goose next (1.5-3x,
limited by message rounds: 1.7x scaling on 8), Futhark 2-6x (helped 1.2-1.7x by one glibc tunable), DuckDB 10-150x (every
dense-id lookup is a hash join), Soufflé 30-200x (B-tree loading; no sort, no bytes). Across all of them two costs
recur: **the level count of the flood** (111 levels: Futhark's ~25 passes per level, Halide's pipeline call per level,
DuckDB's ~25 statements per round, Goose's message exchange per superstep, OpenMP's barrier collapsing under load) and
**page size** (every runtime that let the output or its tables onto 2 MB pages got 1.2-2x). Those became ATTACKS 3 and 2.
ATTACKS 1 (lazy decode in the flood) holds only where a flood can do work per reached node cheaply (C 2x, Goose 1.4x on
1 thread); in Futhark and Halide it is slower than eager, in DuckDB a tie, in Soufflé 2x its own eager but 30x C. The
C references moved too: p1compact (partition the probe side: 1.5x), p2async (level-free), p3a_par (the typelink sort is
two-thirds of layout), and the huge-page relocate at this VM's bandwidth bound. Wrong turns, killed by measurement: the
"bandwidth-bound join" (it was random probes), huge-page inputs for the flood (9-12%), hlink's huge-page gain (noise),
per-source dedup (8%). **What did the oracle number do since the last milestone? If it stalled, what is the blocker,
and what am I doing about it?** From 0/20 + 0 ATTACKS at M0 (15:40Z) to 16/20 + DuckDB 3, and 3 measured ATTACKS. Not
stalled. What remains for M3/M4: problem 4 in two paradigms (0 so far; the C reference and oracle exist), kube-apiserver
for the paradigm cells, second attempts on the falls-downs (DuckDB's single recursive query, Soufflé 3a), quiet-machine
reruns of the 8-thread numbers, and the new columns (GraphBLAS for reach as a semiring BFS).
- 2026-11-16T17:15:35Z triaged M2 skeptic: corrected GRID (DuckDB P2 not yet falls-down; Futhark lazy ties eager-with-decode; Soufflé lazy 2x its eager), ATTACKS 3 scope; the M2 paragraph's 'in Futhark and Halide lazy is slower than eager' holds for Halide only.
- 2026-11-16T17:18:19Z M3/M4 launched: 5 Opus builders (Halide P4, Futhark P4, DuckDB P2 + Soufflé 3a second attempts, GraphBLAS, level-free floods) + Codex kube-apiserver runs.
- 2026-11-16T17:19:05Z frontier histogram (ref/flat/p2levels.c, results/levels.md): 81 of 111 levels < 4K frontier, 6% of work; hybrid wide-parallel / sequential-tail flood suggested.
- 2026-11-16T17:19:34Z relocation histogram (results/relkinds.*): 8 (kind,width) pairs, rel32 56%, top 100 targets = 30% of 2.05 M.
- 2026-11-16T17:20:30Z Codex: codex-p4par (tuned C P4) launched in paradigms-c; codex-kube relaunched with full access (sandbox blocked the systemd bus).
- 2026-11-16T17:23:23Z p2hyb (sequential tail flood): 111 -> 12 barriers but ~44% of pops go sequential; times noise at load 19.
- 2026-11-16T17:28:24Z prior art: ideas/copy-and-patch.md (8 ideas for P4 with kill tests); CP1/CP4 queued for Codex.
- 2026-11-16T17:28:49Z verified CP1 premise: disp width == fits-int8, 0 exceptions in 4,864,669 holes (results/cp1check.*).

- 2026-11-16T17:24Z (codex-p4par): implement P4 OpenMP local scans + chunk-base addition, contiguous function emission balanced by item count, private relocation vectors concatenated in deterministic range order, explicit 2 MB output with fallback. Add warm in-process repeats to serial comparator, oracle check at 1/8 threads, and reproducible interleaved benchmark/count scripts. Oracle passes independently including fallback and constrained team; measurements underway in results/codex-p4par-runs.log.
- 2026-11-16T17:29Z (codex-p4par): five interleaved P4_REPS=64 runs, passive OpenMP, 12 GiB cap, load 14.67–22.45. Serial vs 8-thread median layout 6.945 vs 4.283 ms (overlapping ranges: within noise), emit 145.367 vs 65.553 ms (2.22x, non-overlapping). Every oracle passes; final constrained-team/fallback check passes. Count 60,733,909 copied bytes, 6,333,835 integer/jump patches plus 1,346,646 symbol holes; logical bandwidth scale 5.932–11.349 ms. No-patch delta 7.999 ms at one thread; eight-thread diagnostic inverted under noise, no defensible fraction. Full results and logs: results/codex-p4par.md. This tunes the existing C P4 reference, not a new paradigm cell.
- 2026-11-16T17:30:31Z merged codex-p4par: tuned C P4 (ref/p4/p4par.c); lead reran ref/p4/check.sh: P4 OK at 1 and 8 threads; emit 8 threads 51-68 ms vs 116-174 serial (load ~20); patching only 7.6% of emit.

2026-11-16 CP1/CP4: build decoder-backed disp32-normalized templates and generated top-4096 emitters on codex-cp. CP1 compresses 108,833 to 66,046 keys (82.252% top-4096 node coverage); full oracle matches. Measure the added operand-dependent length pass and CP4's covered-only control against their explicit kill thresholds; preserve child-segment boundaries separately from normalized template identity.
2026-11-16T17:43Z CP1/CP4 measurements complete: 35 interleaved oracle-passing runs. CP1 66,046 keys / 82.252% top-4096 coverage passes counting, but layout median 42.708 ms vs p4 6.805 ms (6.28x) kills runtime relaxation. CP4 covered medians 46.611/27.865 ms (1.67x) overlap in range: within noise, not an established threshold pass; full 1-thread median 110.532 ms misses 80 ms. Perf shows 26% fewer instructions than p4par, no branch-miss reduction. Full report results/codex-cp.md; independent review + SIB/RIP synthetic and negative-offset tests retained.
- 2026-11-16T17:48:24Z merged codex-cp: CP1 template count 108,833 -> 66,046 (82.3% top-4K) but its operand-derived layout 6.3x slower; CP4 generated emitter 1.67x on covered segments, within noise; both P4 OK (lead rerun).
- 2026-11-16T17:59:57Z merged levelfree builder: level count is not Futhark's cost (45 levels; per-level passes are); Goose async 1.3-1.5x; lead reran both checks (P2 OK). ATTACKS 3 updated.
- 2026-11-16T18:10:45Z merged Futhark P4 (lead rebuilt + reran c/multicore: P4bf OK): 3.2x C on 1 core, ~5x on 8; slice-update loop inside a map = bulk copy.
- 2026-11-16T18:20:04Z merged second attempts (Soufflé 3a no longer falls down: 1.9 s ranked chain; DuckDB P2 falls down after 3) and GraphBLAS column (P1/P2 OK, 9-24x tuned C, does not scale: frontiers below its threading threshold); lead reran every check.
- 2026-11-16T18:20:53Z merged codex-kube: all 5 paradigms' P1-P3 cells correct on kube-apiserver; 1-thread scaling 1.5-2.5x; Halide 1a 6.7x flagged.
- 2026-11-16T18:21:18Z ATTACKS 4: emit costs pieces not bytes (0.42 GB/s, patch 7.6%); width-relaxed templates 108,833 -> 66,046.

**M3 (2026-11-16T18:40Z).** Five Opus builders and four Codex jobs in parallel, load 7-26 throughout. Second attempts
settled two cells (Soufflé 3a now stands, DuckDB P2 falls down with three attempts written); the level-free floods
corrected ATTACKS 3's premise in the paradigms (Futhark: 45 levels, the per-level passes are the cost; Goose already had
10 supersteps, async 1.3-1.5x); GraphBLAS became a column (correct, 9-24x C, single-threaded by its own heuristics);
every paradigm's P1-P3 cells are correct on kube-apiserver; problem 4 got a tuned C reference, a Futhark cell and a new
attack (ATTACKS 4). **What did the oracle number do since the last milestone? If it stalled, what is the blocker, and what
am I doing about it?** 16/20 + DuckDB 3 with 3 ATTACKS -> 17/20 + DuckDB 3 + GraphBLAS 2 with 4 ATTACKS. Not stalled,
but slower: the three remaining cells are all problem 4 (Goose, Halide, Soufflé); Halide is building, Goose goes next.
The 8-thread numbers are weak evidence because the machine was oversubscribed by my own helpers; reruns go in the Now line.
- 2026-11-16T18:22:41Z launched Goose P4 builder, Futhark 3b follow-up, codex-cp7 (pieces vs time).
- 2026-11-16T18:33:08Z merged Futhark 3b_blk (slice-update copy; lead rebuilt, P3b_blk OK both backends): 2.7x / 3.6x the old 3b.
- 2026-11-16T18:35:42Z triaged M3 skeptic: 5 accepted (Halide fresh-image check added, ATTACKS 3 retitled/corrected, Halide kube anomaly = load, page-reuse caveat).

2026-11-16 CP7: prepared contiguous runs in C (hole-free-neighbour fusion, whole-function blobs, sequence deduplication), plus an unmerged prepared control to distinguish fewer copies from hoisted metadata lookup. All variants pass bytes, unconditional function offsets and sorted relocations at 1/8 threads and constrained-team/fallback checks. Five interleaved 128-repetition measurements underway; preprocessing separately timed, logical prepared-input footprint counted.
2026-11-16T18:31:29Z CP7 measurement: 55/55 oracle passes, five interleaved P4_REPS=128 runs, 1/8 threads, 12 GiB, load 2.53–4.85. Prepared unmerged D/1 48.474 ms vs p4par/1 94.695; A/1 43.981, B/1 44.689, C/1 46.589 ms. B has 17.43× fewer copies than D but only 1.08× lower time; fusion benefits at eight threads overlap noise. C stores 55,389 distinct runs / 48,985,021 bytes. Report, raw logs, reproducible summary and independent UBSan/negative-offset review committed; no new grid cell or ATTACKS entry, entry 4 refined.
- 2026-11-16T18:36:46Z merged codex-cp7 (lead rerun all P4 OK): flattened per-item stream 2x p4; merging pieces 17x adds only 1.08x -> ATTACKS 4 corrected: indirection per piece is the cost.
- 2026-11-16T18:47:54Z merged Halide P4 (1.5x/1.3x C), Goose P4 (ties p4par on 8), Futhark 4D (flat stream 4.5x its own emit); lead rebuilt + reran each. GRID P4 row, ATTACKS 4 cost split (pieces ~40 ns, holes ~5.8 ns, bytes ~13 ms).

**M4 (2026-11-16T18:55Z).** Problem 4 went from a C reference to three correct paradigm cells and a tuned C in about
1.5 hours. The attack moved twice: "patching is cheap, so the copy is the cost" (p4par) -> "pieces, not bytes" (ATTACKS 4
first draft) -> "lookups per piece; flatten the stream" (codex-cp7's control D, then confirmed in Futhark at 4.5x and
in the paradigms' top-K curves). Writing the draft before the control was run cost one correction; the control was
cheap and should have come first. **What did the oracle number do since the last milestone? If it stalled, what is
the blocker, and what am I doing about it?** 17/20 -> 19/20, ATTACKS 4 backed. The last cell is Soufflé P4, next in
the Now line; after it the grid is full, and the work turns to the open attacks (holes, level sync in Halide).
- 2026-11-16T18:51Z M4 review triaged (GRID Goose P4 join cost, Halide CPU-time label, ATTACKS 3 Futhark worklist); builders: Soufflé P4, Halide batched flood; Codex: hole cost (codex-holes), Goose P4 offset check (codex-goose-fnoff).
- 2026-11-16T18:52Z ref/fc/run.sh + ref/p4/extract.sh take TARGET/PKG (default MinIO); kube-apiserver P4 extraction started (data/kube-p4).
- 2026-11-16T18:54Z P4 on kube-apiserver extracted (data/kube-p4: 203 K functions, 74.5 MB, 93 K templates); p4/p4par P4 OK on it.
- 2026-11-16T18:58Z (Codex, codex-goose-fnoff) Goose P4 now checks all computed function offsets, including the sentinel, in file and queue joins outside timed reps; MinIO 1/8 workers pass both modes, and a temporary +1 offset fault fails both. Commands/output: results/codex-goose-fnoff.md.
- 2026-11-16T18:55Z New column started: Differential Dataflow (Sonnet installer, systems/ddflow). Why: incremental relink is an axis no column has (IDEAS 'New columns'); after a small edit a relink's resolve/reach mostly repeats, and DD maintains the answer under input changes. First: P2 reach + one-edge-batch update vs full recompute.
- 2026-11-16T19:05Z DD installed (rust 1.100, differential-dataflow 0.26.0): plain-edge reach 265,350 ids, a strict subset of the oracle's 663,630 (no method rules yet); edge-batch updates 1.2-37 ms vs 2.68 s full (1 worker, load 11). Opus builder started on the exact P2 + a real-edit incremental relink (results/ddflow-incremental.md).
- (Codex, codex-holes) 2026-11-16T19:08Z codex-holes measured: five rotated rounds, 256 repetitions, 1/8 threads pinned, 50 oracle passes, kernel runs 3.975–56.799 s at load 3.43–13.35. One-core medians B 44.495 ms, H1 28.757 ms, H2 41.275 ms; H1 removes 89% of patch stores but only 35% of time, H2 overlaps B. Checked H0 retains traversal without integer stores (41.559 ms); perf supports metadata/dispatch cost rather than scalar-store latency. Preparation reported separately; eight-core H1/B overlap. Final validation also closes p4par's conditional function-offset check. Results and reproduction: results/codex-holes.md; ATTACKS 4 updated. No push per user instruction.
- 2026-11-16T19:10Z codex-holes merged; lead reran check-holes.sh (13 P4 OK, 3 planted faults caught) and 3 interleaved rounds pinned to CPU 6 at load 12: B 43.0-44.6 vs H1 28.6-29.1 ms emit (1.5x, ranges apart). ATTACKS 4: after flattening, the per-hole cost is record traversal, not the store.
- (Codex, codex-kube-p4) 2026-11-16T19:15Z codex-kube-p4: rebuilt unchanged Futhark 4bf/4D, Halide and C P4 cells; both datasets pass their existing checks, five interleaved 1/8-thread rounds archived in results/codex-kube-p4-raw.json and analyzed in results/codex-kube-p4.md. Goose builds but fails before loading under the requested 8 GiB address-space cap (stack reservations); no code/constants changed. C offset-check limitation documented; independent review verified all table cells and ratios.
- 2026-11-16T19:17Z codex-kube-p4 merged: every P4 cell (C, Futhark 4bf/4D, Halide, Goose) P4 OK on kube-apiserver unchanged; emit scales ~1.0-1.2x for 1.23x the bytes (Halide 1-thread 1.44x, overlapping ranges). Goose rerun by lead under its cgroup cap. p4.c offsets checked unconditionally.
- 2026-11-16T19:31Z Halide batched-level flood merged (results/halide-levelfree.md): k=2/4/8 levels per call 1.0-1.2x slower than one call per level; calls cost ~2 ms of 300; per-level passes (0.63 / 0.23 ms even when empty) are the cost. Lead rebuilt + reran: f_f 294, f_f_e4 348 (1 thr), f_t 64, f_t_e8 84 ms (8), all P2 OK. ATTACKS 3 updated; M4 review item 5 closed.

**2026-11-16T19:33Z, grid full.** Soufflé P4 closed the last cell: correct (bytes, every function offset, sorted
relocations; MinIO and kube), 70x C on 1 core and 130x on 8, which is Soufflé's usual factor on this workload: 8.65 M
output tuples, each a B-tree insert. Its finding is a trap, not a speed: set semantics silently merged 37,703
relocations that were exact duplicates, and only the count check caught it. Datalog's relation is a set; a linker's
relocation list is a multiset. Halide's batched flood answered the M3/M4 reviewers: the calls are free (~0.02 ms), the
per-level passes cost 0.2-0.6 ms even with an empty frontier, so ATTACKS 3's verdict for data-parallel systems
(fewer passes per level, not fewer levels) now holds in Futhark and Halide. The hole cost split: what looked like
5.8 ns per store is record traversal; baking layout-independent integers into prepared blobs is 1.5x (reproduced by
the lead), merging stores is noise. Every P4 cell is correct on kube-apiserver unchanged. **What did the oracle number
do since the last milestone? If it stalled, what is the blocker, and what am I doing about it?** 19/20 -> 20/20; ATTACKS
stayed at 4 backed entries but 3 and 4 gained measured extensions. Not stalled. The grid's frame is filled, so the
next gains are outside it: Differential Dataflow asks whether a relink after a small edit needs most of this work at
all (builder running), and a Halide cell on the flattened P4 stream tests whether ATTACKS 4 transfers (builder running).

exec
/bin/bash -lc 'cat GRID.md ATTACKS.md' in /home/exedev/paradigms-w
 succeeded in 0ms:
# GRID

One cell per (system, problem): correct? (oracle command) · time 1 core / 8 cores, warm / end to end ·
lines · easy / forced / impossible. Dataset: MinIO unless marked. Data contract and rules: `ref/flat/README.md`.

All times are wall ms on this VM (AMD EPYC 9554P under KVM, 8 cores), best of >= 3 runs, load < 1 unless marked.

## Reference: C

Two references, measured 2026-11-16T15:33Z (load 0.4):

- **hlink** (bottom-up's tuned C linker, `ref/hlink` = bottom-up `774e37c`), from its own `-stats` and `HLINK_TSC=1`
  stage clocks, `ref/hlink/bench.sh minio` -> `results/minio.hlink.tsv`. It reads raw Go objects, so its phases
  include work the flat files have done in advance (name hashing, decoding references): it is the speed target,
  not a like-for-like row. Whole link: 537 ms on 1 core, **149 ms on 8 cores** (process, warm).
- **plain C on the flat files** (`ref/flat/p1.c p2.c p3.c`, single-threaded, untuned, one hash table or one loop
  per rule): the like-for-like baseline every system is compared with first. `ref/flat/check.sh` runs them and
  checks every oracle.

| problem | floor (8 cores, FLOOR.md model) | hlink 1 core / 8 cores | plain C, flat, 1 core warm | tuned C, flat (p1par/p2par), 1 / 8 threads | check |
|---|---|---|---|---|---|
| 1a+1b. resolve: join by name, relocation targets | 1.36 (resolve) | 125 / 24 (resolve phase, 15:56Z) | 194-203 (join) + 43-44 (9.4 M targets) | 74-77 / **13.9-16.8** (join 8.6-10.5 + targets 5.3-6.3); p1compact join 28.1-30.7 / 6.5-9.9 vs p1par 41.7-48.7 / 9.5-12.9 same window (16:34Z, load 3-5) | `build/p1 data/minio`, `build/p1par` -> P1 OK |
| 1c. content-hash join | (in resolve) | 37-39 / 8-9 (inside deadcode) | 57-61 (radix-partitioned, p1c.h; was 300-309 with a 400 MB table) | 36.9 / **7.2-8.9** | same |
| 2. reach | 2.78 | 98-103 / 22-23 floods + 7 / 1.5 methods | 39-43 | 27-30 / **13.0-17.0** | `build/p2 data/minio`, `build/p2par` -> P2 OK |
| 2'. reach incl. decoding relocations | | (= the row above: hlink decodes lazily) | dumpflat's untuned eager loop 133-139 + flood 39-43 | eager decode + CSR + flood (p2pipe): 108.7 / 30.1; **lazy decode in the flood (p2lazy): 52.6 / 13.6-15.4** | `build/p2pipe`, `build/p2lazy` -> P2 OK |
| 3a. lay out | 0.60 | 85 / 23-25 (layout phase, includes pclntab sizing and the typelink sort) | 8-19 | p3a_par **4.90–5.41 / 3.66–7.56** (medians 5.00 / 3.89; load 4.1–4.4); kube 7.62–8.09 / 5.41–7.62; [five-run report](results/codex-i.md) | `build-i/p3a_par data/minio` -> addr 0 differ; both datasets, 1/8 threads |
| 3b. relocate | 4.92 (write) | 102-104 / 15 (write phase, includes pclntab and GC bitmaps) | 59-67 (76 MB, 2.05 M relocations); P3_REPS=20 16:13Z: 48.6-56 on 4 KB pages, **23.9-30** on 2 MB pages | p3par 8 threads: 13.4-29 on 4 KB, **5.4-12 (median 6.1-6.8) on 2 MB** (ATTACKS 2) | same; `build/p3 data/minio --pipeline` runs 3a -> 3b from its own addresses -> P3 pipeline OK |
| 4. codegen by shape: emit 136,634 bodies (60.7 MB) from 108,833 byte templates + 7.68 M operands | | fc's tree walker (no separate phase time) | 13-17 (lay out 2.38 M pieces) + 139-147 (emit), load 3.5; lead rerun 16:05Z: 18.4 + 162.4 | p4par (codex 17:27Z, load ~20, 5 interleaved, min/med/max): 1 thread layout 7.8/7.9/10.4 + emit 107/118/150; **8 threads 2.9/4.3/7.8 + 51/66/68** (vs p4 116/145/174 same window). Patching is 7.6% of 1-thread emit: the copy of 2.38 M ~25-byte pieces at ~0.6 GB/s is the cost (per-item, not bandwidth). [results/codex-p4par.md](results/codex-p4par.md). Quiet rerun (codex-cp7, 18:30Z): p4 88.9/98.4/98.6, p4par 8 threads **16.1/18.4/22.2**; flattened stream (p4runs D) 48.5 (1) / 13.7 (8), one run per function (B) 44.7 / 12.3 ([results/codex-cp7.md](results/codex-cp7.md), ATTACKS 4) | `ref/p4/check.sh` -> P4 OK (`ref/p4/README.md`) |

**Floors vs this VM.** FLOOR.md's floors imply ~87 GB/s (minio resolve: 119 MB in 1.36 ms); this VM streams **25 GB/s read on
1 thread, 36 GB/s read / 27 GB/s write on 8** (16:16Z, load 5, a sum and a fill over 1 GB). Floors here are ~2.4x lower than the
column says. Per-kernel bandwidth bound for flat 3b: 76 MB symbol bytes + ~33 MB relocations read, 76 MB written -> ~3-6 ms on
8 threads, which p3par on 2 MB pages (5.4-6.8 median) meets.

Tuned and plain flat rows re-measured 2026-11-16T15:56Z at load 0.8, 3 interleaved rounds (`ref/flat/bench.sh`); hlink's resolve/deadcode from `COLD=0 ref/hlink/bench.sh minio build/hlink 3` right after.

**kube-apiserver** (second dataset, extracted 2026-11-16T15:55Z, 2.2 GB of flat files, 13.27 M slots): every plain and
tuned C reference reproduces its oracles (`ref/flat/check.sh data/kube-apiserver`, loaded machine): p1 434 + 88 ms,
p1c 550 ms, p2 66.5 ms (926,486 live), p3a 30.4 ms, p3b 67.8 ms (72.6 MB, 2.82 M relocations); p1par 26.2 ms and
p2par 19.6 ms on 8 threads. Its oracles come from hlink alone so far (MinIO's live list was also checked against fclink).

**Paradigm cells on kube-apiserver** (codex-kube, 17:21-18:16Z, merged 18:30Z; [results/codex-kube.md](results/codex-kube.md)): every P1-P3 cell of Goose, Futhark, Halide, Soufflé and DuckDB, unchanged, reproduces kube's oracles at 1 and 8 threads (lead spot-checked Halide P1 and Goose P2 at 18:28Z). Load was 17-26 for most of the window, so 8-thread kube/MinIO ratios (2-10x) mostly measure the load; the 1-thread ratios, 1.5-2.5x, are at or below the data's growth (kube has **2.55x** MinIO's slots, 2.65x its named symbols, 2.1x the relocations, 1.4x the live set and 1.2x the placed symbols; an earlier version of this line said "1.4x the ids": wrong). Halide 1a's 6.7x was the load: rerun interleaved at load 2.6 (18:33Z, 1 thread pinned, 3 rounds): MinIO 119-126 ms, kube 320-350, **2.6x, linear in slots** (M3 review finding 4).

Floors are bottom-up's FLOOR.md model (same CPU model, the 2026-11-13 VM); they count the raw object bytes hlink
reads, so for the flat files they are an upper bound on the floor.

## The paradigms

**Caveat (M3 review finding 3).** Warm numbers for Goose P1 (213 ms first run vs 79-115 later) and Futhark 3b reuse
buffers whose pages are already mapped, while the plain C references include their first-touch faults; those warm
cells flatter the paradigm by up to ~2x. The e2e numbers in each system's CELLS.md include the faults.

Added columns (M3): **GraphBLAS** (SuiteSparse 10.5.2, JIT on; lead rebuilt + reran 18:24Z, P1/P2 OK, all files `cmp`). 8 threads warm, min/med/max, load 7-10, tuned C same window: **P2 reach 179/186/213** (p2par 15/20/26: 9x); 1 thread 192-203, i.e. it does not scale (frontier ~7 K is below SuiteSparse's per-call threading threshold: 379 of 382 vxm calls on one thread); 178 synchronous steps (97 BFS + 81 iface levels) in 7 rounds; pull direction 40x slower. **1a** 268/306/339 (18x p1par; the 128-bit key must be hashed into a 60-bit index and checked), **1b** 162/194/203 (21x), **1c** 279/349/392 (24x). P3: not attempted (no scan; the round-up-then-add fold is no monoid). Easy: reach is ~the shortest legible form (masked vxm); forced: the iface rule as its own closure per round. `systems/graphblas/CELLS.md`.

| problem | Goose | Futhark | Halide | Soufflé | DuckDB (added 15:58Z) |
|---|---|---|---|---|---|
| 1. resolve | **P1 OK** (1a/1b/1c). 1a 79-115 / 37-57; 1b 39-57 / 42-67; 1c 97-118 / 39-47 (1 / 8 workers, load 8-13). Forced: shuffle join, one table per worker; impossible: a shared table. | **P1 OK** (1a/1b/1c). 2 MB pages: 1a 79 / **24**, 1b 47 / 18, 1c 111 / 38 (1 / 8, load 5-13; 4 KB 1.2-2.5x slower). Easy: winner as reduce_by_index max; forced: the hash table as rounds of probing. | **P1 OK** (1a/1b/1c). 1a 81-93 / **19.8-27**; 1b 42.7-46 / 9.9-10.9; 1c 122-133 / 33-36.5 (1 / 8; tuned C same window 12.1-20.9 / 7.5-8.9 / 11.7-14.8). Surprise: 1a beats plain C 2.5-3x on 1 core (compact first). Forced: no hash table, so join = rounds of scatter-min + gather. | **P1 OK** (1a/1b/1c x2). 1a 1928-2034 / 636; 1b 5220-5426 / 1080; 1c 1625-1865 / 394-587 (1 / 8 threads, load 3-6): 30-200x C. Easy: join + max aggregate are the rules; forced: B-tree loading of 5.2 M rows. | **P1 OK** (1a/1b/1c). 8 threads: 1a 103-113, 1b 272-375, 1c 128-190 (10-60x tuned C). Easy: the joins are SQL joins; forced: every lookup by dense id is a hash join (SQL has no a[i]). |
| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood over given edges 97 / 58; lazy (own decode) 165 / 90 vs eager with its own decode >= 97 + 47 decode + an unwritten CSR build: **a tie at best, not a win** for ATTACKS 1 (corrected after review: the earlier 'slower than eager' compared unlike work). Cost: 111 levels x ~25 passes each. | **P2 OK** (eager + lazy). 8 thr: lazy 79-90, eager 70-127 (tuned C 25.7-29.1 / 12.7-14.6). Lazy is 1.3-1.5x *slower* than eager in Halide. Forced: no fixpoint (one pipeline call per level, ~100 levels), no worklist; `atomic()` dropped (mutex per element, wrong live sets). | **P2 OK** (eager + lazy). Flood over given edges 1227-1467 / 431. With its own decode: **lazy 2.9-3.2 s vs eager ~6.5 s on 1 thread, 960 ms vs ~1.6 s on 8** (decodes 1.73 M of 9.4 M relocations, 3 rules): ATTACKS 1 holds inside Soufflé; ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: slow as implemented (45 rounds x ~25 statements, hash tables rebuilt each round); Third attempt (M3, one recursive query per method round, `p2rec.sql`; lead rerun 18:22Z P2 OK): 8 threads setup 577-871 + flood 1438-2234 ms, 1.2x the old cell overall. **Falls down after three attempts**: each recursive step rescans all 2.7 M edges; DuckDB has no fast lookup by id (an index pays below ~50 rows, frontiers are 16-24 K). Lazy ties eager. |
| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down: one image array on 8 workers (gather +142-171). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). **3b_blk** (M3: the copy as a sequential slice-update loop inside a map over 64 blocks = memcpy; lead rebuilt + reran P3b_blk OK 18:36Z): 60.4/62.0 (1) / **23.5/30.5 (8)** vs old 3b 159.7/166.7 / 96.4/110.9 same window, load 5: 2.7x / 3.6x, ranges apart. Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK**. 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a: second attempt `layout_rank.dl` (M3; lead rebuilt from source, P3 OK 18:21Z): the order by a ranked successor chain, no pair counting, no C functor: 1 thread 1930 (median, was 8270), 8 threads 1725/1937/3541 (was 2860/3388/7055 same runs), load 6-22. **No longer falls down**, but ~100x plain C: the 1,763-step unit walk is the limit. 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
| 4. codegen by shape (stretch) | **P4 OK** (lead rebuilt + reran 1/8 workers 18:47Z). Workers own ranges of whole functions, no messages while computing: layout+emit 97.1/98.3 (1) / **17.6/21.3 (8)** vs p4.c 105.6 / p4par 17.1, within noise; same 698 M instructions. **Not a tie end to end**: that time leaves out the join of the parts, which is Goose's cost (no shared memory): +120-260 ms through queues, +37-63 ms writing each range into the file (M4 review 3). 482 lines + 40 C | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). **4D** (flattened stream, ATTACKS 4; lead reran P4D OK both backends 18:49Z): emit 65.1/67.8 (1) / **23.4/26.9 (8)** vs 4bf 284/303 / 95/113 same window, 4.5x / 4.2x; the flattening (`prepare`, 50 lines) 324 / 138 ms, so it pays only if the stream is reused | **P4 OK** (lead rebuilt + reran 1/8 18:46Z). Emit **157-162 (1, CPU time; one thread, so ~wall) / 23.0-25.1 (8, wall)** vs p4.c 103-112 / p4par 16.4-20.1 (load 4): 1.5x / 1.3x; layout 18 / 5 (+ template index 9.5 / 7). Relocations by a scatter to the layout's own slot ranges (no C host). Forced: entry-0 indices on untaken select sides (Halide loads both: 200 -> 160 ms); the vector copy only as 32-byte read-modify-write in parity passes (obvious form 26 s). 345 + 288 lines | **P4 OK** (lead rebuilt emit2 + reran 19:29Z: bytes, 136,635 offsets, 1,780,281 relocations; also P4 OK on kube-apiserver). Attempt 2 `emit2.dl` warm 7246/8316/9046 (1) / **2722/3814/4741 (8)** vs p4.c 107/111 / p4par 21.6/27.0 same rounds, load 7-10: ~70x / ~130x; lead rerun 8 threads compute 2571 ms (load 2.7-4.5). 74 lines + a 42-line C writer (memcpy runs, store low bytes, at places the rules computed). Easy: lengths, holes to segments, rel32 as a join with `at`, relocations, offsets. Forced: the prefix sum as a radix-16 tree of block sums (11%); **set semantics silently dropped 37,703 exactly-repeated marker relocations** (each now carries its id). Cost: 8.65 M output tuples, a B-tree insert each | not attempted |

**P4 on kube-apiserver** (19:17Z, results/codex-kube-p4.md): every P4 cell (C p4/p4par, Futhark 4bf/4D, Halide, Goose) is P4 OK on the second dataset unchanged (1.23x the bytes, 0.86x the templates). Emit medians kube/MinIO: C 1.12-1.15x, Futhark 4D 1.16-1.18x, 4bf 1.02-1.08x, Halide 1.44x (1) / 0.89x (8), Goose ~1.3x (8); load 4-24, ranges overlap for Futhark 4bf and Halide. No cell changes rank.

Halide column merged 17:17Z (lead rebuilt every generator and driver in a copy; P1/P2/P3 OK; `systems/halide/CELLS.md`).
DuckDB column merged 17:13Z (lead reran every check in a copy, pipeline included; `systems/duckdb/CELLS.md`).
Soufflé column merged 16:58Z (lead rebuilt all 8 programs from source; every check OK; `systems/souffle/CELLS.md`).
Futhark column merged 16:42Z (lead rebuilt from source; every cell OK; `systems/futhark/CELLS.md`). Surprise: `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, no code change, makes most Futhark cells 1.2-1.7x faster.
Goose column merged 16:35Z; lead reran every check (P1/P2/P3/pipeline OK, a source rebuild of p2par passes). Long form and lines: `systems/goose/CELLS.md`, `NOTES.md`.
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

exec
/bin/bash -lc "rg -n -i 'time left|session|budget|usage|falls down|surpris|unexpl|not chased' JOURNAL.md systems/*/NOTES.md; cat LOOP.md; rg --files systems | head -80" in /home/exedev/paradigms-w
 succeeded in 0ms:
systems/souffle/NOTES.md:9:## Surprises
systems/souffle/NOTES.md:136:**2' reach, lazy decode** (reach_lazy.dl 65 [84], plus prep_lazy.c 67, untimed, as p2lazy's setup). See Surprises.
systems/souffle/NOTES.md:149:a size that is a multiple of 8 (checked by the rule `tlcheck`, empty). Verdict after two attempts was "falls down on
systems/souffle/NOTES.md:165:**Verdict: correct, slow; no longer "falls down".** 8 threads do not help: the unit walk is a 1,763-iteration
systems/souffle/NOTES.md:189:finish in 100 s). (2) duplicates: markers carry their id (Surprises). (3) bytes: the C writer. Impossible: none.
systems/souffle/NOTES.md:194:probes, 1.5x slower; Surprises), a materialized `hole` (arity 7: Surprises), emit/reloc as bries (on the operand-driven
systems/halide/NOTES.md:24:## Surprises
systems/halide/NOTES.md:122:and hands it to the pipeline. Fresh output per run, faults inside the timed region (`p3 DATA 3b`, getrusage):
systems/halide/NOTES.md:161:### 3a lay out (`Layout` in gen_p3.cpp, 154 lines; from the earlier session)
systems/duckdb/NOTES.md:8:## Surprises
systems/duckdb/NOTES.md:130:available, and batching levels would need E^k edges. **Verdict: falls down** after three attempts, ~140x tuned C.
systems/goose/NOTES.md:12:## Surprises
systems/goose/NOTES.md:85:getrusage. Interleaved 16:24-16:25Z (load ~8), 1 thread P3_REPS=20 min per run:
systems/goose/NOTES.md:101:and par.goose 36 shared; flatio.c 102 is the C escape: file reads, page remapping, getrusage).
systems/goose/NOTES.md:137:### Falls down
systems/graphblas/NOTES.md:20:## Surprises
systems/futhark/NOTES.md:9:## Surprises
systems/futhark/NOTES.md:54:   counted: see Surprise 0), each one a join that waits for the slowest descheduled thread. Read all 8-thread numbers below as
systems/futhark/NOTES.md:81:rows were taken at load 5-13 on 8 cores: they measure the machine as much as Futhark (Surprise 2).
systems/futhark/NOTES.md:312:threads got ~3-4 cores). 2 MB pages: 1.4-1.6x at 1 core, 1.2x at 8, faults per warm rep 95 K -> 265 (Surprise 1 again).
systems/futhark/NOTES.md:509:- `futhark_new_u32_1d` copies (no zero-copy wrap of a host pointer on c/multicore); budget the copy
JOURNAL.md:29:- 2026-11-16T16:33:15Z merged Goose (P1/P2/P3 all OK, lead-rerun; 3b on 2 MB pages via C remap 41 vs 92 ms; 8-worker reach scales 1.7x; one-array 8-worker 3b falls down). GRID Goose column filled.
JOURNAL.md:35:- 2026-11-16T16:38:40Z merged Futhark (every cell OK from a lead source rebuild; ATTACKS 1 falls down in Futhark: lazy 165 / 90 slower than its eager flood; surprise: glibc.malloc.hugetlb=2 tunable gives 1.2-1.7x across cells with no code change).
JOURNAL.md:41:- 2026-11-16T16:56:03Z merged Soufflé (all cells OK from a lead rebuild; 30-200x the C refs; 3a falls down: no sort; lazy reach is 3 rules, 2x its eager).
JOURNAL.md:47:- 2026-11-16T17:10:46Z merged DuckDB (all cells OK, lead rerun; 10-150x tuned C: every dense-id lookup is a hash join; P2 falls down: 45 rounds x 25 statements).
JOURNAL.md:89:- 2026-11-16T18:20:04Z merged second attempts (Soufflé 3a no longer falls down: 1.9 s ranked chain; DuckDB P2 falls down after 3) and GraphBLAS column (P1/P2 OK, 9-24x tuned C, does not scale: frontiers below its threading threshold); lead reran every check.
JOURNAL.md:94:settled two cells (Soufflé 3a now stands, DuckDB P2 falls down with three attempts written); the level-free floods
# LOOP: how a continuous run works

Read `CHARTER.md` first (the goal and the rules), then `PLAN.md` (the campaign). This file is
the procedure. A run has no deadline: `./overnight.sh` keeps sessions going until a `STOP`
file exists, and starts a fresh session on PLAN.md whenever one ends. A session ending is not
an event. Never decline or shrink a step because of time, usage or the session ending.

## Files

| file | who writes | what |
|---|---|---|
| `CHARTER.md` | user only | goal, rules, the workload ladder |
| `PLAN.md` | user: the campaign; agent: Milestones log and the Now line | the campaign, its oracle, kill and exit tests |
| `STATUS.md` | agent, at most ten lines | the oracle number, what moved, decisions for Dan |
| `JOURNAL.md` | agent, append-only | one line per commit; a paragraph at each milestone |
| `GRID.md` | agent | one cell per (system, problem): correct, times, size, easy / forced / impossible |
| `ATTACKS.md` | agent | new attacks on the problems, with evidence (CHARTER.md) |
| `systems/<name>/NOTES.md` | agent | the long form of a system's cells, with code excerpts |
| `MORNING.md` | agent, at milestones | the full report (format below) |
| `IDEAS.md` | agent | ideas, each with a kill test and status |
| `reviews/inbox/` | background reviewers | raw reviews, one file each |
| `REVIEWS.md` | agent | triage of every inbox item: accept / reject (why) / defer |

## Working

Work the campaign continuously. There are no numbered iterations.

- Keep the Now line in PLAN.md exact: the next concrete step, ideally the command to run.
  A fresh session with no memory resumes from it.
- Commit and push every working step, with tests for the code it touches
  (each cell's oracle check), and append one line to JOURNAL.md.
- Split the campaign into parts that a stock oracle can check, and hand independent parts to
  subagents in their own git worktrees, so they build in parallel.
- Run long measurements and fuzzing as background scripts that report once.
- Triage `reviews/inbox/*` when something is there: one line each in `REVIEWS.md`, then move
  it to `reviews/done/`. Never wait for a reviewer.

## At each milestone (PLAN.md lists them)

1. Measure the oracle number; append the milestone to PLAN.md's log.
2. Bring `GRID.md` up to date. Say plainly when a number got worse or a cell fell down.
3. Write a JOURNAL.md paragraph that ends with: **"What did the oracle number do since the last
   milestone? If it stalled, what is the blocker, and what am I doing about it?"**
4. Fire the background reviewers (below) on the milestone commit.
5. Overwrite `STATUS.md` and `MORNING.md`.

If several hours pass without a milestone, do steps 1, 3 and 5 anyway.

## Background reviewers (never block on them)

Run each in the background with a time limit; output goes to `reviews/inbox/`.
Run them from a second clone (`/home/exedev/paradigms-w` or a `git worktree`) so they don't
fight over the build directory. Reviewers must not edit code.

    # Codex skeptic
    timeout 40m codex exec --sandbox read-only --model gpt-6-astra -c model_provider=exe-llm \
      -c 'model_providers.exe-llm.name="exe-llm"' \
      -c 'model_providers.exe-llm.base_url="https://llm.int.exe.xyz/v1"' \
      "$(cat prompts/skeptic.md)" </dev/null > reviews/inbox/$(date +%Y%m%dT%H%M)-codex-skeptic.md 2>&1 &

A stalled or refused review gets one line in `REVIEWS.md` and is dropped. If a provider
flags a prompt, reword it around build speed and correctness; don't retry it unchanged.
Correctness findings can block a cell from counting. A finding that work is being avoided must be
answered with a measurement or a commit, not "defer".

## Usage limits

Your 5-hour model limit is shared with Dan's other VMs. Codex runs on its own budget
(`codex exec`, configured as in the reviewer commands above), so give it the work it does as
well as you:
- writing tests and differential cases for a part you've specified
- running and triaging the oracle, fuzzing and measurements, and reporting failures
- mechanical edits and re-reviews of a diff after a fix

Pick the model per job:
- **You (Opus)**: the campaign's direction, design, and diagnosis that has stalled. Fable is not
  used in this campaign.
- **Builders: `model: "opus"`** for a cell (a paradigm on a problem): writing idiomatic, tuned
  code in an unfamiliar system is real design work. One builder per system, in its own worktree,
  so the systems advance in parallel.
- **Sonnet**: prior-art readers (CHARTER.md, Prior art), installs, and subagents that only
  search or summarize.
- **Codex**: the work listed above.

Within that, don't ration. If the model limit is reached the session goes idle; `overnight.sh`'s
watchdog types a "continue" a minute after the limit's printed reset time. Commit often and
nothing is lost.

## Fresh contexts

Most of a long session's usage is re-reading its own context: every turn re-reads all of it.
In the sibling simd-lang run on 2026-11-12/13 the main context had a median of 495k tokens and peaked at 966k, so re-reading
was about 85% of the main thread's cost. The repo is your memory, so a fresh context loses nothing
that matters. The Stop hook tells you when the context passes 250k tokens. Then, at the next
clean point:

1. Wait for your subagents to finish (or stop them, and note in the Now line what they were doing).
2. Commit and push; make PLAN.md's Now line exact, the commands to run included.
3. `touch .claude/fresh` and end your turn.

The watchdog then types `/clear` and the start prompt, and you continue from the Now line with
a context of about 30k tokens. Also take a fresh context after each milestone.

## VM safety

- Claude Code stops for a person on an `rm` it can't rule out as dangerous (a glob or a
  variable it can't trace, in a compound command), even in bypass mode, and nobody is watching.
  Delete with literal paths in a command of its own (`rm -rf -- out` and then `mkdir out`), or
  guard variables (`rm -rf -- "${D:?}/out"`).
- 31 GiB RAM, no swap, and a runaway child OOM-kills this whole session. Cap fuzzers and
  anything with data-dependent allocation: `prlimit --as=8G`, `timeout`, or
  `systemd-run --user --scope -p MemoryMax=8G`.
- Big target projects: clone them under `/home/exedev/targets/`, not inside this repo.

## Stopping

The Stop hook (`.claude/hooks/keep-going.sh`) lets a session end only when `STOP` exists.
Then: bring STATUS.md and the Now line up to date, commit, push.

`MORNING.md` contains: GRID.md, the surprises (both directions), ATTACKS.md's new entries, ideas
tried and killed, review disagreements worth the user's attention, and decisions that are
the user's.
systems/ddflow/Cargo.toml
systems/ddflow/Cargo.lock
systems/goose/p1.build
systems/goose/flatio.c
systems/goose/IDEAS.md
systems/goose/p4.build
systems/goose/p2async.build
systems/goose/p2async.goose
systems/goose/p1par.build
systems/ddflow/src/main.rs
systems/ddflow/NOTES.md
systems/goose/scratch/loadtest.build
systems/goose/scratch/mail.goose
systems/goose/scratch/p4ord.c
systems/goose/scratch/thr.goose
systems/goose/scratch/loadtest.goose
systems/goose/scratch/bench-p4-run1.txt
systems/goose/par.goose
systems/goose/hello/gen.c
systems/goose/hello/c_ref.c
systems/goose/hello/hello8.goose
systems/goose/hello/run.sh
systems/goose/hello/build.sh
systems/goose/hello/build_from_source.sh
systems/goose/hello/hello1.goose
systems/goose/p4.goose
systems/goose/p3par.goose
systems/goose/CELLS.md
systems/goose/p2.goose
systems/goose/NOTES.md
systems/goose/p3.goose
systems/goose/p2rules.goose
systems/goose/p1.goose
systems/goose/p3.build
systems/goose/p2.build
systems/goose/p3par.build
systems/goose/bench-p4.sh
systems/goose/p1par.goose
systems/goose/p2lazy.goose
systems/goose/p2par.goose
systems/goose/bench.sh
systems/goose/flat.goose
systems/goose/p2lazy.build
systems/goose/levelfree.sh
systems/goose/p2par.build
systems/souffle/bench-20261116T1637Z.txt
systems/souffle/resolve_b.dl
systems/souffle/IDEAS.md
systems/souffle/bench-3a-20261116T1756Z.txt
systems/souffle/emit2.dl
systems/souffle/best.sh
systems/souffle/check_p4.c
systems/souffle/resolve_c2.dl
systems/souffle/check_p3.c
systems/souffle/emit.dl
systems/futhark/drv.h
systems/halide/IDEAS.md
systems/futhark/fbin.py
systems/futhark/p1.c
systems/futhark/IDEAS.md
systems/futhark/prof.py
systems/futhark/p4-flat-bench-2026-11-16T1840Z.txt
systems/futhark/p3b-blocks-bench-2026-11-16T1824Z.txt
systems/futhark/msort.fut
systems/futhark/p1.fut
systems/futhark/p3.fut
systems/souffle/hello/reach_c.cpp
systems/souffle/hello/load_c.cpp
systems/souffle/hello/gen.cpp
systems/souffle/hello/run.sh
systems/souffle/hello/build.sh
systems/souffle/hello/load.dl
systems/souffle/hello/reach.dl
systems/souffle/hello/reach_emb.dl
systems/souffle/hello/embed.cpp
systems/souffle/layout_rank.dl
systems/souffle/P4-REPORT.md
systems/souffle/resolve_a.dl
systems/souffle/CELLS.md
systems/souffle/resolve_c.dl

exec
/bin/bash -lc "cat systems/halide/p3.cpp; cat systems/goose/NOTES.md | tail -90; cat systems/duckdb/NOTES.md | tail -65; cat ATTACKS.md | sed -n '48,100p'; ls build data systems/halide/build 2>/dev/null; rg -n 'check|oracle|memcmp' systems/futhark/p3.c systems/halide/p*.cpp systems/graphblas/*.c" in /home/exedev/paradigms-w
 exited 2 in 0ms:
cat: systems/halide/p3.cpp: No such file or directory
- Both `goose run` and `goose build` work with the VM's own gcc and libc headers. First `build` compiles the runtime once ("compiling the runtime for gcc 13.3.0").

## What is implemented vs on paper

Read from the shipped docs (spec Appendix B, implementation.md section 10, bench/summary.md) and the source:

Implemented and exercised by the test suite and benchmarks (per the docs; I used a subset myself):
data stacks and the stack invariant, static roots / reference safety, slices, inline nested arrays
(`T[k]`, `T[]`, `T[..]`, `T[>..]`, `T[>..<]`), fixed and variable enums, relative references and
`slot_pool`/`slice_pool`, `to_bytes`/`from_bytes`, in-place construction, `return from` errors, generics
(monomorphized), function blocks, index types (`u32 in a`, `u32 upto a`) and `checkfree fn`, `simd fn`, thread_fn + typed
queues + contained workers, C interop (`extern fn`, `extern "lib" {}`, sidecar `.build` for own C), packages, LSP.

On paper / open (spec Appendix B, design/ notes), verified absent:
- Column arrays (`T[||..]`, struct of arrays): design/soa.md says "The feature is not implemented."
- Child threads / `par for` (fork-join over shared memory): design/child_threads.md is a proposal; `par for` appears
  in no spec section, reference, or source file.
- Move for resizables, more than one resizable per struct (root structs are the workaround), labeled break,
  mixed-type pools and pool GC, wasm backend, queue select/fairness, SIMD alignment, standard-library access beyond
  file/stdin/time/env (no subprocess, no network, no mmap).
- Bounds-check elimination: indirect indexing keeps its check unless the stored values have an index type; `u64`
  values lose range info (TODO 0a, 0f).

## Hello-world: what was written

`hello/hello1.goose`: `read_file` the whole file into a `u8[>..]` (the only file API is whole-file append: no mmap,
no pread, no seek, no partial read), decode u32 LE with `binary::read_u32le` (checked, per element; there is no
bitcast/reinterpret of a byte array to u32), inclusive prefix sum into `u64[>..]`, 256-bin histogram of the low 8 bits,
checksums. `hello/hello8.goose`: same across N workers, chunk copied to each over a typed queue, worker returns
(len, total, sum of local prefixes, histogram); main folds `base*len + lps`. `hello/gen.c` writes the input file and prints
the oracle; `hello/c_ref.c` is a C reference. Both Goose programs print identical checksums to the oracle.

Input: 50,000,000 xorshift64* u32 (200 MB, /home/exedev/tools/goose/u32_50M.bin, page-cached).
Oracle: `total=107384557821208332 prefix_checksum=2059448577320109362 hist_checksum=10908785116829868540` (all match).

### Timings (ms, wall by Goose `clock()`, three consecutive runs each, 2026-11-16 15:24 UTC, warm page cache)

| program | read | decode | scan+hist (+checksum) | send+spawn | workers+collect | total |
|---|---|---|---|---|---|---|
| C reference (gcc -O2, direct u32 reads) | 94 / 100 / 101 | n/a | 194 / 187 / 200 | | | 287 / 287 / 301 |
| Goose 1 thread (`hello1`, gcc -O2) | 103 / 114 / 117 | 171 / 169 / 175 | 226 / 213 / 236 | | | 501 / 496 / 528 |
| Goose 1 worker (`hello8 1`) | 132 / 108 / 106 | | | 218 / 223 / 225 | 537 / 526 / 526 | 887 / 858 / 856 |
| Goose 2 workers | 95 / 98 / 101 | | | 182 / 171 / 180 | 301 / 251 / 296 | 577 / 521 / 578 |
| Goose 4 workers | 141 / 130 / 104 | | | 223 / 185 / 161 | 188 / 166 / 166 | 552 / 480 / 431 |
| Goose 8 workers | 113 / 107 / 108 | | | 119 / 119 / 117 | 96 / 95 / 77 | 329 / 321 / 302 |

Reading: the 8-worker run is 1.6x faster than 1-thread Goose end to end (500 to ~320 ms) and about equal to C
single-threaded (287). The compute part alone (workers+collect) scales 533 to 77-96 ms (5.6-6.9x on 8). Everything else is serial in main:
the 100 ms file read and the 119 ms of copying chunks into queues (200 MB in ~119 ms = 1.7 GB/s, about 2% of the DRAM floor,
and it includes thread spawn). The checksum phase of `hello1` is 17 ms (a second pass over the prefix array).
Decode via `binary::read_u32le` costs about 170 ms per 50 M values (3.4 ns each), roughly as much as the whole scan:
Goose has no zero-copy view of bytes as u32, so a flat binary column costs a decode pass.

### Gotchas

- `goose run` (TinyCC) refuses programs with threads; use `goose build`.
- File API is whole-file only (`read_file` appends all bytes to a `u8[>..]`; `write_file`). No mmap, pread, seek. I did not add a C FFI
  workaround because the workload fit; the documented way is the program's own C via a sidecar `foo.build` plus `extern fn` (see docs/packages.md section 3), with C pointers passed as `u64` (Goose cannot read through them without `ffi::bytes`, which copies).
- No shared memory between threads, so a prefix sum cannot materialize one global array in parallel; I send chunks, return per-chunk
  totals and fold. Writing the final prefix would need a second queue round trip per chunk (and queues are per type,
  not per worker, so a worker can't be addressed without a distinct message type per worker).
- Queue elements must be flat; a 25 MB `u8[]` chunk worked fine as one message.
- Unsigned overflow wraps silently in release builds; `--debug` adds checks. `u32` converts implicitly to `u64`
  (a redundant `as u64` is a warning); `u64` against signed needs casts.
- Thread count is `hardware_threads()` (8 here); `hello8` takes an optional one-character count.

## Source build

`hello/build_from_source.sh`: cmake 4.4.5 from a pip venv, `-DGOOSE_BUILTIN_LIBS=none`, `-j8`: 1m48s wall on 8 cores, binary
`/home/exedev/tools/goose/build/goose`, `goose v2026.4-43-g3a0aa2eb (runtime a9ca9ac6fac6)` (43 commits past the v2026.4 tag).
Compiled `hello1.goose` with it: identical checksums, total 487 ms (same as the release binary within noise). The release
binary is what the timings above used.

## Judgment: Goose's memory model against a linker's data

For the flat, big, long-lived columns (5.2 M symbols, 9.4 M relocations) Goose's model fits well and costs nothing: each
column is a global `T[>..]` grow-only array (one stack each, bump pointer, no moves), links are `u32 in column` index types
validated once at load, and `checkfree fn` proves the flood and relocation loops have no bounds checks, which is the
Zig-compiler "indices not pointers" design with the compiler checking it (the bench `graph_csr` is exactly a CSR flood).
Struct-of-arrays has to be done by hand since column arrays are unimplemented, and decoding the columns from files costs a
pass (about 3.4 ns per u32), because there is no mmap or byte-to-u32 view. A hash join is hand-written open addressing in
arrays, bounds-check-free only if the slot values are typed as indices (the bench `words` keeps four checks); the 0.74 G/s
random-load floor is untouched by the language, and the winner rule is ordinary code. The graph flood is a single-program
algorithm: the visited set cannot be shared among workers, so it runs on one core unless it is partitioned by owner with per-level message exchange
(the only parallel construct Goose has today is isolated programs; shared-memory `par for` is a design note). The scatter into
the 109 MB image has the same constraint: one array in one program, so parallel scatter means workers build disjoint segments and
send them back as copies, and my measured queue throughput (about 1.7 GB/s including spawn) puts a 109 MB hop near 60 ms
against a floor of about 1.2 ms. So the memory model maps well onto layout, resolve and reach as single-thread, bounds-check-free
column code and badly onto the 8-core goal; expect Goose to land near the C single-thread number with less code, and
not near hlink's parallel result, unless child threads arrive.

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
build:
p1par
p2lazy
p2par
p3par

data:
goroot-p4
kube-apiserver
kube-p4
minio
minio-edit
minio-edit2
minio-p4
rg: systems/halide/p*.cpp: No such file or directory (os error 2)
systems/graphblas/p2gb.c:6:// then the method check, until no method adds a live symbol. Checks oracle/p2_live.u32 and writes out/p2_live.u32.
systems/graphblas/p2gb.c:196:	long bad = check32("live", list, nlive, "oracle/p2_live.u32");
systems/graphblas/p1gb.c:5://       checked to hold one full key (the guard: MIN and MAX of k1 and k2 per group must agree).
systems/graphblas/p1gb.c:8:// Checks oracle/p1_canon.u32, p1_builtin.u32, p1_reltarget.u32, p1c_canon.u32; writes them to out/.
systems/graphblas/p1gb.c:49:// group by key index: w(h(i)) = MAX over i of x(i); h and x have the same pattern. With the guard, also check that
systems/graphblas/p1gb.c:313:	bad += check32("canon", cOut, N, "oracle/p1_canon.u32");
systems/graphblas/p1gb.c:314:	bad += check32("builtin", bOut, nb, "oracle/p1_builtin.u32");
systems/graphblas/p1gb.c:315:	bad += check32("reltarget", tOut, nrel, "oracle/p1_reltarget.u32");
systems/graphblas/p1gb.c:316:	bad += check32("canon after 1c", c2Out, N, "oracle/p1c_canon.u32");
systems/futhark/p3.c:1:// p3.c: driver for p3.fut. Loads the flat files, calls the entries, times them, checks against the oracles.
systems/futhark/p3.c:2://   p3 DATADIR THREADS REPS CELL [nocheck]     CELL: 3a 3a_loop 3b 3b_bytes 3b_blk
systems/futhark/p3.c:10:	int check = !(argc > 5 && strcmp(argv[5], "nocheck") == 0);
systems/futhark/p3.c:25:		// 3a: radix sort + segmented scan; 3a_loop: sequential cursor loop; 3a_cells, 3a_tl, 3a_radix: pieces (no check)
systems/futhark/p3.c:28:		if (strcmp(cell, "3a_cells") == 0) entry = futhark_entry_piece_cells, check = 0;
systems/futhark/p3.c:29:		if (strcmp(cell, "3a_tl") == 0) entry = futhark_entry_piece_typelinks, check = 0;
systems/futhark/p3.c:30:		if (strcmp(cell, "3a_radix") == 0) entry = futhark_entry_piece_radix, check = 0;
systems/futhark/p3.c:34:			fut_check(entry(ctx, &out, nunits, minFunc, bstart, bmin, bucket, cls, size, unit, sym, tlstr, soff, sbin), cell);
systems/futhark/p3.c:39:		if (check) bad += check32("addr", addr, L, "oracle/p3_addr.u32");
systems/futhark/p3.c:42:		// 3b runs on the oracle's addresses, as ref/flat/p3.c does, so it does not depend on 3a
systems/futhark/p3.c:43:		struct futhark_u32_1d *addr = LOAD(u32, "oracle/p3_addr.u32"), *all = LOAD(u32, "p3_addr_all.u32");
systems/futhark/p3.c:55:		// 3b: chunked copy + patch; 3b_bytes: per-byte copy + patch; 3b_blk: blocked copy + patch; 3b_copy, 3b_patch: the halves alone (no check)
systems/futhark/p3.c:59:		if (strcmp(cell, "3b_blkcopy") == 0) entry = futhark_entry_copy_blocks_only, check = 0;
systems/futhark/p3.c:60:		if (strcmp(cell, "3b_copy") == 0) entry = futhark_entry_copy_only, check = 0;
systems/futhark/p3.c:61:		if (strcmp(cell, "3b_patch") == 0) entry = futhark_entry_patch_only, check = 0;
systems/futhark/p3.c:68:			if (fresh) fut_check(futhark_context_clear_caches(ctx), "clear_caches");
systems/futhark/p3.c:69:			fut_check(entry(ctx, &out, nimg, imageBase, addr, bucket, doff, dbin, rstart, roff, rsiz, rkind, radd, rtgt, all, fsect,
systems/futhark/p3.c:81:		if (check) {
systems/futhark/p3.c:82:			const u8 *want = FMAP(u8, "oracle/p3_image.bin", NULL), *pb = FMAP(u8, "p3_bucket.u8", NULL);
systems/futhark/p3.c:83:			const u32 *oaddr = FMAP(u32, "oracle/p3_addr.u32", NULL), *psize = FMAP(u32, "p3_size.u32", NULL);
systems/futhark/p3.c:88:				if (memcmp(img + o, want + o, psize[i]) != 0 && nbad++ < 5) printf("  symbol %u (bucket %u) differs\n", i, pb[i]);
systems/futhark/p3.c:90:			printf("image: %ld placed symbols differ from oracle/p3_image.bin\n", nbad);
systems/futhark/p3.c:95:	if (check) printf(bad ? "P%s FAIL\n" : "P%s OK\n", cell);

codex
Several earlier findings have already been corrected: Soufflé layout is no longer marked “falls down,” and the grid now discloses Goose’s omitted P4 join cost. I’m checking the current oracle code so the review distinguishes remaining gaps from fixed ones.
exec
/bin/bash -lc "rg --files systems/halide; cat systems/goose/NOTES.md | sed -n '125,155p'; cat systems/duckdb/NOTES.md | sed -n '105,133p'; rg -n 'oracle|check|memcmp|OK' systems/halide/*.c; cat systems/graphblas/NOTES.md | head -65; wc -l JOURNAL.md; rg -n 'oracle|check|OK' systems/goose/p3*.goose" in /home/exedev/paradigms-w
 succeeded in 0ms:
systems/halide/IDEAS.md
systems/halide/hello/mkdata.cpp
systems/halide/hello/driver.cpp
systems/halide/hello/gen.cpp
systems/halide/hello/run.sh
systems/halide/hello/build.sh
systems/halide/P4-REPORT.md
systems/halide/CELLS.md
systems/halide/NOTES.md
systems/halide/bench.sh
systems/halide/cells/gen_p3.cpp
systems/halide/cells/p2.cpp
systems/halide/cells/p4.cpp
systems/halide/cells/build.sh
systems/halide/cells/bench.h
systems/halide/cells/gen_p1.cpp
systems/halide/cells/p3.cpp
systems/halide/cells/gen_p2.cpp
systems/halide/cells/gen_p4.cpp
systems/halide/cells/p4_copy32.sh
systems/halide/cells/p1.cpp
systems/halide/cells/join.h
systems/halide/levelfree.sh
- **1c** easy: same shape as 1a. Forced: same shuffle. Impossible: nothing.
- **2 CSR** easy: `checkfree fn drain` over index-typed columns (`u32 in live`, `u32 upto edge_dst`), validated once by
  `from_bytes` at load: zero bounds checks in the flood. Forced: on 8, owner-partitioned BSP with per-owner inbox types and
  main merging the global facts (signatures, reflect, names). Impossible: a shared visited set.
- **2 lazy** easy: the decode is ten lines inside the drain. Forced: cutting relocation edges out of the p2 rows at load;
  every worker holds its own canon. Impossible: nothing.
- **3a** easy: count per (unit, bucket, class), scan, assign; `sort` with a block comparator for typelinks. Forced: on 8,
  counts to main and bases back (three message rounds). Impossible: nothing.
- **3b** easy: `img[base..].move_from(bytes)` and a match on relocation kind. Forced: byte-wise stores; the hugetlb
  output only through C under the runtime. Impossible: one image written by 8 workers in place (they can't share it);
  the gathered 8-worker image is slower than 1 thread.

### Falls down

- **8-core 3b with one output array**: two attempts (workers build segments and main gathers: 3b 53-150 ms gathered;
  then the image left distributed, which is fast, 17-24 ms, but is not one array). Reason: no shared memory, and a queue
  hop copies twice. Distributed is the idiomatic answer and the one reported as the cell.
- **8-core P2 near C**: CSR and lazy floods both scale ~1.7x over 8 workers (two attempts: one queue type per owner with
  batched inboxes and BSP; the lazy variant; a third, asynchronous with no supersteps, p2async: 1.3-1.5x faster than BSP
  under load, still ~1.3-1.9x over 1 worker). Reason: per-superstep message rounds; Goose has no shared-memory
  `par for` (a design note only).

### Core code excerpt (p2lazy.goose, the lazy part of the flood)

```
        if decodes(c) {
            // the lazy part: this symbol's relocations, decoded now
            var skip = 0;     // type 26 hides the next two relocations
            for r in slot_rel[c]..slot_rel[c as i64 + 1] {
                if skip > 0 { skip--; continue; }
                let ty = rel_type[r];
attempts: (1) edges by a bucketed range join, 16.9 s prep; (2) edges as lists + the rules' flag tests as small
relations (`gt`, `rf`) instead of joins with the 5.2 M symbols. A recursive CTE cannot express it (the method
rules need the global sig/named/reflect sets, and a recursive CTE may reference itself once).

**P2 attempt 3 (sql/p2rec.sql, 54 lines + `.repeat`), a different shape: whole floods as recursive CTEs.** The
method rules need global sets, so one CTE cannot hold everything, but they only matter between floods: the C
reference (p2.c) floods to a fixpoint, then decides the calls, 7 rounds on MinIO. So a round is: (1) the plain
closure of the seeds in one WITH RECURSIVE (DuckDB keeps the recursion's dedup table across its iterations); (2) the
signatures, names and reflect the new live code names (three INSERTs); (3) the used-in-interface closure in a second
WITH RECURSIVE over `ie` (gotype -> gotype plain edges and type -> method type), expanding only live, unexpanded
types; (4) the callable candidates, whose mtyp/ifn/tfn are the next seeds. Tuning, each measured: edges deduplicated
(8.15 M -> 5.18 M) and split into inner (target has edges: 2.73 M, walked by the recursion) and leaf (added by one
join afterwards): the round-1 closure 1.0-2.0 s -> 0.47-0.53 s under load ~20; the working edge set `ew` and the
interface edges `iel` pruned as their targets become live/expanded, so no recursion carries an anti join against
the growing live set (that rebuilt live's hash table per iteration: the first version took 3.7 s); `rf` instead of a
join with `sym` for reflect; seeds not filtered against live (a live seed has no edges left in `ew`). Not kept:
sorting the edges by src (zonemaps prune nothing: frontiers are spread over the id range; within noise). Result,
exact: flood 1.6x faster than attempt 2 on 8 threads, cell ~1.2x; 1 thread slower (prep 2.1 s).
Why it stops there (both measured): (a) **every iteration of a recursive CTE re-runs its join with the edge table as
the probe side**, a full scan of 2.7 M rows however small the frontier; round 1 alone is 35 BFS levels (frontiers
16 to 24 K, `p2levels.py`), all 7 rounds ~100+ iterations, so the cost is levels x edges, as in attempts 1-2;
(b) **no lookup that is not a scan or a hash build**: an ART index on `src` is used for a join only when the build side
is tiny (`x_artjoin.sql`, synthetic 5 M edges, `duckdb < x_artjoin.sql`: 40-row frontier 2 ms with the index vs 9-19 ms; 2,000-row frontier 12 vs 15 ms, even
with `dynamic_or_filter_threshold` and `index_scan_max_count` raised), and a dense array as a `getvariable` LIST
copies the list per call (1,000 probes 7.7 s). The frontier-against-a-persistent-index shape is therefore not
available, and batching levels would need E^k edges. **Verdict: falls down** after three attempts, ~140x tuned C.

**3a (layout).** Mostly easy: occupied sizes, per-(unit, bucket, class) sums and every in-class position are window
sums. Forced: "va = round_up(va, align); va += n" over the units is a fold, not a prefix sum (a round-up does not
rg: systems/halide/*.c: IO error for operation on systems/halide/*.c: No such file or directory (os error 2)
# SuiteSparse:GraphBLAS (sparse linear algebra over semirings)

SuiteSparse:GraphBLAS **10.5.2** (GitHub release tarball, 2026-10-25), built from source with cmake 3.28 (apt) and
gcc 13.3, OpenMP on, two builds:

- `/home/exedev/tools/graphblas` (JIT **off**, `-DGRAPHBLAS_USE_JIT=OFF`): only the precompiled "factory" kernels;
  anything else (e.g. `apply` with a bound bit-shift, most `extract` on uint32) runs a generic, function-pointer kernel.
  Build 11.5 min with `make -j6` at load ~15.
- `/home/exedev/tools/graphblas-jit` (JIT **on**): kernels for the exact types/ops are compiled with `cc` on first use
  and cached in `~/.SuiteSparse/GrB10.5.2` (first run of a cell compiles ~30 kernels, a few seconds; later runs load
  them). This is the tuned build: `p1gb-jit`, `p2gb-jit`.

Sources: `/home/exedev/tools/graphblas-src/GraphBLAS-10.5.2` (`build/`, `build-jit/`). No LAGraph: its BFS is a pure
BFS, and the method rules need the loop below anyway.

Threads: `GB_THREADS=n` (sets `GxB_NTHREADS`); `OMP_WAIT_POLICY=passive` for all timings (GraphBLAS runs on OpenMP,
and the active policy has the same barrier problem as ATTACKS 3). `GB_BURBLE=1` prints SuiteSparse's per-call
diagnostics (method chosen, threads, time): every profile below comes from it.

## Surprises

- **The flood does not parallelise, because GraphBLAS parallelises per call and each call is small.** p2gb
  makes 382 vxm/mxv calls; the average BFS frontier is ~7k nodes (663k live / 97 levels), below SuiteSparse's work
  grain (`GxB_CHUNK`, 64k), so almost every call runs on one thread (burble, 8 threads: 379 of 382 vxm/mxv calls on 1 thread). The
  wall time is flat in the thread count: best of 7 at load ~7 (18:10Z), 1/2/4/8 threads 192 / 188 / 187 / 173 ms, while
  process CPU time grows 192 -> 256 ms (spinning up teams that have nothing to do). bench.sh: 1 thread 192-203 ms,
  8 threads 179-213 ms (ranges overlap). A finer grain (`GB_CHUNK=8192`) doubles CPU time (400 ms) for 161-223 ms
  wall, within noise; at load ~18 the same setting was 2-3x slower (1.1-1.5 s). The C level-synchronous flood
  (p2par) gets 30-40 -> 15-26 ms from the same 8 cores, because a level there is one parallel loop over the frontier,
  not ~6 library calls each deciding its own thread count. One solve is 1,166 GraphBLAS calls (382 of them
  vxm/mxv), ~190 us each on average.
- **Push/pull direction optimisation is a 40x loss here.** Pull (`mxv` over A0', unvisited rows) 11.5 s, push 0.26-0.89 s
  (1 thread, same window); storing A0 by column so SuiteSparse picks itself ("col": dot products) 14.3 s. Only 13% of
  the nodes are ever live, so the unvisited set is never small. SuiteSparse does not switch direction by itself for
  `vxm`; it takes the format you give it.
- **Every cell matched its oracle on the first run** (P2, 1a, builtins, 1b, 1c), and the code reads as the rules.
- **A 128-bit key does not fit a GraphBLAS index (60 bits).** Resolve hashes (k1, k2) to 60 bits and checks every
  group for a single full key (MIN and MAX of k1 and of k2 per group agree): 0 collisions on MinIO, but the guard
  costs 4 extra builds per grouping, ~+40% on 1a and 1c. Without the guard, a collision would be silent.
- **JIT on vs off is worth little** here: P2 flood 203-233 vs 219-261 ms, P1 (no guard) 2.46-3.04 vs 2.94-3.32 s (1
  thread, interleaved, load 13). The cost is in sorts (build), hashed lookups and whole-vector passes, not in the
  generic kernels.

## Layout

| file | what |
|---|---|
| `gb.h` | the flat-file reader (`ref/flat/flat.h`), an error check, iso-bool matrix/vector builders |
| `p2gb.c` | P2 reach: loader (CSR rows -> tuples per edge kind), the rule matrices, the round loop |
| `p1gb.c` | P1 resolve: 1a, builtins, 1b, 1c, as whole-vector operations; flat files loaded zero-copy (`GxB_Vector_load`, read-only) |
| `build.sh` | builds `p1gb p2gb` (JIT off) and `p1gb-jit p2gb-jit` (JIT on) |
| `bench.sh` | every timing, interleaved with the tuned C references `p2par`, `p2lazy`, `p1par` |
| `IDEAS.md` | what the measurements suggest |

Knobs: `GB_THREADS`, `GB_DIR=push|pull|col` (P2), `GB_CHUNK` (P2), `GB_JOIN=mxv|extract` (P1's join back),
`GB_NOGUARD=1` (P1 without the key-collision guard), `GB_BURBLE=1`.

## Oracle checks

    cd systems/graphblas && ./build.sh && D=/home/exedev/paradigms/data/minio
    GB_THREADS=8 ./p2gb-jit $D 1 && cmp out/p2_live.u32 $D/oracle/p2_live.u32 && echo P2 ok
    GB_THREADS=8 ./p1gb-jit $D 1 && for f in p1_canon p1_builtin p1_reltarget p1c_canon; do cmp out/$f.u32 $D/oracle/$f.u32 && echo ok $f; done

Each program also compares in-process (`check32`) and prints `P2 OK` / `P1 OK` (P1 also requires 0 key collisions).
All pass (2026-11-16T17:30Z first, every later run since): P2 with push, pull and col; P1 with both joins, with and
142 JOURNAL.md
systems/goose/p3.goose:3:// relocations (ref/flat/README.md; ref/flat/p3.c is the plain C form). Checks itself against oracle/p3_addr.u32
systems/goose/p3.goose:4:// and every placed symbol's bytes in oracle/p3_image.bin.
systems/goose/p3.goose:6:// --pipeline: 3b relocates against this program's own 3a addresses, spread to every id through oracle/p1c_canon.u32
systems/goose/p3.goose:161:    var bad = check32("addr", addr, "oracle/p3_addr.u32");
systems/goose/p3.goose:162:    // 3b's addresses: the oracle's (so that 3b does not depend on 3a), or this program's own (--pipeline)
systems/goose/p3.goose:173:        let canon = load_u32("oracle/p1c_canon.u32");
systems/goose/p3.goose:176:        load("oracle/p3_addr.u32", oaddr);
systems/goose/p3.goose:199:    load("oracle/p3_image.bin", want);
systems/goose/p3.goose:227:           " placed symbols differ from oracle/p3_image.bin");
systems/goose/p3.goose:233:    print(what, if bad == 0 { " OK" } else { " FAIL" });
systems/goose/p3par.goose:42:var addr: u32[>..] = [];        // 3b: the oracle's addresses, so 3b does not depend on 3a (as in ref/flat/p3.c)
systems/goose/p3par.goose:66:    load("oracle/p3_addr.u32", addr);
systems/goose/p3par.goose:344:    var bad = check32("addr", addr, "oracle/p3_addr.u32");
systems/goose/p3par.goose:345:    let oaddr = load_u32("oracle/p3_addr.u32");
systems/goose/p3par.goose:349:    load("oracle/p3_image.bin", want);
systems/goose/p3par.goose:381:    format(note, nbad, " placed symbols differ from oracle/p3_image.bin");
systems/goose/p3par.goose:384:    print(if bad == 0 { "P3 OK" } else { "P3 FAIL" });

exec
/bin/bash -lc 'cat systems/halide/cells/p3.cpp; cat systems/souffle/check_p3.c; cat systems/futhark/p4.c | tail -85; cat systems/halide/cells/p4.cpp | tail -60; cat JOURNAL.md | head -28' in /home/exedev/paradigms-w
 succeeded in 0ms:
// p3 driver: loads the flat files (mmap; ref/flat/flat.h), wraps them zero-copy as Halide buffers, runs the AOT
// Layout (3a) and Relocate (3b) pipelines, checks against the oracles.
//   p3 DATA [all|3a|3b|V]    every schedule variant (or one), warm (bench.h), each checked; 3b also with a fresh
//                            output mapping per run, 4 KB and 2 MB pages, with its page faults
//   p3 DATA e2e LV RV [out]  one run of layout variant LV and relocate variant RV (index into the tables), checked
// Threads: HL_NUM_THREADS.
#include "../../../ref/flat/flat.h"
#include "HalideBuffer.h"
#include <functional>
#include <string>
#include <sys/resource.h>
#include "bench.h"
#include "p3_variants.h"  // generated by build.sh: #includes and the tables below
using Halide::Runtime::Buffer;

// typelink comparator, called from the Halide merge sort (tl_less extern in gen_p3.cpp)
static const u64 *soff;
static const u8 *sbin;
static const u32 *tlstr, *psym;
extern "C" int tl_less(int x, int y) {
    u64 xo = soff[tlstr[x]], xn = soff[tlstr[x] + 1] - xo, yo = soff[tlstr[y]], yn = soff[tlstr[y] + 1] - yo;
    u64 n = xn < yn ? xn : yn;
    int c = n ? memcmp(sbin + xo, sbin + yo, n) : 0;
    if (c) return c < 0;
    if (xn != yn) return xn < yn;
    return psym[x] < psym[y];
}

template <typename T> static Buffer<T, 1> wrap(const char *name, size_t *cnt = nullptr) {
    size_t n;
    const T *p = FMAP(T, name, &n);
    if (cnt) *cnt = n / sizeof(T);
    return Buffer<T, 1>((T *)p, (int)(n / sizeof(T)));
}
static double best(std::function<void()> f, int reps = 5) {
    double b = 1e30;
    for (int i = 0; i < reps; i++) {
        double t = now();
        f();
        b = std::min(b, (now() - t) * 1e3);
    }
    return b;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    double tstart = now();
    flatDir = argc > 1 ? argv[1] : "data/minio";
    std::string mode = argc > 2 ? argv[2] : "all";
    u32 L = (u32)metaGet("p3_placed"), nunits = (u32)metaGet("nunits"), minFunc = (u32)metaGet("minFunc");
    u32 imageBase = (u32)metaGet("imageBase"), ntl = (u32)metaGet("p3_typelinks");
    size_t nimg = metaGet("image_bytes");
    if (ntl >= 65536) { fprintf(stderr, "typelinks %u: raise the merge sort's run limit in gen_p3.cpp\n", ntl); return 2; }
    auto bucket = wrap<u8>("p3_bucket.u8"), cls = wrap<u8>("p3_class.u8");
    auto size = wrap<u32>("p3_size.u32"), unit = wrap<u32>("p3_unit.u32");
    auto bstart = wrap<u32>("p3_bucket_start.u32"), bmin = wrap<u32>("p3_bucket_minalign.u32");
    psym = FMAP(u32, "p3_sym.u32", NULL);
    tlstr = FMAP(u32, "p3_tlstr.u32", NULL);
    soff = FMAP(u64, "p3_str.off.u64", NULL);
    sbin = FMAP(u8, "p3_str.bin", NULL);
    auto tlstrB = wrap<u32>("p3_tlstr.u32");
    auto soffB = wrap<u64>("p3_str.off.u64");
    auto sbinB = wrap<u8>("p3_str.bin");
    auto doff = wrap<u64>("p3_data.off.u64");
    auto dbin = wrap<u8>("p3_data.bin");
    auto rstart = wrap<u32>("p3_rel_start.u32"), roff = wrap<u32>("p3_rel_off.u32"), rtgt = wrap<u32>("p3_rel_tgt.u32");
    auto rsiz = wrap<u8>("p3_rel_siz.u8"), rkind = wrap<u8>("p3_rel_kind.u8");
    auto radd = wrap<i64>("p3_rel_add.i64");
    auto addrAll = wrap<u32>("p3_addr_all.u32");
    Buffer<u32, 1> sect(8);
    const char *sn[8] = {"lay_text", "lay_rodata", "lay_pclntab", "lay_types", "lay_noptrdata", "lay_data", "lay_bss", "lay_noptrbss"};
    for (int k = 0; k < 8; k++) sect(k) = (u32)metaGet(sn[k]);
    u32 unreach = (u32)metaGet("addr_unreachableMethod"), mapinit = (u32)metaGet("addr_mapinitnoop");
    double tload = now();

    Buffer<u32, 1> addr(L);  // 3b alone (mode = a reloc name) starts from the oracle addresses
    memcpy(addr.data(), FMAP(u32, "oracle/p3_addr.u32", NULL), 4 * (size_t)L);
    u8 *imgp = (u8 *)calloc(nimg + 8, 1);  // + 8 scratch bytes (gen_p3.cpp)
    Buffer<u8, 1> img(imgp, (int)nimg + 8);
    Buffer<int32_t, 0> nerr = Buffer<int32_t, 0>::make_scalar();
    auto runLayout = [&](int v) { layouts[v].fn(bucket, cls, size, unit, bstart, bmin, tlstrB, soffB, sbinB, (int)nunits, (int)ntl, minFunc, addr); };
    auto runReloc = [&](int v) {
        relocs[v].fn(bucket, addr, doff, dbin, rstart, roff, rsiz, rkind, radd, rtgt, addrAll, sect, imageBase, unreach, mapinit, img, nerr);
    };
    // 3b check: every placed symbol's [addr, addr+size) equals the oracle image
    size_t nwant;
    const u8 *want = nullptr;
    auto checkImage = [&]() {
        if (!want) want = FMAP(u8, "oracle/p3_image.bin", &nwant);
        long nbad = 0;
        for (u32 i = 0; i < L; i++) {
            u32 b = bucket(i);
            if (b >= 13 && b <= 15) continue;
            size_t o = addr(i) - imageBase;
            if (memcmp(imgp + o, want + o, size(i)) != 0 && nbad++ < 5) printf("  symbol %u (bucket %u) differs\n", i, b);
        }
        printf("image: %ld placed symbols differ from oracle/p3_image.bin, %d relocation errors\n", nbad, nerr());
        return nbad + (nerr() != 0);
    };
    const char *th = getenv("HL_NUM_THREADS");
    long bad = 0;
    if (mode == "e2e") {
        int lv = argc > 3 ? atoi(argv[3]) : 0, rv = argc > 4 ? atoi(argv[4]) : 0;
        double t0 = now();
        runLayout(lv);
        double t1 = now();
        runReloc(rv);
        double t2 = now();
        if (argc > 5) { FILE *f = fopen(argv[5], "wb"); fwrite(imgp, 1, nimg, f); fclose(f); }
        double t3 = now();
        printf("e2e threads=%s layout=%s reloc=%s: load %.1f ms, 3a %.1f ms, 3b %.1f ms, write %.1f ms, total %.1f ms\n", th ? th : "default",
               layouts[lv].name, relocs[rv].name, (tload - tstart) * 1e3, (t1 - t0) * 1e3, (t2 - t1) * 1e3, (t3 - t2) * 1e3, (t3 - tstart) * 1e3);
        bad += check32("addr", addr.data(), L, "oracle/p3_addr.u32") != 0;
        bad += checkImage();
    } else {
        printf("threads=%s\n", th ? th : "default");
        for (int v = 0; v < nLayouts; v++) {
            if (mode != "all" && mode != "3a" && mode != layouts[v].name) continue;
            Stat s = timeRuns([&] { runLayout(v); });
            printf("3a %-16s " STAT_FMT "   ", layouts[v].name, STAT_ARGS(s));
            bad += check32("addr", addr.data(), L, "oracle/p3_addr.u32") != 0;
        }
        memcpy(addr.data(), FMAP(u32, "oracle/p3_addr.u32", NULL), 4 * (size_t)L);
        for (int v = 0; v < nRelocs; v++) {
            if (mode != "all" && mode != "3b" && mode != relocs[v].name) continue;
            // warm: the image buffer is reused (its pages are already mapped)
            memset(imgp, 0, nimg);
            Stat s = timeRuns([&] { runReloc(v); });
            bad += checkImage();
            printf("3b %-16s warm        " STAT_FMT "\n", relocs[v].name, STAT_ARGS(s));
            // fresh: a new zeroed mapping per run, its page faults inside the timed region (ATTACKS.md 2): 4 KB
            // pages (mmap anonymous) and 2 MB pages (MAP_HUGETLB: needs vm.nr_hugepages >= 53)
            for (int huge = 0; huge < 2; huge++) {
                size_t len = (nimg + 8 + (2u << 20) - 1) & ~(size_t)((2u << 20) - 1);
                long faults = 0, n = 0;
                bool ok = true;
                Stat sf = timeRuns([&] {
                    struct rusage r0, r1;
                    getrusage(RUSAGE_SELF, &r0);
                    void *m = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | (huge ? MAP_HUGETLB : 0), -1, 0);
                    if (m == MAP_FAILED) { ok = false; return; }
                    Buffer<u8, 1> im((u8 *)m, (int)nimg + 8);
                    relocs[v].fn(bucket, addr, doff, dbin, rstart, roff, rsiz, rkind, radd, rtgt, addrAll, sect, imageBase, unreach, mapinit, im, nerr);
                    munmap(m, len);
                    getrusage(RUSAGE_SELF, &r1);
                    faults += r1.ru_minflt - r0.ru_minflt, n++;
                });
                if (!ok) { printf("3b %-16s %s: mmap failed (huge pages reserved?)\n", relocs[v].name, huge ? "2 MB" : "4 KB"); continue; }
                {   // check a fresh image too (untimed; M3 review): it must equal the reused buffer, which checkImage() compared with the oracle
                    void *m = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | (huge ? MAP_HUGETLB : 0), -1, 0);
                    if (m == MAP_FAILED) { bad++; printf("3b fresh check: mmap failed\n"); }
                    else {
                        Buffer<u8, 1> im((u8 *)m, (int)nimg + 8);
                        relocs[v].fn(bucket, addr, doff, dbin, rstart, roff, rsiz, rkind, radd, rtgt, addrAll, sect, imageBase, unreach, mapinit, im, nerr);
                        int d = memcmp(m, imgp, nimg) != 0;
                        printf("3b %-16s fresh %s image %s the checked one\n", relocs[v].name, huge ? "2MB" : "4KB", d ? "DIFFERS from" : "equals");
                        bad += d;
                        munmap(m, len);
                    }
                }
                printf("3b %-16s fresh %s " STAT_FMT ", %ld minor faults/run\n", relocs[v].name, huge ? "2MB" : "4KB", STAT_ARGS(sf), faults / n);
            }
        }
    }
    printf(bad ? "P3 FAIL\n" : "P3 OK\n");
    fflush(stdout);
    return bad != 0;
}
// check_p3.c: compare an image with oracle/p3_image.bin on every placed symbol's [addr, addr+size) (README 3b),
// and optionally a 3a address array with oracle/p3_addr.u32.   check_p3 DATA IMAGE [ADDR.u32]
#include "../../ref/flat/flat.h"
int main(int argc, char **argv) {
	if (argc < 3) { fprintf(stderr, "usage: check_p3 DATA IMAGE [ADDR.u32]\n"); return 2; }
	flatDir = argv[1];
	u32 L = (u32)metaGet("p3_placed"), base = (u32)metaGet("imageBase");
	size_t nw, ng, na;
	const u8 *want = FMAP(u8, "oracle/p3_image.bin", &nw), *b = FMAP(u8, "p3_bucket.u8", NULL);
	const u32 *addr = FMAP(u32, "oracle/p3_addr.u32", NULL), *size = FMAP(u32, "p3_size.u32", NULL);
	int bad = 0;
	if (argc > 3) {
		const char *d = flatDir;
		flatDir = argv[3][0] == '/' ? "" : ".";
		const u32 *got = FMAP(u32, argv[3], &na);
		flatDir = d;
		long n = 0;
		for (u32 i = 0; i < L; i++) n += i >= na / 4 || got[i] != addr[i];
		printf("3a: %ld of %u addresses differ%s\n", n, L, na / 4 != L ? " (length differs)" : "");
		bad |= n != 0 || na / 4 != L;
	}
	flatDir = argv[2][0] == '/' ? "" : ".";
	const u8 *got = FMAP(u8, argv[2], &ng);
	long n = 0;
	for (u32 i = 0; i < L; i++) {
		if (b[i] >= 13) continue;
		size_t o = addr[i] - base;
		if (o + size[i] > ng || memcmp(got + o, want + o, size[i])) n++;
	}
	printf("3b: %ld of %u placed symbols differ\n", n, L);
	bad |= n != 0;
	printf(bad ? "P3 FAIL\n" : "P3 OK\n");
	return bad;
}
	if (strcmp(cell, "4lay") == 0) {
		struct futhark_i64_1d *at = NULL;
		TIMEIT(cell, {
			if (at) futhark_free_i64_1d(ctx, at);
			fut_check(futhark_entry_layout(ctx, &at, item_kind, item_ref, item_seg, glue_off, node_tmpl, tmpl_off, tmpl_seg_off, tseg_end), cell);
		});
		fut_report();
		return 0;
	}
	struct TUP *out = NULL;
	if (strcmp(cell, "4D") == 0) {
		struct futhark_opaque_stream *st = NULL;
		int r0 = reps;
		reps = getenv("PREP_REPS") ? atoi(getenv("PREP_REPS")) : 3;
		TIMEIT("prepare", {
			if (st) futhark_free_opaque_stream(ctx, st);
			fut_check(futhark_entry_prepare(ctx, &st, fn_item, fn_mark_off, mark_off, mark_type, mark_sym, mark_add, item_kind,
			                                item_ref, item_seg, glue_off, glue_bin, glue_rel_off, grel_off, grel_siz, grel_type, grel_sym,
			                                grel_add, node_tmpl, node_op, op_val, op_aux, tmpl_off, tmpl_bin, tmpl_seg_off, tseg_end,
			                                tmpl_hole_off, hole_at, hole_w, hole_enc, hole_rtype),
			          "prepare");
		});
		reps = r0;
		TIMEIT(cell, {
			if (out) futhark_free_opaque_tup8_arr1d_u8_arr1d_i64_arr1d_i64_arr1d_u8_arr1d_u16_arr1d_u32_arr1d_i64_i64(ctx, out);
			fut_check(futhark_entry_emit_flat(ctx, &out, st), cell);
		});
	} else
	TIMEIT(cell, {
		if (out) futhark_free_opaque_tup8_arr1d_u8_arr1d_i64_arr1d_i64_arr1d_u8_arr1d_u16_arr1d_u32_arr1d_i64_i64(ctx, out);
		fut_check(futhark_entry_emit(ctx, &out, parts, how, chunk, fn_item, fn_mark_off, mark_off, mark_type, mark_sym, mark_add, item_kind,
		                             item_ref, item_seg, glue_off, glue_bin, glue_rel_off, grel_off, grel_siz, grel_type, grel_sym,
		                             grel_add, node_tmpl, node_op, op_val, op_aux, tmpl_off, tmpl_bin, tmpl_seg_off, tseg_end,
		                             tmpl_hole_off, hole_at, hole_w, hole_enc, hole_rtype),
		          cell);
	});
	fut_report();
	struct futhark_u8_1d *fcode, *frs;
	struct futhark_i64_1d *ffn, *fro, *fra;
	struct futhark_u16_1d *frt;
	struct futhark_u32_1d *fry;
	int64_t nerr;
	PROJ(0)(ctx, &fcode, out); PROJ(1)(ctx, &ffn, out); PROJ(2)(ctx, &fro, out); PROJ(3)(ctx, &frs, out);
	PROJ(4)(ctx, &frt, out); PROJ(5)(ctx, &fry, out); PROJ(6)(ctx, &fra, out); PROJ(7)(ctx, &nerr, out);
	int64_t pos = futhark_shape_u8_1d(ctx, fcode)[0], nr = futhark_shape_i64_1d(ctx, fro)[0];
	double tc = now();
	u8 *code = malloc(pos ? pos : 1);
	futhark_values_u8_1d(ctx, fcode, code);
	u64 *fnoff = malloc(8 * ((size_t)nfn + 1));
	futhark_values_i64_1d(ctx, ffn, (int64_t *)fnoff);
	SYNC();
	printf("copy out %.1f ms, %lld bytes, %lld relocations, %lld bad holes\n", (now() - tc) * 1e3, (long long)pos, (long long)nr,
	       (long long)nerr);
	if (!check) return 0;

	long bad = nerr != 0;
	size_t m;
	const u8 *want = FMAP(u8, "oracle/p4_code.bin", &m);
	const u64 *wfn = FMAP(u64, "oracle/p4_fn_off.u64", NULL);
	if ((u64)pos != m || (u64)pos != ncode || memcmp(code, want, m) != 0) printf("code differs\n"), bad++;
	if (memcmp(fnoff, wfn, 8 * ((size_t)nfn + 1)) != 0) printf("function offsets differ\n"), bad++;
	printf("code: %lld bytes in %u functions, %s oracle/p4_code.bin\n", (long long)pos, nfn, bad ? "NOT equal to" : "equal to");
	// relocations, as sorted lists
	u64 *ro = malloc(8 * nr); u8 *rs = malloc(nr); u16 *rt = malloc(2 * nr); u32 *ry = malloc(4 * nr); i64 *ra = malloc(8 * nr);
	futhark_values_i64_1d(ctx, fro, (int64_t *)ro); futhark_values_u8_1d(ctx, frs, rs); futhark_values_u16_1d(ctx, frt, rt);
	futhark_values_u32_1d(ctx, fry, ry); futhark_values_i64_1d(ctx, fra, ra);
	SYNC();
	Rel *got = malloc(sizeof(Rel) * (nr + 1)), *w = malloc(sizeof(Rel) * ((size_t)nrel + 1));
	for (int64_t i = 0; i < nr; i++) got[i] = (Rel){ro[i], ra[i], ry[i], rt[i], rs[i]};
	const u64 *wo = FMAP(u64, "oracle/p4_rel_off.u64", NULL);
	const u8 *wsiz = FMAP(u8, "oracle/p4_rel_siz.u8", NULL);
	const u16 *wtype = FMAP(u16, "oracle/p4_rel_type.u16", NULL);
	const u32 *wsym = FMAP(u32, "oracle/p4_rel_sym.u32", NULL);
	const i64 *wadd = FMAP(i64, "oracle/p4_rel_add.i64", NULL);
	for (u32 i = 0; i < nrel; i++) w[i] = (Rel){wo[i], wadd[i], wsym[i], wtype[i], wsiz[i]};
	qsort(got, nr, sizeof(Rel), cmpRel);
	qsort(w, nrel, sizeof(Rel), cmpRel);
	long rbad = nr != (int64_t)nrel;
	for (int64_t i = 0; i < nr && i < (int64_t)nrel; i++)
		if (cmpRel(&got[i], &w[i]) != 0 && rbad++ < 5) printf("  relocation %lld differs (offset %llu)\n", (long long)i, (unsigned long long)got[i].off);
	printf("relocations: %lld, oracle %u, %ld differ\n", (long long)nr, nrel, rbad);
	bad += rbad;
	printf(bad ? "P%s FAIL\n" : "P%s OK\n", cell);
	return bad != 0;
}
            const i64 *wa = FMAP(i64, "oracle/p4_rel_add.i64", NULL);
            for (u32 i = 0; i < nrel; i++) wrel.push_back({wo[i], wa[i], wy[i], wt[i], ws[i]});
            std::sort(wrel.begin(), wrel.end(), relLess);
        }
        std::vector<Rel> got(nrel);
        for (u32 i = 0; i < nrel; i++) got[i] = {rOff(i), rAdd(i), rSym(i), rType(i), rSiz(i)};
        std::sort(got.begin(), got.end(), relLess);
        long rbad = 0;
        for (u32 i = 0; i < nrel; i++)
            if (!relEq(got[i], wrel[i]) && rbad++ < 5) printf("  relocation %u differs (offset %llu)\n", i, (unsigned long long)got[i].off);
        printf("relocations: %u, %ld differ from oracle/p4_rel_*\n", nrel, rbad);
        return bad + rbad;
    };

    const char *th = getenv("HL_NUM_THREADS");
    long bad = 0;
    if (mode == "e2e") {
        auto pick = [&](const char *a, auto &tab, int nt) {  // a variant by index or by name
            for (int k = 0; k < nt; k++) if (strcmp(tab[k].name, a) == 0) return k;
            return atoi(a);
        };
        int lv = argc > 3 ? pick(argv[3], layouts, nLayouts) : 0, ev = argc > 4 ? pick(argv[4], emits, nEmits) : 0;
        double t0 = now();
        runSeg();
        double t1 = now();
        runLayout(lv);
        double t2 = now();
        runEmit(ev);
        double t3 = now();
        if (argc > 5) { FILE *f = fopen(argv[5], "wb"); fwrite(codep, 1, ncode, f); fclose(f); }
        double t4 = now();
        printf("e2e threads=%s layout=%s emit=%s huge=%d: load %.1f ms, templates %.1f, layout %.1f, emit %.1f, write %.1f, total %.1f ms\n",
               th ? th : "default", layouts[lv].name, emits[ev].name, huge, (tload - tstart) * 1e3, (t1 - t0) * 1e3, (t2 - t1) * 1e3,
               (t3 - t2) * 1e3, (t4 - t3) * 1e3, (t4 - tstart) * 1e3);
        if (getenv("P4_NOCHECK")) return 0;  // (bench.sh's e2e wall times: the process without the oracle check)
        bad += checkLayout();
        bad += checkEmit();
    } else {
        printf("threads=%s huge=%d\n", th ? th : "default", huge);
        Stat s = timeRuns([&] { runSeg(); });
        printf("4 templates       " STAT_FMT "\n", STAT_ARGS(s));
        for (int v = 0; v < nLayouts; v++) {
            if (mode != "all" && mode != "layout" && strncmp(layouts[v].name, mode.c_str(), mode.size())) continue;
            s = timeRuns([&] { runLayout(v); });
            printf("4 %-16s " STAT_FMT "   ", layouts[v].name, STAT_ARGS(s));
            bad += checkLayout();
        }
        runLayout(0);
        for (int v = 0; v < nEmits; v++) {
            if (mode != "all" && strncmp(emits[v].name, mode.c_str(), mode.size())) continue;
            memset(codep, 0, ncode + 64);
            s = timeRuns([&] { runEmit(v); });
            printf("4 %-16s " STAT_FMT "   ", emits[v].name, STAT_ARGS(s));
            if (strncmp(emits[v].name, "diag", 4) == 0) { printf("(diagnostic: not checked)\n"); continue; }
            bad += checkEmit();
        }
    }
    printf(bad ? "P4 FAIL\n" : "P4 OK\n");
    return bad != 0;
}
# JOURNAL (append-only: one line per commit, a paragraph at each milestone)
- 2026-11-16T15:31:42Z M0 data: MinIO frozen (1,311 archives + main.a), hlink copied to ref/hlink with -dumpflat, flat files + oracles for P1-P3 (ref/flat/README.md); plain 1-core C references reproduce every oracle exactly (ref/flat/check.sh). Installs merged: Futhark 0.29.1, Halide 21, Soufflé 2.5, Goose v2026.4 (systems/*/NOTES.md).
- 2026-11-16T15:34:24Z M0: GRID.md C column (hlink phases on this VM, plain flat C references), IDEAS.md index, four Opus cell builders launched (ideas/BUILDER.md).
- 2026-11-16T15:36:04Z ref/flat/p2par.c: level-synchronous OpenMP flood on the flat CSR, full rules, P2 OK: 33.4 ms 1 thread, 12.1 ms 8 threads (load 14: provisional), 111 levels in 7 rounds; hlink's flood is 98-103 / 22-23 ms on raw objects. Problem 4 data/oracle handed to an Opus agent.
- 2026-11-16T15:37:00Z ref/flat/p1par.c: partitioned join (256 partitions, per-partition tables) + parallel gather, P1 OK: 72.7 ms 1 thread, 19.2 ms 8 threads (load 7, provisional) vs plain 230 ms; hlink resolve 29-33 ms on raw objects.
- 2026-11-16T15:54:54Z triage M0 skeptic: flat README contract fixes (name offsets, p1c_mark meaning, rank bit 30, id 0, p3_addr_all leak); stats.c note
- 2026-11-16T15:57:39Z GRID: tuned flat C column at load 0.8 (p1par 13.9-16.8 ms, p2par 13.0-17.0 ms on 8 threads vs hlink resolve 24, floods 22-23); kube-apiserver flat files + all C refs OK; dumpflat decode-to-CSR timed 133-139 ms; Codex: p3 pipeline + 1c par (codex-c), p2pipe eager decode (codex-d)
- 2026-11-16T15:58:28Z new column DuckDB (Opus builder, same brief): the north star names databases; DuckDB is installed (zero install cost), wildcard's top-2, and is the only vectorized-SQL/morsel-parallel engine in the grid (Soufflé is compiled Datalog, a different axis). Started before M2 because 4 builders + 2 Codex jobs leave the lead idle.
- 2026-11-16T15:59:27Z ref/flat/reachfacts.py: the flood uses only 28.7% (MinIO, 2.39 M of 8.32 M) / 21.0% (kube, 3.27 M of 15.6 M) of the eagerly decoded CSR, and 25.6% / 26.6% of those are duplicate (source, target) pairs. Eager decode does 3.5-5x the decode lazy hlink does; ATTACKS 1 must compare against a frontier-batched lazy decode, not only eager.
- 2026-11-16T16:00:00Z fix timestamps: GRID/PLAN said 16:xxZ for measurements taken at 15:56-15:58Z (the results commit message 'hlink minio bench (16:56Z)' should read 15:56Z)

- 2026-11-16T15:59Z codex-c: partitioned content-hash join shared by plain C/OpenMP; all MinIO oracles and independent all-pairs cases pass; interleaved plain 302.9 -> 57.0 ms, warm par 1/8 threads 36.9/7.2 ms (results/codex-c-p1.md).
- 2026-11-16T15:59Z codex-c: added independent P3 pipeline diagnostic; all 3a addresses match, but missing special-symbol inputs cause 81 relocation errors / 40 differing ranges. Corrected flat contract; details and reproduction in results/codex-c.md. Passing pipeline blocked on extraction extension.
- 2026-11-16T16:02:18Z p3 --pipeline passes on both datasets: dumpflat now extracts the linker-placed relocation targets (p3_fixed_*, 7 on MinIO, 6 on kube) as given inputs; copied into data/* from a scratch re-extraction (other files byte-identical). Merged codex-c: 1c partitioned join 7.2-8.9 ms on 8 threads, 36.9 on 1 (hlink 8-9 / 37-39); plain p1 1c 303 -> 57 ms.
- 2026-11-16T16:03:00Z ref/flat/p2lazy.c: lazy-decode flood over the flat relocation columns (no pre-decoded CSR for relocation edges; aux/kinds 1-3/methods precomputed as in p2pipe): MinIO 52.6-53.3 ms 1 thread / 14.4-15.4 ms 8 (p2par on the prebuilt CSR 26.0 / 8.6-9.7, interleaved, load 0.8); kube 72.7 / 19.9 (p2par 38.0 / 12.4). P2 OK both.
- 2026-11-16T16:03:39Z merged codex-d (p2pipe: eager decode+CSR+flood 108.7 / 30.1 ms MinIO, loses to hlink); ATTACKS 1 written: lazy decode from columns inside the flood (p2lazy) 52.6 / 13.6-15.4 ms, 2x eager, 1.9x/1.5x hlink's flood with caveats (aux, kinds 1-3, methods precomputed). GRID rows 1c, 2', 3b updated.
- 2026-11-16T16:08:14Z ATTACKS 2 draft: relocate's output page faults are ~half of 3b (prefault 45-64 ms vs copy+patch 31-35); MAP_HUGETLB output 25-29 ms vs 48-68 on 1 core; build/p3par (omp) added; vm.nr_hugepages set to 512 (was 0: hlink's hugetlbfs output path was not in use for GRID's hlink numbers); check.sh runs p2pipe, p2lazy, p3par.
- 2026-11-16T16:10:08Z killed per-source dedup (flood ~8% faster on 1 thread for 37% fewer edges); IDEAS rows for ATTACKS 1/2
- 2026-11-16T16:13:40Z p3 P3_REPS (fresh output per rep): relocate on 2 MB pages 23.9-30 vs 48.6-56 ms (1 thread), 5.4-12 (median 6.1-6.8) vs 13.4-29 ms (8 threads) = 1.1-1.4x the 4.92 floor; load 2.4-3.0. ATTACKS 2 no longer a draft.
- 2026-11-16T16:15:20Z hlink huge pages 512 vs 0, 6 interleaved rounds: within noise (193-267 vs 205-249 ms total); earlier 'huge pages help hlink' withdrawn. Builders resumed (ideas/RESUME.md); Codex attack1 review triaged.
- 2026-11-16T16:17:12Z PLAN Now: builders resumed in paradigms-46; codex-f launched on the p1par dense-pass hypothesis (789K of 5.2M slots keyed, 4 passes over 83 MB keys).
- 2026-11-16T16:17:35Z PLAN.md: restored the user's campaign text (sections Campaign 1 through Milestones log), deleted by a Now-line rewrite after 40f4d4b; Now line kept.
- 2026-11-16T16:18:13Z GRID: measured VM bandwidth (25 / 36 GB/s read, 27 write) vs FLOOR.md's implied ~87 GB/s; 3b on huge pages meets the per-kernel bandwidth bound.
- 2026-11-16T16:21:00Z relaxed atomics for the shared flags in p2par/p2pipe flood (Codex review; P2 OK both datasets). Found: under the builders' load, spinning OMP barriers make p2par 8 threads 900-1300 ms instead of 14; OMP_WAIT_POLICY=passive 14-55 ms. check.sh defaults to passive; builders warned (ideas/RESUME.md).

2026-11-16T16:14:34Z codex-e: raw AuxFuncdata columns + opt-in lazy aux flood; exact per-source aux multisets and P2 live oracles on both datasets. Interleaved best-of-five and load in results/codex-e.md. All 67 existing files byte-identical; shared data publication blocked by read-only mount, new columns retained in /tmp/codex-e-data.
- 2026-11-16T16:23:34Z merged codex-e: aux_* columns (add-aux.sh, run by the lead: 67/67 old files byte-identical on both datasets), p2lazy --aux decodes funcdata aux in the flood, P2 OK both datasets 1/8 threads (lead rerun 16:23Z, load 11); +7-9 ms on 1 thread. check.sh runs --aux.
- 2026-11-16T16:25:07Z p2lazy: timed-decoder check (per-source edge hash vs oracle-derived, mutation-tested) and relaxed atomics; closes the attack1 review's accepted items.

exec
/bin/bash -lc "rg -n 'write|dump|check|OK|main' ref/flat/p3par.c; rg -n 'check|oracle|OK' systems/halide/cells/p1.cpp; nl -ba ATTACKS.md | sed -n '112,132p'; nl -ba GRID.md | sed -n '48,72p'; rg -n 'sequential|worklist|next|67/|102' systems/futhark/NOTES.md systems/futhark/IDEAS.md" in /home/exedev/paradigms-w
 succeeded in 0ms:
rg: ref/flat/p3par.c: IO error for operation on ref/flat/p3par.c: No such file or directory (os error 2)
2:// pipelines Counts, Resolve (1a), RelTarget (1b) and ContentJoin (1c), checks against the oracles.
3://   p1 DATA [all]       every schedule variant, warm (bench.h: min/med/max of 5 runs of >= 1 s), each checked
4://   p1 DATA e2e V       one end-to-end run (process start, mmap the files, compute, check) of variant V (0 serial, 1 parallel)
47:    auto check = [&]() {
48:        long bad = check32("canon", canon.data(), (size_t)N + 1, "oracle/p1_canon.u32");
49:        bad += check32("builtin", builtin.data(), nb, "oracle/p1_builtin.u32");
50:        bad += check32("reltarget", tgt.data(), nrel, "oracle/p1_reltarget.u32");
51:        bad += check32("canon after 1c", canon2.data(), (size_t)N + 1, "oracle/p1c_canon.u32");
52:        printf("internal checks (must be 0): 1a %d, 1c %d\n", chk1(), chk3());
71:        bad += check();
83:            bad += check();
86:    printf(bad ? "P1 FAIL\n" : "P1 OK\n");
   112	**Attack.** An asynchronous worklist flood (`ref/flat/p2async.c`, codex-g): per-thread LIFO stacks, chunk stealing,
   113	termination by credits; the rules are monotone (flags only go 0 -> 1, a push on each transition) so level order is not
   114	needed for the live set. One join per round: **7 phases instead of 111 barriers.**
   115	
   116	**Number.** 5 interleaved processes, ms per solve, min / median / max (codex-g):
   117	
   118	| MinIO, 8 threads | p2par active | p2par passive | p2async |
   119	|---|---|---|---|
   120	| ambient load | 593 / 953 / 1071 | 15.2 / 21.4 / 26.6 | 16.7 / 19.9 / 30.3 |
   121	| + 8 busy loops | 2079 / 2290 / 3510 | 105.9 / 152.7 / 156.1 | **81.3 / 89.4 / 93.4** |
   122	
   123	1 thread: within noise of p2par (34-38 vs 37-44 ms). Under deliberate overload async beats passive by 1.7x with ranges
   124	apart; ambient, within noise. P2 OK on 120/120 separate runs (both datasets, 1/2/8 threads) and in check.sh.
   125	
   126	**Scope (after the M2 review).** Measured: in C, overload robustness (async vs passive within noise when lightly loaded,
   127	1.7x under overload). Not measured: that the level count is *the* bottleneck in Futhark, Halide, Goose and DuckDB; the
   128	builders' notes point there (passes, pipeline calls, supersteps, statements per level), but the test is a batched-level or
   129	level-free version of each, counted (passes, barriers, queue operations) and timed against its original. Owed in M3.
   130	
   131	**Measured in the paradigms (M3, 17:20-17:55Z, load 17-26: times provisional; [results/levelfree.md](results/levelfree.md)).**
   132	The premise was half wrong. Futhark's flood has 45 levels, not 111 (it checks methods every level), at ~26 parallel
    48	
    49	**Caveat (M3 review finding 3).** Warm numbers for Goose P1 (213 ms first run vs 79-115 later) and Futhark 3b reuse
    50	buffers whose pages are already mapped, while the plain C references include their first-touch faults; those warm
    51	cells flatter the paradigm by up to ~2x. The e2e numbers in each system's CELLS.md include the faults.
    52	
    53	Added columns (M3): **GraphBLAS** (SuiteSparse 10.5.2, JIT on; lead rebuilt + reran 18:24Z, P1/P2 OK, all files `cmp`). 8 threads warm, min/med/max, load 7-10, tuned C same window: **P2 reach 179/186/213** (p2par 15/20/26: 9x); 1 thread 192-203, i.e. it does not scale (frontier ~7 K is below SuiteSparse's per-call threading threshold: 379 of 382 vxm calls on one thread); 178 synchronous steps (97 BFS + 81 iface levels) in 7 rounds; pull direction 40x slower. **1a** 268/306/339 (18x p1par; the 128-bit key must be hashed into a 60-bit index and checked), **1b** 162/194/203 (21x), **1c** 279/349/392 (24x). P3: not attempted (no scan; the round-up-then-add fold is no monoid). Easy: reach is ~the shortest legible form (masked vxm); forced: the iface rule as its own closure per round. `systems/graphblas/CELLS.md`.
    54	
    55	| problem | Goose | Futhark | Halide | Soufflé | DuckDB (added 15:58Z) |
    56	|---|---|---|---|---|---|
    57	| 1. resolve | **P1 OK** (1a/1b/1c). 1a 79-115 / 37-57; 1b 39-57 / 42-67; 1c 97-118 / 39-47 (1 / 8 workers, load 8-13). Forced: shuffle join, one table per worker; impossible: a shared table. | **P1 OK** (1a/1b/1c). 2 MB pages: 1a 79 / **24**, 1b 47 / 18, 1c 111 / 38 (1 / 8, load 5-13; 4 KB 1.2-2.5x slower). Easy: winner as reduce_by_index max; forced: the hash table as rounds of probing. | **P1 OK** (1a/1b/1c). 1a 81-93 / **19.8-27**; 1b 42.7-46 / 9.9-10.9; 1c 122-133 / 33-36.5 (1 / 8; tuned C same window 12.1-20.9 / 7.5-8.9 / 11.7-14.8). Surprise: 1a beats plain C 2.5-3x on 1 core (compact first). Forced: no hash table, so join = rounds of scatter-min + gather. | **P1 OK** (1a/1b/1c x2). 1a 1928-2034 / 636; 1b 5220-5426 / 1080; 1c 1625-1865 / 394-587 (1 / 8 threads, load 3-6): 30-200x C. Easy: join + max aggregate are the rules; forced: B-tree loading of 5.2 M rows. | **P1 OK** (1a/1b/1c). 8 threads: 1a 103-113, 1b 272-375, 1c 128-190 (10-60x tuned C). Easy: the joins are SQL joins; forced: every lookup by dense id is a hash join (SQL has no a[i]). |
    58	| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood over given edges 97 / 58; lazy (own decode) 165 / 90 vs eager with its own decode >= 97 + 47 decode + an unwritten CSR build: **a tie at best, not a win** for ATTACKS 1 (corrected after review: the earlier 'slower than eager' compared unlike work). Cost: 111 levels x ~25 passes each. | **P2 OK** (eager + lazy). 8 thr: lazy 79-90, eager 70-127 (tuned C 25.7-29.1 / 12.7-14.6). Lazy is 1.3-1.5x *slower* than eager in Halide. Forced: no fixpoint (one pipeline call per level, ~100 levels), no worklist; `atomic()` dropped (mutex per element, wrong live sets). | **P2 OK** (eager + lazy). Flood over given edges 1227-1467 / 431. With its own decode: **lazy 2.9-3.2 s vs eager ~6.5 s on 1 thread, 960 ms vs ~1.6 s on 8** (decodes 1.73 M of 9.4 M relocations, 3 rules): ATTACKS 1 holds inside Soufflé; ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: slow as implemented (45 rounds x ~25 statements, hash tables rebuilt each round); Third attempt (M3, one recursive query per method round, `p2rec.sql`; lead rerun 18:22Z P2 OK): 8 threads setup 577-871 + flood 1438-2234 ms, 1.2x the old cell overall. **Falls down after three attempts**: each recursive step rescans all 2.7 M edges; DuckDB has no fast lookup by id (an index pays below ~50 rows, frontiers are 16-24 K). Lazy ties eager. |
    59	| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down: one image array on 8 workers (gather +142-171). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). **3b_blk** (M3: the copy as a sequential slice-update loop inside a map over 64 blocks = memcpy; lead rebuilt + reran P3b_blk OK 18:36Z): 60.4/62.0 (1) / **23.5/30.5 (8)** vs old 3b 159.7/166.7 / 96.4/110.9 same window, load 5: 2.7x / 3.6x, ranges apart. Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK**. 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a: second attempt `layout_rank.dl` (M3; lead rebuilt from source, P3 OK 18:21Z): the order by a ranked successor chain, no pair counting, no C functor: 1 thread 1930 (median, was 8270), 8 threads 1725/1937/3541 (was 2860/3388/7055 same runs), load 6-22. **No longer falls down**, but ~100x plain C: the 1,763-step unit walk is the limit. 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
    60	| 4. codegen by shape (stretch) | **P4 OK** (lead rebuilt + reran 1/8 workers 18:47Z). Workers own ranges of whole functions, no messages while computing: layout+emit 97.1/98.3 (1) / **17.6/21.3 (8)** vs p4.c 105.6 / p4par 17.1, within noise; same 698 M instructions. **Not a tie end to end**: that time leaves out the join of the parts, which is Goose's cost (no shared memory): +120-260 ms through queues, +37-63 ms writing each range into the file (M4 review 3). 482 lines + 40 C | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). **4D** (flattened stream, ATTACKS 4; lead reran P4D OK both backends 18:49Z): emit 65.1/67.8 (1) / **23.4/26.9 (8)** vs 4bf 284/303 / 95/113 same window, 4.5x / 4.2x; the flattening (`prepare`, 50 lines) 324 / 138 ms, so it pays only if the stream is reused | **P4 OK** (lead rebuilt + reran 1/8 18:46Z). Emit **157-162 (1, CPU time; one thread, so ~wall) / 23.0-25.1 (8, wall)** vs p4.c 103-112 / p4par 16.4-20.1 (load 4): 1.5x / 1.3x; layout 18 / 5 (+ template index 9.5 / 7). Relocations by a scatter to the layout's own slot ranges (no C host). Forced: entry-0 indices on untaken select sides (Halide loads both: 200 -> 160 ms); the vector copy only as 32-byte read-modify-write in parity passes (obvious form 26 s). 345 + 288 lines | **P4 OK** (lead rebuilt emit2 + reran 19:29Z: bytes, 136,635 offsets, 1,780,281 relocations; also P4 OK on kube-apiserver). Attempt 2 `emit2.dl` warm 7246/8316/9046 (1) / **2722/3814/4741 (8)** vs p4.c 107/111 / p4par 21.6/27.0 same rounds, load 7-10: ~70x / ~130x; lead rerun 8 threads compute 2571 ms (load 2.7-4.5). 74 lines + a 42-line C writer (memcpy runs, store low bytes, at places the rules computed). Easy: lengths, holes to segments, rel32 as a join with `at`, relocations, offsets. Forced: the prefix sum as a radix-16 tree of block sums (11%); **set semantics silently dropped 37,703 exactly-repeated marker relocations** (each now carries its id). Cost: 8.65 M output tuples, a B-tree insert each | not attempted |
    61	
    62	**P4 on kube-apiserver** (19:17Z, results/codex-kube-p4.md): every P4 cell (C p4/p4par, Futhark 4bf/4D, Halide, Goose) is P4 OK on the second dataset unchanged (1.23x the bytes, 0.86x the templates). Emit medians kube/MinIO: C 1.12-1.15x, Futhark 4D 1.16-1.18x, 4bf 1.02-1.08x, Halide 1.44x (1) / 0.89x (8), Goose ~1.3x (8); load 4-24, ranges overlap for Futhark 4bf and Halide. No cell changes rank.
    63	
    64	Halide column merged 17:17Z (lead rebuilt every generator and driver in a copy; P1/P2/P3 OK; `systems/halide/CELLS.md`).
    65	DuckDB column merged 17:13Z (lead reran every check in a copy, pipeline included; `systems/duckdb/CELLS.md`).
    66	Soufflé column merged 16:58Z (lead rebuilt all 8 programs from source; every check OK; `systems/souffle/CELLS.md`).
    67	Futhark column merged 16:42Z (lead rebuilt from source; every cell OK; `systems/futhark/CELLS.md`). Surprise: `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, no code change, makes most Futhark cells 1.2-1.7x faster.
    68	Goose column merged 16:35Z; lead reran every check (P1/P2/P3/pipeline OK, a source rebuild of p2par passes). Long form and lines: `systems/goose/CELLS.md`, `NOTES.md`.
systems/futhark/IDEAS.md:48:  exceeds hlink's whole Reach time, or if rounds > 30 (then a worklist/frontier version is required and the idea reduces to I1).
systems/futhark/IDEAS.md:103:  Moderate flattening: only outer parallelism is exploited on the CPU backend (the multicore backend sequentialises inner levels once threads are saturated).
systems/futhark/IDEAS.md:139:  the narrow levels sequentially cuts 45 -> 15 levels (1,211 -> 437 parallel ops) and is within noise of the
systems/futhark/IDEAS.md:140:  original; a fully sequential worklist in Futhark (`2seq`) is ~2x faster at 1 core (min 73-79 vs 137-141). The
systems/futhark/IDEAS.md:144:- **I18. Copy runs of bytes as a map over output blocks with a sequential loop of slice updates inside.** P4: the
systems/futhark/IDEAS.md:152:  (count per block, exscan, each block writes its own range, sequentially); dead if the entry is not below 2x C.
systems/futhark/NOTES.md:3:Futhark 0.29.1, `c` backend (sequential) for 1 core, `multicore` backend for 8. All compute is in `p1.fut`,
systems/futhark/NOTES.md:11:0. **A sequential worklist written in Futhark beats Futhark's data-parallel flood, and halving the levels does
systems/futhark/NOTES.md:17:   sequential drain; 15 levels, 437 ops, 40% of pops sequential) is within noise of the original. So the cost is
systems/futhark/NOTES.md:19:   (`2bat`, a worklist loop inside a map over frontier chunks): 35-45 rounds, duplicate expansions, 1.5-2.5x
systems/futhark/NOTES.md:23:   `row with [a : a + m] = xs[f : f + m]` in a sequential loop compiles to a memcpy (through the runtime's
systems/futhark/NOTES.md:89:| 2 reach (p2_edge CSR) | 140 / 188 | 303 / 349 | 155 / 259 | 347 / 519 | 97 / 105 | 58 / 102 | 135 / 142 | 39-43 / 13-17 |
systems/futhark/NOTES.md:103:`copy_blocks` in p3.fut: the 109 MB image cut into 64 equal blocks; a map over the blocks, in each a sequential
systems/futhark/NOTES.md:132:  materialized. The R_USETYPE skip rule (type 26 skips the next two) becomes a segmented scan of a 3-state
systems/futhark/NOTES.md:138:  58-102, at load 5-7.
systems/futhark/NOTES.md:143:  the lazy shape cheap in C (a sequential walk of a row, decode and mark in the same loop) is a nested,
systems/futhark/NOTES.md:175:| 2hyb lo 1 K hi 4 K | 16 + 2 drains | 463 | 116 / 119 / 168 | 102 / 178 / 339 |
systems/futhark/NOTES.md:176:| 2seq (one worklist) | 0 + 1 drain | 45 | 79 / 156 / 175 | **67 / 102 / 130** |
systems/futhark/NOTES.md:180:a worklist `loop` with `with` updates, outside any map, so it compiles to plain sequential C; the frontier
systems/futhark/NOTES.md:215:  33 K keys; a CPU-shaped bottom-up merge sort is 2 ms). The sequential cursor loop (`3a_loop`, in-place
systems/futhark/NOTES.md:219:  faster, as a map over 64 image blocks with a sequential memcpy loop inside (`3b_blk`), which needs the
systems/futhark/NOTES.md:291:  | `4b` | map over 64 blocks, sequential loop of slice copies (memcpy) inside; then the patch scatter | **348 / 376** |
systems/futhark/NOTES.md:307:| C ref/p4/p4.c (1) | 102-110 / 106-115 | | | | | |
systems/futhark/NOTES.md:310:So: **3.2x the plain C at 1 core** (326 vs 102 ms min), **5.3x the OpenMP C at 8 threads** (161 vs 30), both on a
systems/futhark/NOTES.md:342:  copy is a scatter of (index, byte) pairs, one per byte; the fast copy is a map over blocks with a *sequential*
systems/futhark/NOTES.md:448:- Backends used: `c` (sequential) and `multicore` (pthreads), both just need gcc (gcc present at /usr/bin/gcc).
systems/futhark/NOTES.md:482:| c | 1 (sequential) | 118.7 | 229.3 | 237.0 | 218.5 |
systems/futhark/NOTES.md:488:Reading: 8 threads give only 1.3-1.4x over the sequential C backend (and 3.3x over multicore at 1 thread).

exec
/bin/bash -lc 'OMP_NUM_THREADS=1 OMP_WAIT_POLICY=passive P3_REPS=1 build/p3par data/minio' in /home/exedev/paradigms-w
 succeeded in 131ms:
p3a (1 core): layout 16.808102 ms (360315 symbols, 33261 sorted by type string), reps 1 total 0.016808 s
p3b (1 threads): copy + relocate 61.1 ms, 76359886 bytes, 2053801 relocations, 0 errors; checksum 065963d1b7a64b22
image: 0 placed symbols differ from oracle/p3_image.bin
addr: 360315 entries, 0 differ from oracle/p3_addr.u32
P3 OK

codex
1. **I did not establish a currently unchecked cell.** One weakness remains: Halide P3a initializes its output from the oracle before running layout, so an omitted write can pass unnoticed ([p3.cpp](/home/exedev/paradigms-w/systems/halide/cells/p3.cpp:77)). That needs a poisoned-output check. I ran `OMP_NUM_THREADS=1 OMP_WAIT_POLICY=passive P3_REPS=1 build/p3par data/minio`: **360,315 addresses checked, zero differences; image zero differences; P3 OK**. This verifies C, not Halide.

2. **Goose P3b’s “falls down” is too broad if read as a relocation verdict.** Its second attempt actually succeeds idiomatically: distributed output takes **17–24 ms**; requiring one gathered array causes the failure ([NOTES.md](/home/exedev/paradigms-w/systems/goose/NOTES.md:137)). Measure workers writing disjoint file ranges, including assembly, before generalizing to linker output. DuckDB’s current verdict has stronger support: three attempts and explicit index tuning.

3. **Goose P4’s claimed C tie omits output assembly.** **17.6–21.3 ms** excludes **120–260 ms** through queues or **37–63 ms** through files ([GRID.md](/home/exedev/paradigms-w/GRID.md:60)); STATUS still calls it a tie. Separately, Goose P1 and Futhark P3b compare reused pages against C’s first-touch faults, flattering them by up to **~2×** ([GRID.md](/home/exedev/paradigms-w/GRID.md:49)). Both problems are now disclosed in GRID.

4. **Futhark’s sequential flood win is missing from the headline cell.** Its worklist takes **67/102/130 ms**, versus **156/201/258 ms** for the parallel flood. This was investigated, so “wasn’t chased” is inaccurate; the ignored consequence is that GRID still describes **111 levels**, although the follow-up found **45**, and omits the faster implementation ([ATTACKS.md](/home/exedev/paradigms-w/ATTACKS.md:131), [GRID.md](/home/exedev/paradigms-w/GRID.md:58)).

5. **None of the four ATTACKS entries is wholly unmeasured.** Entry 2’s narrower assertion that the join improvement is “TLB reach, not faults” lacks direct counter evidence: **30.1–32.3 → 20.5–21.2 ms** establishes speed, not its cause ([ATTACKS.md](/home/exedev/paradigms-w/ATTACKS.md:84)). Measure minor faults and dTLB misses/page walks with matched, prefaulted allocations and verified page sizes.

6. **No qualifying JOURNAL passage found.** I read all **142 lines**; none declines or shrinks work because of time, usage, or a session ending. There is no supported quotation to supply.
tokens used
55,683
1. **I did not establish a currently unchecked cell.** One weakness remains: Halide P3a initializes its output from the oracle before running layout, so an omitted write can pass unnoticed ([p3.cpp](/home/exedev/paradigms-w/systems/halide/cells/p3.cpp:77)). That needs a poisoned-output check. I ran `OMP_NUM_THREADS=1 OMP_WAIT_POLICY=passive P3_REPS=1 build/p3par data/minio`: **360,315 addresses checked, zero differences; image zero differences; P3 OK**. This verifies C, not Halide.

2. **Goose P3b’s “falls down” is too broad if read as a relocation verdict.** Its second attempt actually succeeds idiomatically: distributed output takes **17–24 ms**; requiring one gathered array causes the failure ([NOTES.md](/home/exedev/paradigms-w/systems/goose/NOTES.md:137)). Measure workers writing disjoint file ranges, including assembly, before generalizing to linker output. DuckDB’s current verdict has stronger support: three attempts and explicit index tuning.

3. **Goose P4’s claimed C tie omits output assembly.** **17.6–21.3 ms** excludes **120–260 ms** through queues or **37–63 ms** through files ([GRID.md](/home/exedev/paradigms-w/GRID.md:60)); STATUS still calls it a tie. Separately, Goose P1 and Futhark P3b compare reused pages against C’s first-touch faults, flattering them by up to **~2×** ([GRID.md](/home/exedev/paradigms-w/GRID.md:49)). Both problems are now disclosed in GRID.

4. **Futhark’s sequential flood win is missing from the headline cell.** Its worklist takes **67/102/130 ms**, versus **156/201/258 ms** for the parallel flood. This was investigated, so “wasn’t chased” is inaccurate; the ignored consequence is that GRID still describes **111 levels**, although the follow-up found **45**, and omits the faster implementation ([ATTACKS.md](/home/exedev/paradigms-w/ATTACKS.md:131), [GRID.md](/home/exedev/paradigms-w/GRID.md:58)).

5. **None of the four ATTACKS entries is wholly unmeasured.** Entry 2’s narrower assertion that the join improvement is “TLB reach, not faults” lacks direct counter evidence: **30.1–32.3 → 20.5–21.2 ms** establishes speed, not its cause ([ATTACKS.md](/home/exedev/paradigms-w/ATTACKS.md:84)). Measure minor faults and dTLB misses/page walks with matched, prefaulted allocations and verified page sizes.

6. **No qualifying JOURNAL passage found.** I read all **142 lines**; none declines or shrinks work because of time, usage, or a session ending. There is no supported quotation to supply.
