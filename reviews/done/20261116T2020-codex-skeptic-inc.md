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
session id: 01a1e605-284b-7831-a76d-284a07cd1fde
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

Focus this review on ATTACKS.md section 5 (incremental relink), results/c-incremental.md, results/ddflow-incremental.md, ref/flat/p2inc.c and its fuzzer, and the Differential Dataflow column: is the maintained flood's check real (does it compare the full live set after each update, both directions), is the 3 us number timing the same work as the 43-45 ms recompute, and is the edit-2 comparison fair? Data is at /home/exedev/paradigms/data (read-only).
codex
I’ll read the repository notes and trace the incremental flood checks and timings, then run read-only checks where possible.
exec
/bin/bash -lc "pwd; rg --files -g 'CHARTER.md' -g 'PLAN.md' -g 'STATUS.md' -g 'GRID.md' -g 'ATTACKS.md' -g 'JOURNAL.md' -g '*incremental*' -g '*p2inc*' -g 'CLAUDE.md'" in /home/exedev/paradigms-w
 succeeded in 0ms:
/home/exedev/paradigms-w
results/c-incremental-bench.txt
results/c-incremental.md
results/ddflow-incremental.md
CHARTER.md
GRID.md
ATTACKS.md
PLAN.md
ref/flat/bench-p2inc.sh
ref/flat/p2inc.c
STATUS.md
JOURNAL.md

exec
/bin/bash -lc 'cat CHARTER.md PLAN.md STATUS.md GRID.md ATTACKS.md results/c-incremental.md results/ddflow-incremental.md; tail -180 JOURNAL.md' in /home/exedev/paradigms-w
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

Now (2026-11-16T20:25Z): grid full (20/20), 5 ATTACKS backed. Running: (a) Codex job `codex-relayout` in
/home/exedev/paradigms-c (prompt: layout + relocate after edits 1/2, current vs per-unit slack layout, C incremental
3a+3b checked vs from-scratch; log codex-relayout.log, waits on `^EXIT [0-9]+$`; output results/c-relayout.md);
(b) an Opus builder in a worktree: stable identity in C (ref/flat/unify.c vs ddflow's 0.7-1.1 s, checked equal to
diff/idmap_b.u32; results/c-unify.md); (c) Opus builder: GraphIt column, P2 (systems/graphit, IDEAS
new-columns row); (d) Opus builder: maintained resolve in C (ref/flat/p1inc.c, results/c-p1inc.md). When both land: lead reruns their checks, merge, ATTACKS 5 Observation 4
(+ identity cost), GRID. Then: quiet reruns of the 8-thread grid numbers (systems/*/bench.sh one at a time, taskset),
skeptic review.
# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T20:20Z. **Grid full: 20/20 cells** (+ DuckDB 3, GraphBLAS 2, Differential Dataflow 2), **5 ATTACKS backed**.
**Surprise, up: relinking after a real edit.** A 5-line MinIO edit changes 31 symbols of 9.7 M facts; a C maintained
flood (ref/flat/p2inc.c) updates reach in **3 us** (vs 43-45 ms recompute); a deleting edit (12 K symbols die) in
5.5-9.5 ms. Textbook delete-and-rederive is 3x slower than recompute; derivation-order stamps fix it. ATTACKS 5.
Prerequisite: stable symbol identity (positional ids shift on any edit; recovering identity costs 0.7-1.1 s today).
Also today: Soufflé P4 (set semantics dropped 37,703 duplicate relocations), Halide flat-stream P4 3.8x, hole cost = traversal.
Next: layout + relocate after an edit (do addresses have to move?), quiet reruns.

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
| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood over given edges 97 / 58; lazy (own decode) 165 / 90 vs eager with its own decode >= 97 + 47 decode + an unwritten CSR build: **a tie at best, not a win** for ATTACKS 1 (corrected after review: the earlier 'slower than eager' compared unlike work). Cost: 45 levels (methods checked every level; not 111) x ~26 passes each; a sequential worklist in Futhark is faster: 67/102/130 vs 156/201/258 ms on 8 (results/levelfree.md). | **P2 OK** (eager + lazy). 8 thr: lazy 79-90, eager 70-127 (tuned C 25.7-29.1 / 12.7-14.6). Lazy is 1.3-1.5x *slower* than eager in Halide. Forced: no fixpoint (one pipeline call per level, ~100 levels), no worklist; `atomic()` dropped (mutex per element, wrong live sets). | **P2 OK** (eager + lazy). Flood over given edges 1227-1467 / 431. With its own decode: **lazy 2.9-3.2 s vs eager ~6.5 s on 1 thread, 960 ms vs ~1.6 s on 8** (decodes 1.73 M of 9.4 M relocations, 3 rules): ATTACKS 1 holds inside Soufflé; ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: slow as implemented (45 rounds x ~25 statements, hash tables rebuilt each round); Third attempt (M3, one recursive query per method round, `p2rec.sql`; lead rerun 18:22Z P2 OK): 8 threads setup 577-871 + flood 1438-2234 ms, 1.2x the old cell overall. **Falls down after three attempts**: each recursive step rescans all 2.7 M edges; DuckDB has no fast lookup by id (an index pays below ~50 rows, frontiers are 16-24 K). Lazy ties eager. |
| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down only for one gathered Goose array (+142-171). **One file works:** 8 workers 2 MB **72.3/77.3/78.8 ms** including buffered write-out, vs Goose1 101.2 and C1 107.8 medians ([five rounds](results/codex-review5.md#b-goose-p3b-a-shared-file-instead-of-a-gathered-array)). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). **3b_blk** (M3: the copy as a sequential slice-update loop inside a map over 64 blocks = memcpy; lead rebuilt + reran P3b_blk OK 18:36Z): 60.4/62.0 (1) / **23.5/30.5 (8)** vs old 3b 159.7/166.7 / 96.4/110.9 same window, load 5: 2.7x / 3.6x, ranges apart. Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK** (poisoned addresses; skipped-unit check fails). 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a: second attempt `layout_rank.dl` (M3; lead rebuilt from source, P3 OK 18:21Z): the order by a ranked successor chain, no pair counting, no C functor: 1 thread 1930 (median, was 8270), 8 threads 1725/1937/3541 (was 2860/3388/7055 same runs), load 6-22. **No longer falls down**, but ~100x plain C: the 1,763-step unit walk is the limit. 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
| 4. codegen by shape (stretch) | **P4 OK** (lead rebuilt + reran 1/8 workers 18:47Z). Workers own ranges of whole functions, no messages while computing: layout+emit 97.1/98.3 (1) / **17.6/21.3 (8)** vs p4.c 105.6 / p4par 17.1, within noise; same 698 M instructions. **Not a tie end to end**: that time leaves out the join of the parts, which is Goose's cost (no shared memory): +120-260 ms through queues, +37-63 ms writing each range into the file (M4 review 3). 482 lines + 40 C | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). **4D** (flattened stream, ATTACKS 4; lead reran P4D OK both backends 18:49Z): emit 65.1/67.8 (1) / **23.4/26.9 (8)** vs 4bf 284/303 / 95/113 same window, 4.5x / 4.2x; the flattening (`prepare`, 50 lines) 324 / 138 ms, so it pays only if the stream is reused | **P4 OK** (lead rebuilt + reran 1/8 18:46Z). Emit **157-162 (1, CPU time; one thread, so ~wall) / 23.0-25.1 (8, wall)** vs p4.c 103-112 / p4par 16.4-20.1 (load 4): 1.5x / 1.3x; layout 18 / 5 (+ template index 9.5 / 7). Relocations by a scatter to the layout's own slot ranges (no C host). Forced: entry-0 indices on untaken select sides (Halide loads both: 200 -> 160 ms); the vector copy only as 32-byte read-modify-write in parity passes (obvious form 26 s). 345 + 288 lines | **P4 OK** (lead rebuilt emit2 + reran 19:29Z: bytes, 136,635 offsets, 1,780,281 relocations; also P4 OK on kube-apiserver). Attempt 2 `emit2.dl` warm 7246/8316/9046 (1) / **2722/3814/4741 (8)** vs p4.c 107/111 / p4par 21.6/27.0 same rounds, load 7-10: ~70x / ~130x; lead rerun 8 threads compute 2571 ms (load 2.7-4.5). 74 lines + a 42-line C writer (memcpy runs, store low bytes, at places the rules computed). Easy: lengths, holes to segments, rel32 as a join with `at`, relocations, offsets. Forced: the prefix sum as a radix-16 tree of block sums (11%); **set semantics silently dropped 37,703 exactly-repeated marker relocations** (each now carries its id). Cost: 8.65 M output tuples, a B-tree insert each | not attempted |

Added column (2026-11-16T19:55Z): **Differential Dataflow** (incremental relink; rust 1.100, differential-dataflow 0.26.0; lead rebuilt + reran `ddflow inc minio minio-edit`: P2 OK). **P2 reach OK** (MinIO + kube): full 3343 (1) / 1814 (8) median, 60x / 100x C. **P1a+1b OK**: full 9310 / 2023. **Incremental, real edits** (results/ddflow-incremental.md, ATTACKS 5): edit 1 (31 symbols of 9.7 M facts) reach update **1.4 ms (1 wkr) vs C p2 52 / p2par 20.5 (8)**, resolve update **0.7 ms vs p1 287 / p1par 22**; edit 2 (12,320 live ids die) reach update 59-65 (1) / 29 (8, quiet; the earlier 555 was load): slower than C recompute. **C maintained flood** (ref/flat/p2inc.c, DRed with derivation-order stamps; lead reran 20:16Z both edits + 50 fuzz diffs, P2INC OK): **edit 1 0.002-0.003 ms, edit 2 5.5-9.5 ms** vs p2 43-45, p2par 14-17 (8) (results/c-incremental.md). Id unification (positional ids shift) 0.7-1.1 s, outside the update. 71 + 23 lines. `systems/ddflow/CELLS.md`.

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
-- its tables and records are malloc'd and probed at random. The original "TLB reach, not faults"
explanation was unmeasured. [Five-round join-only perf checks](results/codex-review5.md#c-p1compact-tlb-reach-and-first-touch-faults)
now show **both mechanisms, in different phases**: a first join has median 28,407 vs 72 minor faults;
a warmed join has zero faults in either mode, but median page-walk requests fall from 113,467 to 46,073
and L1 dTLB misses from 1,095,355 to 389,057 per join. `smaps` verifies 188 MiB of explicit hugetlb
allocations with the tunable (zero without). Warm mean times are 31.582 vs 26.495 ms median, with
overlapping ranges: this noisy window does not reproduce a precise speedup. Translation pressure
falls after prefaulting; fault savings also matter on the first execution. The floods (p2par, p2lazy)
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
- 2026-11-16T19:40Z full-grid skeptic triaged (5 accepted: Halide P3a poison check, Goose P3b disjoint-file write, ATTACKS 2 TLB counters -> Codex codex-review5; GRID Futhark P2 levels fixed).
- 2026-11-16T19:54Z Halide P4 on the flattened stream merged (results/halide-p4flat.md): D emit 42 / 15 ms (3.8x / 1.5x its template emit), H1 13-16 / 6 ms; lead rebuilt + reran p4f all on MinIO and kube, 8 threads: P4 OK every checked variant (D 43.9 / 15.0, H1 16.0 median). Caveat: C's timed emit also collects relocations run by run; Halide copies prepared relocation columns.
- 2026-11-16T19:56Z DD merged: P2 + P1a/1b exact; real-edit incremental relink (edit 1: update 1.4 ms vs 52 ms C recompute; edit 2 deletion: slower than C). Lead rebuilt + reran inc edit 1: P2 OK, 1.33-1.40 ms. New ATTACKS 5 (stable identity + maintained reach), backed. Unify 0.7-1.1 s stated as outside the update.
- (Codex, codex-review5) 2026-11-16T19:53Z codex-review5: answered skeptic A/B/C with poisoned Halide layout + real skipped-unit failure; Goose disjoint file assembly (8-worker 2 MB 77.3 ms median vs 101.2 Goose1 /107.8 C1 including writes); five-round join-only perf verifies zero warmed faults and 2.46x fewer walk requests, plus first-touch fault reduction 28,407 -> 72. Independent checks pass. Results: results/codex-review5.md. Commit locally on codex-review5; no push per user.
- 2026-11-16T19:58Z codex-review5 merged; lead rebuilt Halide p3 and reran 3a on 8 threads: P3 OK (poisoned output, 0 of 360,315 differ, 14.7 ms median), P3_SKIP_UNIT=0 -> 2 differ, P3 FAIL. Goose P3b: one file from 8 workers 77 ms incl. write-out (no longer falls down as a relocate; only the gathered array). ATTACKS 2: faults and TLB both (counted).
- 2026-11-16T20:18Z p2inc merged (C maintained flood, results/c-incremental.md): edit 1 update 3 us, edit 2 (12 K deaths) 5.5-9.5 ms vs 43-45 ms p2 recompute; lead rebuilt + reran both edits (P2INC OK) and 50 fuzz diffs (0 failed). Textbook DRed is 123 ms (3x recompute): the support stamps are the attack. DD's 555 ms 8-worker update was load (29 quiet).
- 2026-11-16T20:19Z started codex-relayout (layout+relocate after an edit, slack layout, C incremental 3a+3b) and an Opus builder on stable identity in C (vs ddflow unify 0.7-1.1 s): the two open pieces of ATTACKS 5.
- 2026-11-16T20:20Z started builders: GraphIt column (P2; the open new-column idea from Halide's reader) and maintained resolve in C (ATTACKS 5's unmeasured resolve half).

exec
/bin/bash -lc "nl -ba results/c-incremental.md; nl -ba results/ddflow-incremental.md; nl -ba ref/flat/p2inc.c; rg --files | rg '(fuzz|ddflow|bench-p2inc)'" in /home/exedev/paradigms-w
 succeeded in 0ms:
     1	# A maintained flood in plain C (ATTACKS 5, "what it would take")
     2	
     3	**Result.** A 1-core C program that keeps the previous link's reach state and applies a stored diff updates
     4	MinIO's live set in **3 µs for edit 1** (add a call) and **5-10 ms for edit 2** (delete a call: 12,320 ids die),
     5	against 43-45 ms for recomputing with `ref/flat/p2`, 14-17 ms for `p2par` on 8 threads, and 1.4 ms / 29-58 ms for
     6	Differential Dataflow's update. Every update was checked: B's oracle after A->B, A's after B->A, five times per
     7	process, plus 300 random diffs per edit checked against a from-scratch flood. The deletion strategy matters more
     8	than anything else: textbook DRed takes 120-130 ms for edit 2, about 3x a recompute.
     9	
    10	## (1) The diff as flat files
    11	
    12	`ddflow writediff /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit` (and `minio-edit2`)
    13	writes `data/<edit>/diff/` with its own README.md: `idmap_b.u32` (B id -> A's id space, new slots after `nids_a`),
    14	`strmap_b.u32`, edges by kind (`edge_{minus,plus}_{src.u32,kind.u8,val.u64}`), flags
    15	(`flag_{minus,plus}_{id.u32,val.u8}`), roots, method-table rows (empty for both edits), `live_b.u32` (B's oracle in
    16	unified ids) and `meta.txt`. Edit 1: edges -11 +16, flags -14 +16. Edit 2: edges -135 +99, flags -73 +44. The
    17	mapping is described in results/ddflow-incremental.md. Any system can apply these files; nothing here depends on
    18	Rust.
    19	
    20	## (2) `ref/flat/p2inc.c`
    21	
    22	    gcc -O2 -march=native -o build/p2inc ref/flat/p2inc.c
    23	    prlimit --as=12884901888 build/p2inc /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit2 5          # -> P2INC OK
    24	    FUZZ=300 prlimit --as=12884901888 build/p2inc /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit2 1 # -> fuzz: 300 random diffs, 0 failed
    25	    ref/flat/bench-p2inc.sh 5                                                                                                  # the table below
    26	
    27	What the program does:
    28	
    29	- **State kept between links.** Setup takes 365 ms and is not timed: a deduplicated CSR, the reverse CSR of
    30	  kind-0/1 edges, and method indexes by (name, mtyp), by name, by target and by exported. Then a flood of A with
    31	  the same worklist rules (60-90 ms, checked against A's oracle) produces this state:
    32	  - live, iface, typeDone (live + iface + type descriptor, with its type edges and methods applied) and called,
    33	    per method;
    34	  - counts of live sources for sig, named and reflect;
    35	  - a **stamp** per fact: the clock value when the fact was set. Every fact is therefore derived from facts with
    36	    smaller stamps.
    37	- **Overlay graph.** A's CSR stays as it is. A hash set holds the A edges that were removed, and per-source and
    38	  per-target lists hold the added edges. Updates are applied to the overlay.
    39	- **Additions.** These use the same worklist as the flood, so only new targets are pushed. The method and iface
    40	  rules are part of it: a sig count going 0 -> 1 wakes the methods keyed by that sig, and a type becoming
    41	  live + iface activates its type edges and methods.
    42	- **Deletions: DRed (delete and rederive), refined by stamps.**
    43	  1. All removed edges leave the graph first. The fuzzer found that interleaving removals and retractions loses a
    44	     re-check.
    45	  2. Their consequences are retracted transitively. A fact is **kept** if it still has a derivation from a true
    46	     fact with a smaller stamp: a plain or useiface predecessor, a root, a flag, a type edge from an active type,
    47	     or a called method. A call is kept if some live source of its condition has a smaller stamp than the call.
    48	  3. Every retraction re-checks what it supported. So whatever is kept has a chain of decreasing stamps down to
    49	     roots and flags, which means it is derivable. That is the induction the proof rests on.
    50	  4. Everything retracted is then **rederived** if it has any derivation from true facts (one reverse lookup
    51	     each), and the worklist restores what follows.
    52	  5. A per-id resume pointer keeps re-checks cheap. Predecessors before the last support found can never support
    53	     that id again while it keeps its stamp, because a retracted predecessor comes back only with a larger stamp.
    54	- **Why not the alternatives:**
    55	  - Per-id support counts are wrong on cycles. Live code is full of them: a function and its closure keep each
    56	    other alive.
    57	  - The aggregate rule has the same trap, and the fuzzer found it. A sig's remaining live sources can be the
    58	    method's own targets. So "sig count > 0" does not justify keeping a call; a source with a smaller stamp does.
    59	  - "Recompute whenever a live edge is removed" always recomputes. Every real edit removes live edges, because the
    60	    edited function's GC-locals symbols are renamed (edit 1 removes 2).
    61	  - Textbook DRed (`plain`) over-deletes 594,148 of 663,630 live ids for edit 2: everything downstream of a
    62	    retracted sig, named or reflect source. That is 120-130 ms.
    63	- **OpenMP: not attempted.** The edit-1 update is 3 µs. Edit 2's is 5-10 ms of dependent support checks on a
    64	  worklist of ~30 K retractions, already below p2par's 8-thread recompute (14 ms). A level-synchronous parallel
    65	  version pays a barrier per level; p2par needs 111 of them.
    66	
    67	## Times (ms, min / median / max, 5 interleaved rounds, 2026-11-16T20:12:47-20:14:57Z, load 0.7-1.7: a quiet window)
    68	
    69	`ref/flat/bench-p2inc.sh`, raw output in `results/c-incremental-bench.txt`. p2inc runs 5 updates per direction per
    70	process. The first update of a process is listed apart: it touches the state's pages for the first time.
    71	
    72	| | edit 1 A->B | edit 1 B->A | edit 2 A->B | edit 2 B->A |
    73	|---|---|---|---|---|
    74	| **p2inc** (stamps), first update | 0.036 / 0.038 / 0.047 | 0.005 / 0.005 / 0.006 | 18.0 / 18.7 / 21.1 | 5.2 / 5.5 / 7.4 |
    75	| **p2inc** (stamps), later updates | **0.002 / 0.003 / 0.004** | 0.003 / 0.004 / 0.006 | **9.2 / 9.5 / 16.6** | **5.0 / 5.2 / 10.1** |
    76	| p2inc plain DRed | 0.002 / 0.003 / 0.011 | 0.004 / 0.005 / 0.011 | 116 / 123 / 130 | 115 / 121 / 137 |
    77	| DD update, 1 worker | 1.36 / 1.43 / 1.81 | 1.34 / 1.38 / 2.73 | 54.2 / 58.6 / 83.9 | 50.4 / 57.0 / 79.3 |
    78	| DD update, 8 workers | 1.79 / 2.10 / 3.62 | 1.72 / 2.06 / 3.17 | 25.4 / 29.1 / 80.5 | 23.5 / 30.3 / 56.0 |
    79	| C p2 recompute (1 core) | 40.7 / 44.9 / 48.7 | | 35.7 / 42.6 / 51.1 | |
    80	| C p2par recompute, 1 thread | 26.5 / 27.6 / 29.7 | | 25.6 / 27.6 / 28.1 | |
    81	| C p2par recompute, 8 threads | 15.5 / 17.3 / 19.8 | | 13.0 / 14.2 / 18.3 | |
    82	
    83	Medians:
    84	- **Edit 1:** p2inc is ~15,000x faster than p2 and ~500x faster than DD.
    85	- **Edit 2:** p2inc is 4.5x faster than p2 (A->B) to 8x (B->A), 1.5-2.7x faster than p2par on 8 threads, and 3-6x
    86	  faster than DD on 8 workers.
    87	
    88	**Correction to the DD column:** on this quiet machine DD's 8-worker update for edit 2 is 29 ms, not the 526-555 ms
    89	measured at load 4.4-5.6 (bench-20261116T1940Z). The slowdown on 8 workers is a symptom of a loaded machine
    90	(most likely workers waiting for descheduled peers; not profiled), not a property of DD on an idle machine.
    91	
    92	## The mechanism, counted (one update, first round)
    93	
    94	| | over-deleted live ids | rederived directly | ids set again or newly | out-edges read | reverse entries read | support checks |
    95	|---|---|---|---|---|---|---|
    96	| edit 1 A->B (stamps) | 1 | 2 | 2 | 0 | 16 | 2 |
    97	| edit 1 B->A (stamps) | 2 | 1 | 1 | 0 | 7 | 3 |
    98	| edit 2 A->B (stamps) | 32,238 | 5,050 | 19,928 | 176,298 | 3,049,918 | 369,484 |
    99	| edit 2 B->A (stamps) | 8,112 | 13 | 20,422 | 100,280 | 1,088,061 | 98,502 |
   100	| edit 2 A->B (plain DRed) | 594,148 | 137,584 | 581,838 | 3,890,503 | 4,621,812 | 0 |
   101	| recompute (p2inc's own flood of A) | | | 663,630 | 2,197,091 | | |
   102	
   103	Edit 2 A->B retracts 32 K ids for 12,320 real deaths: the stamps cut textbook DRed's cone 18x. Reverse reads
   104	(3.0 M) are now the cost. They are re-checks of popular targets, such as runtime functions with tens of thousands
   105	of predecessors, each time one of their predecessors dies. A per-id count of "live predecessors with a smaller
   106	stamp" would make a re-check O(1). That is the next step if 9.5 ms matters.
   107	
   108	## Checks
   109	
   110	- Each p2inc run checks the live set after every update against B's oracle (A->B) or A's (B->A): 5 + 5 per process,
   111	  20 processes in the bench. The bench aborts on `P2INC OK` missing.
   112	- `FUZZ=300` on each edit, stamp mode: 300 random diffs on top of B, each with up to 200 removed edges from live
   113	  sources and up to 200 added edges of all four kinds with random or existing targets. Each is checked against a
   114	  from-scratch flood of the same overlay graph, then reverted and checked against B's oracle: 0 failures. Plain
   115	  mode: `FUZZ=30`, 0 failures.
   116	- The fuzzer found two real bugs that the edits alone did not show, both fixed before these numbers. One: a call
   117	  kept by a sig count whose remaining sources were the call's own targets. Two: a removed edge whose source was
   118	  retracted by an earlier removal in the same update, so its target was never re-checked.
   119	
   120	## What it changes in ATTACKS 5
   121	
   122	- Maintenance in plain C beats recompute for both edits, including the retracting one, once deletions use
   123	  derivation stamps. The "hybrid: maintain on additions, recompute on removals" policy is unnecessary, and would
   124	  recompute on every real edit anyway.
   125	- DD's engine is not needed for the speed. Its value is the 71-line statement of the rules.
   126	- The 0.7-1.1 s id unification remains the dominant cost of an incremental relink. Stable ids at extraction are
   127	  still step one.
     1	# Differential Dataflow: incremental relink of a real edit (MinIO)
     2	
     3	The question this column adds to the grid: **after a small source edit, how much of a relink's work must be
     4	redone?** Answered here for problem 2 (reach) and problem 1a+1b (resolve), with two real edits of MinIO rebuilt
     5	with the frozen toolchain, and Differential Dataflow (DD 0.26.0 / timely 0.32.0) keeping the result up to date.
     6	Code: `systems/ddflow/` (`src/main.rs` reach + driver, `src/p1.rs` resolve, `src/facts.rs` loading, id unification,
     7	diff). Timings: `systems/ddflow/bench.sh` (interleaved with ref/flat/p2, p2par, p1, p1par),
     8	`bench-20261116T1940Z.txt` (the numbers below; load 4.4-5.6) and `bench-20261116T1930Z.txt` (earlier run, load
     9	2.8-5.3, P2 only, same picture).
    10	
    11	## The two edits (real source edits, rebuilt, re-extracted)
    12	
    13	Both start from a `cp -r` of `/home/exedev/targets/minio` (unchanged), are built by bottom-up's
    14	`link/freeze.sh` (`GOTOOLCHAIN=local` go1.27.4, `go build -a -trimpath`, the stock procedure) into
    15	`/home/exedev/targets/frozen-edit/<name>`, and dumped by `ref/flat/extract.sh` (hlink `-dumpflat`) to
    16	`/home/exedev/paradigms/data/<name>`. `ref/flat/p2` and `p1` pass on both new dumps against their own oracles.
    17	
    18	| | edit 1: `minio-edit` (add a call) | edit 2: `minio-edit2` (delete a call) |
    19	|---|---|---|
    20	| source change | `internal/amztime/iso8601_time.go`: `ISO8601Parse` falls back to `http.ParseTime` (5 lines, `systems/ddflow/edit.patch`) | `cmd/server-main.go`: the console UI start-up block deleted (16 lines, `edit2.patch`) |
    21	| rebuild | 2 min 3 s (`freeze.sh`, nice 15) | 1 min 7 s |
    22	| archives that changed bytes | 4 of 1,311 + main.a (amztime, its importers cmd, lifecycle, object/lock: export fingerprints) | cmd + main.a |
    23	| objects whose symbol lists differ | 1 (amztime) | 1 (cmd) |
    24	| nslots / P2 edges / live | +3 / +30 / **+1** (663,631) | -37 / -48 / **-12,310** (651,320) |
    25	| image | +128 B of types | **-21.5 MB** (the console, go-openapi, swagger, websocket... die) |
    26	
    27	`http.ParseTime` was inlined into `ISO8601Parse`, so edit 1 adds no call edge to it: its new plain edges are to
    28	`net/http.timeFormats` (already live) and new GC-locals symbols.
    29	
    30	## Ids: why a unification step is needed first
    31	
    32	Slot ids are positional (`object base + index`). Edit 1 adds 3 slots to object 109 of 1,421, so every slot in
    33	objects 110..1420 changes id; a raw fact diff would be ~all facts. `facts::unify` maps version B's ids into A's:
    34	
    35	1. objects pair by index (both dumps are in importcfg order, sorted by package path; counts must agree);
    36	2. an object whose slot keys (kind, name, 128-bit name key, 128-bit content hash) are identical maps by position
    37	   (1,420 of 1,421 objects in both edits);
    38	3. in a differing object, a key that occurs exactly once on each side (a named symbol, a content hash) is the
    39	   same symbol wherever it moved; the rest (unnamed DWARF range/location symbols, duplicate names) is aligned in
    40	   order with a Myers diff, with the size/rank bits as part of the key for unnamed slots;
    41	4. linker-made ids map by position; method-name strings by content; unmatched B slots get fresh ids.
    42	
    43	Unify takes 0.7-1.1 s (reading the name and key columns of both dumps). The mapping choice changes the *fact*
    44	diff a little (edit 1: 9 plain edges with ordinal keys, 27 with the final rank-keyed alignment) but not the live
    45	diff. A first try that keyed unnamed slots by ordinal inside the object gave edit 2 a spurious 81,882-edge diff
    46	(every unnamed DWARF symbol after the deleted block shifted); the alignment brings it to 234.
    47	
    48	## Diff sizes (the finding in itself)
    49	
    50	Problem 2 facts (edges by kind, flag bytes, method tables, roots), A has 9,741,684:
    51	
    52	| | edit 1 | edit 2 |
    53	|---|---|---|
    54	| slots only in A / only in B | 14 / 17 | 81 / 44 |
    55	| edges - / + | 11 / 16 | 135 / 99 |
    56	| flag facts - / + | 14 / 16 | 73 / 44 |
    57	| method-table facts, roots | 0, 0 | 0, 0 |
    58	| **symbols with changed facts** | **31** (3.2e-6 of facts) | **141** |
    59	| **live ids changed** | **-1 +2** of 663,630 | **-12,320 +10** (1.9%) |
    60	
    61	Edit 1's live change is one GC-locals symbol replaced by two. Edit 2's 12,320 dead ids are the console
    62	(github.com/minio/console/api 869, madmin-go 846, models 457, go-openapi 1,000+, websocket 331, type name data
    63	2,237, ...); its +10 are 7 closures that the compiler renumbered (`serverMain.func15.2` was `func15.3`: a rename
    64	looks like delete + add to name-keyed identity) and 3 etcd closures whose inlining changed. Full lists:
    65	`results/ddflow-inc-minio-edit.log`, `results/ddflow-inc-minio-edit2.log`.
    66	
    67	Problem 1a+1b facts (named and candidate slots with their keys and ranks, other slots, relocations with raw
    68	targets), A has 14,609,633:
    69	
    70	| | edit 1 | edit 2 |
    71	|---|---|---|
    72	| named-slot facts - / + | 18 / 19 | **34,499 / 34,497** |
    73	| other slots, candidates | 14/16, 1/1 | 79/44, 0/0 |
    74	| relocations - / + | 19 / 52 | 192 / 148 |
    75	| canonical ids changed | -14 +17 | -73 +44 |
    76	| relocation targets changed | -19 +52 | -171 +127 |
    77	
    78	The 34,497 in edit 2 is churn from one representation choice: the winner's tie-break "lowest id" is kept exact by
    79	ordering on (object, index in object) instead of the global id, so other objects growing does not matter, but
    80	every named slot in `cmd` after the deleted code shifts its index. Only 73 canonical ids actually change
    81	(IDEAS.md 2: a tie-break that does not use the position would remove the churn).
    82	
    83	## Checks (every run)
    84	
    85	`ddflow inc A B` checks: the live set after full A equals A's oracle; after the update A->B it equals B's oracle
    86	mapped through the id map; after B->A it equals A's again; a second dataflow run from scratch on B's facts (in
    87	unified ids, same process) equals the updated set exactly and equals B's oracle. `ddflow p1inc A B` checks
    88	canon and every relocation target, A and after the update, against both versions' p1 oracles (5.2 M canon entries,
    89	9.3 M nonzero targets). All pass at 1 and 8 workers for both edits (`grep -c 'P2 OK\|P1 OK'` in the bench output;
    90	the bench aborts on any failure).
    91	
    92	## Times (ms, min / median / max, 5 interleaved rounds, 2026-11-16T19:39-19:49Z, load 4.4-5.6)
    93	
    94	Updates are 5 per process in each direction (25 per cell; P1: 10 per process, 50 per cell); full runs are one per
    95	process. "Recompute" for C is the warm flood only (inputs mapped, CSR given), DD's full is warm too (facts in
    96	memory, arrangements built inside the timing).
    97	
    98	### Problem 2
    99	
   100	| | 1 core / worker | 8 |
   101	|---|---|---|
   102	| C p2 recompute (flood, warm) | 41.7 / 56.5 / 92.6 (edit 1), 46.1 / 48.8 / 52.1 (edit 2) | |
   103	| C p2par recompute (best of 5) | 27.7 / 30.8 / 118.4; 24.8 / 31.2 / 34.2 | 12.7 / 20.5 / 30.1; 16.2 / 19.4 / 34.9 |
   104	| DD full A | 2447 / 2703 / 3839 | 1389 / 1621 / 2118 |
   105	| DD full B from scratch | 2105 / 2143 / 3310 | 491 / 1628 / 1943 |
   106	| **DD update, edit 1** (A->B; B->A) | **1.3 / 1.4 / 3.4; 1.3 / 1.4 / 1.9** | 1.8 / 9.0 / 81.9; 1.8 / 4.3 / 71.6 |
   107	| **DD update, edit 2** | **54.9 / 65.3 / 113; 50.0 / 64.7 / 118** | 28.0 / 555 / 688; 28.2 / 526 / 607 |
   108	
   109	- Edit 1: the update is **~1,900x** DD's own full run, **35-40x faster than the single-core C recompute** and ~20x
   110	  p2par on 1 thread; on 8 cores it is a tie to ~2x faster than p2par's 8-thread recompute (medians 4.3-9.0 vs
   111	  20.5) with a long tail.
   112	- Edit 2 (12 K ids die): the update is ~40x DD's full but **slower than recomputing in C** (65 vs 49 single
   113	  core, 19 on 8 threads). Retracting reachability is DD's expensive case: every dead id's derivation is withdrawn
   114	  round by round through the loop (45 rounds), with the arrangements' history of every round.
   115	- **Corrected 20:12Z (results/c-incremental.md): on a quiet machine (load 0.7-1.7) edit 2 updates in 29 ms on 8
   116	  workers vs 59 on 1, edit 1 in 2.1 vs 1.4; the slowdown below is load.** At load 4.4-5.6, 8 workers made updates *slower* (edit 2: 526-555 median vs 65 on 1 worker): per-round coordination of a
   117	  small, many-round update. The 28 ms minimum shows what it costs when the machine is quiet for a moment.
   118	
   119	### Problem 1a+1b
   120	
   121	| | 1 | 8 |
   122	|---|---|---|
   123	| C p1 (join 1a + targets 1b, warm) | 260.9 / 287.0 / 563.3 | p1par: 19.9 / 22.0 / 43.7 |
   124	| DD full A (1a+1b) | 8659 / 9310 / 10823 | 1285 / 2023 / 3203 |
   125	| **DD update, edit 1** | **0.6 / 0.7 / 1.5** | 0.5 / 29.3 / 195 |
   126	| **DD update, edit 2** | **70.2 / 87.2 / 122** | 43.9 / 64.2 / 237 |
   127	
   128	Edit 1 resolve update is **~400x faster than single-core C recompute and ~30x faster than p1par on 8 threads**.
   129	Edit 2's update (35 K changed facts, mostly the tie-break churn) is ~3x faster than C single-core, ~3-4x slower than
   130	p1par.
   131	
   132	## What P1 (resolve) needs to be incremental
   133	
   134	Measured above, so this is what had to be true for it to work:
   135	
   136	1. **Symbol identity that does not move.** Positional ids shift every later object on any slot-count change.
   137	   The flat files give enough (name, name key, content hash, per-object order) to rebuild identity by matching;
   138	   a linker designed for incrementality would key symbols by (package, name) or content from the start and
   139	   would use the importcfg's package path for objects.
   140	2. **The by-name join keyed by strings, as a grouped reduce.** 1a is `reduce` over the 128-bit name key: a group
   141	   is recomputed only if one of its members changed (edit 1: ~20 groups). DD does this directly
   142	   (`members.reduce(max)`, 23 lines of dataflow for 1a+1b in `src/p1.rs`).
   143	3. **Tie-breaks that are not positional.** "Lowest id" had to become "lowest (object, index in object)" to stay
   144	   exact; that still churns inside the edited object (edit 2: 34 K facts). Ordering ties by something stable
   145	   (name hash) would make the update proportional to the edit.
   146	4. **Relocation targets as (package, index) or name, not base + index.** 1b reads `obj_pkg` bases; the raw
   147	   target `base + idx` was mapped into the stable id space at load. Expressed as a join of (relocation, raw
   148	   target) with canon, 1b updates only changed relocations and relocations whose target's canon changed
   149	   (edit 1: 71 of 9.3 M).
   150	5. **1c (content-hash join) depends on P2's marks**, so an incremental 1c needs the flood's output as an input
   151	   collection, which DD can wire as one dataflow (not done here).
   152	
   153	## Memory
   154	
   155	Peak RSS (`/usr/bin/time %M`): `p2` 1.35-1.43 GB, `inc` (two versions' facts + two dataflows) 2.7 GB, `p1inc`
   156	3.5 GB, all under `prlimit --as=12G`.
     1	// p2inc.c: problem 2 (reach) maintained under an edit, plainly, on one core. It floods version A (the previous
     2	// link's state, kept in memory as a resident linker would), then applies a stored diff (DATA_B/diff/, written by
     3	// `ddflow writediff`, see its README.md) as an update, and back, REPS times; every result is checked against B's
     4	// oracle in A's id space (diff/live_b.u32) or A's oracle.
     5	//   gcc -O2 -march=native -o build/p2inc ref/flat/p2inc.c && build/p2inc data/minio data/minio-edit [REPS] [plain]
     6	//   FUZZ=K build/p2inc ...: then K random diffs (up to 200 edges removed from live sources, 200 added, all kinds)
     7	//   on top of B, each checked against a from-scratch flood of the same graph, and reverted to B's oracle.
     8	//
     9	// The rules are p2.c's (data/README.md). The state kept between links, all derived by the same rules: live, iface,
    10	// typeDone (live, iface, type descriptor: its type edges and methods have been applied), called (per method),
    11	// counts of the aggregate facts sig(name, type), named(name), reflect (how many live sources assert them), and a
    12	// stamp per fact: the clock when it was set, so every fact was derived from facts with smaller stamps.
    13	//
    14	// An update is DRed (delete and rederive, Gupta-Mumick-Subrahmanian 1993), in three steps:
    15	//  1. over-delete, with the old flags: all removed edges leave the graph, then the facts they derived are
    16	//     retracted, transitively -- except that a fact is kept if it still has a derivation from a true fact with a
    17	//     smaller stamp (a predecessor, a root, a flag, a called method; a method keeps its call if a live source of
    18	//     its condition has a smaller stamp than the call). Every retraction re-checks what it supported, so what is
    19	//     kept has a chain of decreasing stamps down to roots and flags: it is derivable. ("plain": textbook DRed,
    20	//     nothing is kept, every fact downstream of a removal is retracted.)
    21	//  2. rederive, with the new flags and edges: each retracted fact with any derivation from true facts is set again
    22	//     (one reverse lookup: predecessors, calling methods, roots); the worklist restores what follows from it;
    23	//  3. insert: added edges of live sources, added roots, and what follows, by the same worklist as the initial
    24	//     flood (only new targets are pushed).
    25	// Why not support counts: liveness is cyclic (a function and its closure keep each other alive), so a count of
    26	// live predecessors never reaches zero for a dead cycle. The same holds for the method rule: a sig's remaining
    27	// sources can be the method's own targets, which is why a method's condition is checked by stamps, not counts
    28	// (found by the fuzzer). Why not "recompute when a live edge is removed": every real edit removes live edges (the
    29	// edited function's GC-locals symbols are renamed), so that policy would always recompute. Why stamps: textbook
    30	// DRed over-deletes 594 K of 663 K live ids for edit 2 (a retracted source of reflect/sig/named retracts every
    31	// method call it may justify, and everything downstream of those); with stamps the cone is 26 K.
    32	// Method-table diffs are not supported (exit 3; neither edit has one).
    33	#include "flat.h"
    34	
    35	enum { fDef = 1, fReflect = 4, fGoType = 8, fIface = 16 };
    36	#define ISTYPE(x) ((flags[x] & (fDef | fGoType)) == (fDef | fGoType))
    37	
    38	static u32 N, nslA, nmeth, nstrU;
    39	static u8 *flags;
    40	// A's edges, deduplicated per source: CSR eoff/ekind/eval (kind 2 val = name << 32 | type; kind 3 val = name)
    41	static u32 *eoff;
    42	static u8 *ekind;
    43	static u64 *eval;
    44	// reverse of A's kind 0 and 1 edges: roff/rsrc/rkind
    45	static u32 *roff, *rsrc;
    46	static u8 *rkind;
    47	// methods (A's tables; method-table diffs are not supported: both edits have none)
    48	static u32 *moff, *mown, *mname, *mtyp, *mifn, *mtfn, *revoff, *revm, *nameoff, *namem, *expm, nexp;
    49	static u8 *mexp;
    50	// state
    51	static u8 *live, *iface, *typeDone, *called, *isRoot;
    52	// derivation order: a fact's stamp is the clock when it was set, so it was derived from facts with smaller stamps
    53	static u32 clk, *ordL, *ordI, *ordT, *ordM;
    54	// where the last support check of x found its support in x's reverse list: entries before it can never support x
    55	// again while x keeps its stamp (a retracted predecessor comes back only with a larger stamp), so the scan resumes
    56	// there; reset when x is stamped anew. This bounds the checks of x to one pass over its predecessors per stamp.
    57	static u32 *posL, *posI;
    58	static int useOrder = 1; // argv[4] "plain": textbook DRed (retract everything downstream, then rederive)
    59	static u64 cChecks;
    60	static u32 *namedCnt, reflectCnt;
    61	// sig table: key -> count, and the methods with that (name, mtyp) as a sorted-run range
    62	typedef struct { u64 key; u32 cnt, mstart, mlen, sstart, slen; } sigent;
    63	static sigent *sigs;
    64	static u64 smask;
    65	static u32 *bysig;
    66	// the sources asserting each aggregate fact in A (for well-founded checks of a method's condition): sig key ->
    67	// sources (sigent.sstart/slen into sigsrc), name -> sources (namesrc CSR), reflect callers (reflsrc)
    68	static u32 *sigsrc, *nsoff, *namesrc, *reflsrc, nrefl;
    69	static u32 *msusp;
    70	static size_t nmsusp, msuspcap;
    71	// overlay: edges removed from A (hash set), edges added (per-source and per-target lists)
    72	typedef struct { u32 src; u8 kind, alive; u64 val; } remEnt;
    73	static remEnt *rem;
    74	static u64 rmask;
    75	static u8 *maybeRem;
    76	typedef struct { u32 src, next, rnext; u8 kind, alive; u64 val; } addEnt;
    77	static addEnt *pool;
    78	static u32 npool, *addHead, *raddHead;
    79	// worklist and counters
    80	enum { LIVE, IFACE, KL, KI };
    81	static u64 *wl;
    82	static size_t wn, wcap;
    83	static u32 epoch, *expanded;
    84	static u64 cEdges, cRev, cKilled, cRestored, cSet;
    85	static u32 *susp;
    86	static size_t nsusp, suspcap;
    87	
    88	static void push(int t, u32 x) {
    89		if (wn == wcap) wl = realloc(wl, 8 * (wcap = wcap ? 2 * wcap : 1 << 20));
    90		wl[wn++] = (u64)t << 32 | x;
    91	}
    92	static void suspect(u32 x) {
    93		if (nsusp == suspcap) susp = realloc(susp, 4 * (suspcap = suspcap ? 2 * suspcap : 1 << 16));
    94		susp[nsusp++] = x;
    95	}
    96	static sigent *sig(u64 k) {
    97		for (u64 h = mix64(k) & smask;; h = (h + 1) & smask) {
    98			if (sigs[h].key == k + 1) return &sigs[h];
    99			if (sigs[h].key == 0) { sigs[h].key = k + 1; return &sigs[h]; }
   100		}
   101	}
   102	static remEnt *remFind(u32 s, u8 k, u64 v, int add) {
   103		for (u64 h = mix64(v * 31 + s * 7 + k) & rmask;; h = (h + 1) & rmask) {
   104			remEnt *e = &rem[h];
   105			if (e->alive != 0xff && e->src == s && e->kind == k && e->val == v) return e;
   106			if (e->alive == 0xff) {
   107				if (!add) return NULL;
   108				e->src = s, e->kind = k, e->val = v, e->alive = 0;
   109				return e;
   110			}
   111		}
   112	}
   113	static inline int removed(u32 s, u8 k, u64 v) {
   114		if (!maybeRem[s]) return 0;
   115		remEnt *e = remFind(s, k, v, 0);
   116		return e && e->alive;
   117	}
   118	// iterate the current out-edges of x: A's minus removed, plus added
   119	#define FOR_EDGES(x, K, V, BODY)                                                       \
   120		do {                                                                               \
   121			if ((x) <= nslA)                                                               \
   122				for (u32 e_ = eoff[x]; e_ < eoff[(x) + 1]; e_++) {                          \
   123					u8 K = ekind[e_];                                                      \
   124					u64 V = eval[e_];                                                      \
   125					cEdges++;                                                              \
   126					if (removed(x, K, V)) continue;                                        \
   127					BODY                                                                   \
   128				}                                                                          \
   129			for (u32 a_ = addHead[x]; a_; a_ = pool[a_].next) {                            \
   130				if (!pool[a_].alive) continue;                                             \
   131				u8 K = pool[a_].kind;                                                      \
   132				u64 V = pool[a_].val;                                                      \
   133				cEdges++;                                                                  \
   134				BODY                                                                       \
   135			}                                                                              \
   136		} while (0)
   137	// iterate the current kind 0/1 predecessors of x
   138	#define FOR_PREDS(x, S, K, BODY)                                                       \
   139		do {                                                                               \
   140			if ((x) <= nslA)                                                               \
   141				for (u32 r_ = roff[x]; r_ < roff[(x) + 1]; r_++) {                          \
   142					u32 S = rsrc[r_];                                                      \
   143					u8 K = rkind[r_];                                                      \
   144					cRev++;                                                                \
   145					if (removed(S, K, x)) continue;                                        \
   146					BODY                                                                   \
   147				}                                                                          \
   148			for (u32 a_ = raddHead[x]; a_; a_ = pool[a_].rnext) {                          \
   149				if (!pool[a_].alive) continue;                                             \
   150				u32 S = pool[a_].src;                                                      \
   151				u8 K = pool[a_].kind;                                                      \
   152				cRev++;                                                                    \
   153				BODY                                                                       \
   154			}                                                                              \
   155		} while (0)
   156	
   157	// ---------------------------------------------------------------- additive rules (the flood)
   158	static inline void setLive(u32 x) {
   159		if (x && !live[x]) { live[x] = 1; ordL[x] = ++clk; posL[x] = 0; cSet++; push(LIVE, x); }
   160	}
   161	static inline void setIface(u32 x) {
   162		if (x && !iface[x]) { iface[x] = 1; ordI[x] = ++clk; posI[x] = 0; if (live[x]) push(IFACE, x); }
   163	}
   164	static inline int cond(u32 m) {
   165		return (reflectCnt && mexp[m]) || sig((u64)mname[m] << 32 | mtyp[m])->cnt || namedCnt[mname[m]];
   166	}
   167	static void tryCall(u32 m) {
   168		if (called[m] || !typeDone[mown[m]] || !cond(m)) return;
   169		called[m] = 1;
   170		ordM[m] = ++clk;
   171		setLive(mtyp[m]), setLive(mifn[m]), setLive(mtfn[m]);
   172	}
   173	static void typeAct(u32 x) {
   174		if (typeDone[x]) return;
   175		typeDone[x] = 1;
   176		ordT[x] = ++clk;
   177		FOR_EDGES(x, k, v, if (k == 0 && ISTYPE((u32)v)) setIface((u32)v););
   178		if (x <= nslA)
   179			for (u32 m = moff[x]; m < moff[x + 1]; m++) setIface(mtyp[m]), tryCall(m);
   180	}
   181	static void sigInc(u64 k) {
   182		sigent *s = sig(k);
   183		if (s->cnt++ == 0)
   184			for (u32 i = 0; i < s->mlen; i++) tryCall(bysig[s->mstart + i]);
   185	}
   186	static void namedInc(u32 n) {
   187		if (namedCnt[n]++ == 0)
   188			for (u32 i = nameoff[n]; i < nameoff[n + 1]; i++) tryCall(namem[i]);
   189	}
   190	static void reflectInc(void) {
   191		if (reflectCnt++ == 0)
   192			for (u32 i = 0; i < nexp; i++) tryCall(expm[i]);
   193	}
   194	static void edgeOn(u32 x, u8 k, u64 v) {
   195		switch (k) {
   196		case 0: setLive((u32)v); if (typeDone[x] && ISTYPE((u32)v)) setIface((u32)v); break;
   197		case 1: setIface((u32)v); break;
   198		case 2: sigInc(v); break;
   199		case 3: namedInc((u32)v); break;
   200		}
   201	}
   202	static void expandLive(u32 x) {
   203		if (!(flags[x] & fDef)) return;
   204		expanded[x] = epoch;
   205		FOR_EDGES(x, k, v, edgeOn(x, k, v););
   206		if (flags[x] & fReflect) reflectInc();
   207		if (ISTYPE(x) && iface[x]) typeAct(x);
   208	}
   209	
   210	// ---------------------------------------------------------------- retraction
   211	// a well-founded support: a derivation from a true fact with a smaller stamp. Keeping x when it has one, and
   212	// re-checking x whenever one of its supports is retracted, leaves only facts with a chain of decreasing stamps
   213	// down to a root (or a flag): derivable. Everything else is retracted, then step 2 rederives what has a support
   214	// with a larger stamp (a cycle-free path that was not the first one found).
   215	static int supLive(u32 x) {
   216		if (!useOrder) return 0;
   217		cChecks++;
   218		if (isRoot[x]) return 1;
   219		u32 o = ordL[x];
   220		if (x <= nslA)
   221			for (u32 r = roff[x] + posL[x]; r < roff[x + 1]; r++) {
   222				u32 s = rsrc[r];
   223				cRev++;
   224				if (rkind[r] == 0 && live[s] && (flags[s] & fDef) && ordL[s] < o && !removed(s, 0, x)) { posL[x] = r - roff[x]; return 1; }
   225			}
   226		for (u32 a = raddHead[x]; a; a = pool[a].rnext)
   227			if (pool[a].alive && pool[a].kind == 0 && live[pool[a].src] && (flags[pool[a].src] & fDef) && ordL[pool[a].src] < o) return 1;
   228		if (x <= nslA)
   229			for (u32 i = revoff[x]; i < revoff[x + 1]; i++) if (called[revm[i]] && ordM[revm[i]] < o) return 1;
   230		return 0;
   231	}
   232	static int supIface(u32 x) {
   233		if (!useOrder) return 0;
   234		cChecks++;
   235		if (flags[x] & fIface) return 1;
   236		u32 o = ordI[x];
   237	#define IOK(s, k) (live[s] && (flags[s] & fDef) && (((k) == 1 && ordL[s] < o) || ((k) == 0 && typeDone[s] && ISTYPE(x) && ordT[s] < o)))
   238		if (x <= nslA)
   239			for (u32 r = roff[x] + posI[x]; r < roff[x + 1]; r++) {
   240				u32 s = rsrc[r];
   241				cRev++;
   242				if (IOK(s, rkind[r]) && !removed(s, rkind[r], x)) { posI[x] = r - roff[x]; return 1; }
   243			}
   244		for (u32 a = raddHead[x]; a; a = pool[a].rnext)
   245			if (pool[a].alive && IOK(pool[a].src, pool[a].kind)) return 1;
   246		if (x <= nslA)
   247			for (u32 i = revoff[x]; i < revoff[x + 1]; i++) if (mtyp[revm[i]] == x && typeDone[mown[revm[i]]] && ordT[mown[revm[i]]] < o) return 1;
   248		return 0;
   249	}
   250	static int forceKill;
   251	static inline void killLive(u32 x) {
   252		if (x && live[x] && (forceKill || !supLive(x))) { live[x] = 0; cKilled++; suspect(x); push(KL, x); }
   253	}
   254	static inline void killIface(u32 x) {
   255		if (x && iface[x] && (forceKill || !supIface(x))) { iface[x] = 0; suspect(x); push(KI, x); }
   256	}
   257	static void uncall(u32 m) {
   258		if (!called[m]) return;
   259		called[m] = 0;
   260		if (nmsusp == msuspcap) msusp = realloc(msusp, 4 * (msuspcap = msuspcap ? 2 * msuspcap : 1 << 12));
   261		msusp[nmsusp++] = m;
   262		killLive(mtyp[m]), killLive(mifn[m]), killLive(mtfn[m]);
   263	}
   264	static void typeDeact(u32 x) {
   265		if (!typeDone[x]) return;
   266		typeDone[x] = 0;
   267		FOR_EDGES(x, k, v, if (k == 0 && ISTYPE((u32)v)) killIface((u32)v););
   268		if (x <= nslA)
   269			for (u32 m = moff[x]; m < moff[x + 1]; m++) killIface(mtyp[m]), uncall(m);
   270	}
   271	// Is called method m still supported by facts with smaller stamps? Its type is (typeDone: else it was uncalled);
   272	// its condition needs a live source asserting it with a stamp below m's. A count alone is not enough: the
   273	// remaining sources may be m's own targets (a cycle through the method). Plain DRed: never (retract, rederive).
   274	static inline int srcOK(u32 s, u8 k, u64 v, u32 o) { return live[s] && (flags[s] & fDef) && ordL[s] < o && !removed(s, k, v); }
   275	static int condWF(u32 m) {
   276		if (!useOrder) return 0;
   277		cChecks++;
   278		u32 o = ordM[m];
   279		u64 key = (u64)mname[m] << 32 | mtyp[m];
   280		sigent *se = sig(key);
   281		if (se->cnt)
   282			for (u32 i = 0; i < se->slen; i++) { cRev++; if (srcOK(sigsrc[se->sstart + i], 2, key, o)) return 1; }
   283		u32 n = mname[m];
   284		if (namedCnt[n])
   285			for (u32 i = nsoff[n]; i < nsoff[n + 1]; i++) { cRev++; if (srcOK(namesrc[i], 3, n, o)) return 1; }
   286		if (mexp[m] && reflectCnt)
   287			for (u32 i = 0; i < nrefl; i++) { cRev++; u32 r = reflsrc[i]; if (live[r] && (flags[r] & (fDef | fReflect)) == (fDef | fReflect) && ordL[r] < o) return 1; }
   288		for (u32 a = 1; a <= npool; a++) // added edges asserting it
   289			if (pool[a].alive && ((pool[a].kind == 2 && pool[a].val == key) || (pool[a].kind == 3 && pool[a].val == n)) && live[pool[a].src] && ordL[pool[a].src] < o) return 1;
   290		return 0;
   291	}
   292	static void sigDec(u64 k) {
   293		sigent *s = sig(k);
   294		--s->cnt;
   295		for (u32 i = 0; i < s->mlen; i++) { u32 m = bysig[s->mstart + i]; if (called[m] && !condWF(m)) uncall(m); }
   296	}
   297	static void namedDec(u32 n) {
   298		--namedCnt[n];
   299		for (u32 i = nameoff[n]; i < nameoff[n + 1]; i++) if (called[namem[i]] && !condWF(namem[i])) uncall(namem[i]);
   300	}
   301	static void reflectDec(void) {
   302		--reflectCnt;
   303		for (u32 i = 0; i < nexp; i++) if (called[expm[i]] && !condWF(expm[i])) uncall(expm[i]);
   304	}
   305	static void edgeOff(u32 x, u8 k, u64 v) {
   306		switch (k) {
   307		case 0: killLive((u32)v); if (typeDone[x] && ISTYPE((u32)v)) killIface((u32)v); break;
   308		case 1: killIface((u32)v); break;
   309		case 2: sigDec(v); break;
   310		case 3: namedDec((u32)v); break;
   311		}
   312	}
   313	
   314	static void drain(void) {
   315		while (wn) {
   316			u64 it = wl[--wn];
   317			u32 x = (u32)it;
   318			switch ((int)(it >> 32)) {
   319			case LIVE: if (live[x]) expandLive(x); break;
   320			case IFACE: if (live[x] && iface[x] && ISTYPE(x) && (flags[x] & fDef)) typeAct(x); break;
   321			case KL:
   322				if (flags[x] & fDef) {
   323					FOR_EDGES(x, k, v, edgeOff(x, k, v););
   324					if (flags[x] & fReflect) reflectDec();
   325				}
   326				typeDeact(x);
   327				break;
   328			case KI: typeDeact(x); break;
   329			}
   330		}
   331	}
   332	
   333	// ---------------------------------------------------------------- rederivation checks
   334	static int derivLive(u32 x) {
   335		if (isRoot[x]) return 1;
   336		FOR_PREDS(x, s, k, if (k == 0 && live[s] && (flags[s] & fDef)) return 1;);
   337		if (x <= nslA)
   338			for (u32 i = revoff[x]; i < revoff[x + 1]; i++) if (called[revm[i]]) return 1;
   339		return 0;
   340	}
   341	static int derivIface(u32 x) {
   342		if (flags[x] & fIface) return 1;
   343		FOR_PREDS(x, s, k, if (live[s] && (flags[s] & fDef) && (k == 1 || (typeDone[s] && ISTYPE(x)))) return 1;);
   344		if (x <= nslA)
   345			for (u32 i = revoff[x]; i < revoff[x + 1]; i++) if (mtyp[revm[i]] == x && typeDone[mown[revm[i]]]) return 1;
   346		return 0;
   347	}
   348	
   349	// ---------------------------------------------------------------- the diff
   350	typedef struct {
   351		size_t ne, nf, nr;
   352		const u32 *esrc, *fid, *root;
   353		const u8 *ekind, *fval;
   354		const u64 *eval;
   355	} side;
   356	static side loadSide(const char *s) {
   357		side d;
   358		char p[64];
   359		size_t n;
   360		snprintf(p, sizeof p, "diff/edge_%s_src.u32", s), d.esrc = FMAP(u32, p, &n), d.ne = n / 4;
   361		snprintf(p, sizeof p, "diff/edge_%s_kind.u8", s), d.ekind = FMAP(u8, p, NULL);
   362		snprintf(p, sizeof p, "diff/edge_%s_val.u64", s), d.eval = FMAP(u64, p, NULL);
   363		snprintf(p, sizeof p, "diff/flag_%s_id.u32", s), d.fid = FMAP(u32, p, &n), d.nf = n / 4;
   364		snprintf(p, sizeof p, "diff/flag_%s_val.u8", s), d.fval = FMAP(u8, p, NULL);
   365		snprintf(p, sizeof p, "diff/root_%s.u32", s), d.root = FMAP(u32, p, &n), d.nr = n / 4;
   366		snprintf(p, sizeof p, "diff/meth_%s_type.u32", s), FMAP(u32, p, &n);
   367		if (n) { fprintf(stderr, "method-table diffs are not supported (recompute instead)\n"); exit(3); }
   368		return d;
   369	}
   370	static int inA(u32 s, u8 k, u64 v) {
   371		if (s > nslA) return 0;
   372		for (u32 e = eoff[s]; e < eoff[s + 1]; e++) if (ekind[e] == k && eval[e] == v) return 1;
   373		return 0;
   374	}
   375	static void edgeRemove(u32 s, u8 k, u64 v) {
   376		if (inA(s, k, v)) { remFind(s, k, v, 1)->alive = 1; maybeRem[s] = 1; return; }
   377		for (u32 a = addHead[s]; a; a = pool[a].next)
   378			if (pool[a].alive && pool[a].kind == k && pool[a].val == v) pool[a].alive = 0;
   379	}
   380	static void edgeAdd(u32 s, u8 k, u64 v) {
   381		if (inA(s, k, v)) { remFind(s, k, v, 1)->alive = 0; return; }
   382		addEnt *a = &pool[++npool];
   383		a->src = s, a->kind = k, a->val = v, a->alive = 1;
   384		a->next = addHead[s], addHead[s] = npool;
   385		if (k <= 1) a->rnext = raddHead[(u32)v], raddHead[(u32)v] = npool;
   386	}
   387	
   388	// apply minus then plus as one update
   389	static void update(const side *mi, const side *pl) {
   390		epoch++;
   391		nsusp = nmsusp = 0;
   392		// 1. over-delete, with the old flags
   393		// (all removed edges leave the graph before any retraction: a source retracted by an earlier removal must
   394		// not leave a later removed edge's target unchecked)
   395		static u8 *was;
   396		static size_t wasCap;
   397		if (mi->ne > wasCap) was = realloc(was, wasCap = mi->ne);
   398		for (size_t i = 0; i < mi->ne; i++) {
   399			u32 s = mi->esrc[i];
   400			was[i] = live[s] && (flags[s] & fDef);
   401			edgeRemove(s, mi->ekind[i], mi->eval[i]);
   402		}
   403		for (size_t i = 0; i < mi->ne; i++)
   404			if (was[i]) edgeOff(mi->esrc[i], mi->ekind[i], mi->eval[i]);
   405		forceKill = 1;
   406		for (size_t i = 0; i < mi->nr; i++) isRoot[mi->root[i]] = 0, killLive(mi->root[i]);
   407		for (size_t i = 0; i < mi->nf; i++) killLive(mi->fid[i]), killIface(mi->fid[i]);
   408		for (size_t i = 0; i < pl->nf; i++) killLive(pl->fid[i]), killIface(pl->fid[i]);
   409		forceKill = 0;
   410		drain();
   411		// the new flags, edges, roots
   412		for (size_t i = 0; i < mi->nf; i++) flags[mi->fid[i]] = 0;
   413		for (size_t i = 0; i < pl->nf; i++) flags[pl->fid[i]] = pl->fval[i];
   414		for (size_t i = 0; i < pl->ne; i++) edgeAdd(pl->esrc[i], pl->ekind[i], pl->eval[i]);
   415		for (size_t i = 0; i < pl->nr; i++) isRoot[pl->root[i]] = 1;
   416		// 2. rederive what still has a derivation; the worklist restores what follows from it
   417		for (size_t i = 0; i < pl->nf; i++) suspect(pl->fid[i]);
   418		for (size_t i = 0; i < nsusp; i++) {
   419			u32 x = susp[i];
   420			if (!live[x] && derivLive(x)) setLive(x), cRestored++;
   421			if (!iface[x] && derivIface(x)) setIface(x);
   422		}
   423		// methods whose condition lost its well-founded support were uncalled; their types may still be active
   424		for (size_t i = 0; i < nmsusp; i++) tryCall(msusp[i]);
   425		drain();
   426		// 3. insert: added edges of live sources not yet expanded with them, added roots
   427		for (size_t i = 0; i < pl->ne; i++) {
   428			u32 s = pl->esrc[i];
   429			if (live[s] && (flags[s] & fDef) && expanded[s] != epoch) edgeOn(s, pl->ekind[i], pl->eval[i]);
   430		}
   431		for (size_t i = 0; i < pl->nr; i++) setLive(pl->root[i]);
   432		drain();
   433		// a method of a still-active type whose condition came back (sig/named counts restored) is retried by
   434		// sigInc/namedInc; one whose type was never deactivated but lost its condition and regained it, too.
   435	}
   436	
   437	static long checkLive(const char *what, const u32 *want, size_t nw) {
   438		size_t k = 0, bad = 0;
   439		for (u32 g = 1; g < N; g++)
   440			if (live[g]) { if (k >= nw || want[k] != g) bad++; else k++; }
   441		bad += nw - k;
   442		printf("check %s: %zu oracle entries, %zu differ -> %s\n", what, nw, bad, bad ? "FAIL" : "OK");
   443		return (long)bad;
   444	}
   445	
   446	int main(int argc, char **argv) {
   447		const char *da = argv[1], *db = argv[2];
   448		int reps = argc > 3 ? atoi(argv[3]) : 5;
   449		{
   450			char p[4096];
   451			snprintf(p, sizeof p, "%s/diff", db);
   452			flatDir = p;
   453			N = (u32)metaGet("n_unified");
   454			nstrU = (u32)metaGet("nstr_unified");
   455			flatDir = strdup(db);
   456		}
   457		side mi = loadSide("minus"), pl = loadSide("plus");
   458		size_t nlb;
   459		const u32 *liveB = FMAP(u32, "diff/live_b.u32", &nlb);
   460		nlb /= 4;
   461		flatDir = da;
   462		nslA = (u32)metaGet("nslots");
   463		size_t nroots, nlA;
   464		const u8 *fl = FMAP(u8, "p2_flags.u8", NULL), *ek = FMAP(u8, "p2_edge_kind.u8", NULL);
   465		const u32 *off = FMAP(u32, "p2_edge_off.u32", NULL), *ed = FMAP(u32, "p2_edge_dst.u32", NULL);
   466		const u32 *sigName = FMAP(u32, "p2_sig_name.u32", NULL), *sigType = FMAP(u32, "p2_sig_type.u32", NULL);
   467		size_t nm;
   468		moff = (u32 *)FMAP(u32, "p2_meth_off.u32", NULL), mname = (u32 *)FMAP(u32, "p2_meth_name.u32", &nm);
   469		nmeth = (u32)(nm / 4);
   470		mtyp = (u32 *)FMAP(u32, "p2_meth_mtyp.u32", NULL), mifn = (u32 *)FMAP(u32, "p2_meth_ifn.u32", NULL);
   471		mtfn = (u32 *)FMAP(u32, "p2_meth_tfn.u32", NULL), mexp = (u8 *)FMAP(u8, "p2_meth_exported.u8", NULL);
   472		const u32 *roots = FMAP(u32, "p2_roots.u32", &nroots);
   473		const u32 *liveA = FMAP(u32, "oracle/p2_live.u32", &nlA);
   474		nroots /= 4, nlA /= 4;
   475	
   476		double ts = now();
   477		// ---- setup (the resident state of the previous link): dedup'd CSR, reverse CSR, method indexes
   478		flags = calloc(N + 1, 1);
   479		for (u32 g = 1; g <= nslA; g++) flags[g] = fl[g] & 0x1d;
   480		eoff = malloc(4 * ((size_t)nslA + 2));
   481		ekind = malloc(off[nslA + 1]), eval = malloc(8 * (size_t)off[nslA + 1]);
   482		u32 ne = 0;
   483		typedef struct { u64 v; u32 k; } kv;
   484		int ckv(const void *a, const void *b) {
   485			const kv *x = a, *y = b;
   486			return x->k != y->k ? (x->k < y->k ? -1 : 1) : x->v < y->v ? -1 : x->v > y->v;
   487		}
   488		kv *tmp = malloc(sizeof(kv) * 65536);
   489		size_t tcap = 65536;
   490		for (u32 s = 0; s <= nslA; s++) {
   491			eoff[s] = ne;
   492			size_t n = 0;
   493			if (off[s + 1] - off[s] > tcap) tmp = realloc(tmp, sizeof(kv) * (tcap = off[s + 1] - off[s]));
   494			for (u32 e = off[s]; e < off[s + 1]; e++) {
   495				u8 k = ek[e];
   496				u32 t = ed[e];
   497				if (k <= 1 && t == 0) continue;
   498				tmp[n].k = k, tmp[n++].v = k == 2 ? (u64)sigName[t] << 32 | sigType[t] : t;
   499			}
   500			if (n > 1) qsort(tmp, n, sizeof(kv), ckv);
   501			for (size_t i = 0; i < n; i++)
   502				if (i == 0 || tmp[i].k != tmp[i - 1].k || tmp[i].v != tmp[i - 1].v) ekind[ne] = (u8)tmp[i].k, eval[ne++] = tmp[i].v;
   503		}
   504		free(tmp);
   505		eoff[nslA + 1] = ne;
   506		roff = calloc((size_t)nslA + 2, 4);
   507		for (u32 e = 0; e < ne; e++) if (ekind[e] <= 1 && eval[e] <= nslA) roff[eval[e] + 1]++;
   508		for (u32 g = 0; g <= nslA; g++) roff[g + 1] += roff[g];
   509		rsrc = malloc(4 * (size_t)roff[nslA + 1]), rkind = malloc(roff[nslA + 1]);
   510		{
   511			u32 *fill = malloc(4 * ((size_t)nslA + 1));
   512			memcpy(fill, roff, 4 * ((size_t)nslA + 1));
   513			for (u32 s = 0; s <= nslA; s++)
   514				for (u32 e = eoff[s]; e < eoff[s + 1]; e++)
   515					if (ekind[e] <= 1 && eval[e] <= nslA) rsrc[fill[eval[e]]] = s, rkind[fill[eval[e]]++] = ekind[e];
   516			free(fill);
   517		}
   518		mown = malloc(4 * (size_t)nmeth);
   519		for (u32 t = 0; t <= nslA; t++) for (u32 m = moff[t]; m < moff[t + 1]; m++) mown[m] = t;
   520		revoff = calloc((size_t)nslA + 2, 4);
   521		for (u32 m = 0; m < nmeth; m++) {
   522			u32 x[3] = {mtyp[m], mifn[m], mtfn[m]};
   523			for (int j = 0; j < 3; j++) if (x[j] && x[j] <= nslA) revoff[x[j] + 1]++;
   524		}
   525		for (u32 g = 0; g <= nslA; g++) revoff[g + 1] += revoff[g];
   526		revm = malloc(4 * (size_t)revoff[nslA + 1]);
   527		{
   528			u32 *fill = malloc(4 * ((size_t)nslA + 1));
   529			memcpy(fill, revoff, 4 * ((size_t)nslA + 1));
   530			for (u32 m = 0; m < nmeth; m++) {
   531				u32 x[3] = {mtyp[m], mifn[m], mtfn[m]};
   532				for (int j = 0; j < 3; j++) if (x[j] && x[j] <= nslA) revm[fill[x[j]]++] = m;
   533			}
   534			free(fill);
   535		}
   536		nameoff = calloc(nstrU + 2, 4);
   537		for (u32 m = 0; m < nmeth; m++) nameoff[mname[m] + 1]++;
   538		for (u32 i = 0; i <= nstrU; i++) nameoff[i + 1] += nameoff[i];
   539		namem = malloc(4 * (size_t)nmeth);
   540		{
   541			u32 *fill = malloc(4 * (nstrU + 1));
   542			memcpy(fill, nameoff, 4 * (nstrU + 1));
   543			for (u32 m = 0; m < nmeth; m++) namem[fill[mname[m]]++] = m;
   544			free(fill);
   545		}
   546		expm = malloc(4 * (size_t)nmeth);
   547		for (u32 m = 0; m < nmeth; m++) if (mexp[m]) expm[nexp++] = m;
   548		smask = (1u << 19) - 1;
   549		sigs = calloc(smask + 1, sizeof(sigent));
   550		{
   551			// group methods by (name, mtyp): sort method ids by key
   552			u64 *km = malloc(8 * (size_t)nmeth * 2);
   553			for (u32 m = 0; m < nmeth; m++) km[2 * m] = (u64)mname[m] << 32 | mtyp[m], km[2 * m + 1] = m;
   554			int c2(const void *a, const void *b) { u64 x = *(u64 *)a, y = *(u64 *)b; return x < y ? -1 : x > y; }
   555			qsort(km, nmeth, 16, c2);
   556			bysig = malloc(4 * (size_t)nmeth);
   557			for (u32 i = 0; i < nmeth; i++) bysig[i] = (u32)km[2 * i + 1];
   558			for (u32 i = 0; i < nmeth;) {
   559				u32 j = i;
   560				while (j < nmeth && km[2 * j] == km[2 * i]) j++;
   561				sigent *s = sig(km[2 * i]);
   562				s->mstart = i, s->mlen = j - i;
   563				i = j;
   564			}
   565			free(km);
   566		}
   567		{
   568			// sources of each sig key and name, reflect callers
   569			u64 *ks = malloc(16 * (size_t)ne);
   570			size_t nk = 0;
   571			nsoff = calloc(nstrU + 2, 4);
   572			for (u32 x = 0; x <= nslA; x++)
   573				for (u32 e = eoff[x]; e < eoff[x + 1]; e++) {
   574					if (ekind[e] == 2) ks[2 * nk] = eval[e], ks[2 * nk++ + 1] = x;
   575					if (ekind[e] == 3) nsoff[eval[e] + 1]++;
   576				}
   577			int c3(const void *a, const void *b) { u64 x = *(u64 *)a, y = *(u64 *)b; return x < y ? -1 : x > y; }
   578			qsort(ks, nk, 16, c3);
   579			sigsrc = malloc(4 * (nk + 1));
   580			for (size_t i = 0; i < nk;) {
   581				size_t j = i;
   582				while (j < nk && ks[2 * j] == ks[2 * i]) sigsrc[j] = (u32)ks[2 * j + 1], j++;
   583				sigent *se = sig(ks[2 * i]);
   584				se->sstart = (u32)i, se->slen = (u32)(j - i);
   585				i = j;
   586			}
   587			free(ks);
   588			for (u32 i = 0; i <= nstrU; i++) nsoff[i + 1] += nsoff[i];
   589			namesrc = malloc(4 * ((size_t)nsoff[nstrU + 1] + 1));
   590			u32 *fill = malloc(4 * (nstrU + 1));
   591			memcpy(fill, nsoff, 4 * (nstrU + 1));
   592			for (u32 x = 0; x <= nslA; x++)
   593				for (u32 e = eoff[x]; e < eoff[x + 1]; e++) if (ekind[e] == 3) namesrc[fill[eval[e]]++] = x;
   594			free(fill);
   595			reflsrc = malloc(4 * ((size_t)N + 1));
   596			for (u32 g = 1; g <= nslA; g++) if ((flags[g] & (fDef | fReflect)) == (fDef | fReflect)) reflsrc[nrefl++] = g;
   597			for (size_t i = 0; i < pl.nf; i++) if ((pl.fval[i] & (fDef | fReflect)) == (fDef | fReflect)) reflsrc[nrefl++] = pl.fid[i];
   598			for (size_t i = 0; i < mi.nf; i++) if ((mi.fval[i] & (fDef | fReflect)) == (fDef | fReflect) && mi.fid[i] > nslA) reflsrc[nrefl++] = mi.fid[i];
   599		}
   600		rmask = (1u << 16) - 1;
   601		rem = malloc((rmask + 1) * sizeof(remEnt));
   602		for (u64 i = 0; i <= rmask; i++) rem[i].alive = 0xff;
   603		maybeRem = calloc(N + 1, 1);
   604		pool = calloc(1 + 4 * (mi.ne + pl.ne + 16) + (getenv("FUZZ") ? 256 * (size_t)atoi(getenv("FUZZ")) : 0), sizeof(addEnt));
   605		addHead = calloc(N + 1, 4), raddHead = calloc(N + 1, 4);
   606		live = calloc(N + 1, 1), iface = calloc(N + 1, 1), typeDone = calloc(N + 1, 1), isRoot = calloc(N + 1, 1);
   607		posL = calloc(N + 1, 4), posI = calloc(N + 1, 4);
   608		ordL = calloc(N + 1, 4), ordI = calloc(N + 1, 4), ordT = calloc(N + 1, 4), ordM = calloc(nmeth, 4);
   609		if (argc > 4 && strcmp(argv[4], "plain") == 0) useOrder = 0;
   610		called = calloc(nmeth, 1), namedCnt = calloc(nstrU + 1, 4), expanded = calloc(N + 1, 4);
   611		double tsetup = now() - ts;
   612	
   613		// ---- the previous link: flood A with the same rules (its state is what an update maintains)
   614		double t0 = now();
   615		epoch = 1;
   616		for (u32 g = 1; g <= nslA; g++) if (flags[g] & fIface) iface[g] = 1;
   617		for (size_t i = 0; i < nroots; i++) isRoot[roots[i]] = 1, setLive(roots[i]);
   618		drain();
   619		double tfull = now() - t0;
   620		printf("p2inc (%s): setup %.1f ms, flood A (same rules, worklist) %.1f ms, %llu edges read\n", useOrder ? "DRed + derivation-order supports" : "plain DRed", tsetup * 1e3, tfull * 1e3,
   621		       (unsigned long long)cEdges);
   622		long bad = checkLive("A", liveA, nlA);
   623		printf("diff: edges -%zu +%zu, flags -%zu +%zu, roots -%zu +%zu\n", mi.ne, pl.ne, mi.nf, pl.nf, mi.nr, pl.nr);
   624		double best[2] = {1e9, 1e9};
   625		for (int r = 0; r < reps; r++)
   626			for (int dir = 0; dir < 2; dir++) {
   627				cEdges = cRev = cKilled = cRestored = cSet = cChecks = 0;
   628				double t = now();
   629				if (dir == 0) update(&mi, &pl); else update(&pl, &mi);
   630				t = now() - t;
   631				if (t < best[dir]) best[dir] = t;
   632				printf("update %s\t%.3f ms\trep %d\tover-deleted %llu live, rederived %llu directly, set %llu, edges read %llu, "
   633				       "reverse entries %llu, support checks %llu\n",
   634				       dir ? "B->A" : "A->B", t * 1e3, r, (unsigned long long)cKilled, (unsigned long long)cRestored,
   635				       (unsigned long long)cSet, (unsigned long long)cEdges, (unsigned long long)cRev, (unsigned long long)cChecks);
   636				bad += dir ? checkLive("B->A vs A oracle", liveA, nlA) : checkLive("A->B vs B oracle (unified ids)", liveB, nlb);
   637			}
   638		printf("best update A->B %.3f ms, B->A %.3f ms\n", best[0] * 1e3, best[1] * 1e3);
   639		// FUZZ=K: K random diffs of random size (up to 200 edges removed from live sources, up to 200 added, all
   640		// kinds), each applied as an update on top of B and checked against a from-scratch flood of the same
   641		// graph by the same rules, then reverted and checked against B again
   642		int fz = getenv("FUZZ") ? atoi(getenv("FUZZ")) : 0;
   643		if (fz) {
   644			update(&mi, &pl);
   645			u64 rs = 88172645463325252ull;
   646	#define RND() (rs ^= rs << 13, rs ^= rs >> 7, rs ^= rs << 17)
   647			u8 *snap = malloc(N + 1);
   648			long fbad = 0;
   649			for (int f = 0; f < fz; f++) {
   650				size_t nr = RND() % 200, na = RND() % 200;
   651				u32 *rsrc_ = malloc(4 * (nr + na)), *asrc = rsrc_ + nr;
   652				u8 *rk = malloc(nr + na), *ak = rk + nr;
   653				u64 *rv = malloc(8 * (nr + na)), *av = rv + nr;
   654				size_t n1 = 0, n2 = 0;
   655				while (n1 < nr) { // an existing A edge of a live source, not already removed
   656					u32 sx = (u32)(RND() % nslA) + 1;
   657					if (!live[sx] || eoff[sx] == eoff[sx + 1]) continue;
   658					u32 e = eoff[sx] + (u32)(RND() % (eoff[sx + 1] - eoff[sx]));
   659					if (removed(sx, ekind[e], eval[e])) continue;
   660					int dup = 0;
   661					for (size_t j = 0; j < n1; j++) dup |= rsrc_[j] == sx && rk[j] == ekind[e] && rv[j] == eval[e];
   662					if (dup) continue;
   663					rsrc_[n1] = sx, rk[n1] = ekind[e], rv[n1++] = eval[e];
   664				}
   665				while (n2 < na) { // a new edge from a random defined source to a random id, or a random existing payload
   666					u32 sx = (u32)(RND() % nslA) + 1, sy = (u32)(RND() % nslA) + 1;
   667					if (!(flags[sx] & fDef) || eoff[sy] == eoff[sy + 1]) continue;
   668					u32 e = eoff[sy] + (u32)(RND() % (eoff[sy + 1] - eoff[sy]));
   669					u8 k = ekind[e];
   670					u64 v = (k <= 1 && RND() % 2) ? (u64)(RND() % nslA) + 1 : eval[e];
   671					if (inA(sx, k, v)) continue;
   672					int dup = 0;
   673					for (size_t j = 0; j < n2; j++) dup |= asrc[j] == sx && ak[j] == k && av[j] == v;
   674					for (u32 a = addHead[sx]; a; a = pool[a].next) dup |= pool[a].alive && pool[a].kind == k && pool[a].val == v;
   675					if (dup) continue;
   676					asrc[n2] = sx, ak[n2] = k, av[n2++] = v;
   677				}
   678				side m2 = {nr, 0, 0, rsrc_, NULL, NULL, rk, NULL, rv}, p2 = {na, 0, 0, asrc, NULL, NULL, ak, NULL, av};
   679				update(&m2, &p2);
   680				memcpy(snap, live, N + 1);
   681				u8 *sI = getenv("FUZZV") ? malloc(N + 1) : NULL, *sT = sI ? malloc(N + 1) : NULL, *sC = sI ? malloc(nmeth) : NULL;
   682				if (sI) memcpy(sI, iface, N + 1), memcpy(sT, typeDone, N + 1), memcpy(sC, called, nmeth);
   683				u32 *sS = sI ? malloc(4 * (smask + 1)) : NULL, *sN = sI ? malloc(4 * (nstrU + 1)) : NULL, sR = reflectCnt;
   684				if (sI) { for (u64 h = 0; h <= smask; h++) sS[h] = sigs[h].cnt; memcpy(sN, namedCnt, 4 * (nstrU + 1)); }
   685				// from scratch on the same (current) graph
   686				memset(live, 0, N + 1), memset(iface, 0, N + 1), memset(typeDone, 0, N + 1), memset(called, 0, nmeth);
   687				memset(namedCnt, 0, 4 * (nstrU + 1)), reflectCnt = 0;
   688				for (u64 h = 0; h <= smask; h++) sigs[h].cnt = 0;
   689				for (u32 g = 1; g <= N; g++) if (flags[g] & fIface) iface[g] = 1;
   690				epoch++;
   691				for (u32 g = 1; g <= N; g++) if (isRoot[g]) setLive(g);
   692				drain();
   693				long d = 0;
   694				for (u32 g = 0; g <= N; g++) d += snap[g] != live[g];
   695				if (d) printf("fuzz %d: -%zu +%zu edges: %ld ids differ from scratch\n", f, nr, na, d);
   696				if (d && getenv("FUZZV"))
   697					for (u32 g = 0, c = 0; g <= N && c < 12; g++)
   698						if (snap[g] != live[g]) {
   699							c++, printf("  id %u: update %d scratch %d flags %d iface %d type %d revm %u\n", g, snap[g], live[g], flags[g], iface[g], ISTYPE(g), g <= nslA ? revoff[g + 1] - revoff[g] : 0);
   700							FOR_PREDS(g, ps, pk, if (pk == 0 && snap[ps]) printf("    pred %u (update-live, scratch %d)\n", ps, live[ps]););
   701							if (g <= nslA) for (u32 i = revoff[g]; i < revoff[g + 1]; i++) { u32 m = revm[i]; printf("    meth %u owner %u: update called %d typeDone %d live %d iface %d; scratch called %d typeDone %d\n", m, mown[m], sC[m], sT[mown[m]], snap[mown[m]], sI[mown[m]], called[m], typeDone[mown[m]]);
   702									sigent *se = sig((u64)mname[m] << 32 | mtyp[m]); printf("      exp %d reflect %u/%u sig %u/%u named %u/%u\n", mexp[m], sR, reflectCnt, sS[se - sigs], se->cnt, sN[mname[m]], namedCnt[mname[m]]); }
   703						}
   704				fbad += d != 0;
   705				update(&p2, &m2); // revert: must be B again
   706				if (checkLive("fuzz revert vs B oracle", liveB, nlb)) fbad++;
   707				free(rsrc_), free(rk), free(rv);
   708			}
   709			printf("fuzz: %d random diffs, %ld failed\n", fz, fbad);
   710			bad += fbad;
   711		}
   712		printf(bad ? "P2INC FAIL\n" : "P2INC OK\n");
   713		return bad != 0;
   714	}
results/ddflow-inc-minio-edit.log
results/ddflow-inc-minio-edit2.log
results/ddflow-incremental.md
systems/ddflow/bench-20261116T1930Z.txt
systems/ddflow/IDEAS.md
systems/ddflow/Cargo.toml
systems/ddflow/Cargo.lock
systems/ddflow/src/p1.rs
systems/ddflow/src/facts.rs
systems/ddflow/src/writediff.rs
systems/ddflow/src/main.rs
systems/ddflow/CELLS.md
systems/ddflow/NOTES.md
systems/ddflow/edit2.patch
systems/ddflow/bench.sh
systems/ddflow/bench-20261116T1940Z.txt
systems/ddflow/edit.patch
ref/flat/bench-p2inc.sh

exec
/bin/bash -lc "nl -ba ATTACKS.md | tail -115; nl -ba GRID.md | tail -55; nl -ba systems/ddflow/src/main.rs | tail -210; cat ref/flat/bench-p2inc.sh; ls build/p2inc /home/exedev/paradigms/build/p2inc; rg -n 'time left|usage|session|budget|remaining|enough time' JOURNAL.md" in /home/exedev/paradigms-w
 succeeded in 0ms:
   154	mechanism (stealing, credits) for ~15 lines of rules. A build farm's machines are shared like this VM, so robustness
   155	under load is part of the speed. **Next:** a quiet-machine run (does async lose anything with nothing else running?),
   156	and whether a level-free flood also cuts Futhark's/Goose's cost (fewer, larger rounds: e.g. expand several levels per
   157	superstep, a "delta-stepping" of the flood).
   158	
   159	## 4. Code generation by shape: the emit costs indirection per piece, not pieces or bytes; flatten the stream, merge templates by width
   160	
   161	**Problem.** Code generation by shape (4): emit fc's 60.7 MB of MinIO function bodies from 108,833 byte templates,
   162	7.68 M operands and 17.6 MB of glue (ref/p4/README.md).
   163	
   164	**Before.** The obvious model, from copy-and-patch compilers (Xu and Kjolstad 2021; ideas/copy-and-patch.md): a
   165	template table plus a patch pass; speed is bandwidth, and the table should be keyed by typed shape (SHAPES.md: 4,096
   166	shapes cover 96.7% of nodes).
   167	
   168	**Observation.** (a) The plain C emit runs at **0.42-0.62 GB/s** (1 core, 98-145 ms medians across windows), 40-60x
   169	below this VM's read bandwidth, and patching holes is only **7.6%** of it (codex-p4par, P4_NOPATCH split). (b) It is
   170	not the number of pieces either (codex-cp7, results/codex-cp7.md, 5 interleaved runs, quiet machine, all P4 OK, lead
   171	rerun): precomputing each of the 2.38 M items as a flat blob with its holes rebased (control D, same piece count)
   172	emits in **48.3/48.5/48.9 ms vs p4's 88.9/98.4/98.6** (2x); merging pieces 2.3x (A: hole-free neighbours) or 17.4x
   173	(B: one run per function) then adds only 1.08-1.10x (A 44.0, B 44.7 ms). On 8 threads: p4par 16.1/18.4/22.2, D 13.7,
   174	A 11.9, B 12.3 ms median. **The cost is the indirection per piece** (item -> template -> segment -> hole range), not
   175	the copies. The paradigm cells' top-K curves give the same split from the other side (Halide, Goose: keep
   176	the commonest K templates, the rest as glue): copying all 60.7 MB as 136 K hole-free runs takes 12.8 ms (1 thread) /
   177	4.8 (8); each hole costs ~5.8 ns (8.0 M holes ~ 46 ms) and each template piece ~40 ns. So once the stream is flat, the
   178	**holes** are most of what is left: C's B variant (one run per function) at 44.7 ms is almost exactly its 8 M holes. (c) The typed shape does not fix the bytes (74.6% of nodes in the top 4,096 *byte* templates); one
   179	splitter is purely mechanical: disp8 exactly when the operand fits in int8, **0 exceptions in 4,864,669 holes**.
   180	
   181	**Attack.** Two independent steps. (1) Flatten the emit stream: resolve item -> template -> segment -> holes once
   182	(an extraction-time "decode", like ATTACKS 1's lazy decode in reverse: here the eager form wins because the emit is
   183	the hot loop), so the emit is a sequential read of (blob, holes) runs: 2x on 1 core, 1.3x on 8 over p4par. The price
   184	is input size (416 MB of columns vs 217 MB; dedupe of per-function runs, C, finds 55,389 distinct of 136,634, 49 MB).
   185	(2) Key templates with the displacement width relaxed: **108,833 -> 66,046 templates; the top 4,096 cover 82.3% of
   186	nodes instead of 74.6%**, oracle reproduced byte for byte (codex-cp, `ref/p4/check-cp.sh`, lead rerun), but keep each
   187	node's length precomputed (deriving it from operands made the scan 6.3x slower). Specialising hot templates into
   188	straight-line C (Deegen's method) cut instructions 26% and the covered segments' emit 1.67x (median), but ranges
   189	overlap; 2.9 MB of generated C.
   190	
   191	**What it says about the paradigms.** Futhark expresses the whole problem in 159 lines (lengths by map, places by
   192	scan, holes as a flat map, rel32 by gather, relocations by expand) at 3.2x C on 1 core; what it had to force is the
   193	copy itself (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x
   194	cheaper than scattering bytes, and the same trick made its 3b relocate 2.7-3.6x faster). The flattened stream (1) is
   195	exactly the shape a data-parallel language wants (no nested lookups), so it should help Futhark and Halide more than C:
   196	**next**, a Futhark or Halide cell reading D's representation.
   197	
   198	
   199	**Hole follow-up (codex-holes).** Before: B's 8.02 M records emit in 44.495 ms at one core.
   200	Observation: 5.63 M are integers, 0.70 M rel32, and 1.69 M relocations that write no code.
   201	Skipping integer stores while retaining their records takes 41.559 ms. Fix: pre-bake integers
   202	into each function blob and drop their records (H1): **28.757 ms, 1.55x**, nonoverlapping
   203	one-core ranges, but preparation rises from 343.769 to 401.802 ms median. This moves work into
   204	preparation; it is not an end-to-end compiler speedup. H2 merges 3.53 M integer holes into
   205	1.76 M masked stores: 28% fewer patch stores, **41.275 ms**, overlapping B's range (within
   206	noise). H1 removes 89% of patch stores but only 35% of emit time: no proportional scaling.
   207	Eight-core H1/B ranges overlap under load 3.43–13.35. Full bytes/offsets/relocations pass,
   208	three planted faults fail, independent recount/UBSan pass. User-mode perf and the checked
   209	no-store control point to metadata traversal/dispatch and memory traffic, not a 5.8 ns
   210	scalar-store cost. Counts, five interleaved rounds, preparation and counter scope:
   211	[results/codex-holes.md](results/codex-holes.md).
   212	
   213	## 5. Relink: after a small edit, maintain the answer instead of recomputing it; first make symbol identity stable
   214	
   215	**Problem.** Resolve (1) and reach (2) after a small source edit: every build today relinks from scratch.
   216	
   217	**Old shape.** A from-scratch join and flood over ~10 M facts on every link (C: p2 52 ms 1 core, p2par 17-20 ms on 8;
   218	p1 287 / p1par 22 ms).
   219	
   220	**Where the attack came from.** The Differential Dataflow column (added 2026-11-16, IDEAS wildcard 3), built to ask a
   221	question no other column can: how much of a relink's work does a small edit change? Two real edits of MinIO, rebuilt
   222	with the frozen toolchain and re-extracted ([results/ddflow-incremental.md](results/ddflow-incremental.md)).
   223	
   224	**Observation 1: the edit is tiny, the ids are not.** Edit 1 (a 5-line fallback added in one function) changes the
   225	facts of **31 symbols out of 9.7 M facts** (3.2e-6) and **3 live ids** of 663,630; edit 2 (16 lines deleted: the console
   226	UI start-up) changes 141 symbols and 234 edges but kills **12,320 live ids** (21.5 MB of image). But slot ids are
   227	positional (object base + index), so 3 added slots in object 109 renumber every slot in objects 110-1420: a raw diff
   228	is ~all facts. Recovering identity (pair objects, match by unique name/content key, Myers-align the unnamed DWARF
   229	remainder) takes **0.7-1.1 s**, outside the update times below. A "lowest id" tie-break churns 34 K resolve facts in
   230	edit 2 for 73 real changes.
   231	
   232	**Observation 2: an additive edit updates 35-400x faster than recompute; a retracting one does not.** DD, lead rebuilt
   233	and reran 19:55Z (edit 1, 1 worker: update 1.33-1.40 ms, P2 OK on both versions and from scratch). Bench 19:40Z, load
   234	4.4-5.6, medians: reach update edit 1 **1.4 ms (1 worker) vs C p2 52 / p2par 31 (1 thread), 9.0 (8 workers) vs p2par
   235	20.5**; resolve update edit 1 **0.7 ms vs p1 287 / p1par 22**. Edit 2: reach update **65 ms (1) / 555 ms (8): slower than
   236	C recompute**; retracting 12 K reachable ids withdraws each derivation round by round through 45 loop rounds. DD's own
   237	full run is 60-100x C (2.7 s / 1.6 s) and holds 2.7 GB of arrangements.
   238	
   239	**Observation 3: in plain C, maintenance wins both edits, retractions included, if deletions use derivation
   240	stamps** ([results/c-incremental.md](results/c-incremental.md), `ref/flat/p2inc.c`, 1 core, reading the stored diff
   241	`data/minio-edit{,2}/diff/`). Bench 20:12Z, load 0.7-1.7, 5 interleaved rounds, medians: edit 1 update **3 µs** (DD
   242	1.4 ms, p2 45 ms, p2par 17 ms on 8); edit 2 update **9.5 ms A->B / 5.2 ms B->A** (p2 43, p2par 14 on 8, DD 59 on 1
   243	worker / 29 on 8). Deletion is DRed (delete, rederive) where a fact survives the delete step if it has a derivation
   244	from a true fact with a smaller stamp (the clock when it was set): edit 2 retracts 32 K ids for 12 K real deaths;
   245	textbook DRed retracts 594 K and takes 123 ms (3x recompute). Checked on both edits and directions and by 300
   246	random diffs per edit against a from-scratch flood (the fuzzer found two bugs the edits did not: a method kept by a
   247	sig count whose remaining sources were its own targets, and a lost re-check across removals in one update). The
   248	same quiet window corrects DD's 8-worker edit-2 update: 29 ms, not 555 (that was load).
   249	
   250	**Attack.** (1) Give symbols a stable identity at extraction: (package path, name) or content hash, per-object
   251	order only as a last resort, and tie-breaks by name hash, not position; then the input diff of an edit is the edit's
   252	size (31 symbols), and nothing pays 0.7-1.1 s to find it. (2) Keep reach and resolve as maintained collections
   253	(semi-naive deltas, a grouped reduce per name key): additions propagate in µs (C) to ~1 ms (DD). (3) For deletions,
   254	DRed with derivation stamps (Observation 3): exact on cycles, where per-symbol support counts are not, and its cone
   255	stays near the real deaths. The hybrid "maintain on additions, recompute on removals" is unnecessary, and would
   256	recompute on every real edit (each removes the edited function's old GC-locals edges).
   257	
   258	**What it would take.** A stable-id extraction (hlink -dumpflat keyed by name, or a mapping table written once; today
   259	`ddflow writediff` writes the mapping and diff once as flat files). The maintained flood is done (`p2inc.c`, ~700
   260	lines with checks and fuzzer); its edit-2 cost is now 3 M reverse-list re-checks of popular targets (a per-id count
   261	of smaller-stamp live predecessors would make each O(1)). Not measured yet: resolve maintained in C, 1c, layout and
   262	relocate after an edit (layout shifts every address after the edited function, so 3a/3b are mostly recompute unless
   263	sections are padded per package).
   264	
   265	**What it says about the paradigms.** None of the five original columns can state "the answer after this change";
   266	Datalog engines have the same semi-naive deltas inside one evaluation but discard them. DD expresses the whole
   267	reach as 71 lines and the update as two `feed` calls, but it is not a from-scratch engine (60-100x C), its updates get
   268	slower with 8 workers on a loaded machine (on a quiet one, edit 2: 29 ms on 8 vs 59 on 1), and identity is something it assumes, not something it provides.
    16	- **plain C on the flat files** (`ref/flat/p1.c p2.c p3.c`, single-threaded, untuned, one hash table or one loop
    17	  per rule): the like-for-like baseline every system is compared with first. `ref/flat/check.sh` runs them and
    18	  checks every oracle.
    19	
    20	| problem | floor (8 cores, FLOOR.md model) | hlink 1 core / 8 cores | plain C, flat, 1 core warm | tuned C, flat (p1par/p2par), 1 / 8 threads | check |
    21	|---|---|---|---|---|---|
    22	| 1a+1b. resolve: join by name, relocation targets | 1.36 (resolve) | 125 / 24 (resolve phase, 15:56Z) | 194-203 (join) + 43-44 (9.4 M targets) | 74-77 / **13.9-16.8** (join 8.6-10.5 + targets 5.3-6.3); p1compact join 28.1-30.7 / 6.5-9.9 vs p1par 41.7-48.7 / 9.5-12.9 same window (16:34Z, load 3-5) | `build/p1 data/minio`, `build/p1par` -> P1 OK |
    23	| 1c. content-hash join | (in resolve) | 37-39 / 8-9 (inside deadcode) | 57-61 (radix-partitioned, p1c.h; was 300-309 with a 400 MB table) | 36.9 / **7.2-8.9** | same |
    24	| 2. reach | 2.78 | 98-103 / 22-23 floods + 7 / 1.5 methods | 39-43 | 27-30 / **13.0-17.0** | `build/p2 data/minio`, `build/p2par` -> P2 OK |
    25	| 2'. reach incl. decoding relocations | | (= the row above: hlink decodes lazily) | dumpflat's untuned eager loop 133-139 + flood 39-43 | eager decode + CSR + flood (p2pipe): 108.7 / 30.1; **lazy decode in the flood (p2lazy): 52.6 / 13.6-15.4** | `build/p2pipe`, `build/p2lazy` -> P2 OK |
    26	| 3a. lay out | 0.60 | 85 / 23-25 (layout phase, includes pclntab sizing and the typelink sort) | 8-19 | p3a_par **4.90–5.41 / 3.66–7.56** (medians 5.00 / 3.89; load 4.1–4.4); kube 7.62–8.09 / 5.41–7.62; [five-run report](results/codex-i.md) | `build-i/p3a_par data/minio` -> addr 0 differ; both datasets, 1/8 threads |
    27	| 3b. relocate | 4.92 (write) | 102-104 / 15 (write phase, includes pclntab and GC bitmaps) | 59-67 (76 MB, 2.05 M relocations); P3_REPS=20 16:13Z: 48.6-56 on 4 KB pages, **23.9-30** on 2 MB pages | p3par 8 threads: 13.4-29 on 4 KB, **5.4-12 (median 6.1-6.8) on 2 MB** (ATTACKS 2) | same; `build/p3 data/minio --pipeline` runs 3a -> 3b from its own addresses -> P3 pipeline OK |
    28	| 4. codegen by shape: emit 136,634 bodies (60.7 MB) from 108,833 byte templates + 7.68 M operands | | fc's tree walker (no separate phase time) | 13-17 (lay out 2.38 M pieces) + 139-147 (emit), load 3.5; lead rerun 16:05Z: 18.4 + 162.4 | p4par (codex 17:27Z, load ~20, 5 interleaved, min/med/max): 1 thread layout 7.8/7.9/10.4 + emit 107/118/150; **8 threads 2.9/4.3/7.8 + 51/66/68** (vs p4 116/145/174 same window). Patching is 7.6% of 1-thread emit: the copy of 2.38 M ~25-byte pieces at ~0.6 GB/s is the cost (per-item, not bandwidth). [results/codex-p4par.md](results/codex-p4par.md). Quiet rerun (codex-cp7, 18:30Z): p4 88.9/98.4/98.6, p4par 8 threads **16.1/18.4/22.2**; flattened stream (p4runs D) 48.5 (1) / 13.7 (8), one run per function (B) 44.7 / 12.3 ([results/codex-cp7.md](results/codex-cp7.md), ATTACKS 4) | `ref/p4/check.sh` -> P4 OK (`ref/p4/README.md`) |
    29	
    30	**Floors vs this VM.** FLOOR.md's floors imply ~87 GB/s (minio resolve: 119 MB in 1.36 ms); this VM streams **25 GB/s read on
    31	1 thread, 36 GB/s read / 27 GB/s write on 8** (16:16Z, load 5, a sum and a fill over 1 GB). Floors here are ~2.4x lower than the
    32	column says. Per-kernel bandwidth bound for flat 3b: 76 MB symbol bytes + ~33 MB relocations read, 76 MB written -> ~3-6 ms on
    33	8 threads, which p3par on 2 MB pages (5.4-6.8 median) meets.
    34	
    35	Tuned and plain flat rows re-measured 2026-11-16T15:56Z at load 0.8, 3 interleaved rounds (`ref/flat/bench.sh`); hlink's resolve/deadcode from `COLD=0 ref/hlink/bench.sh minio build/hlink 3` right after.
    36	
    37	**kube-apiserver** (second dataset, extracted 2026-11-16T15:55Z, 2.2 GB of flat files, 13.27 M slots): every plain and
    38	tuned C reference reproduces its oracles (`ref/flat/check.sh data/kube-apiserver`, loaded machine): p1 434 + 88 ms,
    39	p1c 550 ms, p2 66.5 ms (926,486 live), p3a 30.4 ms, p3b 67.8 ms (72.6 MB, 2.82 M relocations); p1par 26.2 ms and
    40	p2par 19.6 ms on 8 threads. Its oracles come from hlink alone so far (MinIO's live list was also checked against fclink).
    41	
    42	**Paradigm cells on kube-apiserver** (codex-kube, 17:21-18:16Z, merged 18:30Z; [results/codex-kube.md](results/codex-kube.md)): every P1-P3 cell of Goose, Futhark, Halide, Soufflé and DuckDB, unchanged, reproduces kube's oracles at 1 and 8 threads (lead spot-checked Halide P1 and Goose P2 at 18:28Z). Load was 17-26 for most of the window, so 8-thread kube/MinIO ratios (2-10x) mostly measure the load; the 1-thread ratios, 1.5-2.5x, are at or below the data's growth (kube has **2.55x** MinIO's slots, 2.65x its named symbols, 2.1x the relocations, 1.4x the live set and 1.2x the placed symbols; an earlier version of this line said "1.4x the ids": wrong). Halide 1a's 6.7x was the load: rerun interleaved at load 2.6 (18:33Z, 1 thread pinned, 3 rounds): MinIO 119-126 ms, kube 320-350, **2.6x, linear in slots** (M3 review finding 4).
    43	
    44	Floors are bottom-up's FLOOR.md model (same CPU model, the 2026-11-13 VM); they count the raw object bytes hlink
    45	reads, so for the flat files they are an upper bound on the floor.
    46	
    47	## The paradigms
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
    58	| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood over given edges 97 / 58; lazy (own decode) 165 / 90 vs eager with its own decode >= 97 + 47 decode + an unwritten CSR build: **a tie at best, not a win** for ATTACKS 1 (corrected after review: the earlier 'slower than eager' compared unlike work). Cost: 45 levels (methods checked every level; not 111) x ~26 passes each; a sequential worklist in Futhark is faster: 67/102/130 vs 156/201/258 ms on 8 (results/levelfree.md). | **P2 OK** (eager + lazy). 8 thr: lazy 79-90, eager 70-127 (tuned C 25.7-29.1 / 12.7-14.6). Lazy is 1.3-1.5x *slower* than eager in Halide. Forced: no fixpoint (one pipeline call per level, ~100 levels), no worklist; `atomic()` dropped (mutex per element, wrong live sets). | **P2 OK** (eager + lazy). Flood over given edges 1227-1467 / 431. With its own decode: **lazy 2.9-3.2 s vs eager ~6.5 s on 1 thread, 960 ms vs ~1.6 s on 8** (decodes 1.73 M of 9.4 M relocations, 3 rules): ATTACKS 1 holds inside Soufflé; ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: slow as implemented (45 rounds x ~25 statements, hash tables rebuilt each round); Third attempt (M3, one recursive query per method round, `p2rec.sql`; lead rerun 18:22Z P2 OK): 8 threads setup 577-871 + flood 1438-2234 ms, 1.2x the old cell overall. **Falls down after three attempts**: each recursive step rescans all 2.7 M edges; DuckDB has no fast lookup by id (an index pays below ~50 rows, frontiers are 16-24 K). Lazy ties eager. |
    59	| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down only for one gathered Goose array (+142-171). **One file works:** 8 workers 2 MB **72.3/77.3/78.8 ms** including buffered write-out, vs Goose1 101.2 and C1 107.8 medians ([five rounds](results/codex-review5.md#b-goose-p3b-a-shared-file-instead-of-a-gathered-array)). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). **3b_blk** (M3: the copy as a sequential slice-update loop inside a map over 64 blocks = memcpy; lead rebuilt + reran P3b_blk OK 18:36Z): 60.4/62.0 (1) / **23.5/30.5 (8)** vs old 3b 159.7/166.7 / 96.4/110.9 same window, load 5: 2.7x / 3.6x, ranges apart. Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK** (poisoned addresses; skipped-unit check fails). 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a: second attempt `layout_rank.dl` (M3; lead rebuilt from source, P3 OK 18:21Z): the order by a ranked successor chain, no pair counting, no C functor: 1 thread 1930 (median, was 8270), 8 threads 1725/1937/3541 (was 2860/3388/7055 same runs), load 6-22. **No longer falls down**, but ~100x plain C: the 1,763-step unit walk is the limit. 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
    60	| 4. codegen by shape (stretch) | **P4 OK** (lead rebuilt + reran 1/8 workers 18:47Z). Workers own ranges of whole functions, no messages while computing: layout+emit 97.1/98.3 (1) / **17.6/21.3 (8)** vs p4.c 105.6 / p4par 17.1, within noise; same 698 M instructions. **Not a tie end to end**: that time leaves out the join of the parts, which is Goose's cost (no shared memory): +120-260 ms through queues, +37-63 ms writing each range into the file (M4 review 3). 482 lines + 40 C | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). **4D** (flattened stream, ATTACKS 4; lead reran P4D OK both backends 18:49Z): emit 65.1/67.8 (1) / **23.4/26.9 (8)** vs 4bf 284/303 / 95/113 same window, 4.5x / 4.2x; the flattening (`prepare`, 50 lines) 324 / 138 ms, so it pays only if the stream is reused | **P4 OK** (lead rebuilt + reran 1/8 18:46Z). Emit **157-162 (1, CPU time; one thread, so ~wall) / 23.0-25.1 (8, wall)** vs p4.c 103-112 / p4par 16.4-20.1 (load 4): 1.5x / 1.3x; layout 18 / 5 (+ template index 9.5 / 7). Relocations by a scatter to the layout's own slot ranges (no C host). Forced: entry-0 indices on untaken select sides (Halide loads both: 200 -> 160 ms); the vector copy only as 32-byte read-modify-write in parity passes (obvious form 26 s). 345 + 288 lines | **P4 OK** (lead rebuilt emit2 + reran 19:29Z: bytes, 136,635 offsets, 1,780,281 relocations; also P4 OK on kube-apiserver). Attempt 2 `emit2.dl` warm 7246/8316/9046 (1) / **2722/3814/4741 (8)** vs p4.c 107/111 / p4par 21.6/27.0 same rounds, load 7-10: ~70x / ~130x; lead rerun 8 threads compute 2571 ms (load 2.7-4.5). 74 lines + a 42-line C writer (memcpy runs, store low bytes, at places the rules computed). Easy: lengths, holes to segments, rel32 as a join with `at`, relocations, offsets. Forced: the prefix sum as a radix-16 tree of block sums (11%); **set semantics silently dropped 37,703 exactly-repeated marker relocations** (each now carries its id). Cost: 8.65 M output tuples, a B-tree insert each | not attempted |
    61	
    62	Added column (2026-11-16T19:55Z): **Differential Dataflow** (incremental relink; rust 1.100, differential-dataflow 0.26.0; lead rebuilt + reran `ddflow inc minio minio-edit`: P2 OK). **P2 reach OK** (MinIO + kube): full 3343 (1) / 1814 (8) median, 60x / 100x C. **P1a+1b OK**: full 9310 / 2023. **Incremental, real edits** (results/ddflow-incremental.md, ATTACKS 5): edit 1 (31 symbols of 9.7 M facts) reach update **1.4 ms (1 wkr) vs C p2 52 / p2par 20.5 (8)**, resolve update **0.7 ms vs p1 287 / p1par 22**; edit 2 (12,320 live ids die) reach update 59-65 (1) / 29 (8, quiet; the earlier 555 was load): slower than C recompute. **C maintained flood** (ref/flat/p2inc.c, DRed with derivation-order stamps; lead reran 20:16Z both edits + 50 fuzz diffs, P2INC OK): **edit 1 0.002-0.003 ms, edit 2 5.5-9.5 ms** vs p2 43-45, p2par 14-17 (8) (results/c-incremental.md). Id unification (positional ids shift) 0.7-1.1 s, outside the update. 71 + 23 lines. `systems/ddflow/CELLS.md`.
    63	
    64	**P4 on kube-apiserver** (19:17Z, results/codex-kube-p4.md): every P4 cell (C p4/p4par, Futhark 4bf/4D, Halide, Goose) is P4 OK on the second dataset unchanged (1.23x the bytes, 0.86x the templates). Emit medians kube/MinIO: C 1.12-1.15x, Futhark 4D 1.16-1.18x, 4bf 1.02-1.08x, Halide 1.44x (1) / 0.89x (8), Goose ~1.3x (8); load 4-24, ranges overlap for Futhark 4bf and Halide. No cell changes rank.
    65	
    66	Halide column merged 17:17Z (lead rebuilt every generator and driver in a copy; P1/P2/P3 OK; `systems/halide/CELLS.md`).
    67	DuckDB column merged 17:13Z (lead reran every check in a copy, pipeline included; `systems/duckdb/CELLS.md`).
    68	Soufflé column merged 16:58Z (lead rebuilt all 8 programs from source; every check OK; `systems/souffle/CELLS.md`).
    69	Futhark column merged 16:42Z (lead rebuilt from source; every cell OK; `systems/futhark/CELLS.md`). Surprise: `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, no code change, makes most Futhark cells 1.2-1.7x faster.
    70	Goose column merged 16:35Z; lead reran every check (P1/P2/P3/pipeline OK, a source rebuild of p2par passes). Long form and lines: `systems/goose/CELLS.md`, `NOTES.md`.
   181	        self.edges.flush();
   182	        self.flags.flush();
   183	        self.methods.flush();
   184	    }
   185	}
   186	
   187	type Snapshots = Arc<Mutex<HashMap<String, Vec<Id>>>>;
   188	
   189	fn check(label: &str, got: &mut Vec<Id>, want: &[Id]) -> bool {
   190	    got.sort_unstable();
   191	    let (extra, missing) = facts::diff(got, want);
   192	    let ok = extra.is_empty() && missing.is_empty();
   193	    println!("check {label}: {} live, oracle {}, extra {}, missing {} -> {}", got.len(), want.len(), extra.len(), missing.len(), if ok { "OK" } else { "FAIL" });
   194	    ok
   195	}
   196	
   197	fn ms(t: Instant) -> f64 {
   198	    t.elapsed().as_secs_f64() * 1e3
   199	}
   200	
   201	fn main() {
   202	    let t_start = Instant::now();
   203	    let args: Vec<String> = std::env::args().collect();
   204	    let mode = args[1].clone();
   205	    let pos: Vec<String> = args[2..].iter().take_while(|a| !a.starts_with('-')).cloned().collect();
   206	    let rest: Vec<String> = args[2 + pos.len()..].to_vec();
   207	    if mode == "writediff" {
   208	        return writediff::main(&pos);
   209	    }
   210	    if mode == "p1inc" {
   211	        return p1::main(&pos, rest);
   212	    }
   213	    let (fa, fb, reps, oracle_b, ub) = match mode.as_str() {
   214	        "p2" => {
   215	            let f = facts::load(&pos[0], None, None);
   216	            (f, None, 0, None, None)
   217	        }
   218	        "inc" => {
   219	            let t = Instant::now();
   220	            let u = facts::unify(&pos[0], &pos[1]);
   221	            let t_unify = ms(t);
   222	            let t = Instant::now();
   223	            let mut a = facts::load(&pos[0], None, None);
   224	            let mut b = facts::load(&pos[1], Some(&u.idm_b), Some(&u.strm_b));
   225	            a.sort();
   226	            b.sort();
   227	            let t_load = ms(t);
   228	            let ob: Vec<Id> = {
   229	                let mut v: Vec<Id> = facts::rd_u32(&format!("{}/oracle/p2_live.u32", pos[1])).iter().map(|&x| u.idm_b[x as usize]).collect();
   230	                v.sort_unstable();
   231	                v
   232	            };
   233	            println!(
   234	                "unify {t_unify:.0} ms: objects that differ {:?}; slots only in A {}, only in B {}; load+sort {t_load:.0} ms",
   235	                u.objs_differ,
   236	                u.a_only.len(),
   237	                u.b_only.len()
   238	            );
   239	            for &g in &u.a_only {
   240	                println!("  only in A: {}", u.name(g));
   241	            }
   242	            for &g in &u.b_only {
   243	                println!("  only in B: {}", u.name(u.idm_b[g as usize]));
   244	            }
   245	            (a, Some(b), pos.get(2).map_or(5, |r| r.parse().unwrap()), Some(ob), Some(u))
   246	        }
   247	        _ => panic!("mode p2 | inc"),
   248	    };
   249	    let oracle_a = facts::rd_u32(&format!("{}/oracle/p2_live.u32", pos[0]));
   250	    let diff = fb.as_ref().map(|b| {
   251	        let t = Instant::now();
   252	        let d = facts::fact_diff(&fa, b);
   253	        println!(
   254	            "diff {:.0} ms: edges -{} +{}, flags -{} +{}, methods -{} +{}, roots -{} +{} (of {} A facts)",
   255	            ms(t),
   256	            d.minus.edges.len(),
   257	            d.plus.edges.len(),
   258	            d.minus.flags.len(),
   259	            d.plus.flags.len(),
   260	            d.minus.methods.len(),
   261	            d.plus.methods.len(),
   262	            d.minus.roots.len(),
   263	            d.plus.roots.len(),
   264	            fa.len()
   265	        );
   266	        // symbols whose facts changed (as a source of an edge, a flag, a type of a method, a root)
   267	        let mut syms: Vec<Id> = Vec::new();
   268	        for f in [&d.minus, &d.plus] {
   269	            syms.extend(f.edges.iter().map(|e| e.0));
   270	            syms.extend(f.flags.iter().map(|e| e.0));
   271	            syms.extend(f.methods.iter().map(|e| e.0));
   272	            syms.extend(f.roots.iter().copied());
   273	        }
   274	        syms.sort_unstable();
   275	        syms.dedup();
   276	        println!("symbols with changed facts: {}", syms.len());
   277	        let u = ub.as_ref().unwrap();
   278	        for f in [("-", &d.minus), ("+", &d.plus)] {
   279	            for e in &f.1.edges {
   280	                let t = if e.1 .0 < 2 { u.name(e.1 .1 as u32) } else { format!("{:#x}", e.1 .1) };
   281	                println!("  edge {} {} -> kind {} {}", f.0, u.name(e.0), e.1 .0, t);
   282	            }
   283	            for e in &f.1.flags {
   284	                println!("  flag {} {} = {}", f.0, u.name(e.0), e.1);
   285	            }
   286	        }
   287	        d
   288	    });
   289	    let (fa, diff, fb2) = (Arc::new(fa), Arc::new(diff), Arc::new(fb));
   290	    let snaps: Snapshots = Arc::new(Mutex::new(HashMap::new()));
   291	    let snaps2 = snaps.clone();
   292	    let inc = mode == "inc";
   293	    let phased = pos.get(1).map_or(false, |p| p == "phased");
   294	    println!("setup (read, decode{}) {:.0} ms", if inc { ", unify, diff" } else { "" }, ms(t_start));
   295	
   296	    timely::execute_from_args(rest.into_iter(), move |worker| {
   297	        let (idx, peers) = (worker.index(), worker.peers());
   298	        let mut df = Dataflow::new(worker);
   299	        let snap = |df: &Dataflow, label: &str| {
   300	            snaps2.lock().unwrap().entry(label.to_string()).or_default().extend(df.live());
   301	        };
   302	        let say = |s: String| {
   303	            if idx == 0 {
   304	                println!("{s}")
   305	            }
   306	        };
   307	        if !inc {
   308	            // full: every fact and the roots at once (warm: the facts are in memory), or by phase: index the
   309	            // facts first (no roots: nothing is live), then the flood alone (the roots into the indexed facts)
   310	            if !phased {
   311	                let tm = Instant::now();
   312	                df.inp.feed(&fa, 1, true, idx, peers);
   313	                df.run(worker);
   314	                say(format!("full\t{:.1} ms\t{} rounds", ms(tm), ROUNDS.with(|r| r.get())));
   315	            } else {
   316	                let tm = Instant::now();
   317	                df.inp.feed(&fa, 1, false, idx, peers);
   318	                df.run(worker);
   319	                say(format!("index (facts, no roots)\t{:.1} ms", ms(tm)));
   320	                let tm = Instant::now();
   321	                for x in fa.roots.iter().skip(idx).step_by(peers) {
   322	                    df.inp.roots.update(*x, 1);
   323	                }
   324	                df.run(worker);
   325	                say(format!("flood (roots into indexed facts)\t{:.1} ms", ms(tm)));
   326	            }
   327	            snap(&df, "A");
   328	        } else {
   329	            let d = diff.as_ref().as_ref().unwrap();
   330	            let tm = Instant::now();
   331	            df.inp.feed(&fa, 1, true, idx, peers);
   332	            df.run(worker);
   333	            say(format!("full A\t{:.1} ms", ms(tm)));
   334	            snap(&df, "A");
   335	            for rep in 0..reps {
   336	                let tm = Instant::now();
   337	                df.inp.feed(&d.minus, -1, true, idx, peers);
   338	                df.inp.feed(&d.plus, 1, true, idx, peers);
   339	                df.run(worker);
   340	                say(format!("update A->B\t{:.2} ms\trep {rep}", ms(tm)));
   341	                if rep == 0 {
   342	                    snap(&df, "B");
   343	                }
   344	                let tm = Instant::now();
   345	                df.inp.feed(&d.plus, -1, true, idx, peers);
   346	                df.inp.feed(&d.minus, 1, true, idx, peers);
   347	                df.run(worker);
   348	                say(format!("update B->A\t{:.2} ms\trep {rep}", ms(tm)));
   349	                if rep == 0 {
   350	                    snap(&df, "A again");
   351	                }
   352	            }
   353	            // from scratch on B (unified ids), in a second dataflow of its own
   354	            let mut df2 = Dataflow::new(worker);
   355	            let tm = Instant::now();
   356	            df2.inp.feed(fb2.as_ref().as_ref().unwrap(), 1, true, idx, peers);
   357	            df2.run(worker);
   358	            say(format!("full B (from scratch)\t{:.1} ms", ms(tm)));
   359	            snap(&df2, "B scratch");
   360	        }
   361	    })
   362	    .unwrap();
   363	    let mut s = snaps.lock().unwrap();
   364	    let mut ok = check("A", s.get_mut("A").unwrap(), &oracle_a);
   365	    if let Some(ob) = oracle_b {
   366	        ok &= check("A->B vs B oracle (unified ids)", s.get_mut("B").unwrap(), &ob);
   367	        ok &= check("B->A vs A oracle", s.get_mut("A again").unwrap(), &oracle_a);
   368	        let (mut a, mut b) = (s["A"].clone(), s["B"].clone());
   369	        a.sort_unstable();
   370	        b.sort_unstable();
   371	        let (m, p) = facts::diff(&a, &b);
   372	        println!("live ids changed: -{} +{}", m.len(), p.len());
   373	        let u = ub.as_ref().unwrap();
   374	        for x in &m {
   375	            println!("  live - {}", u.name(*x));
   376	        }
   377	        for x in &p {
   378	            println!("  live + {}", u.name(*x));
   379	        }
   380	        let mut bs = s["B scratch"].clone();
   381	        bs.sort_unstable();
   382	        let same = bs == b;
   383	        println!("check A->B update vs B from scratch (same process, unified ids): {}", if same { "OK" } else { "FAIL" });
   384	        ok &= same;
   385	        ok &= check("B from scratch vs B oracle", &mut bs, &ob);
   386	    }
   387	    println!("{}", if ok { "P2 OK" } else { "P2 FAIL" });
   388	    println!("e2e (process start to here) {:.0} ms", ms(t_start));
   389	    std::process::exit(if ok { 0 } else { 1 });
   390	}
#!/usr/bin/env bash
# Interleaved timings for results/c-incremental.md: the maintained flood (p2inc, DRed with derivation-order supports
# and plain DRed) vs Differential Dataflow's update vs recomputing (p2, p2par), on both real MinIO edits.
#   ref/flat/bench-p2inc.sh [ROUNDS=5]
# Needs data/minio-edit{,2}/diff (systems/ddflow: `ddflow writediff data/minio data/minio-edit`).
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd)
D=/home/exedev/paradigms/data
N=${1:-5}
B=$here/build
mkdir -p "$B"
gcc -O2 -march=native -o "$B/p2inc" "$here/ref/flat/p2inc.c"
gcc -O2 -march=native -o "$B/p2" "$here/ref/flat/p2.c"
gcc -O2 -march=native -fopenmp -o "$B/p2par" "$here/ref/flat/p2par.c"
export RUSTUP_HOME=/home/exedev/tools/rust/rustup CARGO_HOME=/home/exedev/tools/rust/cargo PATH=/home/exedev/tools/rust/cargo/bin:$PATH
export CARGO_TARGET_DIR=/home/exedev/tools/rust/target-ddflow
(cd "$here/systems/ddflow" && nice -n 10 cargo build --release -q)
DD=$CARGO_TARGET_DIR/release/ddflow
export OMP_WAIT_POLICY=passive
cap="prlimit --as=12884901888"
out=$(mktemp); tmp=$(mktemp)
rec() { echo "$1	$2	$3" >> "$out"; }
echo "start $(date -u +%FT%TZ) $(uptime)"
for r in $(seq "$N"); do
  for e in minio-edit minio-edit2; do
    for mode in order plain; do
      $cap "$B/p2inc" $D/minio $D/$e 5 $mode > "$tmp"; grep -q 'P2INC OK' "$tmp"
      for dir in 'A->B' 'B->A'; do
        awk -F'\t' -v d="update $dir" '$1 == d && $3 == "rep 0" {print $2+0}' "$tmp" | while read -r v; do rec "p2inc $mode $e $dir first update" 1 "$v"; done
        awk -F'\t' -v d="update $dir" '$1 == d && $3 != "rep 0" {print $2+0}' "$tmp" | while read -r v; do rec "p2inc $mode $e $dir later updates (4 per process)" 1 "$v"; done
      done
      [ "$r" = 1 ] && grep '^update' "$tmp" | sed -n '1,2p' | cut -f1,4 | sed "s/^/  counts $mode $e: /" >&2
    done
    $cap "$B/p2" $D/$e > "$tmp"; grep -q 'P2 OK' "$tmp"
    rec "C p2 $e recompute (flood)" 1 "$(sed -n 's/.*flood \([0-9.]*\) ms.*/\1/p' "$tmp")"
    for c in 1 8; do
      OMP_NUM_THREADS=$c $cap "$B/p2par" $D/$e 5 > "$tmp"; grep -q 'P2 OK' "$tmp"
      rec "C p2par $e recompute (best of 5)" $c "$(sed -n 's/.*flood best \([0-9.]*\) ms.*/\1/p' "$tmp")"
      $cap $DD inc $D/minio $D/$e 5 -w $c > "$tmp"; grep -q 'P2 OK' "$tmp"
      awk -F'\t' '/^update A->B/ {print $2+0}' "$tmp" | while read -r v; do rec "DD $e A->B update" $c "$v"; done
      awk -F'\t' '/^update B->A/ {print $2+0}' "$tmp" | while read -r v; do rec "DD $e B->A update" $c "$v"; done
    done
  done
done
echo "end $(date -u +%FT%TZ) $(uptime)"
python3 -c '
import sys, statistics as st, collections
d = collections.OrderedDict()
for l in open(sys.argv[1]):
    c, w, v = l.rstrip("\n").split("\t"); d.setdefault((c, w), []).append(float(v))
for (c, w), v in sorted(d.items()):
    print(f"{c}\t{w} thr\tmin {min(v):.3f}\tmed {st.median(v):.3f}\tmax {max(v):.3f}\tms (n={len(v)})")
' "$out"
rm -f "$out" "$tmp"
ls: cannot access 'build/p2inc': No such file or directory
/home/exedev/paradigms/build/p2inc
100:but slower: the three remaining cells are all problem 4 (Goose, Halide, Soufflé); Halide is building, Goose goes next.

codex
The C check does compare live-set membership after every forward and reverse update. DD checks both directions only on the first repetition. The 3 µs timer excludes setup, diff construction and the oracle scan; I’m checking the ID boundary and edit-2 comparison next.
exec
/bin/bash -lc "cat /home/exedev/paradigms/data/minio-edit2/diff/meta.txt; nl -ba systems/ddflow/src/writediff.rs | tail -70; nl -ba systems/ddflow/CELLS.md; nl -ba ref/flat/p2.c | tail -65; nl -ba JOURNAL.md | head -55; nl -ba systems/ddflow/src/main.rs | sed -n '90,155p'; nl -ba results/ddflow-incremental.md | tail -55" in /home/exedev/paradigms-w
 succeeded in 0ms:
nids_a 5205435
nids_b 5205398
n_unified 5205479
nstr_a 18937
nstr_unified 18937
edge_minus 135
flag_minus 73
root_minus 0
meth_minus 0
edge_plus 99
flag_plus 44
root_plus 0
meth_plus 0
live_b 651320
     9	    std::fs::write(format!("{dir}/{name}"), b).unwrap();
    10	}
    11	
    12	fn put(dir: &str, side: &str, f: &Facts) {
    13	    w(dir, &format!("edge_{side}_src.u32"), &f.edges.iter().map(|e| e.0).collect::<Vec<u32>>());
    14	    w(dir, &format!("edge_{side}_kind.u8"), &f.edges.iter().map(|e| e.1 .0).collect::<Vec<u8>>());
    15	    w(dir, &format!("edge_{side}_val.u64"), &f.edges.iter().map(|e| e.1 .1).collect::<Vec<u64>>());
    16	    w(dir, &format!("flag_{side}_id.u32"), &f.flags.iter().map(|e| e.0).collect::<Vec<u32>>());
    17	    w(dir, &format!("flag_{side}_val.u8"), &f.flags.iter().map(|e| e.1).collect::<Vec<u8>>());
    18	    w(dir, &format!("root_{side}.u32"), &f.roots);
    19	    w(dir, &format!("meth_{side}_type.u32"), &f.methods.iter().map(|e| e.0).collect::<Vec<u32>>());
    20	    w(dir, &format!("meth_{side}_name.u32"), &f.methods.iter().map(|e| e.1 .0).collect::<Vec<u32>>());
    21	    w(dir, &format!("meth_{side}_mtyp.u32"), &f.methods.iter().map(|e| e.1 .1).collect::<Vec<u32>>());
    22	    w(dir, &format!("meth_{side}_ifn.u32"), &f.methods.iter().map(|e| e.1 .2).collect::<Vec<u32>>());
    23	    w(dir, &format!("meth_{side}_tfn.u32"), &f.methods.iter().map(|e| e.1 .3).collect::<Vec<u32>>());
    24	    w(dir, &format!("meth_{side}_exported.u8"), &f.methods.iter().map(|e| e.1 .4).collect::<Vec<u8>>());
    25	}
    26	
    27	const README: &str = r#"# Problem 2 diff of this dataset (B) against data/minio (A), in A's id space
    28	
    29	Written by `systems/ddflow`: `ddflow writediff /home/exedev/paradigms/data/minio <this dataset>`. Raw
    30	little-endian arrays, no header, as in ref/flat/README.md.
    31	
    32	**Ids.** The unified id space is A's: an A id is itself. `idmap_b.u32` (`nids_b` entries) maps every B id to its
    33	unified id; B slots with no A counterpart get ids `nids_a .. n_unified-1`. Ids are matched by object, then by
    34	unique (kind, name, name key, content hash) and a Myers alignment of the rest (systems/ddflow/src/facts.rs).
    35	`strmap_b.u32` maps B's `p2_str` ids to unified string ids (A's, then new ones from `nstr_a`).
    36	
    37	**Facts.** The same facts problem 2's rules read, as sets (deduplicated), in unified ids:
    38	- edges `(src, kind, val)`: kind 0 plain and 1 useiface: val = target id; kind 2 ifacemethod: val =
    39	  name string << 32 | func type id (the signature decoded through p2_sig_name/p2_sig_type); kind 3 namedmethod:
    40	  val = name string. Edges with a zero target are not facts.
    41	- flags `(id, bits)`: p2_flags masked to 0x1d (1 def, 4 reflect, 8 gotype, 16 iface); a change is minus old, plus new.
    42	- roots; method-table rows `(type, name, mtyp, ifn, tfn, exported)`.
    43	
    44	`*_minus_*`: in A, not in B. `*_plus_*`: in B, not in A. Applying minus then plus to A's facts gives B's.
    45	`live_b.u32`: B's oracle/p2_live.u32 mapped through idmap_b, sorted (to check a result in unified ids).
    46	`meta.txt`: the counts.
    47	"#;
    48	
    49	pub fn main(pos: &[String]) {
    50	    let (da, db) = (&pos[0], &pos[1]);
    51	    let u = facts::unify(da, db);
    52	    let mut a = facts::load(da, None, None);
    53	    let mut b = facts::load(db, Some(&u.idm_b), Some(&u.strm_b));
    54	    a.sort();
    55	    b.sort();
    56	    let d = facts::fact_diff(&a, &b);
    57	    let out = format!("{db}/diff");
    58	    std::fs::create_dir_all(&out).unwrap();
    59	    w(&out, "idmap_b.u32", &u.idm_b);
    60	    w(&out, "strmap_b.u32", &u.strm_b);
    61	    put(&out, "minus", &d.minus);
    62	    put(&out, "plus", &d.plus);
    63	    let mut lb: Vec<u32> = rd_u32(&format!("{db}/oracle/p2_live.u32")).iter().map(|&x| u.idm_b[x as usize]).collect();
    64	    lb.sort_unstable();
    65	    w(&out, "live_b.u32", &lb);
    66	    let nids_a = facts::meta(da, "nids");
    67	    let n_unified = u.idm_b.iter().copied().max().unwrap().max(nids_a as u32 - 1) + 1;
    68	    let nstr_a = std::fs::metadata(format!("{da}/p2_str.off.u64")).unwrap().len() / 8 - 1;
    69	    let nstr_u = u.strm_b.iter().copied().max().unwrap_or(0).max(nstr_a as u32 - 1) + 1;
    70	    let mut m = std::fs::File::create(format!("{out}/meta.txt")).unwrap();
    71	    writeln!(m, "nids_a {nids_a}\nnids_b {}\nn_unified {n_unified}\nnstr_a {nstr_a}\nnstr_unified {nstr_u}", u.idm_b.len()).unwrap();
    72	    for (s, f) in [("minus", &d.minus), ("plus", &d.plus)] {
    73	        writeln!(m, "edge_{s} {}\nflag_{s} {}\nroot_{s} {}\nmeth_{s} {}", f.edges.len(), f.flags.len(), f.roots.len(), f.methods.len()).unwrap();
    74	    }
    75	    writeln!(m, "live_b {}", lb.len()).unwrap();
    76	    std::fs::write(format!("{out}/README.md"), README).unwrap();
    77	    println!("wrote {out}: edges -{} +{}, flags -{} +{}, n_unified {n_unified}", d.minus.edges.len(), d.plus.edges.len(), d.minus.flags.len(), d.plus.flags.len());
    78	}
     1	# Differential Dataflow cells (for GRID.md; column: incremental relink)
     2	
     3	Times: ms, min / median / max of 5 interleaved rounds, 2026-11-16T19:39-19:49Z, load 4.4-5.6
     4	(`systems/ddflow/bench.sh`, output `bench-20261116T1940Z.txt`). warm = facts in memory, arrangements built inside
     5	the timing; e2e = process. Updates: 25 per cell (P1: 50). Checks are built in (exit 1 on mismatch); commands in
     6	NOTES.md. Edits: edit 1 = `ISO8601Parse` gains an `http.ParseTime` fallback (amztime), edit 2 = the console UI
     7	start-up deleted from `serverMain` (cmd); both rebuilt with freeze.sh and re-extracted (`data/minio-edit{,2}`).
     8	Same window C: p2 52.4 median (1 core), p2par 17.2 (8); p1 287 (1), p1par 22.0 (8).
     9	
    10	**1a+1b resolve** — **P1 OK** on MinIO and both edits (`ddflow p1inc $D/minio $D/minio-edit 5 -w 8` -> canon and
    11	targets 0 extra / 0 missing on both versions) · full: 1 wkr 8659 / 9310 / 10823 · 8 wkr 1285 / 2023 / 3203 ·
    12	**update edit 1: 0.6 / 0.7 / 1.5 (1 wkr), 0.5 / 29 / 195 (8)**; edit 2: 70 / 87 / 122, 44 / 64 / 237 · 23 lines
    13	of dataflow · easy: winner = `reduce` max per name key, candidates = semijoin/antijoin on groups, 1b = a join;
    14	forced: "lowest id" tie-break as (object, index in object) so other objects' growth does not churn it (still
    15	churns 34 K facts inside an edited object), package-table decode at load. ~30x C full; update 400x C.
    16	
    17	**2 reach** — **P2 OK** MinIO and kube-apiserver (`ddflow p2 $D/minio -w 8` -> extra 0, missing 0) · MinIO 1 wkr
    18	warm 3077 / 3343 / 3591, e2e 3468-4103 · 8 wkr warm 1491 / 1814 / 2439, e2e 1903-2907 (index facts 751 + flood
    19	1257) · kube 1 wkr 5605 / 5945 / 6787, 8 wkr 2608 / 2762 / 2948 · 71 lines · easy: all deadcode rules as two
    20	mutually recursive Variables (`live`, `iface`) in one iterative scope, the method rule as three semijoins;
    21	forced: static joins hoisted out of the loop, sig ids decoded at load. **60x / 100x C**: not a from-scratch engine.
    22	
    23	**2 incremental relink** — **OK**: after the update the live set = B's oracle (mapped) = a from-scratch run on B
    24	(`ddflow inc $D/minio $D/minio-edit 5 -w 1`) · diff edit 1: 27 edges + 30 flags, 31 symbols with changed facts of
    25	9.7 M facts, live -1 +2 · **update 1.3 / 1.4 / 3.4 (1 wkr) vs C recompute 52 / p2par 31 (1 thr)**; 8 wkr
    26	1.8 / 9.0 / 82 vs p2par 20.5 · diff edit 2: 234 edges, 141 symbols, **live -12,320 +10** · update 55 / 65 / 113
    27	(1), 28 / 555 / 688 (8): **slower than recomputing in C** (retraction through 45 rounds; 8 workers 8x worse at load 4-6; quiet 20:12Z: 29 on 8 vs 59 on 1) ·
    28	code: the full run plus `feed(minus, -1); feed(plus, +1)` · forced: a stable id space (positional ids shift on
    29	any edit): name/hash keys + Myers alignment, `facts.rs`, outside DD.
    30	
    31	1c, 3a, 3b: not attempted (outside this column's question).
    67					ifaceDone[c] = 1;
    68				}
    69				for (u32 e = eoff[c]; e < eoff[c + 1]; e++) {
    70					u32 t = ed[e];
    71					switch (ek[e]) {
    72					case 0:
    73						if (uii && (fl[t] & fGoType) && (fl[t] & fDef) && !iface[t]) {
    74							iface[t] = 1;
    75							if (live[t]) stack[sp++] = t;
    76						}
    77						mark(t);
    78						break;
    79					case 1: setIface(t); break;
    80					case 2:
    81						if (!sigSeen[t]) {
    82							sigSeen[t] = 1;
    83							u64 k = (u64)sigName[t] << 32 | sigType[t];
    84							for (u64 h = mix64(k) & (hs - 1);; h = (h + 1) & (hs - 1)) {
    85								if (sigTab[h] == 0) { sigTab[h] = k + 1; break; }
    86								if (sigTab[h] == k + 1) break;
    87							}
    88						}
    89						break;
    90					case 3: named[t] = 1; break;
    91					}
    92				}
    93				if (uii)
    94					for (u32 m = moff[c]; m < moff[c + 1]; m++) {
    95						setIface(mTyp[m]);
    96						markable[nm++] = m;
    97					}
    98			}
    99			// the methods something can now call
   100			u32 keep = 0;
   101			size_t before = sp;
   102			for (u32 i = 0; i < nm; i++) {
   103				u32 m = markable[i];
   104				int call = (reflectSeen && mExp[m]) || named[mName[m]];
   105				if (!call) {
   106					u64 k = (u64)mName[m] << 32 | mTyp[m];
   107					for (u64 h = mix64(k) & (hs - 1); sigTab[h]; h = (h + 1) & (hs - 1))
   108						if (sigTab[h] == k + 1) { call = 1; break; }
   109				}
   110				if (call) {
   111					mark(mTyp[m]);
   112					mark(mIfn[m]);
   113					mark(mTfn[m]);
   114				} else {
   115					markable[keep++] = m;
   116				}
   117			}
   118			nm = keep;
   119			if (sp == before && sp == 0) break;
   120		}
   121		double t1 = now();
   122		size_t nlive = 0;
   123		for (u32 g = 1; g < N; g++) nlive += live[g];
   124		u32 *list = malloc(4 * nlive), k = 0;
   125		for (u32 g = 1; g < N; g++)
   126			if (live[g]) list[k++] = g;
   127		printf("p2 (1 core): flood %.1f ms, %zu live, %d rounds, %u methods pruned, reflect %d\n", (t1 - t0) * 1e3, nlive, rounds, nm, reflectSeen);
   128		long bad = check32("live", list, nlive, "oracle/p2_live.u32");
   129		printf(bad ? "P2 FAIL\n" : "P2 OK\n");
   130		return bad != 0;
   131	}
     1	# JOURNAL (append-only: one line per commit, a paragraph at each milestone)
     2	- 2026-11-16T15:31:42Z M0 data: MinIO frozen (1,311 archives + main.a), hlink copied to ref/hlink with -dumpflat, flat files + oracles for P1-P3 (ref/flat/README.md); plain 1-core C references reproduce every oracle exactly (ref/flat/check.sh). Installs merged: Futhark 0.29.1, Halide 21, Soufflé 2.5, Goose v2026.4 (systems/*/NOTES.md).
     3	- 2026-11-16T15:34:24Z M0: GRID.md C column (hlink phases on this VM, plain flat C references), IDEAS.md index, four Opus cell builders launched (ideas/BUILDER.md).
     4	- 2026-11-16T15:36:04Z ref/flat/p2par.c: level-synchronous OpenMP flood on the flat CSR, full rules, P2 OK: 33.4 ms 1 thread, 12.1 ms 8 threads (load 14: provisional), 111 levels in 7 rounds; hlink's flood is 98-103 / 22-23 ms on raw objects. Problem 4 data/oracle handed to an Opus agent.
     5	- 2026-11-16T15:37:00Z ref/flat/p1par.c: partitioned join (256 partitions, per-partition tables) + parallel gather, P1 OK: 72.7 ms 1 thread, 19.2 ms 8 threads (load 7, provisional) vs plain 230 ms; hlink resolve 29-33 ms on raw objects.
     6	- 2026-11-16T15:54:54Z triage M0 skeptic: flat README contract fixes (name offsets, p1c_mark meaning, rank bit 30, id 0, p3_addr_all leak); stats.c note
     7	- 2026-11-16T15:57:39Z GRID: tuned flat C column at load 0.8 (p1par 13.9-16.8 ms, p2par 13.0-17.0 ms on 8 threads vs hlink resolve 24, floods 22-23); kube-apiserver flat files + all C refs OK; dumpflat decode-to-CSR timed 133-139 ms; Codex: p3 pipeline + 1c par (codex-c), p2pipe eager decode (codex-d)
     8	- 2026-11-16T15:58:28Z new column DuckDB (Opus builder, same brief): the north star names databases; DuckDB is installed (zero install cost), wildcard's top-2, and is the only vectorized-SQL/morsel-parallel engine in the grid (Soufflé is compiled Datalog, a different axis). Started before M2 because 4 builders + 2 Codex jobs leave the lead idle.
     9	- 2026-11-16T15:59:27Z ref/flat/reachfacts.py: the flood uses only 28.7% (MinIO, 2.39 M of 8.32 M) / 21.0% (kube, 3.27 M of 15.6 M) of the eagerly decoded CSR, and 25.6% / 26.6% of those are duplicate (source, target) pairs. Eager decode does 3.5-5x the decode lazy hlink does; ATTACKS 1 must compare against a frontier-batched lazy decode, not only eager.
    10	- 2026-11-16T16:00:00Z fix timestamps: GRID/PLAN said 16:xxZ for measurements taken at 15:56-15:58Z (the results commit message 'hlink minio bench (16:56Z)' should read 15:56Z)
    11	
    12	- 2026-11-16T15:59Z codex-c: partitioned content-hash join shared by plain C/OpenMP; all MinIO oracles and independent all-pairs cases pass; interleaved plain 302.9 -> 57.0 ms, warm par 1/8 threads 36.9/7.2 ms (results/codex-c-p1.md).
    13	- 2026-11-16T15:59Z codex-c: added independent P3 pipeline diagnostic; all 3a addresses match, but missing special-symbol inputs cause 81 relocation errors / 40 differing ranges. Corrected flat contract; details and reproduction in results/codex-c.md. Passing pipeline blocked on extraction extension.
    14	- 2026-11-16T16:02:18Z p3 --pipeline passes on both datasets: dumpflat now extracts the linker-placed relocation targets (p3_fixed_*, 7 on MinIO, 6 on kube) as given inputs; copied into data/* from a scratch re-extraction (other files byte-identical). Merged codex-c: 1c partitioned join 7.2-8.9 ms on 8 threads, 36.9 on 1 (hlink 8-9 / 37-39); plain p1 1c 303 -> 57 ms.
    15	- 2026-11-16T16:03:00Z ref/flat/p2lazy.c: lazy-decode flood over the flat relocation columns (no pre-decoded CSR for relocation edges; aux/kinds 1-3/methods precomputed as in p2pipe): MinIO 52.6-53.3 ms 1 thread / 14.4-15.4 ms 8 (p2par on the prebuilt CSR 26.0 / 8.6-9.7, interleaved, load 0.8); kube 72.7 / 19.9 (p2par 38.0 / 12.4). P2 OK both.
    16	- 2026-11-16T16:03:39Z merged codex-d (p2pipe: eager decode+CSR+flood 108.7 / 30.1 ms MinIO, loses to hlink); ATTACKS 1 written: lazy decode from columns inside the flood (p2lazy) 52.6 / 13.6-15.4 ms, 2x eager, 1.9x/1.5x hlink's flood with caveats (aux, kinds 1-3, methods precomputed). GRID rows 1c, 2', 3b updated.
    17	- 2026-11-16T16:08:14Z ATTACKS 2 draft: relocate's output page faults are ~half of 3b (prefault 45-64 ms vs copy+patch 31-35); MAP_HUGETLB output 25-29 ms vs 48-68 on 1 core; build/p3par (omp) added; vm.nr_hugepages set to 512 (was 0: hlink's hugetlbfs output path was not in use for GRID's hlink numbers); check.sh runs p2pipe, p2lazy, p3par.
    18	- 2026-11-16T16:10:08Z killed per-source dedup (flood ~8% faster on 1 thread for 37% fewer edges); IDEAS rows for ATTACKS 1/2
    19	- 2026-11-16T16:13:40Z p3 P3_REPS (fresh output per rep): relocate on 2 MB pages 23.9-30 vs 48.6-56 ms (1 thread), 5.4-12 (median 6.1-6.8) vs 13.4-29 ms (8 threads) = 1.1-1.4x the 4.92 floor; load 2.4-3.0. ATTACKS 2 no longer a draft.
    20	- 2026-11-16T16:15:20Z hlink huge pages 512 vs 0, 6 interleaved rounds: within noise (193-267 vs 205-249 ms total); earlier 'huge pages help hlink' withdrawn. Builders resumed (ideas/RESUME.md); Codex attack1 review triaged.
    21	- 2026-11-16T16:17:12Z PLAN Now: builders resumed in paradigms-46; codex-f launched on the p1par dense-pass hypothesis (789K of 5.2M slots keyed, 4 passes over 83 MB keys).
    22	- 2026-11-16T16:17:35Z PLAN.md: restored the user's campaign text (sections Campaign 1 through Milestones log), deleted by a Now-line rewrite after 40f4d4b; Now line kept.
    23	- 2026-11-16T16:18:13Z GRID: measured VM bandwidth (25 / 36 GB/s read, 27 write) vs FLOOR.md's implied ~87 GB/s; 3b on huge pages meets the per-kernel bandwidth bound.
    24	- 2026-11-16T16:21:00Z relaxed atomics for the shared flags in p2par/p2pipe flood (Codex review; P2 OK both datasets). Found: under the builders' load, spinning OMP barriers make p2par 8 threads 900-1300 ms instead of 14; OMP_WAIT_POLICY=passive 14-55 ms. check.sh defaults to passive; builders warned (ideas/RESUME.md).
    25	
    26	2026-11-16T16:14:34Z codex-e: raw AuxFuncdata columns + opt-in lazy aux flood; exact per-source aux multisets and P2 live oracles on both datasets. Interleaved best-of-five and load in results/codex-e.md. All 67 existing files byte-identical; shared data publication blocked by read-only mount, new columns retained in /tmp/codex-e-data.
    27	- 2026-11-16T16:23:34Z merged codex-e: aux_* columns (add-aux.sh, run by the lead: 67/67 old files byte-identical on both datasets), p2lazy --aux decodes funcdata aux in the flood, P2 OK both datasets 1/8 threads (lead rerun 16:23Z, load 11); +7-9 ms on 1 thread. check.sh runs --aux.
    28	- 2026-11-16T16:25:07Z p2lazy: timed-decoder check (per-source edge hash vs oracle-derived, mutation-tested) and relaxed atomics; closes the attack1 review's accepted items.
    29	- 2026-11-16T16:33:15Z merged Goose (P1/P2/P3 all OK, lead-rerun; 3b on 2 MB pages via C remap 41 vs 92 ms; 8-worker reach scales 1.7x; one-array 8-worker 3b falls down). GRID Goose column filled.
    30	
    31	2026-11-16T16:22Z codex-f: test the dense-name-array traffic hypothesis with a single-pass compact member/candidate join; partition ownership replaces global candidate probes. MinIO 8-thread and independent kube-apiserver 1-thread checks agree with all four oracles. Added mean timing instrumentation to both versions; interleaved 100-repetition measurements in progress, build-f only, 8 GiB address-space cap.
    32	
    33	2026-11-16T16:31Z codex-f: completed 40 interleaved oracle-checked runs (100 repetitions each, 1/8 threads, MinIO/kube-apiserver, 8 GiB cap). Kube one-thread join median 124.077 -> 77.424 ms, non-overlapping ranges; other join comparisons within noise. Counted name-key coverage corrects MinIO's four-full-pass premise to 202.683 -> 83.287 MB; compact staging leaves modeled total streams 348.910 -> 286.290 MB. perf unavailable. Full ranges, arithmetic, scripts and raw evidence in results/codex-f.md. The earlier implementation-entry timestamp was written ahead of the VM clock; its commit occurred before 16:21Z.
    34	- 2026-11-16T16:37:02Z merged codex-f (p1compact, P1 OK both datasets); lead rerun: join 1.5x faster on 1 thread (ranges apart), ~1.35-1.5x medians on 8; modeled bytes only -18-21%, so mechanism open (IDEAS).
    35	- 2026-11-16T16:38:40Z merged Futhark (every cell OK from a lead source rebuild; ATTACKS 1 falls down in Futhark: lazy 165 / 90 slower than its eager flood; surprise: glibc.malloc.hugetlb=2 tunable gives 1.2-1.7x across cells with no code change).
    36	2026-11-16T16:30Z codex-g: implemented pthread asynchronous P2 flood (seven join phases, chunk stealing, outstanding-work credits); 120/120 oracle checks plus independent six-case review and full check.sh on both datasets passed. Five interleaved >=1s samples per case: ambient 8-thread MinIO active/passive/async medians 953.49/21.40/19.93 ms, kube 940.07/33.49/34.38 ms; with eight busy workers MinIO 2289.64/152.71/89.40 ms. Passive vs async ambient within noise; deliberate-load ranges separate. Quiet claim untested; identical live sets do not imply identical queue-pop counts. Raw load, timing, counts and commands in results/codex-g.md.
    37	- 2026-11-16T16:39:31Z ATTACKS 3: the level count is reach's cost across paradigms; p2async (7 joins vs 111 barriers) 1.7x over passive OpenMP under overload, 25x over default active.
    38	- 2026-11-16T16:40:19Z ATTACKS 2: glibc.malloc.hugetlb=2 tunable gives plain C relocate the same 2x (24 vs 55 ms, 39 vs 19,109 faults) with no code change.
    39	- 2026-11-16T16:40:57Z hugetlb tunable: p1compact join 1.5x on 1 thread (20.5-21.2 vs 30.1-32.3), floods within noise (inputs are mmap'd files).
    40	- 2026-11-16T16:41:52Z flat.h FLAT_HUGE=1 (inputs copied onto 2 MB pages): floods 9-12% faster, join unchanged: killed as an attack; vm.nr_hugepages raised 512 -> 1536 (builders use the tunable too).
    41	- 2026-11-16T16:56:03Z merged Soufflé (all cells OK from a lead rebuild; 30-200x the C refs; 3a falls down: no sort; lazy reach is 3 rules, 2x its eager).
    42	
    43	- 2026-11-16T16:43:09.060031+00:00: codex-h separates compact emission (onlyA) from partition-owned candidate joins (onlyB); both datasets pass all P1 oracles; interleaved measurements in progress.
    44	
    45	- 2026-11-16T16:56:23.217995+00:00: codex-h completed 80 interleaved comparison and 20 phase processes, all P1 OK. One-thread medians baseline/A/B/compact: MinIO 46.085/36.849/29.528/28.917 ms; kube 101.934/84.405/73.838/63.428 ms. Partition ownership carries most of the gain; candidate/member readback dominates the phase savings. Full ranges and commands in results/codex-h.md; check.sh unchanged.
    46	- 2026-11-16T16:57:49Z merged codex-h: p1compact's 1.5x is mostly partition-local candidates (random probes into 256 tables), not fewer dense passes; bandwidth hypothesis killed (IDEAS).
    47	- 2026-11-16T17:10:46Z merged DuckDB (all cells OK, lead rerun; 10-150x tuned C: every dense-id lookup is a hash join; P2 falls down: 45 rounds x 25 statements).
    48	- 2026-11-16T17:13:26Z merged Halide (P1/P2/P3 OK, lead rebuild; 1a beats plain C 2.5-3x on 1 core; lazy reach slower than eager in Halide; no fixpoint/worklist).
    49	
    50	2026-11-16T17:03Z codex-i: profile 3a and add tuned unit-parallel/MSD radix C layout; both datasets match every address at 1/8 threads. Five interleaved >=1 s runs: MinIO medians 7.37 ms plain, 5.00 / 3.89 ms tuned; hugepage request within noise, floor not reached. Details/results: results/codex-i.md.
    51	- 2026-11-16T17:14:11Z merged codex-i: p3a_par 3a 5.0 / 3.9 ms (plain 7.4); the typelink string sort is 4.7 of 7.4 ms; 6.5x FLOOR.md's 0.60. Lead rerun: addr 0 differ both datasets 1/8 threads.
    52	
    53	**M1 + M2, 2026-11-16T17:20Z.** All five paradigms are correct on problems 1-3 on MinIO; the lead rebuilt every cell from
    54	its source and reran its oracle. None of them beats tuned C on any cell; the distances are the finding. Halide is closest
    55	(1a 20-27 vs 12-21 ms on 8 threads; its 1a beats *plain* C 2.5-3x on one core by compacting first), Goose next (1.5-3x,
    90	        let iface_t = lit.clone().join_core(typeedge.enter(inner), |_, _, &t| Some(t));
    91	        let iface_m = lit.clone().join_core(methodtype.enter(inner), |_, _, &t| Some(t));
    92	        let cand = lit.join_core(typemethod.enter(inner), |_, _, &m| Some(m));
    93	        // the methods something can call
    94	        let by_reflect = cand.clone().filter(|m| m.4 != 0).map(|m| ((), m)).semijoin(reflect).map(|(_, m)| m);
    95	        let by_sig = cand.clone().map(|m| ((m.0, m.1), m)).semijoin(sig).map(|(_, m)| m);
    96	        let by_name = cand.map(|m| (m.0, m)).semijoin(named).map(|(_, m)| m);
    97	        let called = by_reflect
    98	            .concat(by_sig)
    99	            .concat(by_name)
   100	            .flat_map(|(_, mt, ifn, tfn, _)| [mt, ifn, tfn])
   101	            .filter(|&x| x != 0);
   102	        live_v.set(roots.enter(inner).concat(targets).concat(called).distinct_core::<Diff>());
   103	        iface_v.set(iface0.enter(inner).concat(useiface).concat(iface_t).concat(iface_m).distinct_core::<Diff>());
   104	        if std::env::var("DD_ROUNDS").is_ok() {
   105	            // diagnostic: the number of rounds the loop took (the largest inner time of a live change)
   106	            live.clone().inspect_batch(|_, d| {
   107	                ROUNDS.with(|r| r.set(d.iter().fold(r.get(), |m, x| m.max(x.1.inner as u64))));
   108	            });
   109	        }
   110	        live.leave(scope)
   111	    })
   112	}
   113	
   114	/// One instance of the dataflow on a worker: its inputs, its probe, and this worker's share of the live set
   115	/// (accumulated from the output's changes).
   116	struct Dataflow {
   117	    inp: Inputs,
   118	    probe: Handle<Time>,
   119	    live: Rc<RefCell<HashMap<Id, Diff>>>,
   120	    t: Time,
   121	}
   122	impl Dataflow {
   123	    fn new(worker: &mut timely::worker::Worker) -> Dataflow {
   124	        let probe = Handle::new();
   125	        let live: Rc<RefCell<HashMap<Id, Diff>>> = Rc::new(RefCell::new(HashMap::new()));
   126	        let l2 = live.clone();
   127	        let inp = worker.dataflow(|scope| {
   128	            let (ri, r) = scope.new_collection();
   129	            let (ei, e) = scope.new_collection();
   130	            let (fi, f) = scope.new_collection();
   131	            let (mi, m) = scope.new_collection();
   132	            reach(r, e, f, m)
   133	                .inspect(move |(x, _t, d)| *l2.borrow_mut().entry(*x).or_insert(0) += *d)
   134	                .probe_with(&probe);
   135	            Inputs { roots: ri, edges: ei, flags: fi, methods: mi }
   136	        });
   137	        Dataflow { inp, probe, live, t: 0 }
   138	    }
   139	    /// close the current time and run until the output has caught up with it
   140	    fn run(&mut self, worker: &mut timely::worker::Worker) {
   141	        self.t += 1;
   142	        self.inp.advance(self.t);
   143	        let (p, t) = (&self.probe, self.t);
   144	        worker.step_while(|| p.less_than(&t));
   145	    }
   146	    fn live(&self) -> Vec<Id> {
   147	        self.live.borrow().iter().filter(|(_, &c)| c > 0).map(|(&k, _)| k).collect()
   148	    }
   149	}
   150	
   151	struct Inputs {
   152	    roots: InputSession<Time, Id, Diff>,
   153	    edges: InputSession<Time, (Id, EdgeVal), Diff>,
   154	    flags: InputSession<Time, (Id, u8), Diff>,
   155	    methods: InputSession<Time, (Id, Meth), Diff>,
   102	| C p2 recompute (flood, warm) | 41.7 / 56.5 / 92.6 (edit 1), 46.1 / 48.8 / 52.1 (edit 2) | |
   103	| C p2par recompute (best of 5) | 27.7 / 30.8 / 118.4; 24.8 / 31.2 / 34.2 | 12.7 / 20.5 / 30.1; 16.2 / 19.4 / 34.9 |
   104	| DD full A | 2447 / 2703 / 3839 | 1389 / 1621 / 2118 |
   105	| DD full B from scratch | 2105 / 2143 / 3310 | 491 / 1628 / 1943 |
   106	| **DD update, edit 1** (A->B; B->A) | **1.3 / 1.4 / 3.4; 1.3 / 1.4 / 1.9** | 1.8 / 9.0 / 81.9; 1.8 / 4.3 / 71.6 |
   107	| **DD update, edit 2** | **54.9 / 65.3 / 113; 50.0 / 64.7 / 118** | 28.0 / 555 / 688; 28.2 / 526 / 607 |
   108	
   109	- Edit 1: the update is **~1,900x** DD's own full run, **35-40x faster than the single-core C recompute** and ~20x
   110	  p2par on 1 thread; on 8 cores it is a tie to ~2x faster than p2par's 8-thread recompute (medians 4.3-9.0 vs
   111	  20.5) with a long tail.
   112	- Edit 2 (12 K ids die): the update is ~40x DD's full but **slower than recomputing in C** (65 vs 49 single
   113	  core, 19 on 8 threads). Retracting reachability is DD's expensive case: every dead id's derivation is withdrawn
   114	  round by round through the loop (45 rounds), with the arrangements' history of every round.
   115	- **Corrected 20:12Z (results/c-incremental.md): on a quiet machine (load 0.7-1.7) edit 2 updates in 29 ms on 8
   116	  workers vs 59 on 1, edit 1 in 2.1 vs 1.4; the slowdown below is load.** At load 4.4-5.6, 8 workers made updates *slower* (edit 2: 526-555 median vs 65 on 1 worker): per-round coordination of a
   117	  small, many-round update. The 28 ms minimum shows what it costs when the machine is quiet for a moment.
   118	
   119	### Problem 1a+1b
   120	
   121	| | 1 | 8 |
   122	|---|---|---|
   123	| C p1 (join 1a + targets 1b, warm) | 260.9 / 287.0 / 563.3 | p1par: 19.9 / 22.0 / 43.7 |
   124	| DD full A (1a+1b) | 8659 / 9310 / 10823 | 1285 / 2023 / 3203 |
   125	| **DD update, edit 1** | **0.6 / 0.7 / 1.5** | 0.5 / 29.3 / 195 |
   126	| **DD update, edit 2** | **70.2 / 87.2 / 122** | 43.9 / 64.2 / 237 |
   127	
   128	Edit 1 resolve update is **~400x faster than single-core C recompute and ~30x faster than p1par on 8 threads**.
   129	Edit 2's update (35 K changed facts, mostly the tie-break churn) is ~3x faster than C single-core, ~3-4x slower than
   130	p1par.
   131	
   132	## What P1 (resolve) needs to be incremental
   133	
   134	Measured above, so this is what had to be true for it to work:
   135	
   136	1. **Symbol identity that does not move.** Positional ids shift every later object on any slot-count change.
   137	   The flat files give enough (name, name key, content hash, per-object order) to rebuild identity by matching;
   138	   a linker designed for incrementality would key symbols by (package, name) or content from the start and
   139	   would use the importcfg's package path for objects.
   140	2. **The by-name join keyed by strings, as a grouped reduce.** 1a is `reduce` over the 128-bit name key: a group
   141	   is recomputed only if one of its members changed (edit 1: ~20 groups). DD does this directly
   142	   (`members.reduce(max)`, 23 lines of dataflow for 1a+1b in `src/p1.rs`).
   143	3. **Tie-breaks that are not positional.** "Lowest id" had to become "lowest (object, index in object)" to stay
   144	   exact; that still churns inside the edited object (edit 2: 34 K facts). Ordering ties by something stable
   145	   (name hash) would make the update proportional to the edit.
   146	4. **Relocation targets as (package, index) or name, not base + index.** 1b reads `obj_pkg` bases; the raw
   147	   target `base + idx` was mapped into the stable id space at load. Expressed as a join of (relocation, raw
   148	   target) with canon, 1b updates only changed relocations and relocations whose target's canon changed
   149	   (edit 1: 71 of 9.3 M).
   150	5. **1c (content-hash join) depends on P2's marks**, so an incremental 1c needs the flood's output as an input
   151	   collection, which DD can wire as one dataflow (not done here).
   152	
   153	## Memory
   154	
   155	Peak RSS (`/usr/bin/time %M`): `p2` 1.35-1.43 GB, `inc` (two versions' facts + two dataflows) 2.7 GB, `p1inc`
   156	3.5 GB, all under `prlimit --as=12G`.

exec
/bin/bash -lc 'FUZZ=50 prlimit --as=12884901888 /home/exedev/paradigms/build/p2inc /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit2 5' in /home/exedev/paradigms-w
 succeeded in 5613ms:
p2inc (DRed + derivation-order supports): setup 432.1 ms, flood A (same rules, worklist) 96.2 ms, 2197091 edges read
check A: 663630 oracle entries, 0 differ -> OK
diff: edges -135 +99, flags -73 +44, roots -0 +0
update A->B	20.595 ms	rep 0	over-deleted 32238 live, rederived 5050 directly, set 19928, edges read 176298, reverse entries 3049918, support checks 369484
check A->B vs B oracle (unified ids): 651320 oracle entries, 0 differ -> OK
update B->A	6.766 ms	rep 0	over-deleted 8112 live, rederived 13 directly, set 20422, edges read 100280, reverse entries 1088061, support checks 98502
check B->A vs A oracle: 663630 oracle entries, 0 differ -> OK
update A->B	11.633 ms	rep 1	over-deleted 20412 live, rederived 1 directly, set 8102, edges read 100218, reverse entries 2257483, support checks 222738
check A->B vs B oracle (unified ids): 651320 oracle entries, 0 differ -> OK
update B->A	7.514 ms	rep 1	over-deleted 8102 live, rederived 3 directly, set 20412, edges read 100108, reverse entries 1086145, support checks 98536
check B->A vs A oracle: 663630 oracle entries, 0 differ -> OK
update A->B	11.114 ms	rep 2	over-deleted 20412 live, rederived 1 directly, set 8102, edges read 100176, reverse entries 2253023, support checks 222742
check A->B vs B oracle (unified ids): 651320 oracle entries, 0 differ -> OK
update B->A	5.776 ms	rep 2	over-deleted 8102 live, rederived 3 directly, set 20412, edges read 100098, reverse entries 1086132, support checks 98540
check B->A vs A oracle: 663630 oracle entries, 0 differ -> OK
update A->B	10.249 ms	rep 3	over-deleted 20412 live, rederived 1 directly, set 8102, edges read 100176, reverse entries 2253023, support checks 222742
check A->B vs B oracle (unified ids): 651320 oracle entries, 0 differ -> OK
update B->A	5.881 ms	rep 3	over-deleted 8102 live, rederived 3 directly, set 20412, edges read 100098, reverse entries 1086132, support checks 98540
check B->A vs A oracle: 663630 oracle entries, 0 differ -> OK
update A->B	10.870 ms	rep 4	over-deleted 20412 live, rederived 1 directly, set 8102, edges read 100176, reverse entries 2253023, support checks 222742
check A->B vs B oracle (unified ids): 651320 oracle entries, 0 differ -> OK
update B->A	5.911 ms	rep 4	over-deleted 8102 live, rederived 3 directly, set 20412, edges read 100098, reverse entries 1086132, support checks 98540
check B->A vs A oracle: 663630 oracle entries, 0 differ -> OK
best update A->B 10.249 ms, B->A 5.776 ms
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
check fuzz revert vs B oracle: 651320 oracle entries, 0 differ -> OK
fuzz: 50 random diffs, 0 failed
P2INC OK

exec
/bin/bash -lc '/home/exedev/paradigms/build/p2inc /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit 5' in /home/exedev/paradigms-w
 succeeded in 530ms:
p2inc (DRed + derivation-order supports): setup 462.9 ms, flood A (same rules, worklist) 97.7 ms, 2197091 edges read
check A: 663630 oracle entries, 0 differ -> OK
diff: edges -11 +16, flags -14 +16, roots -0 +0
update A->B	0.035 ms	rep 0	over-deleted 1 live, rederived 2 directly, set 2, edges read 0, reverse entries 16, support checks 2
check A->B vs B oracle (unified ids): 663631 oracle entries, 0 differ -> OK
update B->A	0.005 ms	rep 0	over-deleted 2 live, rederived 1 directly, set 1, edges read 0, reverse entries 7, support checks 3
check B->A vs A oracle: 663630 oracle entries, 0 differ -> OK
update A->B	0.002 ms	rep 1	over-deleted 1 live, rederived 2 directly, set 2, edges read 0, reverse entries 14, support checks 2
check A->B vs B oracle (unified ids): 663631 oracle entries, 0 differ -> OK
update B->A	0.005 ms	rep 1	over-deleted 2 live, rederived 1 directly, set 1, edges read 0, reverse entries 7, support checks 3
check B->A vs A oracle: 663630 oracle entries, 0 differ -> OK
update A->B	0.003 ms	rep 2	over-deleted 1 live, rederived 2 directly, set 2, edges read 0, reverse entries 14, support checks 2
check A->B vs B oracle (unified ids): 663631 oracle entries, 0 differ -> OK
update B->A	0.003 ms	rep 2	over-deleted 2 live, rederived 1 directly, set 1, edges read 0, reverse entries 7, support checks 3
check B->A vs A oracle: 663630 oracle entries, 0 differ -> OK
update A->B	0.002 ms	rep 3	over-deleted 1 live, rederived 2 directly, set 2, edges read 0, reverse entries 14, support checks 2
check A->B vs B oracle (unified ids): 663631 oracle entries, 0 differ -> OK
update B->A	0.003 ms	rep 3	over-deleted 2 live, rederived 1 directly, set 1, edges read 0, reverse entries 7, support checks 3
check B->A vs A oracle: 663630 oracle entries, 0 differ -> OK
update A->B	0.002 ms	rep 4	over-deleted 1 live, rederived 2 directly, set 2, edges read 0, reverse entries 14, support checks 2
check A->B vs B oracle (unified ids): 663631 oracle entries, 0 differ -> OK
update B->A	0.005 ms	rep 4	over-deleted 2 live, rederived 1 directly, set 1, edges read 0, reverse entries 7, support checks 3
check B->A vs A oracle: 663630 oracle entries, 0 differ -> OK
best update A->B 0.002 ms, B->A 0.003 ms
P2INC OK

exec
/bin/bash -lc "/home/exedev/tools/rust/target-ddflow/release/ddflow inc /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit2 2 -w 1 | rg '"'^(check|update|P2 |full |e2e)'"'" in /home/exedev/paradigms-w
 succeeded in 6967ms:
full A	2469.5 ms
update A->B	57.00 ms	rep 0
update B->A	60.00 ms	rep 0
update A->B	60.22 ms	rep 1
update B->A	56.39 ms	rep 1
full B (from scratch)	1981.7 ms
check A: 663630 live, oracle 663630, extra 0, missing 0 -> OK
check A->B vs B oracle (unified ids): 651320 live, oracle 651320, extra 0, missing 0 -> OK
check B->A vs A oracle: 663630 live, oracle 663630, extra 0, missing 0 -> OK
check A->B update vs B from scratch (same process, unified ids): OK
check B from scratch vs B oracle: 651320 live, oracle 651320, extra 0, missing 0 -> OK
P2 OK
e2e (process start to here) 6929 ms

exec
/bin/bash -lc "sed -n '1,158p' ATTACKS.md; sed -n '1,55p' ref/flat/p2.c; sed -n '1,75p' ref/flat/p2inc.c; rg -n 'session|usage|budget|time|ending|defer|shrink' JOURNAL.md" in /home/exedev/paradigms-w
 succeeded in 0ms:
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
-- its tables and records are malloc'd and probed at random. The original "TLB reach, not faults"
explanation was unmeasured. [Five-round join-only perf checks](results/codex-review5.md#c-p1compact-tlb-reach-and-first-touch-faults)
now show **both mechanisms, in different phases**: a first join has median 28,407 vs 72 minor faults;
a warmed join has zero faults in either mode, but median page-walk requests fall from 113,467 to 46,073
and L1 dTLB misses from 1,095,355 to 389,057 per join. `smaps` verifies 188 MiB of explicit hugetlb
allocations with the tunable (zero without). Warm mean times are 31.582 vs 26.495 ms median, with
overlapping ranges: this noisy window does not reproduce a precise speedup. Translation pressure
falls after prefaulting; fault savings also matter on the first execution. The floods (p2par, p2lazy)
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

// p2.c: problem 2 (reach) from the flat files, plainly, on one core, with a worklist. Checks itself against
// oracle/p2_live.u32. The rules are data/README.md's (they are cmd/link's deadcode.go):
//   live(r)  for every root r
//   live(t)  <- live(s), plain(s,t)
//   iface(t) <- live(s), iface(s), gotype(s), plain(s,t), gotype(t), def(t)
//   iface(t) <- live(s), useiface(s,t)
//   sig(x)   <- live(s), ifacemethod(s,x);   named(n) <- live(s), namedmethod(s,n);   reflect <- live(s), reflect(s)
//   iface(m.mtyp) and markable(m) <- live(s), iface(s), gotype(s), method(s,m)
//   live(m.mtyp), live(m.ifn), live(m.tfn) <- markable(m), (reflect & exported(m) | sig(m.name, m.mtyp) | named(m.name))
// (only defined symbols have edges; edges are given only for them).
//   ref/flat/p2 data/minio
#include "flat.h"

enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16 };

static u8 *live, *iface, *ifaceDone;
static u32 *stack;
static size_t sp;
static inline void mark(u32 c) {
	if (c && !live[c]) {
		live[c] = 1;
		stack[sp++] = c;
	}
}
static inline void setIface(u32 c) {
	if (c && !iface[c]) {
		iface[c] = 1;
		if (live[c]) stack[sp++] = c;
	}
}

int main(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "data/minio";
	u32 N = (u32)metaGet("nids"), nsl = (u32)metaGet("nslots");
	size_t nroots, nsig, nstr, nmeth;
	const u8 *fl = FMAP(u8, "p2_flags.u8", NULL), *ek = FMAP(u8, "p2_edge_kind.u8", NULL);
	const u32 *eoff = FMAP(u32, "p2_edge_off.u32", NULL), *ed = FMAP(u32, "p2_edge_dst.u32", NULL);
	const u32 *sigName = FMAP(u32, "p2_sig_name.u32", &nsig), *sigType = FMAP(u32, "p2_sig_type.u32", NULL);
	const u32 *moff = FMAP(u32, "p2_meth_off.u32", NULL), *mName = FMAP(u32, "p2_meth_name.u32", &nmeth);
	const u32 *mTyp = FMAP(u32, "p2_meth_mtyp.u32", NULL), *mIfn = FMAP(u32, "p2_meth_ifn.u32", NULL), *mTfn = FMAP(u32, "p2_meth_tfn.u32", NULL);
	const u8 *mExp = FMAP(u8, "p2_meth_exported.u8", NULL);
	const u32 *roots = FMAP(u32, "p2_roots.u32", &nroots);
	FMAP(u64, "p2_str.off.u64", &nstr);
	nroots /= 4, nsig /= 4, nmeth /= 4, nstr = nstr / 8 - 1;

	double t0 = now();
	live = calloc(N, 1), iface = calloc(N, 1), ifaceDone = calloc(N, 1);
	stack = malloc(4 * (size_t)N * 3);
	for (u32 g = 1; g <= nsl; g++) iface[g] = (fl[g] & fIface) != 0;
	u8 *sigSeen = calloc(nsig, 1), *named = calloc(nstr + 1, 1);
	// sig ids are (name, type) pairs; a method's (name, mtyp) is looked up in a hash of the live sigs
	u64 hs = 1;
	while (hs < 4 * nsig + 64) hs <<= 1;
	u64 *sigTab = calloc(hs, 8);
	u32 *markable = malloc(4 * (nmeth + 1)), nm = 0;
// p2inc.c: problem 2 (reach) maintained under an edit, plainly, on one core. It floods version A (the previous
// link's state, kept in memory as a resident linker would), then applies a stored diff (DATA_B/diff/, written by
// `ddflow writediff`, see its README.md) as an update, and back, REPS times; every result is checked against B's
// oracle in A's id space (diff/live_b.u32) or A's oracle.
//   gcc -O2 -march=native -o build/p2inc ref/flat/p2inc.c && build/p2inc data/minio data/minio-edit [REPS] [plain]
//   FUZZ=K build/p2inc ...: then K random diffs (up to 200 edges removed from live sources, 200 added, all kinds)
//   on top of B, each checked against a from-scratch flood of the same graph, and reverted to B's oracle.
//
// The rules are p2.c's (data/README.md). The state kept between links, all derived by the same rules: live, iface,
// typeDone (live, iface, type descriptor: its type edges and methods have been applied), called (per method),
// counts of the aggregate facts sig(name, type), named(name), reflect (how many live sources assert them), and a
// stamp per fact: the clock when it was set, so every fact was derived from facts with smaller stamps.
//
// An update is DRed (delete and rederive, Gupta-Mumick-Subrahmanian 1993), in three steps:
//  1. over-delete, with the old flags: all removed edges leave the graph, then the facts they derived are
//     retracted, transitively -- except that a fact is kept if it still has a derivation from a true fact with a
//     smaller stamp (a predecessor, a root, a flag, a called method; a method keeps its call if a live source of
//     its condition has a smaller stamp than the call). Every retraction re-checks what it supported, so what is
//     kept has a chain of decreasing stamps down to roots and flags: it is derivable. ("plain": textbook DRed,
//     nothing is kept, every fact downstream of a removal is retracted.)
//  2. rederive, with the new flags and edges: each retracted fact with any derivation from true facts is set again
//     (one reverse lookup: predecessors, calling methods, roots); the worklist restores what follows from it;
//  3. insert: added edges of live sources, added roots, and what follows, by the same worklist as the initial
//     flood (only new targets are pushed).
// Why not support counts: liveness is cyclic (a function and its closure keep each other alive), so a count of
// live predecessors never reaches zero for a dead cycle. The same holds for the method rule: a sig's remaining
// sources can be the method's own targets, which is why a method's condition is checked by stamps, not counts
// (found by the fuzzer). Why not "recompute when a live edge is removed": every real edit removes live edges (the
// edited function's GC-locals symbols are renamed), so that policy would always recompute. Why stamps: textbook
// DRed over-deletes 594 K of 663 K live ids for edit 2 (a retracted source of reflect/sig/named retracts every
// method call it may justify, and everything downstream of those); with stamps the cone is 26 K.
// Method-table diffs are not supported (exit 3; neither edit has one).
#include "flat.h"

enum { fDef = 1, fReflect = 4, fGoType = 8, fIface = 16 };
#define ISTYPE(x) ((flags[x] & (fDef | fGoType)) == (fDef | fGoType))

static u32 N, nslA, nmeth, nstrU;
static u8 *flags;
// A's edges, deduplicated per source: CSR eoff/ekind/eval (kind 2 val = name << 32 | type; kind 3 val = name)
static u32 *eoff;
static u8 *ekind;
static u64 *eval;
// reverse of A's kind 0 and 1 edges: roff/rsrc/rkind
static u32 *roff, *rsrc;
static u8 *rkind;
// methods (A's tables; method-table diffs are not supported: both edits have none)
static u32 *moff, *mown, *mname, *mtyp, *mifn, *mtfn, *revoff, *revm, *nameoff, *namem, *expm, nexp;
static u8 *mexp;
// state
static u8 *live, *iface, *typeDone, *called, *isRoot;
// derivation order: a fact's stamp is the clock when it was set, so it was derived from facts with smaller stamps
static u32 clk, *ordL, *ordI, *ordT, *ordM;
// where the last support check of x found its support in x's reverse list: entries before it can never support x
// again while x keeps its stamp (a retracted predecessor comes back only with a larger stamp), so the scan resumes
// there; reset when x is stamped anew. This bounds the checks of x to one pass over its predecessors per stamp.
static u32 *posL, *posI;
static int useOrder = 1; // argv[4] "plain": textbook DRed (retract everything downstream, then rederive)
static u64 cChecks;
static u32 *namedCnt, reflectCnt;
// sig table: key -> count, and the methods with that (name, mtyp) as a sorted-run range
typedef struct { u64 key; u32 cnt, mstart, mlen, sstart, slen; } sigent;
static sigent *sigs;
static u64 smask;
static u32 *bysig;
// the sources asserting each aggregate fact in A (for well-founded checks of a method's condition): sig key ->
// sources (sigent.sstart/slen into sigsrc), name -> sources (namesrc CSR), reflect callers (reflsrc)
static u32 *sigsrc, *nsoff, *namesrc, *reflsrc, nrefl;
static u32 *msusp;
static size_t nmsusp, msuspcap;
// overlay: edges removed from A (hash set), edges added (per-source and per-target lists)
typedef struct { u32 src; u8 kind, alive; u64 val; } remEnt;
static remEnt *rem;
static u64 rmask;
static u8 *maybeRem;
7:- 2026-11-16T15:57:39Z GRID: tuned flat C column at load 0.8 (p1par 13.9-16.8 ms, p2par 13.0-17.0 ms on 8 threads vs hlink resolve 24, floods 22-23); kube-apiserver flat files + all C refs OK; dumpflat decode-to-CSR timed 133-139 ms; Codex: p3 pipeline + 1c par (codex-c), p2pipe eager decode (codex-d)
10:- 2026-11-16T16:00:00Z fix timestamps: GRID/PLAN said 16:xxZ for measurements taken at 15:56-15:58Z (the results commit message 'hlink minio bench (16:56Z)' should read 15:56Z)
28:- 2026-11-16T16:25:07Z p2lazy: timed-decoder check (per-source edge hash vs oracle-derived, mutation-tested) and relaxed atomics; closes the attack1 review's accepted items.
33:2026-11-16T16:31Z codex-f: completed 40 interleaved oracle-checked runs (100 repetitions each, 1/8 threads, MinIO/kube-apiserver, 8 GiB cap). Kube one-thread join median 124.077 -> 77.424 ms, non-overlapping ranges; other join comparisons within noise. Counted name-key coverage corrects MinIO's four-full-pass premise to 202.683 -> 83.287 MB; compact staging leaves modeled total streams 348.910 -> 286.290 MB. perf unavailable. Full ranges, arithmetic, scripts and raw evidence in results/codex-f.md. The earlier implementation-entry timestamp was written ahead of the VM clock; its commit occurred before 16:21Z.
60:**page size** (every runtime that let the output or its tables onto 2 MB pages got 1.2-2x). Those became ATTACKS 3 and 2.
76:- 2026-11-16T17:23:23Z p2hyb (sequential tail flood): 111 -> 12 barriers but ~44% of pops go sequential; times noise at load 19.
85:2026-11-16T17:43Z CP1/CP4 measurements complete: 35 interleaved oracle-passing runs. CP1 66,046 keys / 82.252% top-4096 coverage passes counting, but layout median 42.708 ms vs p4 6.805 ms (6.28x) kills runtime relaxation. CP4 covered medians 46.611/27.865 ms (1.67x) overlap in range: within noise, not an established threshold pass; full 1-thread median 110.532 ms misses 80 ms. Perf shows 26% fewer instructions than p4par, no branch-miss reduction. Full report results/codex-cp.md; independent review + SIB/RIP synthetic and negative-offset tests retained.
102:- 2026-11-16T18:22:41Z launched Goose P4 builder, Futhark 3b follow-up, codex-cp7 (pieces vs time).
106:2026-11-16 CP7: prepared contiguous runs in C (hole-free-neighbour fusion, whole-function blobs, sequence deduplication), plus an unmerged prepared control to distinguish fewer copies from hoisted metadata lookup. All variants pass bytes, unconditional function offsets and sorted relocations at 1/8 threads and constrained-team/fallback checks. Five interleaved 128-repetition measurements underway; preprocessing separately timed, logical prepared-input footprint counted.
107:2026-11-16T18:31:29Z CP7 measurement: 55/55 oracle passes, five interleaved P4_REPS=128 runs, 1/8 threads, 12 GiB, load 2.53–4.85. Prepared unmerged D/1 48.474 ms vs p4par/1 94.695; A/1 43.981, B/1 44.689, C/1 46.589 ms. B has 17.43× fewer copies than D but only 1.08× lower time; fusion benefits at eight threads overlap noise. C stores 55,389 distinct runs / 48,985,021 bytes. Report, raw logs, reproducible summary and independent UBSan/negative-offset review committed; no new grid cell or ATTACKS entry, entry 4 refined.
118:- 2026-11-16T18:51Z M4 review triaged (GRID Goose P4 join cost, Halide CPU-time label, ATTACKS 3 Futhark worklist); builders: Soufflé P4, Halide batched flood; Codex: hole cost (codex-holes), Goose P4 offset check (codex-goose-fnoff).
121:- 2026-11-16T18:58Z (Codex, codex-goose-fnoff) Goose P4 now checks all computed function offsets, including the sentinel, in file and queue joins outside timed reps; MinIO 1/8 workers pass both modes, and a temporary +1 offset fault fails both. Commands/output: results/codex-goose-fnoff.md.
124:- (Codex, codex-holes) 2026-11-16T19:08Z codex-holes measured: five rotated rounds, 256 repetitions, 1/8 threads pinned, 50 oracle passes, kernel runs 3.975–56.799 s at load 3.43–13.35. One-core medians B 44.495 ms, H1 28.757 ms, H2 41.275 ms; H1 removes 89% of patch stores but only 35% of time, H2 overlaps B. Checked H0 retains traversal without integer stores (41.559 ms); perf supports metadata/dispatch cost rather than scalar-store latency. Preparation reported separately; eight-core H1/B overlap. Final validation also closes p4par's conditional function-offset check. Results and reproduction: results/codex-holes.md; ATTACKS 4 updated. No push per user instruction.
144:- 2026-11-16T19:54Z Halide P4 on the flattened stream merged (results/halide-p4flat.md): D emit 42 / 15 ms (3.8x / 1.5x its template emit), H1 13-16 / 6 ms; lead rebuilt + reran p4f all on MinIO and kube, 8 threads: P4 OK every checked variant (D 43.9 / 15.0, H1 16.0 median). Caveat: C's timed emit also collects relocations run by run; Halide copies prepared relocation columns.

codex
1. **No unchecked-output cell found in the focused scope.** C’s maintained flood checks the full live set, detecting missing and extra IDs after **every update in both directions** ([p2inc.c:437](/home/exedev/paradigms-w/ref/flat/p2inc.c:437), [calls:636](/home/exedev/paradigms-w/ref/flat/p2inc.c:636)). I ran the existing binary on both edits, five round trips each: **P2INC OK**; edit 2 with `FUZZ=50`: **0 failures**. DD also checks full membership, but **only repetition 0**, not every timed update ([main.rs:335](/home/exedev/paradigms-w/systems/ddflow/src/main.rs:335)); my edit-2 run passed. The fuzzer’s limitation: it recomputes using the same flood implementation, then reverses from that recomputed state—not the saved incremental state ([p2inc.c:679](/home/exedev/paradigms-w/ref/flat/p2inc.c:679)). It does not establish correctness across arbitrary maintained edit sequences.

2. **DD’s broad “retractions lose / eight workers get worse” verdict is premature.** [ATTACKS.md:232](/home/exedev/paradigms-w/ATTACKS.md:232) still foregrounds **555 ms**, although the quiet measurement is **29 ms**, versus **59 ms** on one worker. That loses to eight-thread C’s **14.2 ms**, but beats serial C’s **42.6 ms**; “slower than C” needs the comparator specified. The **9.5–16.6 ms** C-maintenance and **13.0–18.3 ms** parallel-recompute ranges overlap, so the claimed **1.5×** advantage does not meet the charter’s nonoverlap rule ([results:75](/home/exedev/paradigms-w/results/c-incremental.md:75)).

3. **3 µs versus 43–45 ms is maintenance versus fresh computation, not equivalent starting conditions.** Both produce the same reach answer; both exclude oracle checking. But C maintenance times only `update`, with indexes, previous reach and stored diff ready. Serial `p2` includes allocations and initialization ([p2.c:46](/home/exedev/paradigms-w/ref/flat/p2.c:46)). Maintenance excludes **365 ms setup**, **60–90 ms initial flood**, and **0.7–1.1 s identity recovery**, plus diff preparation. Its first edit-1 update is **38 µs median**, versus **3 µs** on repeats ([results:29](/home/exedev/paradigms-w/results/c-incremental.md:29), [table:74](/home/exedev/paradigms-w/results/c-incremental.md:74)). Also, the benchmark recomputes **B only**: using that baseline for **B→A** compares different destination graphs ([bench-p2inc.sh](/home/exedev/paradigms-w/ref/flat/bench-p2inc.sh)).

4. **The unexplored surprise is history-dependent work, not just cache warming.** The report attributes the first-update difference to first-touch pages ([results:69](/home/exedev/paradigms-w/results/c-incremental.md:69)). My edit-2 run changed from **32,238 retractions / 3,049,918 reverse reads** initially to **20,412 / ~2,253,000** on repeats. The algorithm does less work after a round trip. Measure cache warming separately from stamp/support-history changes before presenting repeated-toggle timing as representative of a fresh edit. Edit 1 also adds no `http.ParseTime` call edge: it was inlined, and the live change is merely **one GC-locals symbol replaced by two** ([DD results:27](/home/exedev/paradigms-w/results/ddflow-incremental.md:27)).

5. **Entry 5 has measured reach maintenance, but not a measured incremental relink.** Stable-ID extraction eliminating identity-recovery cost, maintained C resolve, 1c, and layout/relocation remain unmeasured ([ATTACKS.md:250](/home/exedev/paradigms-w/ATTACKS.md:250)). Measure complete edit sequences from changed objects to checked output, including identity/diff construction, all maintained phases, memory, and fallback costs. Include method-table changes: C currently does not support them ([p2inc.c:31](/home/exedev/paradigms-w/ref/flat/p2inc.c:31)).

6. **No qualifying JOURNAL passage found.** The recent entries do not decline or shrink work because of time, usage, or session ending; there is no such passage to quote.

No files edited; checks used existing binaries, without rebuilding.
tokens used
71,769
1. **No unchecked-output cell found in the focused scope.** C’s maintained flood checks the full live set, detecting missing and extra IDs after **every update in both directions** ([p2inc.c:437](/home/exedev/paradigms-w/ref/flat/p2inc.c:437), [calls:636](/home/exedev/paradigms-w/ref/flat/p2inc.c:636)). I ran the existing binary on both edits, five round trips each: **P2INC OK**; edit 2 with `FUZZ=50`: **0 failures**. DD also checks full membership, but **only repetition 0**, not every timed update ([main.rs:335](/home/exedev/paradigms-w/systems/ddflow/src/main.rs:335)); my edit-2 run passed. The fuzzer’s limitation: it recomputes using the same flood implementation, then reverses from that recomputed state—not the saved incremental state ([p2inc.c:679](/home/exedev/paradigms-w/ref/flat/p2inc.c:679)). It does not establish correctness across arbitrary maintained edit sequences.

2. **DD’s broad “retractions lose / eight workers get worse” verdict is premature.** [ATTACKS.md:232](/home/exedev/paradigms-w/ATTACKS.md:232) still foregrounds **555 ms**, although the quiet measurement is **29 ms**, versus **59 ms** on one worker. That loses to eight-thread C’s **14.2 ms**, but beats serial C’s **42.6 ms**; “slower than C” needs the comparator specified. The **9.5–16.6 ms** C-maintenance and **13.0–18.3 ms** parallel-recompute ranges overlap, so the claimed **1.5×** advantage does not meet the charter’s nonoverlap rule ([results:75](/home/exedev/paradigms-w/results/c-incremental.md:75)).

3. **3 µs versus 43–45 ms is maintenance versus fresh computation, not equivalent starting conditions.** Both produce the same reach answer; both exclude oracle checking. But C maintenance times only `update`, with indexes, previous reach and stored diff ready. Serial `p2` includes allocations and initialization ([p2.c:46](/home/exedev/paradigms-w/ref/flat/p2.c:46)). Maintenance excludes **365 ms setup**, **60–90 ms initial flood**, and **0.7–1.1 s identity recovery**, plus diff preparation. Its first edit-1 update is **38 µs median**, versus **3 µs** on repeats ([results:29](/home/exedev/paradigms-w/results/c-incremental.md:29), [table:74](/home/exedev/paradigms-w/results/c-incremental.md:74)). Also, the benchmark recomputes **B only**: using that baseline for **B→A** compares different destination graphs ([bench-p2inc.sh](/home/exedev/paradigms-w/ref/flat/bench-p2inc.sh)).

4. **The unexplored surprise is history-dependent work, not just cache warming.** The report attributes the first-update difference to first-touch pages ([results:69](/home/exedev/paradigms-w/results/c-incremental.md:69)). My edit-2 run changed from **32,238 retractions / 3,049,918 reverse reads** initially to **20,412 / ~2,253,000** on repeats. The algorithm does less work after a round trip. Measure cache warming separately from stamp/support-history changes before presenting repeated-toggle timing as representative of a fresh edit. Edit 1 also adds no `http.ParseTime` call edge: it was inlined, and the live change is merely **one GC-locals symbol replaced by two** ([DD results:27](/home/exedev/paradigms-w/results/ddflow-incremental.md:27)).

5. **Entry 5 has measured reach maintenance, but not a measured incremental relink.** Stable-ID extraction eliminating identity-recovery cost, maintained C resolve, 1c, and layout/relocation remain unmeasured ([ATTACKS.md:250](/home/exedev/paradigms-w/ATTACKS.md:250)). Measure complete edit sequences from changed objects to checked output, including identity/diff construction, all maintained phases, memory, and fallback costs. Include method-table changes: C currently does not support them ([p2inc.c:31](/home/exedev/paradigms-w/ref/flat/p2inc.c:31)).

6. **No qualifying JOURNAL passage found.** The recent entries do not decline or shrink work because of time, usage, or session ending; there is no such passage to quote.

No files edited; checks used existing binaries, without rebuilding.
EXIT 0
