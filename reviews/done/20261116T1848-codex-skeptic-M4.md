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
session id: 01a1e5b0-6849-7830-bf82-28a7587a4e41
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
I’ll read the review files, trace the claims to their checks and measurements, and run the relevant oracle if the read-only environment permits it.
exec
/bin/bash -lc "pwd; rg --files -g 'CHARTER.md' -g 'PLAN.md' -g 'STATUS.md' -g 'GRID.md' -g 'ATTACKS.md' -g 'JOURNAL.md' -g 'CLAUDE.md' -g 'README*' -g 'AGENTS.md'" in /home/exedev/paradigms-w
 succeeded in 0ms:
/home/exedev/paradigms-w
PLAN.md
ref/p4/README.md
JOURNAL.md
STATUS.md
README.md
GRID.md
CHARTER.md
ATTACKS.md
ref/flat/README.md

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

Now (2026-11-16T18:55Z): M4 done; M4 skeptic fired (reviews/inbox/*-M4.md: triage into REVIEWS.md). No builders
running. Next, in order: (1) Soufflé P4 (an Opus builder in a worktree; brief = ideas/BUILDER.md + ref/p4/README.md +
ATTACKS 4; Datalog has no bytes, so the rules compute positions and patch values and a C writer stores bytes, as its
3b did: that is a finding either way) -> 20/20. (2) A Halide batched-level or async flood vs its 100 pipeline calls
(M3 review 5). (3) The hole cost (ATTACKS 4): after flattening, 8 M holes ~46 ms: test fewer holes per byte (merge
adjacent holes of one operand, or holes whose value repeats within a function) as a Codex C job. (4) Quiet reruns of
the 8-thread grid numbers while nothing else runs (M3 numbers were at load 7-26): systems/*/bench.sh one at a time.
(5) A Halide cell on the flattened stream (Futhark gained 4.5x).
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
| 4. codegen by shape (stretch) | **P4 OK** (lead rebuilt + reran 1/8 workers 18:47Z). Workers own ranges of whole functions, no messages while computing: layout+emit 97.1/98.3 (1) / **17.6/21.3 (8)** vs p4.c 105.6 / p4par 17.1, within noise; same 698 M instructions. The join of the parts is the cost: 120-260 ms through queues, 37-63 ms writing each range into the file. 482 lines + 40 C | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). **4D** (flattened stream, ATTACKS 4; lead reran P4D OK both backends 18:49Z): emit 65.1/67.8 (1) / **23.4/26.9 (8)** vs 4bf 284/303 / 95/113 same window, 4.5x / 4.2x; the flattening (`prepare`, 50 lines) 324 / 138 ms, so it pays only if the stream is reused | **P4 OK** (lead rebuilt + reran 1/8 18:46Z). Emit **157-162 (1) / 23.0-25.1 (8)** vs p4.c 103-112 / p4par 16.4-20.1 (load 4): 1.5x / 1.3x; layout 18 / 5 (+ template index 9.5 / 7). Relocations by a scatter to the layout's own slot ranges (no C host). Forced: entry-0 indices on untaken select sides (Halide loads both: 200 -> 160 ms); the vector copy only as 32-byte read-modify-write in parity passes (obvious form 26 s). 345 + 288 lines | not attempted | not attempted |

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
dropping level sync pays where waiting on a descheduled worker is the cost (C under load, Goose), not where each
level is many whole-array passes (Futhark); there the fix is fewer passes per level, which no variant here achieved.

**What it says about the paradigms.** None of the grid's data-parallel systems can say "a fixpoint without levels":
Futhark and Halide have no worklist; Soufflé's semi-naive evaluation is level-synchronous by construction; DuckDB's
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

exec
/bin/bash -lc 'cat README.md LOOP.md; cat GRID.md' in /home/exedev/paradigms-w
 succeeded in 0ms:
# paradigms

Paradigms that run at hardware speed elsewhere (data-parallel, dataflow, memory models,
algorithm/schedule splits), set against a linker's and a code generator's work on real Go
programs. CHARTER.md is the goal, PLAN.md the campaign, LOOP.md the procedure, GRID.md the
scoreboard, ATTACKS.md what was learned.

Run: `./overnight.sh [LIMIT]` inside tmux (or Start run in vm-web). `touch STOP` to stop.
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
| 4. codegen by shape (stretch) | **P4 OK** (lead rebuilt + reran 1/8 workers 18:47Z). Workers own ranges of whole functions, no messages while computing: layout+emit 97.1/98.3 (1) / **17.6/21.3 (8)** vs p4.c 105.6 / p4par 17.1, within noise; same 698 M instructions. The join of the parts is the cost: 120-260 ms through queues, 37-63 ms writing each range into the file. 482 lines + 40 C | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). **4D** (flattened stream, ATTACKS 4; lead reran P4D OK both backends 18:49Z): emit 65.1/67.8 (1) / **23.4/26.9 (8)** vs 4bf 284/303 / 95/113 same window, 4.5x / 4.2x; the flattening (`prepare`, 50 lines) 324 / 138 ms, so it pays only if the stream is reused | **P4 OK** (lead rebuilt + reran 1/8 18:46Z). Emit **157-162 (1) / 23.0-25.1 (8)** vs p4.c 103-112 / p4par 16.4-20.1 (load 4): 1.5x / 1.3x; layout 18 / 5 (+ template index 9.5 / 7). Relocations by a scatter to the layout's own slot ranges (no C host). Forced: entry-0 indices on untaken select sides (Halide loads both: 200 -> 160 ms); the vector copy only as 32-byte read-modify-write in parity passes (obvious form 26 s). 345 + 288 lines | not attempted | not attempted |

Halide column merged 17:17Z (lead rebuilt every generator and driver in a copy; P1/P2/P3 OK; `systems/halide/CELLS.md`).
DuckDB column merged 17:13Z (lead reran every check in a copy, pipeline included; `systems/duckdb/CELLS.md`).
Soufflé column merged 16:58Z (lead rebuilt all 8 programs from source; every check OK; `systems/souffle/CELLS.md`).
Futhark column merged 16:42Z (lead rebuilt from source; every cell OK; `systems/futhark/CELLS.md`). Surprise: `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, no code change, makes most Futhark cells 1.2-1.7x faster.
Goose column merged 16:35Z; lead reran every check (P1/P2/P3/pipeline OK, a source rebuild of p2par passes). Long form and lines: `systems/goose/CELLS.md`, `NOTES.md`.

exec
/bin/bash -lc "cat ATTACKS.md; rg -n -i 'session|time left|usage|budget|defer|remaining|ending|time limit' JOURNAL.md; rg --files systems reviews results | head -n 160" in /home/exedev/paradigms-w
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
dropping level sync pays where waiting on a descheduled worker is the cost (C under load, Goose), not where each
level is many whole-array passes (Futhark); there the fix is fewer passes per level, which no variant here achieved.

**What it says about the paradigms.** None of the grid's data-parallel systems can say "a fixpoint without levels":
Futhark and Halide have no worklist; Soufflé's semi-naive evaluation is level-synchronous by construction; DuckDB's
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
100:but slower: the three remaining cells are all problem 4 (Goose, Halide, Soufflé); Halide is building, Goose goes next.
systems/futhark/drv.h
systems/futhark/fbin.py
systems/futhark/p1.c
systems/futhark/IDEAS.md
systems/futhark/prof.py
systems/futhark/p4-flat-bench-2026-11-16T1840Z.txt
systems/futhark/p3b-blocks-bench-2026-11-16T1824Z.txt
systems/futhark/msort.fut
systems/futhark/p1.fut
systems/futhark/p3.fut
systems/futhark/hello/hello.fut
systems/futhark/hello/gen.c
systems/futhark/hello/driver.c
systems/futhark/hello/run.sh
systems/futhark/hello/build.sh
systems/futhark/P4-REPORT.md
systems/futhark/CELLS.md
systems/futhark/bench3b.sh
systems/futhark/p2.c
systems/futhark/build.sh
systems/futhark/bench4D.sh
systems/futhark/NOTES.md
systems/futhark/p4.c
systems/futhark/bench.sh
systems/futhark/p3.c
systems/futhark/p4.fut
systems/futhark/levelfree.sh
systems/futhark/p2.fut
systems/futhark/lib/sorts/merge_sort.fut
systems/futhark/lib/sorts/radix_sort.fut
systems/futhark/lib/segmented/segmented.fut
systems/futhark/p4-bench-2026-11-16T1800Z.txt
results/summarize-cp.py
results/codex-kube-halide-build.log
results/minio.stats.txt
results/levels-kube.txt
results/second-attempts.md
results/codex-kube-souffle-build.log
results/codex-f-bytes.py
results/cp4-check-8.log
results/codex-kube-goose-report.py
results/codex-i-runs.json
results/codex-h.md
results/levels-minio.txt
results/codex-kube-duckdb-review.md
results/codex-kube-c-raw.log
results/codex-p4par-runs.log
results/cp7-review-check.sh
results/cp1check.txt
results/cp4-final-compile.log
results/codex-kube-c-report.py
results/minio.hlink.huge.tsv
results/codex-kube-halide-check.log
results/codex-g-check.log
results/codex-kube-souffle-raw.txt
results/cp4-generation.log
results/codex-kube-souffle-table.md
results/codex-p4par-final-check.log
results/codex-g-bench.jsonl
results/codex-kube-fh.md
results/codex-p4par-review.md
results/codex-kube-duckdb-run.sh
results/codex-kube-c.md
results/codex-kube-sql.md
results/codex-kube-c-round.sh
results/codex-i-profile.md
results/codex-g-bench.py
results/codex-kube-souffle-bench.txt
results/codex-kube-halide-bench.log
results/codex-kube-futhark-bench.log
results/codex-kube-halide-review.md
results/codex-kube-futhark-review.md
results/codex-g-review.md
results/relkinds.txt
results/codex-kube-goose-bench.log
results/cp4-implementation.md
results/codex-cp-review-cp1.log
results/codex-cp7-check.log
results/codex-c.md
results/codex-kube-fh-run.sh
results/codex-p4par-check.log
results/codex-cp-review-negative.log
results/minio.hlink.tsv
results/codex-f.md
results/summarize-cp7.py
results/codex-d.md
results/codex-g.md
results/codex-kube-duckdb-build.log
results/codex-cp-review-cp4.log
results/codex-kube-duckdb-raw.txt
results/codex-cp7-runs.log
results/codex-kube-goose.md
results/codex-kube-duckdb-huge-check.txt
results/codex-kube-goose-build.log
results/codex-kube-duckdb-table.md
results/codex-h-onlyB-notes.md
results/codex-cp-review-synthetic.log
results/codex-cp1-check.log
results/codex-cp.md
systems/goose/p1.build
systems/goose/flatio.c
systems/goose/IDEAS.md
systems/goose/p4.build
systems/goose/p2async.build
systems/goose/p2async.goose
systems/goose/p1par.build
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
systems/souffle/best.sh
systems/souffle/resolve_c2.dl
systems/souffle/check_p3.c
results/codex-kube-fh-report.py
results/codex-kube-goose-huge-check.log
results/relkinds.py
results/codex-cp7.md
results/codex-p4par.md
results/codex-d-runs.txt
results/codex-kube-goose-run.sh
results/codex-kube.md
systems/halide/IDEAS.md
results/codex-kube-duckdb-console.log
results/codex-f-bench.py
results/codex-cp-runs.log
results/codex-g-validation.log

exec
/bin/bash -lc 'ls reviews/inbox; cat REVIEWS.md; cat systems/halide/p4*cpp; cat systems/goose/bench-p4.sh' in /home/exedev/paradigms-w
 succeeded in 0ms:
# REVIEWS (one line per inbox item: accept / reject (why) / defer)
- 20261116T1534-codex-skeptic: **accept** contract fixes (verified 2026-11-16T16:00Z: name table is off[g-1]..off[g]; 807,866 mark-1 slots not in p2_live; rank bit 30 only for non-DUPOK; zero-id guard; p3_addr_all holds 3a's answer) -> README now says each exactly, and that a 3a cell must not read p3_addr_all. **accept** "P3 C ref does not test layout->relocate as a pipeline" -> add a pipeline mode to p3.c (Now line). **accept** "decode-not-walk is untested" -> ATTACKS entry 1 waits on a same-implementation decode+flood measurement (Now line). **accept** the 1c content-hash join, 310-380 ms flat vs 37-39 ms hlink, is unexplained: the flat C ref is untuned (400 MB table); p1par covers it? check. No cell counted yet, so nothing blocked.
- 20261116T1603-codex-attack1 (skeptic on ATTACKS 1): **accept** "2x holds for the kernels, not end to end" and **accept** "the hlink comparison is not equivalent work in either direction" -> ATTACKS 1 now claims only lazy vs eager (2x, Codex reproduced 2.1-2.6x), hlink ratio withdrawn. **accept** the duplicate count mixes namespaces -> (kind, target) numbers in. **accept** "the timed decoder is not checked" -> add a per-source hash of edges decoded inside the timed flood, compared with the setup decode (after codex-e lands its p2lazy aux change; Now line). **accept** the data races (plain reads of live/iface next to atomic writes, plain same-value writes to sigSeen/named in p2par/p2pipe/p2lazy) -> relaxed atomics (Now line; no wrong answer observed, but it is UB). **note** reachfacts counts live-row occupancy, not edge visits: the 28.7% is a bound on the edges read, stated as such. **Done 16:27Z**: p2lazy checks the timed decoder (an untimed extra rep stores a per-source mix64 sum of decoded targets, compared with sums from the oracle-checked targets and dumpflat's aux edges; 0 wrong on both datasets, both modes; two planted decoder faults caught: 1288 and 462 wrong sources); relaxed atomics in p2par, p2pipe, p2lazy.
- 20261116T1713-codex-skeptic (M2): 1 no unchecked cell (it reran p3 --pipeline: OK) -> noted. 2 **accept** DuckDB P2 "falls down" premature (both attempts kept 45 rounds x 25 statements, tables rebuilt per round) -> GRID says "slow as implemented, third attempt owed" (persistent index / one recursive query; M3). 3 **accept** Futhark lazy (165) vs eager (97) left out eager's own decode (47 + CSR) -> GRID/MORNING: a tie at best. 4 **accept** Soufflé's lazy win understated (2.9-3.2 vs ~6.5 s 1 thread; 960 vs ~1.6 s on 8) -> GRID fixed. 5 **accept** ATTACKS 3's claim beyond C is unmeasured -> scope paragraph added; batched-level Futhark/Goose test owed in M3. 6 none found.

## M3 skeptic (reviews/done/20261116T1821-codex-skeptic-M3.md), triaged 2026-11-16T18:40Z
1. Halide 3b fresh-output images unchecked -> **accept, fixed**: p3.cpp now checks one untimed fresh image per page size against the oracle-checked buffer; all 8 equal, P3 OK (18:39Z).
2. ATTACKS 3 still called Futhark lazy a falls-down -> **accept, fixed** (text corrected: it ties eager once eager's decode is counted).
3. Goose P1 / Futhark 3b warm numbers reuse mapped pages vs C's first touch -> **accept**: caveat in GRID; e2e numbers carry the faults.
4. Halide 1a kube 6.7x uninvestigated -> **accept, measured**: interleaved at load 2.6, 2.6x for 2.55x the slots (linear); the 6.7x was the load, and GRID's "1.4x ids" was wrong (fixed).
5. ATTACKS 3's general claim contradicted by Futhark -> **accept**: retitled to its measured scope; Halide/DuckDB batched variants not run (DuckDB's rounds already measured in its third attempt; Halide owed: Now line).
6. No work-avoidance passage -> nothing to do.
cat: 'systems/halide/p4*cpp': No such file or directory
#!/usr/bin/env bash
# systems/goose/bench-p4.sh [rounds=5] [data dir]: problem 4, Goose interleaved with the C references (ref/p4/p4.c,
# p4par.c). Builds everything, then per round runs each variant once and records its numbers; prints one line per
# (cell, cores, kind): min / median / max over the rounds, plus the load seen.
#   warm: in-process reps (P4_REPS=20 after one warm-up); value = min rep of layout + emit (C: its "lay out" +
#         "emit"; Goose: the slowest worker's layout + emit). "goose-wall" is main's go-to-last-done per rep.
#   e2e:  wall of one process, one rep: start, read the flat files, compute, output, self-check against the oracle
#         (C sorts its relocations with qsort; Goose reads its output files back and sorts with std sort).
#         "e2e-nocheck": Goose without the check (its output is the two files written by the workers).
#   K variants: P4_K=k keeps the commonest k templates, the rest becomes glue runs (made at load, untimed).
set -u
cd "$(dirname "$0")"
R=${1:-5}
D=${2:-/home/exedev/paradigms/data/minio-p4}
G=${GOOSE:-/home/exedev/tools/goose/rel/goose-v2026.4-linux-x64/goose}
B=../../build
mkdir -p bin "$B"
$G build p4.goose -o bin/p4 >/dev/null 2>&1 || { echo "build p4 failed"; exit 1; }
gcc -O2 -march=native -o "$B/p4" ../../ref/p4/p4.c || exit 1
gcc -O2 -march=native -fopenmp -o "$B/p4par" ../../ref/p4/p4par.c || exit 1
export OMP_WAIT_POLICY=passive
run() { systemd-run --user --scope -q -p MemoryMax=10G "$@"; }     # Goose reserves 2 GB/stack: no prlimit --as
out=$(mktemp)
wall() { local t0 t1; t0=$(date +%s%N); "$@" >/dev/null 2>&1 || echo "FAIL $*" >&2; t1=$(date +%s%N); echo "scale=1; ($t1-$t0)/1000000" | bc; }
cw() { awk '{for(i=1;i<=NF;i++){if($i=="out"){l=$(i+1)} if($i=="emit"){e=$(i+1)}}} END{printf "%.1f\n", l+e}'; }    # C: lay out + emit
gw() { tr ' ' '\n' | awk '/^layout$/{getline; split($0,a,"/"); l=a[1]} /^emit$/{getline; split($0,a,"/"); e=a[1]} END{printf "%.1f\n", l+e}'; }
gwall() { grep -o 'rep wall [0-9.]*' | awk '{print $3}'; }
echo "goose p4 bench: $R rounds, $(date -u +%FT%TZ), load $(cut -d' ' -f1-3 /proc/loadavg)"
for r in $(seq "$R"); do
  l=$(cut -d' ' -f1 /proc/loadavg); echo "round $r $(date -u +%T) load $l" >&2; echo "load - - $l" >>"$out"
  echo "c-p4 1 warm $(P4_REPS=20 "$B/p4" "$D" | cw)" >>"$out"
  x=$(P4_REPS=20 run bin/p4 "$D" 1); echo "goose 1 warm $(echo "$x" | gw)" >>"$out"; echo "goose-wall 1 warm $(echo "$x" | gwall)" >>"$out"
  echo "c-p4par 8 warm $(OMP_NUM_THREADS=8 P4_REPS=20 "$B/p4par" "$D" | cw)" >>"$out"
  x=$(P4_REPS=20 run bin/p4 "$D" 8); echo "goose 8 warm $(echo "$x" | gw)" >>"$out"; echo "goose-wall 8 warm $(echo "$x" | gwall)" >>"$out"
  echo "c-p4par 1 warm $(OMP_NUM_THREADS=1 P4_REPS=20 "$B/p4par" "$D" | cw)" >>"$out"
  for k in 0 256 4096; do
    for w in 1 8; do echo "goose-K$k $w warm $(P4_K=$k P4_REPS=20 run bin/p4 "$D" $w | gw)" >>"$out"; done
  done
  echo "c-p4 1 e2e $(wall "$B/p4" "$D")" >>"$out"
  echo "goose 1 e2e $(wall run bin/p4 "$D" 1)" >>"$out"
  echo "goose 1 e2e-nocheck $(P4_NOCHECK=1 wall run bin/p4 "$D" 1)" >>"$out"
  echo "c-p4par 8 e2e $(OMP_NUM_THREADS=8 wall "$B/p4par" "$D")" >>"$out"
  echo "goose 8 e2e $(wall run bin/p4 "$D" 8)" >>"$out"
  echo "goose 8 e2e-nocheck $(P4_NOCHECK=1 wall run bin/p4 "$D" 8)" >>"$out"
done
echo "cell cores kind   min / median / max ms"
awk '{k=$1" "$2" "$3; v[k]=v[k]" "$4; if(!(k in seen)){seen[k]=1; order[++n]=k}}
  END{for(i=1;i<=n;i++){k=order[i]; m=split(v[k],a," "); for(x=1;x<=m;x++)for(y=x+1;y<=m;y++)if(a[y]+0<a[x]+0){t=a[x];a[x]=a[y];a[y]=t}
      printf "%-22s %s / %s / %s\n", k, a[1], a[int((m+1)/2)], a[m]}}' "$out"
rm -f "$out" /tmp/goose_p4_code.bin /tmp/goose_p4_rel.bin

exec
/bin/bash -lc "rg -n 'check|oracle|memcmp|P4 OK|P3 OK' systems/goose/flatio.c systems/halide/*.cpp systems/futhark/p4.c; cat results/second-attempts.md; cat systems/goose/CELLS.md" in /home/exedev/paradigms-w
 succeeded in 0ms:
rg: systems/halide/*.cpp: No such file or directory (os error 2)
systems/futhark/p4.c:1:// p4.c: driver for p4.fut. Loads the flat files, calls the entry, times it, checks against the oracles
systems/futhark/p4.c:3://   p4 DATADIR THREADS REPS CELL [nocheck]     CELL (p4.fut's `how`):
systems/futhark/p4.c:12://   PARTS=n (bits: 1 copy, 2 patch, 4 relocations; default 7) switches pieces off for timing (no check)
systems/futhark/p4.c:39:	int check = !(argc > 5 && strcmp(argv[5], "nocheck") == 0);
systems/futhark/p4.c:46:	if ((parts & 7) != 7) check = 0;
systems/futhark/p4.c:78:			fut_check(futhark_entry_layout(ctx, &at, item_kind, item_ref, item_seg, glue_off, node_tmpl, tmpl_off, tmpl_seg_off, tseg_end), cell);
systems/futhark/p4.c:90:			fut_check(futhark_entry_prepare(ctx, &st, fn_item, fn_mark_off, mark_off, mark_type, mark_sym, mark_add, item_kind,
systems/futhark/p4.c:99:			fut_check(futhark_entry_emit_flat(ctx, &out, st), cell);
systems/futhark/p4.c:104:		fut_check(futhark_entry_emit(ctx, &out, parts, how, chunk, fn_item, fn_mark_off, mark_off, mark_type, mark_sym, mark_add, item_kind,
systems/futhark/p4.c:127:	if (!check) return 0;
systems/futhark/p4.c:131:	const u8 *want = FMAP(u8, "oracle/p4_code.bin", &m);
systems/futhark/p4.c:132:	const u64 *wfn = FMAP(u64, "oracle/p4_fn_off.u64", NULL);
systems/futhark/p4.c:133:	if ((u64)pos != m || (u64)pos != ncode || memcmp(code, want, m) != 0) printf("code differs\n"), bad++;
systems/futhark/p4.c:134:	if (memcmp(fnoff, wfn, 8 * ((size_t)nfn + 1)) != 0) printf("function offsets differ\n"), bad++;
systems/futhark/p4.c:135:	printf("code: %lld bytes in %u functions, %s oracle/p4_code.bin\n", (long long)pos, nfn, bad ? "NOT equal to" : "equal to");
systems/futhark/p4.c:143:	const u64 *wo = FMAP(u64, "oracle/p4_rel_off.u64", NULL);
systems/futhark/p4.c:144:	const u8 *wsiz = FMAP(u8, "oracle/p4_rel_siz.u8", NULL);
systems/futhark/p4.c:145:	const u16 *wtype = FMAP(u16, "oracle/p4_rel_type.u16", NULL);
systems/futhark/p4.c:146:	const u32 *wsym = FMAP(u32, "oracle/p4_rel_sym.u32", NULL);
systems/futhark/p4.c:147:	const i64 *wadd = FMAP(i64, "oracle/p4_rel_add.i64", NULL);
systems/futhark/p4.c:154:	printf("relocations: %lld, oracle %u, %ld differ\n", (long long)nr, nrel, rbad);
# Second attempts owed by the M2 review (REVIEWS.md 20261116T1713): DuckDB P2, Soufflé 3a

Branch `worktree-agent-aa77583b9c2460e4b`. Both new cells match their oracles exactly. Times are ms,
min-median-max over 5 interleaved runs, warm unless marked e2e. The machine was shared (load 6-22 during the work,
noted per bench).

## Soufflé 3a (lay out): attempt 3, `systems/souffle/layout_rank.dl`. No longer falls down

Check: `build/layout_rank -j8 -F $D -D out && build/check_p3 $D $D/oracle/p3_image.bin out/p3_addr.u32` gives P3 OK
(0 of 360,315 addresses differ). Rerun: `ONLY='3a|3a-rank' systems/souffle/bench.sh 5`.

Shape: the typelink order with no sort, no pair counting and no C functor.
- The loader shows each type string as big-endian 8-byte words (`words8="p3_str"`, like the existing `key8`: bytes as
  numbers, no comparison done in C).
- A string's prefix of c words is a Soufflé record `[prefix, word]`. The record table interns records, so equal
  prefixes are the same value, and a prefix can be a group key (`node`, 164 K records).
- rank(s) = the sum, over the prefix groups s shares with other strings, of the members whose next word is smaller.
  A string that ends inside a group has next word 0, so shorter strings sort first.
- "Members with a smaller word" uses a radix-256 trie of group-by counts. For each byte L of the word, it counts the
  members that agree above L and have a smaller byte at L. Each sum ranges over at most 255 distinct bytes, never
  over members. That is 871 K tuples, against about 10^9 pair visits before.
- A typelink's address = the sizes of the typelinks with a smaller rank (the same trie over the rank's bytes), plus
  the typelinks with the same string and a smaller id (a range of an index).

| 17:56Z, load 11-13 | 1 thread warm | 1 thread e2e | 8 threads warm | 8 threads e2e |
|---|---|---|---|---|
| attempt 3 (layout_rank) | 1712-1930-3086 | 1893-2061-3409 | **1725-1937-3541** | 1922-2128-3790 |
| attempt 2 (layout), same window | 7647-8270-9097 | 7800-8427-9310 | 2860-3388-7055 | 3006-3604-7302 |

What this shows:
- The typelink part dropped from ~95% of the cell (~7 s on 1 thread) to ~1.1 s on 1 thread and ~0.65 s on 8
  (souffleprof).
- The cell is still ~100x the plain C (8-19 ms) and ~400x the tuned C (p3a_par 3.9-5.0 ms). The sort stays in
  Datalog. Verdict: **slow, not falls down.**
- 8 threads do not help. The 1,763-iteration unit-walk recursion costs ~0.6 s of per-iteration overhead at -j8.

Traps found on the way:
1. A binary 64-level trie produced 7 M tuples and lost to the old O(n²) version on 8 threads.
2. An expression inside an aggregate's body atom, `cnt(p, j, (w bshr j) - 1, k)`, makes Soufflé materialize the body
   with only a prefix of the key bound. The run went past 100 s, so every key is now spelled out as its own relation.
3. `tl(_, _, _, _, c2), c2 != c` in `tlcheck` scanned all of tl for each typelink (also present in attempt 2). It now
   goes through a class relation.

## DuckDB P2 (reach): attempt 3, `systems/duckdb/sql/p2rec.sql`. Falls down

Check: `./dk $D 8 sql/load2.sql sql/p2rec.sql && cmp out/p2_live.u32 $D/oracle/p2_live.u32` matches (663,630 live).
Rerun: `ONLY=p2 systems/duckdb/bench.sh`.

Shape: the C reference's outer loop, 7 method rounds. Each round runs two WITH RECURSIVE floods: the plain closure,
and the used-in-interface closure over gotype->gotype edges plus type->method type. Between floods, the
sig/named/reflect sets and the callable methods are computed. This replaces 45 BFS rounds x ~25 statements.

Tuning, each step measured:
- Edges deduplicated (8.15 M -> 5.18 M) and split by target into inner edges (2.73 M, walked by the recursion) and
  leaf edges (added by one join after it).
- The working edge sets are pruned as they are consumed, so no recursion anti-joins against the growing live set.
  The first version did, rebuilding live's hash table every iteration, and took 3.7 s.
- Reflect is tested through a small relation instead of a join with `sym`.
- Seeds are not re-filtered against live.
- Sorting the edges did not help (within noise), so it was dropped.

| 18:00Z, load 6.6-10.9 | 8 thr prep + flood | 8 thr e2e | 1 thr prep + flood | 1 thr e2e |
|---|---|---|---|---|
| attempt 3 (p2rec) | 577-643-871 + **1438-1666-2234** | 2168-2336-3401 | 2058-2104-2220 + 2761-2821-3134 | 5060-5249-7048 |
| attempt 2 (p2loop), same window | 112-128-175 + 2362-2698-3159 | 2502-2709-3104 | 201-221-250 + 2879-3375-3804 | 3708-4503-5067 |

On 8 threads the flood is 1.6x faster (the ranges do not overlap) and the whole cell is ~1.2x faster (2.3 vs 2.8 s,
~140x the tuned C). On 1 thread the cell is slower because the prep grows to 2.1 s.

Verdict: **falls down** after three attempts. Both reasons are measured:
1. **Every iteration of a recursive CTE re-runs its join with the whole edge table as the probe side.** It scans
   2.7 M rows however small the frontier is. Round 1 alone is 35 BFS levels, with frontiers of 16 to 24 K
   (`systems/duckdb/p2levels.py`). So the cost is levels x edges, the same as attempts 1-2.
2. **DuckDB has no lookup for a frontier-sized probe that avoids a scan or a hash build.**
   - An ART index on `src` is used in a join only when the build side is tiny. On 5 M synthetic edges
     (`systems/duckdb/x_artjoin.sql`), a 40-row frontier takes 2 ms with the index vs 9-19 ms without. A 2,000-row
     frontier takes 12 vs 15 ms, even with `dynamic_or_filter_threshold` and `index_scan_max_count` raised.
   - A dense array held as a `getvariable` LIST is copied on every call: 1,000 probes take 7.7 s.

   That rules out the persistent-index/frontier shape. Batching several levels per statement would need the E^k
   edge relation.

`dk.c` gained `DK_ALLROUNDS=1`, which prints every `.repeat` round.
# Goose cells for GRID.md

MinIO. ms, min-max over 5 interleaved rounds of `systems/goose/bench.sh` at 2026-11-16T16:22-16:25Z, **load 8-13**
(provisional). Warm = min rep of in-process repeats; e2e = one process, one rep. 8 = 8 shared-nothing workers (queues
block on a condition variable; no spin-wait policy exists). Check: `systems/goose/bin/<cell> /home/exedev/paradigms/data/minio [8]`.

**1a join by name** — correct: P1 OK (`bin/p1`, `bin/p1par D 8`; canon + builtin vs oracle) · 1 thread 79-115 warm
(first rep with table first-touch 213) / 8 workers 37-57 · e2e 1a+1b+1c 852-1066 / 518-687 · 195 / 411 lines ·
easy: open addressing over inline structs, winner = max(rank<<32 | ~id) · forced: shuffle join, a queue type per
worker (`Mail<u8[j+1],M>`) · impossible: a shared table.

**1b relocation targets** — correct: P1 OK (reltarget vs oracle) · 39-57 / 42-67 (8 includes sending 9.4 M targets to
main) · lines in p1/p1par · easy: one gather per relocation · forced: canon allgather to every worker, results shipped
to main · impossible: -.

**1c content-hash join** — correct: P1 OK (p1c_canon vs oracle) · 97-118 / 39-47 · lines in p1/p1par · easy: same as 1a ·
forced: same shuffle · impossible: -.

**2 reach (CSR flood)** — correct: P2 OK (`bin/p2`, `bin/p2par D 8`) · 41-49 / 22-32 · e2e 205-269 / 225-316 · 187 /
241+341 lines · easy: `checkfree` drain over index-typed columns validated once by `from_bytes` (zero bounds checks) ·
forced: owner-partitioned BSP, per-owner inbox types, main merges global facts · impossible: a shared visited set;
scales only ~1.7x on 8.

**2' reach incl. decode (lazy, ATTACKS 1)** — correct: P2 OK (`bin/p2lazy`, `bin/p2par D 8 lazy`) · 74-79 / 37-49
(C p2lazy same window: 61-71 / 21-31; eager C pipeline 108.7 / 30.1) · e2e 731-859 / 411-807 · 286 lines + lazy mode
in p2rules · easy: decode inside the drain, ten lines · forced: cutting relocation edges out of the p2 rows at load;
a canon copy per worker · impossible: -. Beats the eager C pipeline on 1 thread, not on 8.

**2 reach, no supersteps (async, ATTACKS 3)** — correct: P2 OK (`bin/p2async D 8`, `bin/p2async D 8 lazy`) · 8 workers
best flood 33.4 / 38.9 / 44.2 vs BSP p2par 48.1 / 52.9 / 62.5 (min/med/max, interleaved, 17:45Z, load 21-23; second
window 28.2 / 37.4 / 46.6 vs 33.8 / 48.0 / 80.0); flush every 4 K ids 30.1 / 33.8 / 35.7; lazy 60.4 / 64.6 / 76.6 vs
66.7 / 95.4 / 103.0; 1 worker = BSP within noise · 233 lines + p2rules · easy: `qpoll` between local fixpoints,
termination by per-pair batch counts reported when idle · forced: facts broadcast to every worker in the data
batches · impossible: a shared visited set. 10 supersteps -> 0; scaling still ~1.3-1.9x (the partition, not the
barriers; results/levelfree.md).

**3a layout** — correct: P3 OK (`bin/p3`, `bin/p3par D 8`; addr vs oracle/p3_addr) · 15-26 / 17-31 · e2e 3a+3b 338-392 /
595-730 · 251 / 386 lines · easy: count, scan, assign; block-comparator sort for typelinks · forced: counts to main and
bases back · impossible: -. `bin/p3 D --pipeline`: 3b from its own 3a addresses -> P3 pipeline OK.

**3b relocate (ATTACKS 2)** — correct: P3 OK (image vs oracle/p3_image.bin per symbol) · 4 KB pages: 87-99 / 33-40;
**2 MB pages (P3_THP=1): 41-42 / 17-24**; minor faults/rep 26,731 -> 258-457 (1 thread) · 8 = image left distributed
by address range; gathered into one array in main +142-171 · easy: `move_from` + match on kind · forced: byte-wise
stores; huge pages only via C remapping the pages under the builder's stack top (runtime maps 4 KB, MADV_NOHUGEPAGE) ·
impossible: one image written in place by 8 workers (falls down as one array; distributed is the cell).

**4 code generation by shape** — correct: P4 OK (`bin/p4 /home/exedev/paradigms/data/minio-p4 [1|8]`: the code byte for
byte vs oracle/p4_code.bin, 1,780,281 relocations sorted vs oracle/p4_rel_*; also every `P4_K`) · warm (layout + emit,
min/median/max of 5 interleaved rounds, 18:37Z, load 1.7-2.6) 1 worker 97.1 / 98.3 / 100.6 (C p4 105.6 / 106.1 / 107.0)
/ 8 workers 17.6 / 21.3 / 22.9 (C p4par 17.1 / 18.0 / 21.2) · e2e with self-check 1255-1305 / 883-911 (C 593-605 /
390-446), without the check 656-686 / 267-375 · K=4096 commonest templates, rest glue: 63.7-66.3 / 10.6-15.5; K=0:
13.3-14.4 / 2.8-4.9 (time follows pieces: ~30-37 ns each) · 482 lines (+40 C in flatio.c) · easy: ranges of whole
functions are independent, no message during the compute; templates as inline records (`Seg` with its hole range as
`u32 upto holes`), no binary search · forced: one output image: through the queue 120-260 ms, through the output files
(each worker pwrites its range, C sidecar) 37-63 ms; a template table copy per worker · impossible: one in-memory image
written by all workers.

codex
The current files already correct several earlier review findings. I’m checking the remaining output checks and timing boundaries so the review distinguishes open gaps from resolved ones.
exec
/bin/bash -lc "rg --files systems/halide; ls -ld data build systems/*/bin; tail -n 8 GRID.md; rg -n 'check|oracle|P4 OK|memcmp' systems/halide; rg -n 'offset|check|P4 OK|oracle' systems/goose/p4.goose" in /home/exedev/paradigms-w
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
drwxr-xr-x 2 exedev exedev 4096 Nov 16 17:20 build
lrwxrwxrwx 1 exedev exedev   27 Nov 16 17:13 data -> /home/exedev/paradigms/data
drwxr-xr-x 2 exedev exedev 4096 Nov 16 17:22 systems/goose/bin
| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down: one image array on 8 workers (gather +142-171). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). **3b_blk** (M3: the copy as a sequential slice-update loop inside a map over 64 blocks = memcpy; lead rebuilt + reran P3b_blk OK 18:36Z): 60.4/62.0 (1) / **23.5/30.5 (8)** vs old 3b 159.7/166.7 / 96.4/110.9 same window, load 5: 2.7x / 3.6x, ranges apart. Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK**. 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a: second attempt `layout_rank.dl` (M3; lead rebuilt from source, P3 OK 18:21Z): the order by a ranked successor chain, no pair counting, no C functor: 1 thread 1930 (median, was 8270), 8 threads 1725/1937/3541 (was 2860/3388/7055 same runs), load 6-22. **No longer falls down**, but ~100x plain C: the 1,763-step unit walk is the limit. 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
| 4. codegen by shape (stretch) | **P4 OK** (lead rebuilt + reran 1/8 workers 18:47Z). Workers own ranges of whole functions, no messages while computing: layout+emit 97.1/98.3 (1) / **17.6/21.3 (8)** vs p4.c 105.6 / p4par 17.1, within noise; same 698 M instructions. The join of the parts is the cost: 120-260 ms through queues, 37-63 ms writing each range into the file. 482 lines + 40 C | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). **4D** (flattened stream, ATTACKS 4; lead reran P4D OK both backends 18:49Z): emit 65.1/67.8 (1) / **23.4/26.9 (8)** vs 4bf 284/303 / 95/113 same window, 4.5x / 4.2x; the flattening (`prepare`, 50 lines) 324 / 138 ms, so it pays only if the stream is reused | **P4 OK** (lead rebuilt + reran 1/8 18:46Z). Emit **157-162 (1) / 23.0-25.1 (8)** vs p4.c 103-112 / p4par 16.4-20.1 (load 4): 1.5x / 1.3x; layout 18 / 5 (+ template index 9.5 / 7). Relocations by a scatter to the layout's own slot ranges (no C host). Forced: entry-0 indices on untaken select sides (Halide loads both: 200 -> 160 ms); the vector copy only as 32-byte read-modify-write in parity passes (obvious form 26 s). 345 + 288 lines | not attempted | not attempted |

Halide column merged 17:17Z (lead rebuilt every generator and driver in a copy; P1/P2/P3 OK; `systems/halide/CELLS.md`).
DuckDB column merged 17:13Z (lead reran every check in a copy, pipeline included; `systems/duckdb/CELLS.md`).
Soufflé column merged 16:58Z (lead rebuilt all 8 programs from source; every check OK; `systems/souffle/CELLS.md`).
Futhark column merged 16:42Z (lead rebuilt from source; every cell OK; `systems/futhark/CELLS.md`). Surprise: `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, no code change, makes most Futhark cells 1.2-1.7x faster.
Goose column merged 16:35Z; lead reran every check (P1/P2/P3/pipeline OK, a source rebuild of p2par passes). Long form and lines: `systems/goose/CELLS.md`, `NOTES.md`.
systems/halide/IDEAS.md:27:- Kill test: build the reloc graph (5.2 M nodes, 9.4 M edges, CSR from the flat column files) from MinIO; run the flood from the entry roots with best GraphIt schedule at 8 threads. KILL if it is slower than 2x the hlink reach phase time or the result differs from the oracle live set (360,376). Also report ms vs the floor (9.4 M edges x 4-8 B at 92 GB/s is ~1 ms; random vertex touches at 0.74 G/s is ~7 ms for 5.2 M).
systems/halide/IDEAS.md:81:- Kill test: none worth running; recorded so nobody re-checks it. Mark as killed on reading.
systems/halide/NOTES.md:6:wrap them zero-copy as `Halide::Runtime::Buffer`, call the AOT pipelines, check against the oracles), `bench.h`
systems/halide/NOTES.md:10:Build and check (each driver checks every variant it runs against the oracles and ends with `P<n> OK`):
systems/halide/NOTES.md:125:- Forced: the kind folded into the key (kind 2 gets k2 = 2^40); a check output for sizes >= 2^31.
systems/halide/NOTES.md:188:build.sh 60. Drivers are loading, wrapping, the level loop (P2), the untimed edge split (P2) and checking.
systems/halide/NOTES.md:194:run, check; the top-K rewrite), built by `cells/build.sh p4` (one `.a` per schedule variant: 4 layouts, 38 emits + 4 diagnostics).
systems/halide/NOTES.md:195:Data contract: `ref/p4/README.md`. Check (each run checks itself; ends with `P4 OK`):
systems/halide/NOTES.md:199:    HL_NUM_THREADS=8 $B/p4 $D4 e2e lay_t_lf emit_t_i2_v32_p1     # code == oracle/p4_code.bin (136,634 functions,
systems/halide/NOTES.md:201:                                                                  # sorted == oracle/p4_rel_*: 0 differ
systems/halide/NOTES.md:202:    HL_NUM_THREADS=1 $B/p4 $D4 all                                # every variant, warm, each checked
systems/halide/NOTES.md:203:    P4_TOPK=4096 HL_NUM_THREADS=8 $B/p4 $D4 e2e lay_t_lf emit_t_i2_v32_p1   # the top-K variant, same oracle
systems/halide/NOTES.md:206:Every variant built is correct (all emit variants except the `diag*` diagnostics, and all 4 layouts, checked
systems/halide/NOTES.md:207:against the oracle at 1 and 8 threads; top-K for K = 0, 256, 1024, 4096, 16384, all).
systems/halide/NOTES.md:301:oracle, P4 OK for every K. Warm, 18:26Z, load 3.4-4.3 (1 thread CPU time, 8 threads wall, min-max):
systems/halide/NOTES.md:316:Cross-check of the per-piece claim in C (`cells/p4_copy32.sh REF_P4_C`: main's ref/p4/p4.c with each item's
systems/halide/NOTES.md:319:93.5-96.9 vs 91.6-100.3 (within noise); all P4 OK. How the bytes are copied moves C's emit by at most ~5%: its cost
systems/halide/NOTES.md:365:path is ~90), p4.cpp 288 (of which the top-K rewrite 97 and the checks ~50), build.sh +8.
systems/halide/NOTES.md:380:- Python: `python3 -m venv /home/exedev/tools/halide/venv && venv/bin/pip install halide==22.1.0 numpy` (JIT sanity-checked).
systems/halide/NOTES.md:387:3. `driver.cpp` mmaps the files (MAP_POPULATE), wraps them zero-copy in `Halide::Runtime::Buffer`, calls the AOT functions, checks each result against plain C++, reports best of 5.
systems/halide/bench.sh:5:# e2e lines: the whole process (start, mmap of the flat files, compute, oracle check excluded where the driver
systems/halide/bench.sh:38:# output on 2 MB pages (P4_HUGE=1) and Halide's own allocations too (glibc.malloc.hugetlb=2), no oracle check.
systems/halide/CELLS.md:29:**4. codegen by shape** — correct: P4 OK (`HL_NUM_THREADS=8 $B/p4 $D4 e2e lay_t_lf emit_t_i2_v32_p1`, D4=data/minio-p4:
systems/halide/CELLS.md:30:code == oracle/p4_code.bin, fn offsets == p4_fn_off.u64, 1,780,281 relocations sorted == p4_rel_*; every variant and
systems/halide/CELLS.md:31:P4_TOPK=0..all checked). 2026-11-16T18:31Z, load 4.1-4.9, min-max of 5; 1 thread = CPU time. templates (template-only
systems/halide/P4-REPORT.md:9:prints `P4 OK`: the 60,733,909 code bytes equal oracle/p4_code.bin (all 136,634 functions, offsets equal
systems/halide/P4-REPORT.md:10:p4_fn_off.u64), and the 1,780,281 relocations, sorted, equal oracle/p4_rel_*. Every schedule variant (4 layouts,
systems/halide/P4-REPORT.md:11:38 emit variants; the 4 `diag*` ones are diagnostics, left unchecked) passes at 1 and 8 threads, and so does the top-K
systems/halide/P4-REPORT.md:12:variant for K = 0, 256, 1024, 4096, 16384 and all. The check can fail: a top-K bug (items over 2^17 bytes) printed
systems/halide/P4-REPORT.md:35:path is about 90), p4.cpp 288 (the top-K rewrite is 97, the checks about 50). Non-blank, non-comment.
systems/halide/cells/gen_p4.cpp:18:// min/max per access (the CSR arrays are well formed; p4.cpp checks the sizes it relies on).
systems/halide/cells/gen_p3.cpp:139:        for (int w = 1, lg = 0; w < (1 << 16); w *= 2, lg++) {  // runs up to 2^16 > the typelink count (checked in p3.cpp)
systems/halide/cells/p1.cpp:2:// pipelines Counts, Resolve (1a), RelTarget (1b) and ContentJoin (1c), checks against the oracles.
systems/halide/cells/p1.cpp:3://   p1 DATA [all]       every schedule variant, warm (bench.h: min/med/max of 5 runs of >= 1 s), each checked
systems/halide/cells/p1.cpp:4://   p1 DATA e2e V       one end-to-end run (process start, mmap the files, compute, check) of variant V (0 serial, 1 parallel)
systems/halide/cells/p1.cpp:47:    auto check = [&]() {
systems/halide/cells/p1.cpp:48:        long bad = check32("canon", canon.data(), (size_t)N + 1, "oracle/p1_canon.u32");
systems/halide/cells/p1.cpp:49:        bad += check32("builtin", builtin.data(), nb, "oracle/p1_builtin.u32");
systems/halide/cells/p1.cpp:50:        bad += check32("reltarget", tgt.data(), nrel, "oracle/p1_reltarget.u32");
systems/halide/cells/p1.cpp:51:        bad += check32("canon after 1c", canon2.data(), (size_t)N + 1, "oracle/p1c_canon.u32");
systems/halide/cells/p1.cpp:52:        printf("internal checks (must be 0): 1a %d, 1c %d\n", chk1(), chk3());
systems/halide/cells/p1.cpp:71:        bad += check();
systems/halide/cells/p1.cpp:83:            bad += check();
systems/halide/cells/join.h:10:// the creating entries that never settled (it must be 0; the driver checks).
systems/halide/cells/gen_p2.cpp:210:            // two" never skips anything else, checked in p2.cpp)
systems/halide/cells/p2.cpp:2:// until nothing changes, checks the live set against oracle/p2_live.u32.
systems/halide/cells/p2.cpp:3://   p2 DATA [all]   every flood variant, warm (bench.h), each checked
systems/halide/cells/p2.cpp:6:// Lazy variants decode the relocations inside the flood (from rel_*, through the 1a canon: its oracle, as
systems/halide/cells/p2.cpp:33:    auto canon = wrap<u32>("oracle/p1_canon.u32"), builtin = wrap<u32>("oracle/p1_builtin.u32");
systems/halide/cells/p2.cpp:63:            if (t == 26) {  // hlink skips the next two: on MinIO they are always type 26 as well (checked)
systems/halide/cells/p2.cpp:111:    auto check = [&]() {
systems/halide/cells/p2.cpp:116:        return check32("live", list.data(), list.size(), "oracle/p2_live.u32") != 0;
systems/halide/cells/p2.cpp:128:        bad += check();
systems/halide/cells/p2.cpp:135:            bad += check();
systems/halide/cells/p3.cpp:2:// Layout (3a) and Relocate (3b) pipelines, checks against the oracles.
systems/halide/cells/p3.cpp:3://   p3 DATA [all|3a|3b|V]    every schedule variant (or one), warm (bench.h), each checked; 3b also with a fresh
systems/halide/cells/p3.cpp:5://   p3 DATA e2e LV RV [out]  one run of layout variant LV and relocate variant RV (index into the tables), checked
systems/halide/cells/p3.cpp:23:    int c = n ? memcmp(sbin + xo, sbin + yo, n) : 0;
systems/halide/cells/p3.cpp:76:    Buffer<u32, 1> addr(L);  // 3b alone (mode = a reloc name) starts from the oracle addresses
systems/halide/cells/p3.cpp:77:    memcpy(addr.data(), FMAP(u32, "oracle/p3_addr.u32", NULL), 4 * (size_t)L);
systems/halide/cells/p3.cpp:85:    // 3b check: every placed symbol's [addr, addr+size) equals the oracle image
systems/halide/cells/p3.cpp:88:    auto checkImage = [&]() {
systems/halide/cells/p3.cpp:89:        if (!want) want = FMAP(u8, "oracle/p3_image.bin", &nwant);
systems/halide/cells/p3.cpp:95:            if (memcmp(imgp + o, want + o, size(i)) != 0 && nbad++ < 5) printf("  symbol %u (bucket %u) differs\n", i, b);
systems/halide/cells/p3.cpp:97:        printf("image: %ld placed symbols differ from oracle/p3_image.bin, %d relocation errors\n", nbad, nerr());
systems/halide/cells/p3.cpp:113:        bad += check32("addr", addr.data(), L, "oracle/p3_addr.u32") != 0;
systems/halide/cells/p3.cpp:114:        bad += checkImage();
systems/halide/cells/p3.cpp:121:            bad += check32("addr", addr.data(), L, "oracle/p3_addr.u32") != 0;
systems/halide/cells/p3.cpp:123:        memcpy(addr.data(), FMAP(u32, "oracle/p3_addr.u32", NULL), 4 * (size_t)L);
systems/halide/cells/p3.cpp:129:            bad += checkImage();
systems/halide/cells/p3.cpp:149:                {   // check a fresh image too (untimed; M3 review): it must equal the reused buffer, which checkImage() compared with the oracle
systems/halide/cells/p3.cpp:151:                    if (m == MAP_FAILED) { bad++; printf("3b fresh check: mmap failed\n"); }
systems/halide/cells/p3.cpp:155:                        int d = memcmp(m, imgp, nimg) != 0;
systems/halide/cells/p3.cpp:156:                        printf("3b %-16s fresh %s image %s the checked one\n", relocs[v].name, huge ? "2MB" : "4KB", d ? "DIFFERS from" : "equals");
systems/halide/cells/gen_p1.cpp:69:    Output<Buffer<int32_t, 0>> check{"check"};  // entries that found no group: 0
systems/halide/cells/gen_p1.cpp:110:        check() = J.left();
systems/halide/cells/gen_p1.cpp:179:    Output<Buffer<int32_t, 0>> check{"check"};  // entries that found no group, sizes >= 2^31: 0
systems/halide/cells/gen_p1.cpp:217:        check() = J.left() + sum(select(big, 1, 0));
systems/halide/cells/p4.cpp:2:// SegHoles, Layout and Emit pipelines, checks the code against oracle/p4_code.bin (+ p4_fn_off.u64) byte for byte
systems/halide/cells/p4.cpp:3:// and the relocations, sorted, against oracle/p4_rel_*.
systems/halide/cells/p4.cpp:4://   p4 DATA [all|layout|NAME]   every schedule variant (or those whose name starts with NAME), warm (bench.h), checked
systems/halide/cells/p4.cpp:5://   p4 DATA e2e LV EV [out]     one run of layout variant LV and emit variant EV (index or name), checked
systems/halide/cells/p4.cpp:7:// emits only the nodes of the K commonest templates (the rest as glue); P4_NOCHECK=1 skips the e2e check;
systems/halide/cells/p4.cpp:105:        const u8 *code0 = FMAP(u8, "oracle/p4_code.bin", NULL);
systems/halide/cells/p4.cpp:228:    // ---- checks (outside every timed region)
systems/halide/cells/p4.cpp:229:    const u64 *wfn = FMAP(u64, "oracle/p4_fn_off.u64", NULL);
systems/halide/cells/p4.cpp:231:    const u8 *want = FMAP(u8, "oracle/p4_code.bin", &nwant);
systems/halide/cells/p4.cpp:233:    auto checkLayout = [&]() {
systems/halide/cells/p4.cpp:236:            if (at(fnItem(f)) != wfn[f] && bad++ < 5) printf("  function %u at %u, oracle %llu\n", f, at(fnItem(f)), (unsigned long long)wfn[f]);
systems/halide/cells/p4.cpp:237:        printf("layout: %u bytes, %u relocations; %ld function offsets differ from oracle/p4_fn_off.u64\n", at(nitem), rat(nitem), bad);
systems/halide/cells/p4.cpp:240:    auto checkEmit = [&]() {
systems/halide/cells/p4.cpp:243:        else if (memcmp(codep, want, ncode) != 0) {
systems/halide/cells/p4.cpp:245:                if (memcmp(codep + wfn[f], want + wfn[f], wfn[f + 1] - wfn[f]) != 0 && bad++ < 5) printf("  function %u differs\n", f);
systems/halide/cells/p4.cpp:248:        printf("code: %zu bytes in %u functions, %ld functions differ from oracle/p4_code.bin\n", ncode, nfn, bad);
systems/halide/cells/p4.cpp:250:            const u64 *wo = FMAP(u64, "oracle/p4_rel_off.u64", NULL);
systems/halide/cells/p4.cpp:251:            const u8 *ws = FMAP(u8, "oracle/p4_rel_siz.u8", NULL);
systems/halide/cells/p4.cpp:252:            const u16 *wt = FMAP(u16, "oracle/p4_rel_type.u16", NULL);
systems/halide/cells/p4.cpp:253:            const u32 *wy = FMAP(u32, "oracle/p4_rel_sym.u32", NULL);
systems/halide/cells/p4.cpp:254:            const i64 *wa = FMAP(i64, "oracle/p4_rel_add.i64", NULL);
systems/halide/cells/p4.cpp:264:        printf("relocations: %u, %ld differ from oracle/p4_rel_*\n", nrel, rbad);
systems/halide/cells/p4.cpp:288:        if (getenv("P4_NOCHECK")) return 0;  // (bench.sh's e2e wall times: the process without the oracle check)
systems/halide/cells/p4.cpp:289:        bad += checkLayout();
systems/halide/cells/p4.cpp:290:        bad += checkEmit();
systems/halide/cells/p4.cpp:299:            bad += checkLayout();
systems/halide/cells/p4.cpp:307:            if (strncmp(emits[v].name, "diag", 4) == 0) { printf("(diagnostic: not checked)\n"); continue; }
systems/halide/cells/p4.cpp:308:            bad += checkEmit();
systems/halide/cells/p4.cpp:311:    printf(bad ? "P4 FAIL\n" : "P4 OK\n");
5:// with no message in between, it lays out its items from 0 (a jump is within one function, so local offsets are
8:// offsets of two output files ($P4_OUT or /tmp: goose_p4_code.bin, goose_p4_rel.bin). Main reads them back and
9:// checks them against the oracle (code byte for byte, relocations as a sorted list).
13:// rest is glue. P4_JOIN=queue: the parts go to main through the queue instead. P4_NOCHECK=1: no oracle check.
22:struct Hole { at: u32, w: u8, enc: u8, rtype: u16 }            // at: offset in its segment
214:// the kept nodes are pointed at (new item, offset in it).
235:    var newid: i64[>..] = [];       // per old item: the new item it falls in, and its offset there
284:// the output files (flatio.c): created by main, written by each worker at its own offset
356:// the code byte for byte against oracle/p4_code.bin, the relocations as a sorted list against oracle/p4_rel_*
357:fn check(img: u8[:], rel: Rel[>..]&) -> i64 {
360:    load("oracle/p4_code.bin", want);
361:    let wfn = load_u64("oracle/p4_fn_off.u64");
362:    if img.len != want.len { print("code: ", img.len, " bytes, oracle ", want.len); bad++; }
370:    let wo = load_u64("oracle/p4_rel_off.u64");
371:    let wsiz = load_u8("oracle/p4_rel_siz.u8");
372:    let wtype = load_u16("oracle/p4_rel_type.u16");
373:    let wsym = load_u32("oracle/p4_rel_sym.u32");
374:    let wadd = load_i64("oracle/p4_rel_add.i64");
383:            if rbad < 5 { print("  relocation ", i, " differs (offset ", x.off, ")"); }
387:    print("code: ", img.len, " bytes; relocations: ", rel.len, ", oracle ", wr.len, ", ", rbad, " differ");
477:    guard !env_set("P4_NOCHECK") else { return; }      // e2e without the check: the output is the files
481:    let bad = nerr + check(img, rel);
482:    print(if bad == 0 { "P4 OK" } else { "P4 FAIL" });

exec
/bin/bash -lc "sed -n '220,315p' systems/halide/cells/p4.cpp; sed -n '350,390p' systems/goose/p4.goose; cat systems/souffle/check_p3.c; cat results/levelfree.md; ls build; ls /home/exedev/paradigms/systems/halide/cells" in /home/exedev/paradigms-w
 succeeded in 0ms:
    };
    auto runEmit = [&](int v) {
        emits[v].fn(itemKind, itemRef, itemSeg, glueOff, glueRelOff, nodeTmpl, nodeOp, tsegOff, tsegEnd, tholeOff,
                    segLo, segSrc, segH0, segR0, relHole, src, holeAt, holeW, holeEnc, holeRtype, opVal, opAux, grelOff, grelSiz,
                    grelType, grelSym, grelAdd, fnItem, fnMarkOff, markOff, markType, markSym, markAdd, at, rat, code,
                    rOff, rType, rSym, rAdd, rSiz);
    };

    // ---- checks (outside every timed region)
    const u64 *wfn = FMAP(u64, "oracle/p4_fn_off.u64", NULL);
    size_t nwant;
    const u8 *want = FMAP(u8, "oracle/p4_code.bin", &nwant);
    std::vector<Rel> wrel;
    auto checkLayout = [&]() {
        long bad = at(nitem) != ncode || rat(nitem) != nrel;
        for (u32 f = 0; f <= nfn; f++)
            if (at(fnItem(f)) != wfn[f] && bad++ < 5) printf("  function %u at %u, oracle %llu\n", f, at(fnItem(f)), (unsigned long long)wfn[f]);
        printf("layout: %u bytes, %u relocations; %ld function offsets differ from oracle/p4_fn_off.u64\n", at(nitem), rat(nitem), bad);
        return bad;
    };
    auto checkEmit = [&]() {
        long bad = 0;
        if (nwant != ncode) bad++;
        else if (memcmp(codep, want, ncode) != 0) {
            for (u32 f = 0; f < nfn; f++)
                if (memcmp(codep + wfn[f], want + wfn[f], wfn[f + 1] - wfn[f]) != 0 && bad++ < 5) printf("  function %u differs\n", f);
            if (!bad) bad = 1;
        }
        printf("code: %zu bytes in %u functions, %ld functions differ from oracle/p4_code.bin\n", ncode, nfn, bad);
        if (wrel.empty()) {
            const u64 *wo = FMAP(u64, "oracle/p4_rel_off.u64", NULL);
            const u8 *ws = FMAP(u8, "oracle/p4_rel_siz.u8", NULL);
            const u16 *wt = FMAP(u16, "oracle/p4_rel_type.u16", NULL);
            const u32 *wy = FMAP(u32, "oracle/p4_rel_sym.u32", NULL);
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
    if x.rtype != y.rtype { return x.rtype < y.rtype; }
    if x.sym != y.sym { return x.sym < y.sym; }
    if x.add != y.add { return x.add < y.add; }
    return x.siz < y.siz;
}

// the code byte for byte against oracle/p4_code.bin, the relocations as a sorted list against oracle/p4_rel_*
fn check(img: u8[:], rel: Rel[>..]&) -> i64 {
    var bad = 0;
    var want: u8[>..] = [];
    load("oracle/p4_code.bin", want);
    let wfn = load_u64("oracle/p4_fn_off.u64");
    if img.len != want.len { print("code: ", img.len, " bytes, oracle ", want.len); bad++; }
    else if img != want[..] {
        var nf = 0;
        for g in wfn.len - 1 { if img[wfn[g]..wfn[g + 1]] != want[wfn[g]..wfn[g + 1]] { if nf < 5 { print("  function ", g, " differs"); } nf++; } }
        print("code: ", nf, " functions differ");
        bad++;
    }
    sort(rel) { x, y => rel_less(x, y) };
    let wo = load_u64("oracle/p4_rel_off.u64");
    let wsiz = load_u8("oracle/p4_rel_siz.u8");
    let wtype = load_u16("oracle/p4_rel_type.u16");
    let wsym = load_u32("oracle/p4_rel_sym.u32");
    let wadd = load_i64("oracle/p4_rel_add.i64");
    var wr: Rel[>..] = [];
    for i in wo.len { wr.push(Rel { off: wo[i] as i64, add: wadd[i], sym: wsym[i], rtype: wtype[i], siz: wsiz[i] }); }
    sort(wr) { x, y => rel_less(x, y) };
    var rbad = if rel.len != wr.len { 1 } else { 0 };
    for i in min(rel.len, wr.len) {
        let x = rel[i];
        let y = wr[i];
        if x.off != y.off || x.add != y.add || x.sym != y.sym || x.rtype != y.rtype || x.siz != y.siz {
            if rbad < 5 { print("  relocation ", i, " differs (offset ", x.off, ")"); }
            rbad++;
        }
    }
    print("code: ", img.len, " bytes; relocations: ", rel.len, ", oracle ", wr.len, ", ", rbad, " differ");
    return bad + rbad;
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
# ATTACKS 3 addendum (draft): is the level count the cost, in Futhark and in Goose?

Builder, 2026-11-16T17:20-17:55Z. Scope (ATTACKS.md 3, after M2): build a batched-level or level-free flood in
each system, count its mechanism (levels, rounds, parallel operations, barriers, messages) and time it against the
system's original, interleaved. All variants reproduce `oracle/p2_live.u32` exactly (P2 OK, 663,630 ids).

**Load.** The whole window ran at load **17-26** on the 8 cores (other builders: duckdb, cc1, dk, futhark).
Every time below is min / median / max over 5-7 interleaved rounds (each round one process per configuration,
min of its in-process reps); 8-thread numbers are oversubscribed. Rerun: `systems/futhark/levelfree.sh counts`,
`systems/futhark/levelfree.sh time 7`, `systems/goose/levelfree.sh 7`.

## Two corrections to the premise

- **Futhark's flood has 45 levels, not 111.** 111 is the C level-by-level count (results/levels.md: methods
  checked once per round, 7 rounds); Futhark's flood checks the method rule every level, so method firings join
  the next level instead of opening a new round. Measured with a counting entry (`reach_levels`): 45 levels,
  **1,211 parallel operations per flood** (FUT_PROFILE task count), i.e. 43 of setup and **~26 per level**.
- **Goose's p2par has 10 supersteps, not 111.** Each worker already drains its partition to a local fixpoint
  (method rule included) before it exchanges: a superstep is a cross-partition wave. MinIO, 8 workers:
  10 supersteps, 1,036,250 ids in 560 inbox batches (W x (W-1) per step) + 80 status + 80 go messages.

## Futhark

Attempt 1, **hybrid** (`2hyb`, p2.fut `flood_hybrid`): a data-parallel level while the frontier is >= lo, else a
sequential worklist drain (a `loop` outside any map: Futhark compiles it to plain host C, no parallel operation)
until its stack exceeds hi (or after `budget` pops), which then hands its stack back as the frontier. One
array `ex` (expanded as plain / as live iface gotype) lets the two modes share one frontier. `2seq` is the limit
case: one drain, the whole flood a sequential worklist. Attempt 2, **batched levels** (`2bat`, `flood_batch`): the
"loop inside the map": each round cuts the frontier into C chunks, each chunk runs a sequential worklist over
plain edges for B pops into its own buffers (edges walked, symbols expanded), and the round applies the rules to
all of it with the level's scatters.

Counts (MinIO, deterministic except bat's duplicates):

| variant | data-parallel levels / rounds | drains | parallel ops / flood | pops in drains | symbols expanded |
|---|---|---|---|---|---|
| orig (`2`) | 45 | 0 | 1,211 | 0 | 678,996 |
| hyb lo 0 (control: ex bookkeeping only) | 45 | 0 | 1,213 | 0 | 678,996 |
| hyb lo 1 K, hi 4 K | 16 | 2 | 463 | 271,779 (40%) | |
| hyb lo 4 K, hi 16 K | 15 | 2 | 437 | 273,043 (40%) | |
| hyb lo 4 K, budget 2 K pops | 15 | 134 | 701 | 273,043 | |
| `2seq` (one worklist) | 0 | 1 | 45 (setup) | 670,228 (100%) | |
| bat C 64, B 1 K | 35-42 rounds | | 1,576 | | 700 K distinct, 861 K with chunks' duplicates; 3.3 M edges walked vs 2.5 M |
| bat C 8, B 4 K | 45 rounds | | 1,943 | | 778 K with duplicates |
| bat C 1, B unbounded | 25 rounds | | | | (25 = the floor set by iface/method rules, which a chunk does not follow) |

Times, ms (warm, `c` backend = 1 core, `multicore` stock runtime = 8 threads), two interleaved windows:

| variant | 1 core, 17:40Z (5 rounds) | 8 threads, 17:40Z | 1 core, 17:47Z (7 rounds) | 8 threads, 17:47Z |
|---|---|---|---|---|
| orig | 137 / 171 / 313 | 156 / 201 / 258 | 141 / 265 / 416 | 144 / 245 / 388 |
| hyb lo 0 | 147 / 176 / 260 | 146 / 187 / 242 | | |
| hyb 1 K | 116 / 119 / 168 | 102 / 178 / 339 | 120 / 178 / 312 | 138 / 184 / 288 |
| hyb 4 K | 161 / 197 / 280 | 129 / 171 / 335 | 116 / 161 / 246 | 152 / 231 / 273 |
| hyb 4 K, budget 2 K | 166 / 212 / 382 | 209 / 234 / 240 | | |
| `2seq` | 79 / 156 / 175 | **67 / 102 / 130** | 73 / 131 / 181 | 88 / 112 / 160 |
| bat C 64 | 220 / 340 / 555 | 211 / 370 / 490 | 222 / 356 / 474 | 263 / 355 / 540 |
| bat C 8 | 199 / 313 / 687 | 159 / 266 / 662 | | |

Reading:
- Cutting levels 45 -> 15 (parallel ops 1,211 -> 437) is **within noise** at both widths. If the per-level cost
  were the cost, the hybrid would sit near `2seq`; it sits near the original.
- A **sequential worklist in Futhark beats the level-synchronous flood**: 1 core min 73-79 vs 137-141, medians
  131-156 vs 171-265 (ranges overlap); on 8 threads `2seq` (which uses one) beats the 8-thread flood with ranges
  apart in the first window (67-130 vs 156-258) and overlapping in the second (88-160 vs 144-388).
- So **in Futhark the cost is per element, not per level**: a data-parallel level touches each frontier item and
  each edge in ~26 passes (filters, expands = scan + scatter + segmented iota, scatters, the dedup), each with
  its own allocation; a sequential pop touches them once. At 1 core (no barriers at all) the 15 wide levels that
  remain in the hybrid (60% of the pops) cost about as much as the whole sequential flood. On 8 threads,
  oversubscribed, the wide levels' parallelism does not repay their passes.
- Attempt 2 loses (1.5-2.5x slower than orig): rounds barely drop (35-45; 25 even with one unbounded chunk,
  because iface and method rules still need rounds), chunks re-expand symbols that several of them reach
  (up to +27% expansions, +32% edges), and every chunk's fixed-size buffers are initialised each round (Futhark
  has no uninitialised array: a `replicate` per buffer per round).
- A Futhark-specific trap found on the way: the drain's stack first grew by `++` (an existential-size loop
  variable); the generated C then did several memory-block reference-count updates per push, and the drain ran
  **3x slower** (255 vs 76 ms, 1 core). A fixed-size stack (bounded by `hi + widest row + 3 x methods`) fixed it.

**Verdict, Futhark: no.** The level count is not the cost; the per-element passes of the flattened level are.
The level-free version that wins is the one with no data parallelism at all (`2seq`), which is also the
shortest-reading one (a worklist loop with in-place updates), but it is a sequential program written in Futhark.
Halving the levels changes nothing measurable.

## Goose

**p2async.goose** (233 lines + p2rules.goose shared with p2par): the same owner partition and local rules,
no supersteps. A worker floods its part to a local fixpoint (or until `P2FLUSH` ids for others wait), posts
what it found for others straight into their inboxes (ids with op bits; any new global fact, i.e. signature,
method name, reflect, broadcast to every worker), takes whatever has arrived with `qpoll`, and goes on. Idle, it
reports its per-peer sent/received batch counts to main and blocks on its inbox. Main stops the flood when every
worker's latest report is idle and sent[i][j] == received[j][i] for all pairs (sound because a worker reports
only when idle and only wakes on a message, so an in-flight or unreported message always shows as a mismatch).

Counts (MinIO, 8 workers, one flood; async counts vary by scheduling):

| | barriers (supersteps) | inbox batches | ids sent | control messages |
|---|---|---|---|---|
| p2par (BSP) | 10 | 560 | 1,036,250 | 80 status + 80 go |
| p2async | **0** | 482-687 | 1,036,064-1,036,337 | 102-174 idle reports + 16 start/stop |
| p2async, flush every 4 K ids | 0 | 1,715-2,000 | 1,023,920-1,036,035 | 196-368 idle reports |

Times, best flood ms, min / median / max (interleaved; load 21-23 at 17:45Z, 17-21 at 17:51Z):

| | 17:45Z, 5 rounds | 17:51Z, 7 rounds |
|---|---|---|
| BSP, 1 worker | 34.6 / 86.0 / 106.8 | 44.0 / 47.9 / 59.9 |
| async, 1 worker | 46.5 / 73.6 / 105.2 | 44.6 / 48.4 / 106.7 |
| BSP, 8 | 48.1 / 52.9 / 62.5 | 33.8 / 48.0 / 80.0 |
| **async, 8** | **33.4 / 38.9 / 44.2** | 28.2 / 37.4 / 46.6 |
| async, 8, flush 4 K | 30.1 / 33.8 / 35.7 | 25.0 / 34.5 / 54.9 |
| BSP, 8, lazy decode | 66.7 / 95.4 / 103.0 | 64.0 / 84.7 / 119.8 |
| async, 8, lazy decode | 60.4 / 64.6 / 76.6 | 47.0 / 52.9 / 63.7 |

Where the time goes (P2TRACE, one 8-worker flood at load 18, 17:53Z): BSP flood 54.5 ms, each worker drains
4-23 ms of it and waits the rest; async 28.0 ms, per worker local 4-20 + flush 0.5-10 + absorb 0.1-1.9 ms, wait
7-23 ms. Summed over workers the partitioned work is ~70-80 ms against 43 ms for one worker (cross-partition ids
are applied twice: once as an edge, once as a message; and the 1 M ids are bucketed and copied through queues).

Reading:
- Dropping the 10 barriers helps under this load: async 8 beats BSP 8 by **1.3-1.4x in median** with ranges apart
  in the first window (33-44 vs 48-63), overlapping in the second; lazy decode 1.4-1.6x (ranges touch). Same
  mechanism as codex-g's C result: at every barrier all workers wait for whichever one was descheduled.
- At 1 worker the two are the same program (within noise), so the async machinery costs nothing.
- But scaling stays poor: medians async 8 workers 37-39 vs 1 worker 48-74, BSP 48-53 vs 48-86. The busiest
  partition's own work (17-21 ms) and the 1 M ids crossing partitions (flush up to 10 ms per worker) bound it,
  not the superstep count.

**Verdict, Goose: partly.** It was never 111 supersteps (the BSP version already floods each partition to a local
fixpoint: 10 cross-partition waves). Removing those 10 barriers is worth ~1.3-1.5x at load ~20, but the limit on
8 workers is the partition: duplicated work across owners, one busy partition and a million ids through queues.

## What this adds to ATTACKS 3

The level count is a cost of *synchronization*, and only where a barrier waits on a descheduled thread (C p2par
with spinning waits; Goose's supersteps, ~1.4x). In Futhark it is not the cost: the flattened level's per-element
passes are, and a sequential worklist in the same language is 1.5-2x faster at 1 core and no slower than 8
threads. The hybrid shape (wide levels parallel, tail sequential) cuts levels 3x in Futhark but saves nothing,
because the tail held only 40% of the pops and the expensive part is the wide levels' per-element passes.
Still owed: the same runs on a quiet machine (every number above is at load 17-26).
p1par
p2lazy
p2par
p3par
bench.h
build
build.sh
gen_p1.cpp
gen_p2.cpp
gen_p3.cpp
gen_p4.cpp
join.h
p1.cpp
p2.cpp
p3.cpp
p4.cpp
p4_copy32.sh

exec
/bin/bash -lc 'HL_NUM_THREADS=8 /home/exedev/paradigms/systems/halide/cells/build/p4 /home/exedev/paradigms/data/minio-p4 e2e lay_t_lf emit_t_i2_v32_p1' in /home/exedev/paradigms-w
 exited 127 in 0ms:
/bin/bash: line 1: /home/exedev/paradigms/systems/halide/cells/build/p4: No such file or directory

exec
/bin/bash -lc "nl -ba systems/goose/p4.goose | sed -n '390,483p'; nl -ba systems/goose/p3par.goose | tail -n 85; nl -ba JOURNAL.md | tail -n 32; nl -ba ATTACKS.md | sed -n '95,175p'; nl -ba systems/halide/cells/bench.h | head -n 100" in /home/exedev/paradigms-w
 succeeded in 0ms:
   390	
   391	fn main() {
   392	    let W = workers_arg();
   393	    let t0 = clock();
   394	    let reps = env_int("P4_REPS", 1);
   395	    // functions cut where the items' quantiles fall
   396	    let fi = load_u32("fn_item.u32");
   397	    let nfn = fi.len - 1;
   398	    var cut: i64[>..] = [];
   399	    var f = 0;
   400	    for k in W + 1 {
   401	        let want = part_lo(fi[nfn], k, W);
   402	        while f < nfn && fi[f] < want { f++; }
   403	        cut.push(if k == W { nfn } else { f });
   404	    }
   405	    var ids: i64[>..] = [];
   406	    for k in W { ids.push(thread_spawn(worker, k, W, cut[k], cut[k + 1])); }
   407	    for k in W { let r = qget<Ready>(); }
   408	    let t1 = clock();
   409	    // ---- the timed reps: from the go to the last worker done
   410	    var wall: f64[>..] = [];
   411	    var lay: f64[>..] = [];
   412	    var emt: f64[>..] = [];
   413	    var size: i64[>..] = [];
   414	    size.resize(W, 0);
   415	    var nrel: i64[>..] = [];
   416	    nrel.resize(W, 0);
   417	    var pieces = 0;
   418	    var nerr = 0;
   419	    for rep in reps + 1 {
   420	        let ta = clock();
   421	        for j in W { send(j, Go { go: true }); }
   422	        var lmax = 0.0;
   423	        var emax = 0.0;
   424	        nerr = 0;
   425	        pieces = 0;
   426	        for j in W {
   427	            let d = qget<Done>();
   428	            lmax = max(lmax, d.layout);
   429	            emax = max(emax, d.emit);
   430	            size[d.k] = d.bytes;
   431	            nrel[d.k] = d.nrel;
   432	            nerr += d.nerr;
   433	            pieces = pieces + d.pieces;
   434	        }
   435	        let tb = clock();
   436	        if rep > 0 { wall.push(ms(ta, tb)); lay.push(lmax); emt.push(emax); }
   437	    }
   438	    let t2 = clock();
   439	    // ---- join: the workers' parts in order into one image (P4_JOIN=queue: sent to main; else written to files)
   440	    let file = !env_set("P4_JOIN");
   441	    var base: i64[>..] = [];
   442	    var rbase: i64[>..] = [];
   443	    var total = 0;
   444	    var rtotal = 0;
   445	    for j in W { base.push(total); total += size[j]; rbase.push(rtotal); rtotal += nrel[j]; }
   446	    var img: u8[>..] = [];
   447	    var rel: Rel[>..] = [];
   448	    var wmax = 0.0;
   449	    if file {
   450	        guard flat_create(out_path("code"), total) && flat_create(out_path("rel"), rtotal * relBytes) else { abort("cannot create the output"); }
   451	    } else {
   452	        img.resize(total, 0);
   453	    }
   454	    let t2a = clock();
   455	    for j in W { send(j, Place { file: file, base: base[j], rbase: rbase[j] }); }
   456	    for j in W {
   457	        let o = qget<Out>();
   458	        wmax = max(wmax, o.ms);
   459	        if file { continue; }
   460	        img[base[o.k]..base[o.k] + o.code.len].move_from(o.code[..]);
   461	        let b = base[o.k];
   462	        for r in o.rels { rel.push(Rel { off: r.off + b, add: r.add, sym: r.sym, rtype: r.rtype, siz: r.siz }); }
   463	    }
   464	    let t3 = clock();
   465	    for id in ids { thread_wait(id); }
   466	    var note: u8[>..] = [];
   467	    format(note, "p4 goose (", W, " workers, x", reps, "): ");
   468	    report(note, "spawn+load", t0, t1);
   469	    spread(note, "rep wall", wall);
   470	    spread(note, "layout", lay);
   471	    spread(note, "emit", emt);
   472	    report(note, if file { "join (files)" } else { "join (queue)" }, t2a, t3);
   473	    format(note, "(slowest worker ");
   474	    format_flt(note, wmax, 1);
   475	    format(note, " ms)  ", total, " bytes, ", pieces, " pieces, ", rtotal, " relocations");
   476	    print(note);
   477	    guard !env_set("P4_NOCHECK") else { return; }      // e2e without the check: the output is the files
   478	    if file {
   479	        guard flat_read(out_path("code"), img, 1) && flat_read(out_path("rel"), rel, relBytes) else { abort("cannot read the output back"); }
   480	    }
   481	    let bad = nerr + check(img, rel);
   482	    print(if bad == 0 { "P4 OK" } else { "P4 FAIL" });
   483	    if bad != 0 { exit(1); }
   302	        addr[p.lo..p.lo + p.vals.len].move_from(p.vals[..]);
   303	    }
   304	    place_typelinks(tlv, addr);
   305	    let t2 = clock();
   306	    // ---- 3b distributed: from the go to the last worker's segment built, P3_REPS times
   307	    let reps = env_int("P3_REPS", 1);
   308	    let huge = env_set("P3_THP");
   309	    var times: f64[>..] = [];
   310	    var wmax = 0.0;
   311	    var flt = 0;
   312	    var hb = 0;
   313	    for rep in reps {
   314	        let ta = clock();
   315	        for j in W { send(j, Go { go: true }); }
   316	        wmax = 0.0;
   317	        flt = 0;
   318	        hb = 0;
   319	        for j in W {
   320	            let d = qget<Done>();
   321	            wmax = max(wmax, d.ms);
   322	            flt += d.faults;
   323	            hb += if d.huge > 0 { d.huge } else { 0 };
   324	        }
   325	        times.push((clock() - ta) * 1000.0);
   326	    }
   327	    let t2a = clock();
   328	    // ---- 3b gathered: the segments into one array in main
   329	    var img: u8[>..] = [];
   330	    let f0 = minor_faults(false);
   331	    let t2b = clock();
   332	    let mhb = fresh_pages(img, meta("image_bytes"), huge);
   333	    img.resize(meta("image_bytes"), 0);
   334	    let t2c = clock();
   335	    for j in W { send(j, Go { go: true }); }
   336	    for j in W {
   337	        let s = qget<Seg>();
   338	        img[s.at..s.at + s.bytes.len].move_from(s.bytes[..]);
   339	    }
   340	    let t3 = clock();
   341	    let gflt = minor_faults(false) - f0;
   342	    for id in ids { thread_wait(id); }
   343	    let t4 = clock();
   344	    var bad = check32("addr", addr, "oracle/p3_addr.u32");
   345	    let oaddr = load_u32("oracle/p3_addr.u32");
   346	    let pbucket = load_u8("p3_bucket.u8");
   347	    let psize = load_u32("p3_size.u32");
   348	    var want: u8[>..] = [];
   349	    load("oracle/p3_image.bin", want);
   350	    let imageBase = meta("imageBase");
   351	    var nbad = 0;
   352	    for i in Lm {
   353	        guard pbucket[i] < bBSS else { continue; }
   354	        let o = oaddr[i] - imageBase;
   355	        let n = psize[i] as i64;
   356	        if img[o..o + n] != want[o..o + n] {
   357	            if nbad < 5 { print("  symbol ", i, " (bucket ", pbucket[i], ") differs"); }
   358	            nbad++;
   359	        }
   360	    }
   361	    var note: u8[>..] = [];
   362	    format(note, "p3par goose (", W, " workers): ");
   363	    report(note, "spawn+load", t0, t1);
   364	    report(note, "3a", t1, t2);
   365	    format(note, "3b distributed (", if huge { "2 MB" } else { "4 KB" }, " pages, x", reps, ") ");
   366	    sort(times) { x, y => x < y };
   367	    format_flt(note, times[0], 1);
   368	    format(note, " / ");
   369	    format_flt(note, times[times.len / 2], 1);
   370	    format(note, " / ");
   371	    format_flt(note, times[times.len - 1], 1);
   372	    format(note, " ms min/median/max (slowest worker ");
   373	    format_flt(note, wmax, 1);
   374	    format(note, " ms, ", flt, " minor faults/rep in workers, ", hb, " bytes on huge pages)  ");
   375	    report(note, "gather: image alloc", t2b, t2c);
   376	    report(note, "segments into it", t2c, t3);
   377	    format(note, "(", gflt, " faults in the process, ", mhb, " image bytes on huge pages)  ");
   378	    format(note, "warm 3a + best 3b distributed ");
   379	    format_flt(note, ms(t1, t2) + times[0], 1);
   380	    format(note, " ms  ");
   381	    format(note, nbad, " placed symbols differ from oracle/p3_image.bin");
   382	    print(note);
   383	    bad += nbad;
   384	    print(if bad == 0 { "P3 OK" } else { "P3 FAIL" });
   385	    if bad != 0 { exit(1); }
   386	}
    86	- 2026-11-16T17:48:24Z merged codex-cp: CP1 template count 108,833 -> 66,046 (82.3% top-4K) but its operand-derived layout 6.3x slower; CP4 generated emitter 1.67x on covered segments, within noise; both P4 OK (lead rerun).
    87	- 2026-11-16T17:59:57Z merged levelfree builder: level count is not Futhark's cost (45 levels; per-level passes are); Goose async 1.3-1.5x; lead reran both checks (P2 OK). ATTACKS 3 updated.
    88	- 2026-11-16T18:10:45Z merged Futhark P4 (lead rebuilt + reran c/multicore: P4bf OK): 3.2x C on 1 core, ~5x on 8; slice-update loop inside a map = bulk copy.
    89	- 2026-11-16T18:20:04Z merged second attempts (Soufflé 3a no longer falls down: 1.9 s ranked chain; DuckDB P2 falls down after 3) and GraphBLAS column (P1/P2 OK, 9-24x tuned C, does not scale: frontiers below its threading threshold); lead reran every check.
    90	- 2026-11-16T18:20:53Z merged codex-kube: all 5 paradigms' P1-P3 cells correct on kube-apiserver; 1-thread scaling 1.5-2.5x; Halide 1a 6.7x flagged.
    91	- 2026-11-16T18:21:18Z ATTACKS 4: emit costs pieces not bytes (0.42 GB/s, patch 7.6%); width-relaxed templates 108,833 -> 66,046.
    92	
    93	**M3 (2026-11-16T18:40Z).** Five Opus builders and four Codex jobs in parallel, load 7-26 throughout. Second attempts
    94	settled two cells (Soufflé 3a now stands, DuckDB P2 falls down with three attempts written); the level-free floods
    95	corrected ATTACKS 3's premise in the paradigms (Futhark: 45 levels, the per-level passes are the cost; Goose already had
    96	10 supersteps, async 1.3-1.5x); GraphBLAS became a column (correct, 9-24x C, single-threaded by its own heuristics);
    97	every paradigm's P1-P3 cells are correct on kube-apiserver; problem 4 got a tuned C reference, a Futhark cell and a new
    98	attack (ATTACKS 4). **What did the oracle number do since the last milestone? If it stalled, what is the blocker, and what
    99	am I doing about it?** 16/20 + DuckDB 3 with 3 ATTACKS -> 17/20 + DuckDB 3 + GraphBLAS 2 with 4 ATTACKS. Not stalled,
   100	but slower: the three remaining cells are all problem 4 (Goose, Halide, Soufflé); Halide is building, Goose goes next.
   101	The 8-thread numbers are weak evidence because the machine was oversubscribed by my own helpers; reruns go in the Now line.
   102	- 2026-11-16T18:22:41Z launched Goose P4 builder, Futhark 3b follow-up, codex-cp7 (pieces vs time).
   103	- 2026-11-16T18:33:08Z merged Futhark 3b_blk (slice-update copy; lead rebuilt, P3b_blk OK both backends): 2.7x / 3.6x the old 3b.
   104	- 2026-11-16T18:35:42Z triaged M3 skeptic: 5 accepted (Halide fresh-image check added, ATTACKS 3 retitled/corrected, Halide kube anomaly = load, page-reuse caveat).
   105	
   106	2026-11-16 CP7: prepared contiguous runs in C (hole-free-neighbour fusion, whole-function blobs, sequence deduplication), plus an unmerged prepared control to distinguish fewer copies from hoisted metadata lookup. All variants pass bytes, unconditional function offsets and sorted relocations at 1/8 threads and constrained-team/fallback checks. Five interleaved 128-repetition measurements underway; preprocessing separately timed, logical prepared-input footprint counted.
   107	2026-11-16T18:31:29Z CP7 measurement: 55/55 oracle passes, five interleaved P4_REPS=128 runs, 1/8 threads, 12 GiB, load 2.53–4.85. Prepared unmerged D/1 48.474 ms vs p4par/1 94.695; A/1 43.981, B/1 44.689, C/1 46.589 ms. B has 17.43× fewer copies than D but only 1.08× lower time; fusion benefits at eight threads overlap noise. C stores 55,389 distinct runs / 48,985,021 bytes. Report, raw logs, reproducible summary and independent UBSan/negative-offset review committed; no new grid cell or ATTACKS entry, entry 4 refined.
   108	- 2026-11-16T18:36:46Z merged codex-cp7 (lead rerun all P4 OK): flattened per-item stream 2x p4; merging pieces 17x adds only 1.08x -> ATTACKS 4 corrected: indirection per piece is the cost.
   109	- 2026-11-16T18:47:54Z merged Halide P4 (1.5x/1.3x C), Goose P4 (ties p4par on 8), Futhark 4D (flat stream 4.5x its own emit); lead rebuilt + reran each. GRID P4 row, ATTACKS 4 cost split (pieces ~40 ns, holes ~5.8 ns, bytes ~13 ms).
   110	
   111	**M4 (2026-11-16T18:55Z).** Problem 4 went from a C reference to three correct paradigm cells and a tuned C in about
   112	1.5 hours. The attack moved twice: "patching is cheap, so the copy is the cost" (p4par) -> "pieces, not bytes" (ATTACKS 4
   113	first draft) -> "lookups per piece; flatten the stream" (codex-cp7's control D, then confirmed in Futhark at 4.5x and
   114	in the paradigms' top-K curves). Writing the draft before the control was run cost one correction; the control was
   115	cheap and should have come first. **What did the oracle number do since the last milestone? If it stalled, what is
   116	the blocker, and what am I doing about it?** 17/20 -> 19/20, ATTACKS 4 backed. The last cell is Soufflé P4, next in
   117	the Now line; after it the grid is full, and the work turns to the open attacks (holes, level sync in Halide).
    95	the output live on huge pages (Futhark's and Halide's buffers, Goose's arenas, DuckDB's blobs), because on this
    96	problem that matters as much as the loop.
    97	
    98	## 3. Reach: drop level synchronization where waiting is the cost (C under load, Goose), not where each level is many passes (Futhark)
    99	
   100	**Problem.** Reach (2), the same flood as entry 1.
   101	
   102	**Before.** Every data-parallel shape of the flood is level-synchronous: expand the frontier, barrier, swap. MinIO's
   103	reach has **111 levels in 7 rounds** (kube 103). p2par (OpenMP) pays one team barrier per level; Futhark pays ~26
   104	whole-array passes per level (45 levels; its lazy flood, 165 / 90 ms, ties its eager flood once eager's own decode is
   105	counted, so it is not a falls-down; corrected after the M3 review); Goose pays a message exchange per superstep and scales only 1.7x on 8 workers; DuckDB iterates 45 SQL rounds.
   106	
   107	**Observation.** On this shared 8-core VM the barrier is not a constant: with OpenMP's default spinning wait, p2par's
   108	8-thread flood takes **593-1071 ms instead of 15-27** (MinIO, ambient load from the builders; kube 856-1064 vs 28-103),
   109	because 111 times per solve all threads wait on whichever one was descheduled (codex-g, results/codex-g.md, 16:25-16:28Z;
   110	lead saw the same at 16:20Z). A passive wait policy removes most of it, but not under oversubscription.
   111	
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
   133	operations each; Goose's p2par already floods each partition to a local fixpoint, so it has 10 supersteps. In
   134	**Futhark the level count is not the cost**: a hybrid (parallel while the frontier is wide, sequential drain when
   135	narrow) cuts 45 levels / 1,211 parallel ops to 15 / 437 and is within noise of the original at 8 threads; a plain
   136	sequential worklist written in Futhark beats both (8 threads 67/102/130 vs 156/201/258 ms). The cost is the passes
   137	inside each level. In **Goose** an async flood (no supersteps, termination by message counts) is 1.3-1.5x the
   138	bulk-synchronous one at 8 workers (33/39/44 vs 48/53/63 ms, ranges apart in one window; lead one-round rerun 32 vs
   139	61); the limit after that is the busiest partition and ~1.04 M ids through the queues. In tuned C the same hybrid
   140	(ref/flat/p2hyb.c) drops 111 barriers to 12 but moves ~44% of pops to one thread (results/levels.md). So:
   141	dropping level sync pays where waiting on a descheduled worker is the cost (C under load, Goose), not where each
   142	level is many whole-array passes (Futhark); there the fix is fewer passes per level, which no variant here achieved.
   143	
   144	**What it says about the paradigms.** None of the grid's data-parallel systems can say "a fixpoint without levels":
   145	Futhark and Halide have no worklist; Soufflé's semi-naive evaluation is level-synchronous by construction; DuckDB's
   146	recursive CTE iterates rounds; Goose has the queues but no shared visited set. The C worklist is ~200 lines of
   147	mechanism (stealing, credits) for ~15 lines of rules. A build farm's machines are shared like this VM, so robustness
   148	under load is part of the speed. **Next:** a quiet-machine run (does async lose anything with nothing else running?),
   149	and whether a level-free flood also cuts Futhark's/Goose's cost (fewer, larger rounds: e.g. expand several levels per
   150	superstep, a "delta-stepping" of the flood).
   151	
   152	## 4. Code generation by shape: the emit costs indirection per piece, not pieces or bytes; flatten the stream, merge templates by width
   153	
   154	**Problem.** Code generation by shape (4): emit fc's 60.7 MB of MinIO function bodies from 108,833 byte templates,
   155	7.68 M operands and 17.6 MB of glue (ref/p4/README.md).
   156	
   157	**Before.** The obvious model, from copy-and-patch compilers (Xu and Kjolstad 2021; ideas/copy-and-patch.md): a
   158	template table plus a patch pass; speed is bandwidth, and the table should be keyed by typed shape (SHAPES.md: 4,096
   159	shapes cover 96.7% of nodes).
   160	
   161	**Observation.** (a) The plain C emit runs at **0.42-0.62 GB/s** (1 core, 98-145 ms medians across windows), 40-60x
   162	below this VM's read bandwidth, and patching holes is only **7.6%** of it (codex-p4par, P4_NOPATCH split). (b) It is
   163	not the number of pieces either (codex-cp7, results/codex-cp7.md, 5 interleaved runs, quiet machine, all P4 OK, lead
   164	rerun): precomputing each of the 2.38 M items as a flat blob with its holes rebased (control D, same piece count)
   165	emits in **48.3/48.5/48.9 ms vs p4's 88.9/98.4/98.6** (2x); merging pieces 2.3x (A: hole-free neighbours) or 17.4x
   166	(B: one run per function) then adds only 1.08-1.10x (A 44.0, B 44.7 ms). On 8 threads: p4par 16.1/18.4/22.2, D 13.7,
   167	A 11.9, B 12.3 ms median. **The cost is the indirection per piece** (item -> template -> segment -> hole range), not
   168	the copies. The paradigm cells' top-K curves give the same split from the other side (Halide, Goose: keep
   169	the commonest K templates, the rest as glue): copying all 60.7 MB as 136 K hole-free runs takes 12.8 ms (1 thread) /
   170	4.8 (8); each hole costs ~5.8 ns (8.0 M holes ~ 46 ms) and each template piece ~40 ns. So once the stream is flat, the
   171	**holes** are most of what is left: C's B variant (one run per function) at 44.7 ms is almost exactly its 8 M holes. (c) The typed shape does not fix the bytes (74.6% of nodes in the top 4,096 *byte* templates); one
   172	splitter is purely mechanical: disp8 exactly when the operand fits in int8, **0 exceptions in 4,864,669 holes**.
   173	
   174	**Attack.** Two independent steps. (1) Flatten the emit stream: resolve item -> template -> segment -> holes once
   175	(an extraction-time "decode", like ATTACKS 1's lazy decode in reverse: here the eager form wins because the emit is
     1	// bench.h: timing on a noisy machine (CHARTER.md): repeat the kernel in-process until one run takes >= 1 s,
     2	// five runs, report per-iteration min / median / max in ms.
     3	#pragma once
     4	#include <algorithm>
     5	#include <cmath>
     6	#include <functional>
     7	#include <vector>
     8	#include <ctime>
     9	struct Stat { double mn, md, mx; int k; };
    10	// BENCH_CPU=1: process CPU time instead of wall time (on a loaded machine, a 1-thread run's CPU time leaves out the
    11	// time it was descheduled; it still includes cache and SMT interference)
    12	static double benchNow() {
    13	    static int cpu = getenv("BENCH_CPU") ? atoi(getenv("BENCH_CPU")) : 0;
    14	    if (!cpu) return now();
    15	    struct timespec t;
    16	    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    17	    return t.tv_sec + t.tv_nsec * 1e-9;
    18	}
    19	static Stat timeRuns(std::function<void()> f, int runs = 5, double minRun = 1.0) {
    20	    double t = benchNow();
    21	    f();
    22	    double one = benchNow() - t;
    23	    int k = std::max(1, (int)std::ceil(minRun / std::max(one, 1e-6)));
    24	    std::vector<double> v;
    25	    for (int r = 0; r < runs; r++) {
    26	        t = benchNow();
    27	        for (int i = 0; i < k; i++) f();
    28	        v.push_back((benchNow() - t) / k * 1e3);
    29	    }
    30	    std::sort(v.begin(), v.end());
    31	    return {v[0], v[runs / 2], v[runs - 1], k};
    32	}
    33	#define STAT_FMT "min %8.2f  med %8.2f  max %8.2f ms (x%d)"
    34	#define STAT_ARGS(s) (s).mn, (s).md, (s).mx, (s).k

exec
/bin/bash -lc "ls systems/goose/bin; rg --files /home/exedev/paradigms/systems/halide/cells/build | head -n 15; nl -ba ref/p4/README.md | head -n 100; nl -ba systems/halide/CELLS.md | tail -n 18; nl -ba GRID.md | tail -n 24; nl -ba systems/goose/p3par.goose | sed -n '180,225p'" in /home/exedev/paradigms-w
 succeeded in 0ms:
p1
p1par
p2
p2lazy
p2par
p3
p3par
/home/exedev/paradigms/systems/halide/cells/build/layout_c1_w1.h
/home/exedev/paradigms/systems/halide/cells/build/layout_c0_w1.h
/home/exedev/paradigms/systems/halide/cells/build/layout_c1_w0.registration.cpp
/home/exedev/paradigms/systems/halide/cells/build/layout_c3_w0.registration.cpp
/home/exedev/paradigms/systems/halide/cells/build/layout_c1_w0.a
/home/exedev/paradigms/systems/halide/cells/build/layout_c3_w1.a
/home/exedev/paradigms/systems/halide/cells/build/layout_c3_w0.h
/home/exedev/paradigms/systems/halide/cells/build/reloc_f0_pf_v0.h
/home/exedev/paradigms/systems/halide/cells/build/layout_c0_w0.registration.cpp
/home/exedev/paradigms/systems/halide/cells/build/p3_layout.list
/home/exedev/paradigms/systems/halide/cells/build/layout_c0_w0.a
/home/exedev/paradigms/systems/halide/cells/build/reloc_f0_pt_v0.registration.cpp
/home/exedev/paradigms/systems/halide/cells/build/reloc_f0_pt_v0.a
/home/exedev/paradigms/systems/halide/cells/build/reloc_f0_pf_v32.a
/home/exedev/paradigms/systems/halide/cells/build/layout_c0_w1.registration.cpp
     1	# Problem 4: code generation by shape (the flat files)
     2	
     3	Every system in the grid reads these and nothing else. They are written by `ref/p4/extract.sh` into
     4	`data/minio-p4/` (git-ignored, about 340 MB for MinIO, plus the raw dump), from a copy of simd-lang's compiler
     5	fc (`ref/fc/src` = simd-lang `driver/fc` at `a99d791`) with one added file, `ref/fc/src/p4dump.go`, and
     6	one-line hooks in the walker and the encoder (`ref/fc/src/*.go` against simd-lang's: `diff -r`). That copy
     7	builds MinIO to a program byte-identical to the one it builds with the dump off, and that program runs
     8	(`--version`); extract.sh checks both every time.
     9	
    10	`ref/p4/check.sh` builds a plain single-core C program (`ref/p4/p4.c`) that reads only these files and
    11	reproduces the oracle exactly: that is the proof that the files say everything the problem needs. Read it
    12	next to this file. Conventions (raw little-endian arrays, CSR, strings, `meta.txt`) are `ref/flat/README.md`'s.
    13	
    14	## The problem
    15	
    16	fc compiles a Go function with one walk over its typed syntax tree, writing x86 bytes as it goes; every
    17	local lives in a frame slot below BP, a value is in AX (or X0) only between two steps of an expression. Each
    18	expression node (SHAPES.md's definition: an operator, a call, a selector, an index, a slice, a conversion, a
    19	literal, a closure; not a variable, a constant, a type or `pkg.Name`, which are its parent's operands) is the
    20	interval of bytes written while the walker was inside it. Its **own bytes** are that interval minus its
    21	children's intervals: segment 0, child 1, segment 1, ..., child n, segment n.
    22	
    23	A node's own bytes, with the operand-dependent fields zeroed, are its **template**; the zeroed fields are
    24	**holes**; the values they had are the node's **operands**. Problem 4 is: given each node's template and
    25	operands, and the bytes no node owns (**glue**: statements, prologue-free bodies of wrappers, epilogues),
    26	emit every function body exactly as fc did. Two steps: a scan (the length of every piece, then where it
    27	goes) and a scatter (copy the template bytes, patch each hole, including jumps, whose rel32 depends on where
    28	their target landed).
    29	
    30	The templates come from fc's own output, so the files hold every template the program needs; the measure of
    31	the idea is how few there are and how much the commonest cover (NOTES.md). A system may emit only the nodes
    32	of the commonest K templates and take the rest as glue; the oracle is the same.
    33	
    34	## Files
    35	
    36	| file | what |
    37	|---|---|
    38	| `meta.txt` | `nfn nitem nglue nnode ntmpl nshape nmode nsym nop nthole ntseg nmark ngrel code_bytes nrel` |
    39	| `fn_name.*` | per function: its symbol name (`<1>` appended for an ABIInternal-vs-ABI0 twin) |
    40	| `fn_item.u32` | CSR (`nfn+1`): the items of function `f`, in output order, are `[fn_item[f], fn_item[f+1])` |
    41	| `item_kind.u8 item_ref.u32 item_seg.u32` | per item: kind 0 glue (`ref` = glue id), kind 1 segment `seg` of node `ref` |
    42	| `glue.off.u64 glue.bin` | per glue id: its bytes, verbatim |
    43	| `glue_rel_off.u32` + `grel_off.u32 grel_siz.u8 grel_type.u16 grel_sym.u32 grel_add.i64` | CSR per glue id: its relocations (offset in the glue run, width, cmd/internal/objabi type, symbol, addend) |
    44	| `fn_mark_off.u32` + `mark_off.u32 mark_type.u16 mark_sym.u32 mark_add.i64` | CSR per function: its zero-width marker relocations (R_USEIFACE and the like), at `mark_off` from the body's start |
    45	| `node_tmpl.u32` | per node: its template |
    46	| `node_shape.u32 node_mode.u8` | per node: its typed shape (`shape.*`: explore/wholeprog's key, e.g. `bin+ local:i64 K:i64 ->i64`) and the walker's entry it came through (`mode.*`: expr, exprAs, exprTo, cond, call, lvalue, multi, elem, addr) |
    47	| `node_parent.u32 node_fn.u32` | per node: its parent node (0xFFFFFFFF: a statement's) and its function |
    48	| `node_op.u32` + `op_val.i64 op_aux.i64` | CSR per node: one operand per hole of its template, in hole order |
    49	| `tmpl_shape.u32 tmpl_mode.u8` | per template: the (shape, mode) all its nodes have |
    50	| `tmpl.off.u64 tmpl.bin` | per template: its bytes (the segments back to back, holes zeroed; relocated fields as fc left them, zero) |
    51	| `tmpl_seg_off.u32` + `tseg_end.u32` | CSR per template: the end offset of each segment in its bytes (`n_children + 1` segments) |
    52	| `tmpl_hole_off.u32` + `hole_at.u32 hole_w.u8 hole_enc.u8 hole_kind.u8 hole_rtype.u16` | CSR per template: its holes, sorted by offset: offset in the template bytes, width, encoding, kind, relocation type |
    53	| `shape.* mode.* sym.*` | strings; symbol 0 is "none" (the TLS relocation) |
    54	
    55	Hole kinds (what the operand is): 1 a BP displacement (a local, a temporary, a parameter's home), 2 an SP
    56	displacement (an outgoing argument), 3 a displacement from another base (a field or element offset through a
    57	pointer), 4 an immediate, 5 the frame size, 6 a jump target, 7 a symbol.
    58	
    59	**4. Emit.** Item `i`'s length is its glue run's length, or segment `s` of its node's template:
    60	`tseg_end[s] - tseg_end[s-1]` (`tseg_end[-1] = 0`). Items are laid back to back from 0 in item order
    61	(functions follow one another): `at[i]`. Function `f`'s body is `[at[fn_item[f]], at[fn_item[f+1]])`. Copy
    62	each item's bytes to `at[i]`; for a node segment, patch each of its template's holes with offset `h` in the
    63	segment's range `[lo, hi)`, at `P = at[i] + h - lo`, with its operand `(v, a)`:
    64	
    65	| encoding | what is written |
    66	|---|---|
    67	| 1 integer | the low `w` bytes of `v` (little-endian) |
    68	| 2 rel32 | `int32(at[v] + a - (P + 4))`: `v` is the item the jump lands in (same function), `a` the offset in it |
    69	| 3 relocation | nothing (the template's bytes stay); a relocation `(P, w, rtype, symbol v, addend a)` |
    70	
    71	Glue relocations go at `at[i] + grel_off`, markers at the function's start `+ mark_off`.
    72	Oracle: `oracle/p4_code.bin` (every body fc wrote, functions in `fn_name` order; `oracle/p4_fn_off.u64` its
    73	`nfn+1` offsets) must equal the output byte for byte, and the relocations, as a sorted list of
    74	`(offset, type, symbol, addend, width)`, must equal `oracle/p4_rel_{off.u64,siz.u8,type.u16,sym.u32,add.i64}`.
    75	
    76	## What the extraction did for the systems
    77	
    78	The body is fc's before the prologue goes in front (the prologue needs the final frame size; everything in
    79	the body is BP-relative, so no body byte depends on it except the frame-size holes) and before the object
    80	writer: relocations are fc's own list, with symbol names interned. Jumps fc took back (a `jmp` to the next
    81	instruction, `place` in gen.go) are gone from the bytes and from the nodes. Which template a node gets is
    82	given: fc chose it (registers, encodings, whether a value was spilled); a system that picks templates itself
    83	from `(shape, mode)` and operand classes is the next problem, and NOTES.md says how far apart the two are.
    84	
    85	## Prepared runs (CP7)
    86	
    87	`p4runs.c DATA A|B|C|D` prepares a run representation once, times that preparation
    88	separately, then times repeated emission with `P4_REPS`. A absorbs hole-free
    89	neighbours; B makes one zeroed-hole blob per function; C deduplicates B by template
    90	sequence and byte/hole shape; D keeps the original item boundaries as a prepared
    91	control. These trade additional prepared input for less runtime lookup work.
    92	`check-runs.sh DATA` checks bytes, every function offset and sorted relocations;
    93	`bench-runs.sh DATA` compares them with p4/p4par in five interleaved rounds at 1/8
    94	threads. Counts, input sizes, timing scope and conclusions: `results/codex-cp7.md`.
    23	**3b. relocate** — correct: `$B/p3 $D 3b` (image 0 differ, 0 errors). warm 78-82 / **24.9-27**; fresh output 4 KB
    24	pages 113-119 (19,109 faults) / 42.7-45 (19,187); **2 MB pages 84.6-88 (39 faults) / 27-30.8 (170)** · e2e (3a+3b)
    25	0.21-0.25 / 0.10-0.12 s · 106 lines. Easy: ragged copy, 8-kind select chain. Forced: values stored byte by byte with
    26	scratch bytes for unwritten ones (a non-innermost where is not a loop bound); chunked parallel. ATTACKS 2: yes, the
    27	caller maps the output MAP_HUGETLB; faults 19 K -> 39, 1.3-1.6x faster, but 4-5x C on 2 MB pages.
    28	
    29	**4. codegen by shape** — correct: P4 OK (`HL_NUM_THREADS=8 $B/p4 $D4 e2e lay_t_lf emit_t_i2_v32_p1`, D4=data/minio-p4:
    30	code == oracle/p4_code.bin, fn offsets == p4_fn_off.u64, 1,780,281 relocations sorted == p4_rel_*; every variant and
    31	P4_TOPK=0..all checked). 2026-11-16T18:31Z, load 4.1-4.9, min-max of 5; 1 thread = CPU time. templates (template-only
    32	index) 9.4-9.6 / **6.8-7.7** · layout 17.9-18.7 / **4.9-5.6** (p4.c 6.5-7.0, p4par 8 thr 1.3-2.0) · emit 157-162 /
    33	**23.0-25.1** (p4.c 103-112, p4par 97-99 / 16.4-20.1) · gather-form emit 237-251 / 56-72 · e2e process 0.21-0.24 /
    34	0.07-0.10 s (2 MB pages) · gen_p4 345 + driver 288 lines. Easy: lengths + two-column scan (at, rat), rel32 as a gather
    35	of at, the relocation list as a scatter to rat[i]+k (no C host needed), chunked parallel, caller-owned 2 MB output.
    36	Forced: the template index (CSR inverse scatter, binary search as an RDom update); the vector copy only as a 32-byte
    37	read-modify-write in parity passes (the obvious split+vectorize: 26 s); the patch as an 8-lane read-modify-write;
    38	entry-0 indices on untaken select sides (Halide evaluates both: 200 -> 160 ms). Impossible: fusing copy and patch
    39	(two updates of one Func; compute_with refused); exact-width stores in a vector loop. Top K templates (rest as glue):
    40	emit is per piece (~42 ns) and per hole (~5.8 ns), not per byte (all-glue copy of 60.7 MB: 12.8 / 4.8 ms).
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
    58	| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood over given edges 97 / 58; lazy (own decode) 165 / 90 vs eager with its own decode >= 97 + 47 decode + an unwritten CSR build: **a tie at best, not a win** for ATTACKS 1 (corrected after review: the earlier 'slower than eager' compared unlike work). Cost: 111 levels x ~25 passes each. | **P2 OK** (eager + lazy). 8 thr: lazy 79-90, eager 70-127 (tuned C 25.7-29.1 / 12.7-14.6). Lazy is 1.3-1.5x *slower* than eager in Halide. Forced: no fixpoint (one pipeline call per level, ~100 levels), no worklist; `atomic()` dropped (mutex per element, wrong live sets). | **P2 OK** (eager + lazy). Flood over given edges 1227-1467 / 431. With its own decode: **lazy 2.9-3.2 s vs eager ~6.5 s on 1 thread, 960 ms vs ~1.6 s on 8** (decodes 1.73 M of 9.4 M relocations, 3 rules): ATTACKS 1 holds inside Soufflé; ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: slow as implemented (45 rounds x ~25 statements, hash tables rebuilt each round); Third attempt (M3, one recursive query per method round, `p2rec.sql`; lead rerun 18:22Z P2 OK): 8 threads setup 577-871 + flood 1438-2234 ms, 1.2x the old cell overall. **Falls down after three attempts**: each recursive step rescans all 2.7 M edges; DuckDB has no fast lookup by id (an index pays below ~50 rows, frontiers are 16-24 K). Lazy ties eager. |
    59	| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down: one image array on 8 workers (gather +142-171). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). **3b_blk** (M3: the copy as a sequential slice-update loop inside a map over 64 blocks = memcpy; lead rebuilt + reran P3b_blk OK 18:36Z): 60.4/62.0 (1) / **23.5/30.5 (8)** vs old 3b 159.7/166.7 / 96.4/110.9 same window, load 5: 2.7x / 3.6x, ranges apart. Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK**. 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a: second attempt `layout_rank.dl` (M3; lead rebuilt from source, P3 OK 18:21Z): the order by a ranked successor chain, no pair counting, no C functor: 1 thread 1930 (median, was 8270), 8 threads 1725/1937/3541 (was 2860/3388/7055 same runs), load 6-22. **No longer falls down**, but ~100x plain C: the 1,763-step unit walk is the limit. 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
    60	| 4. codegen by shape (stretch) | **P4 OK** (lead rebuilt + reran 1/8 workers 18:47Z). Workers own ranges of whole functions, no messages while computing: layout+emit 97.1/98.3 (1) / **17.6/21.3 (8)** vs p4.c 105.6 / p4par 17.1, within noise; same 698 M instructions. The join of the parts is the cost: 120-260 ms through queues, 37-63 ms writing each range into the file. 482 lines + 40 C | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). **4D** (flattened stream, ATTACKS 4; lead reran P4D OK both backends 18:49Z): emit 65.1/67.8 (1) / **23.4/26.9 (8)** vs 4bf 284/303 / 95/113 same window, 4.5x / 4.2x; the flattening (`prepare`, 50 lines) 324 / 138 ms, so it pays only if the stream is reused | **P4 OK** (lead rebuilt + reran 1/8 18:46Z). Emit **157-162 (1) / 23.0-25.1 (8)** vs p4.c 103-112 / p4par 16.4-20.1 (load 4): 1.5x / 1.3x; layout 18 / 5 (+ template index 9.5 / 7). Relocations by a scatter to the layout's own slot ranges (no C host). Forced: entry-0 indices on untaken select sides (Halide loads both: 200 -> 160 ms); the vector copy only as 32-byte read-modify-write in parity passes (obvious form 26 s). 345 + 288 lines | not attempted | not attempted |
    61	
    62	Halide column merged 17:17Z (lead rebuilt every generator and driver in a copy; P1/P2/P3 OK; `systems/halide/CELLS.md`).
    63	DuckDB column merged 17:13Z (lead reran every check in a copy, pipeline included; `systems/duckdb/CELLS.md`).
    64	Soufflé column merged 16:58Z (lead rebuilt all 8 programs from source; every check OK; `systems/souffle/CELLS.md`).
    65	Futhark column merged 16:42Z (lead rebuilt from source; every cell OK; `systems/futhark/CELLS.md`). Surprise: `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, no code change, makes most Futhark cells 1.2-1.7x faster.
    66	Goose column merged 16:35Z; lead reran every check (P1/P2/P3/pipeline OK, a source rebuild of p2par passes). Long form and lines: `systems/goose/CELLS.md`, `NOTES.md`.
   180	        let at = ((punit[i] - ulo) * nBuckets + pbucket[i]) * nClasses + pclass[i];
   181	        out.push(next[at]);
   182	        next[at] += occupied(i);
   183	    }
   184	    qput(AddrPart { lo: lo, vals: out[..] });
   185	    let t1 = clock();
   186	    // ---- 3b: this worker's part of the image, P3_REPS times (main starts each rep)
   187	    let nimg = meta("image_bytes");
   188	    let a0 = part_lo(nimg, k, W);
   189	    let a1 = part_lo(nimg, k + 1, W);
   190	    let reps = env_int("P3_REPS", 1);
   191	    let huge = env_set("P3_THP");
   192	    var seg: u8[>..] = [];
   193	    var nerr = 0;
   194	    var t2 = 0.0;
   195	    for rep in reps {
   196	        let g = qget<Mail<T, Go>>();
   197	        seg.clear();
   198	        let f0 = minor_faults(true);
   199	        let ta = clock();
   200	        let hb = fresh_pages(seg, a1 - a0, huge);
   201	        nerr = build_segment(a0, a1, seg);
   202	        t2 = clock();
   203	        qput(Done { k: k, ms: (t2 - ta) * 1000.0, faults: minor_faults(true) - f0, huge: hb });
   204	    }
   205	    let g = qget<Mail<T, Go>>();
   206	    qput(Seg { at: a0, bytes: seg[..] });
   207	    if nerr != 0 { print("worker ", k, ": ", nerr, " relocation errors"); }
   208	    var note: u8[>..] = [];
   209	    format(note, "  worker ", k, ": symbols ", lo, "..", hi, ", image ", a0, "..", a1, "; ");
   210	    report(note, "3a", t0, t1);
   211	    if env_set("P3TRACE") { print(note); }
   212	}
   213	
   214	thread_fn worker(k: i64, W: i64) {
   215	    match k {
   216	        0 => run(k, W, [0 as u8; 1]),
   217	        1 => run(k, W, [0 as u8; 2]),
   218	        2 => run(k, W, [0 as u8; 3]),
   219	        3 => run(k, W, [0 as u8; 4]),
   220	        4 => run(k, W, [0 as u8; 5]),
   221	        5 => run(k, W, [0 as u8; 6]),
   222	        6 => run(k, W, [0 as u8; 7]),
   223	        7 => run(k, W, [0 as u8; 8]),
   224	        _ => abort("at most 8 workers"),
   225	    }

exec
/bin/bash -lc 'P4_JOIN=queue P4_REPS=1 systems/goose/bin/p4 data/minio-p4 1' in /home/exedev/paradigms-w
 exited 127 in 0ms:
/bin/bash: line 1: systems/goose/bin/p4: No such file or directory

exec
/bin/bash -lc 'systems/goose/bin/p3 data/minio --pipeline' in /home/exedev/paradigms-w
 succeeded in 238ms:
addr: 360315 entries, 0 differ from oracle/p3_addr.u32
p3 goose (1 thread, pipeline): load 88.3 ms  3a layout 15.5 ms  3b inputs 41.6 ms  3b (fresh image, 4 KB pages, x1) 93.0 / 93.0 / 93.0 ms min/median/max, 26731 minor faults/rep; 76359886 bytes, 2053801 relocations, 0 errors; 0 placed symbols differ from oracle/p3_image.bin
P3 pipeline OK

exec
/bin/bash -lc "nl -ba systems/halide/CELLS.md | sed -n '29,42p'; nl -ba results/second-attempts.md | sed -n '64,87p'; nl -ba results/levelfree.md | sed -n '47,82p'; nl -ba PLAN.md | tail -n 12; git status --short" in /home/exedev/paradigms-w
 succeeded in 0ms:
    29	**4. codegen by shape** — correct: P4 OK (`HL_NUM_THREADS=8 $B/p4 $D4 e2e lay_t_lf emit_t_i2_v32_p1`, D4=data/minio-p4:
    30	code == oracle/p4_code.bin, fn offsets == p4_fn_off.u64, 1,780,281 relocations sorted == p4_rel_*; every variant and
    31	P4_TOPK=0..all checked). 2026-11-16T18:31Z, load 4.1-4.9, min-max of 5; 1 thread = CPU time. templates (template-only
    32	index) 9.4-9.6 / **6.8-7.7** · layout 17.9-18.7 / **4.9-5.6** (p4.c 6.5-7.0, p4par 8 thr 1.3-2.0) · emit 157-162 /
    33	**23.0-25.1** (p4.c 103-112, p4par 97-99 / 16.4-20.1) · gather-form emit 237-251 / 56-72 · e2e process 0.21-0.24 /
    34	0.07-0.10 s (2 MB pages) · gen_p4 345 + driver 288 lines. Easy: lengths + two-column scan (at, rat), rel32 as a gather
    35	of at, the relocation list as a scatter to rat[i]+k (no C host needed), chunked parallel, caller-owned 2 MB output.
    36	Forced: the template index (CSR inverse scatter, binary search as an RDom update); the vector copy only as a 32-byte
    37	read-modify-write in parity passes (the obvious split+vectorize: 26 s); the patch as an 8-lane read-modify-write;
    38	entry-0 indices on untaken select sides (Halide evaluates both: 200 -> 160 ms). Impossible: fusing copy and patch
    39	(two updates of one Func; compute_with refused); exact-width stores in a vector loop. Top K templates (rest as glue):
    40	emit is per piece (~42 ns) and per hole (~5.8 ns), not per byte (all-glue copy of 60.7 MB: 12.8 / 4.8 ms).
    64	| attempt 3 (p2rec) | 577-643-871 + **1438-1666-2234** | 2168-2336-3401 | 2058-2104-2220 + 2761-2821-3134 | 5060-5249-7048 |
    65	| attempt 2 (p2loop), same window | 112-128-175 + 2362-2698-3159 | 2502-2709-3104 | 201-221-250 + 2879-3375-3804 | 3708-4503-5067 |
    66	
    67	On 8 threads the flood is 1.6x faster (the ranges do not overlap) and the whole cell is ~1.2x faster (2.3 vs 2.8 s,
    68	~140x the tuned C). On 1 thread the cell is slower because the prep grows to 2.1 s.
    69	
    70	Verdict: **falls down** after three attempts. Both reasons are measured:
    71	1. **Every iteration of a recursive CTE re-runs its join with the whole edge table as the probe side.** It scans
    72	   2.7 M rows however small the frontier is. Round 1 alone is 35 BFS levels, with frontiers of 16 to 24 K
    73	   (`systems/duckdb/p2levels.py`). So the cost is levels x edges, the same as attempts 1-2.
    74	2. **DuckDB has no lookup for a frontier-sized probe that avoids a scan or a hash build.**
    75	   - An ART index on `src` is used in a join only when the build side is tiny. On 5 M synthetic edges
    76	     (`systems/duckdb/x_artjoin.sql`), a 40-row frontier takes 2 ms with the index vs 9-19 ms without. A 2,000-row
    77	     frontier takes 12 vs 15 ms, even with `dynamic_or_filter_threshold` and `index_scan_max_count` raised.
    78	   - A dense array held as a `getvariable` LIST is copied on every call: 1,000 probes take 7.7 s.
    79	
    80	   That rules out the persistent-index/frontier shape. Batching several levels per statement would need the E^k
    81	   edge relation.
    82	
    83	`dk.c` gained `DK_ALLROUNDS=1`, which prints every `.repeat` round.
    47	Times, ms (warm, `c` backend = 1 core, `multicore` stock runtime = 8 threads), two interleaved windows:
    48	
    49	| variant | 1 core, 17:40Z (5 rounds) | 8 threads, 17:40Z | 1 core, 17:47Z (7 rounds) | 8 threads, 17:47Z |
    50	|---|---|---|---|---|
    51	| orig | 137 / 171 / 313 | 156 / 201 / 258 | 141 / 265 / 416 | 144 / 245 / 388 |
    52	| hyb lo 0 | 147 / 176 / 260 | 146 / 187 / 242 | | |
    53	| hyb 1 K | 116 / 119 / 168 | 102 / 178 / 339 | 120 / 178 / 312 | 138 / 184 / 288 |
    54	| hyb 4 K | 161 / 197 / 280 | 129 / 171 / 335 | 116 / 161 / 246 | 152 / 231 / 273 |
    55	| hyb 4 K, budget 2 K | 166 / 212 / 382 | 209 / 234 / 240 | | |
    56	| `2seq` | 79 / 156 / 175 | **67 / 102 / 130** | 73 / 131 / 181 | 88 / 112 / 160 |
    57	| bat C 64 | 220 / 340 / 555 | 211 / 370 / 490 | 222 / 356 / 474 | 263 / 355 / 540 |
    58	| bat C 8 | 199 / 313 / 687 | 159 / 266 / 662 | | |
    59	
    60	Reading:
    61	- Cutting levels 45 -> 15 (parallel ops 1,211 -> 437) is **within noise** at both widths. If the per-level cost
    62	  were the cost, the hybrid would sit near `2seq`; it sits near the original.
    63	- A **sequential worklist in Futhark beats the level-synchronous flood**: 1 core min 73-79 vs 137-141, medians
    64	  131-156 vs 171-265 (ranges overlap); on 8 threads `2seq` (which uses one) beats the 8-thread flood with ranges
    65	  apart in the first window (67-130 vs 156-258) and overlapping in the second (88-160 vs 144-388).
    66	- So **in Futhark the cost is per element, not per level**: a data-parallel level touches each frontier item and
    67	  each edge in ~26 passes (filters, expands = scan + scatter + segmented iota, scatters, the dedup), each with
    68	  its own allocation; a sequential pop touches them once. At 1 core (no barriers at all) the 15 wide levels that
    69	  remain in the hybrid (60% of the pops) cost about as much as the whole sequential flood. On 8 threads,
    70	  oversubscribed, the wide levels' parallelism does not repay their passes.
    71	- Attempt 2 loses (1.5-2.5x slower than orig): rounds barely drop (35-45; 25 even with one unbounded chunk,
    72	  because iface and method rules still need rounds), chunks re-expand symbols that several of them reach
    73	  (up to +27% expansions, +32% edges), and every chunk's fixed-size buffers are initialised each round (Futhark
    74	  has no uninitialised array: a `replicate` per buffer per round).
    75	- A Futhark-specific trap found on the way: the drain's stack first grew by `++` (an existential-size loop
    76	  variable); the generated C then did several memory-block reference-count updates per push, and the drain ran
    77	  **3x slower** (255 vs 76 ms, 1 core). A fixed-size stack (bounded by `hi + widest row + 3 x methods`) fixed it.
    78	
    79	**Verdict, Futhark: no.** The level count is not the cost; the per-element passes of the flattened level are.
    80	The level-free version that wins is the one with no data parallelism at all (`2seq`), which is also the
    81	shortest-reading one (a worklist loop with in-place updates), but it is a sequential program written in Futhark.
    82	Halving the levels changes nothing measurable.
    55	- 2026-11-16T18:55Z M4: problem 4 correct in Futhark (4bf, flat 4D), Halide and Goose, plus tuned C (p4par) and C
    56	  kill tests (CP1 split, CP4 killed, CP7: indirection not pieces). Oracle number: **19/20** (+ DuckDB 3, GraphBLAS 2);
    57	  **4 ATTACKS backed**.
    58	
    59	Now (2026-11-16T18:55Z): M4 done; M4 skeptic fired (reviews/inbox/*-M4.md: triage into REVIEWS.md). No builders
    60	running. Next, in order: (1) Soufflé P4 (an Opus builder in a worktree; brief = ideas/BUILDER.md + ref/p4/README.md +
    61	ATTACKS 4; Datalog has no bytes, so the rules compute positions and patch values and a C writer stores bytes, as its
    62	3b did: that is a finding either way) -> 20/20. (2) A Halide batched-level or async flood vs its 100 pipeline calls
    63	(M3 review 5). (3) The hole cost (ATTACKS 4): after flattening, 8 M holes ~46 ms: test fewer holes per byte (merge
    64	adjacent holes of one operand, or holes whose value repeats within a function) as a Codex C job. (4) Quiet reruns of
    65	the 8-thread grid numbers while nothing else runs (M3 numbers were at load 7-26): systems/*/bench.sh one at a time.
    66	(5) A Halide cell on the flattened stream (Futhark gained 4.5x).
?? data
?? systems/souffle/out/p1_builtin.u32
?? systems/souffle/out/p1_canon.u32
?? systems/souffle/out/p1_reltarget.u32
?? systems/souffle/out/p1c_canon.u32
?? systems/souffle/out/p2_live.u32
?? systems/souffle/out/p3_image.out
?? systems/souffle/out/relocate_error_kind.u8
?? systems/souffle/out/relocate_error_pos.i64

codex
1. **Goose P4 has an incomplete oracle check.** Its [checker](/home/exedev/paradigms-w/systems/goose/p4.goose:357) compares bytes and relocations, but uses oracle function offsets only to locate byte differences; it never compares computed function offsets. That is narrower than “output unchecked.” The P4 executable was missing, so I couldn’t rerun it. I did run `systems/goose/bin/p3 data/minio --pipeline`: **360,315 addresses, zero differences; zero image differences; P3 pipeline OK**.

2. **The earlier DuckDB P2 verdict was premature; the current one has substantially better support.** The first attempts retained **45 rounds × ~25 statements**. The [third attempt](/home/exedev/paradigms-w/results/second-attempts.md:70) uses recursive queries and tests ART indexes with tuning: **40 probes: 2 ms indexed versus 9–19 ms; 2,000 probes: 12 versus 15 ms**. I wouldn’t repeat the old “ignored indexes” objection against this revision. Soufflé’s premature “no sort” verdict was also withdrawn.

3. **Goose P4’s “ties C” comparison leaves out assembling the result.** [GRID](/home/exedev/paradigms-w/GRID.md:60) compares **17.6–21.3 ms** distributed layout/emission against C’s **17.1 ms**, while Goose additionally needs **120–260 ms** through queues or **37–63 ms** writing worker ranges. Also, Halide’s **157–162 ms** single-thread P4 number is explicitly **CPU time**, despite GRID’s blanket wall-time description ([CELLS](/home/exedev/paradigms-w/systems/halide/CELLS.md:31)).

4. **Futhark’s sequential worklist deserves the follow-up.** It takes **67/102/130 ms**, versus **156/201/258 ms** for the eight-thread parallel flood; another window overlaps. [The report](/home/exedev/paradigms-w/results/levelfree.md:63) acknowledges this, but [ATTACKS](/home/exedev/paradigms-w/ATTACKS.md:144) still says “Futhark … [has] no worklist.” The surprise is documented but not settled: rerun on a lightly loaded machine and compare generated-code allocation/pass counts.

5. **No entire ATTACKS entry is measurement-free now.** The remaining unsupported extension is **entry 3 applied to Halide**: approximately **100 pipeline calls** are blamed without a batched-level comparison. Measure original versus batched closure, identical inputs and live-set checks, counting pipeline calls and passes, with five interleaved runs. [PLAN](/home/exedev/paradigms-w/PLAN.md:62) explicitly says this remains owed.

6. **No qualifying JOURNAL passage found.** The recent [M4 passage](/home/exedev/paradigms-w/JOURNAL.md:116) says: “The last cell is Soufflé P4, next in the Now line”; it does not decline or shrink work because of time, usage, or session ending.

No files edited.
tokens used
67,960
1. **Goose P4 has an incomplete oracle check.** Its [checker](/home/exedev/paradigms-w/systems/goose/p4.goose:357) compares bytes and relocations, but uses oracle function offsets only to locate byte differences; it never compares computed function offsets. That is narrower than “output unchecked.” The P4 executable was missing, so I couldn’t rerun it. I did run `systems/goose/bin/p3 data/minio --pipeline`: **360,315 addresses, zero differences; zero image differences; P3 pipeline OK**.

2. **The earlier DuckDB P2 verdict was premature; the current one has substantially better support.** The first attempts retained **45 rounds × ~25 statements**. The [third attempt](/home/exedev/paradigms-w/results/second-attempts.md:70) uses recursive queries and tests ART indexes with tuning: **40 probes: 2 ms indexed versus 9–19 ms; 2,000 probes: 12 versus 15 ms**. I wouldn’t repeat the old “ignored indexes” objection against this revision. Soufflé’s premature “no sort” verdict was also withdrawn.

3. **Goose P4’s “ties C” comparison leaves out assembling the result.** [GRID](/home/exedev/paradigms-w/GRID.md:60) compares **17.6–21.3 ms** distributed layout/emission against C’s **17.1 ms**, while Goose additionally needs **120–260 ms** through queues or **37–63 ms** writing worker ranges. Also, Halide’s **157–162 ms** single-thread P4 number is explicitly **CPU time**, despite GRID’s blanket wall-time description ([CELLS](/home/exedev/paradigms-w/systems/halide/CELLS.md:31)).

4. **Futhark’s sequential worklist deserves the follow-up.** It takes **67/102/130 ms**, versus **156/201/258 ms** for the eight-thread parallel flood; another window overlaps. [The report](/home/exedev/paradigms-w/results/levelfree.md:63) acknowledges this, but [ATTACKS](/home/exedev/paradigms-w/ATTACKS.md:144) still says “Futhark … [has] no worklist.” The surprise is documented but not settled: rerun on a lightly loaded machine and compare generated-code allocation/pass counts.

5. **No entire ATTACKS entry is measurement-free now.** The remaining unsupported extension is **entry 3 applied to Halide**: approximately **100 pipeline calls** are blamed without a batched-level comparison. Measure original versus batched closure, identical inputs and live-set checks, counting pipeline calls and passes, with five interleaved runs. [PLAN](/home/exedev/paradigms-w/PLAN.md:62) explicitly says this remains owed.

6. **No qualifying JOURNAL passage found.** The recent [M4 passage](/home/exedev/paradigms-w/JOURNAL.md:116) says: “The last cell is Soufflé P4, next in the Now line”; it does not decline or shrink work because of time, usage, or session ending.

No files edited.
