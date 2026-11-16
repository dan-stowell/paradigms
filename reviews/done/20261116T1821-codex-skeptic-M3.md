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
session id: 01a1e597-b721-7611-9a49-0593a8d2d134
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
I’ll read the repository’s review rules and evidence, then run the relevant oracle check if the environment permits it.
exec
/bin/bash -lc "pwd; rg --files -g 'AGENTS.md' -g 'CLAUDE.md' -g 'README*' -g 'CHARTER.md' -g 'PLAN.md' -g 'STATUS.md' -g 'GRID.md' -g 'ATTACKS.md' -g 'JOURNAL.md'" in /home/exedev/paradigms-w
 succeeded in 0ms:
/home/exedev/paradigms-w
STATUS.md
README.md
GRID.md
CHARTER.md
ATTACKS.md
ref/p4/README.md
PLAN.md
JOURNAL.md
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

Now (2026-11-16T17:20Z, session 3): M3/M4 running. Opus builders in worktrees (Agent, isolation=worktree): Halide P4,
Futhark P4, DuckDB P2 + Soufflé 3a second attempts (results/second-attempts.md), GraphBLAS column (systems/graphblas),
level-free floods in Futhark + Goose (results/levelfree.md). Codex in ~/paradigms-w branch codex-kube: every paradigm's
cells on kube-apiserver (results/codex-kube.md); codex-p4par merged; Codex in ~/paradigms-c branch codex-cp: CP1 width-relaxed templates + CP4 generated emitter (results/codex-cp.md). Next: as each lands, rebuild + rerun its
oracle check in a copy, merge, paste CELLS into GRID.md; then M3/M4 milestone steps (LOOP.md). If the session was lost,
check `git branch -a` for the builders' worktree branches and their reports before relaunching.
# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T17:22Z. **M1 + M2 done**: Goose, Futhark, Halide, Soufflé and DuckDB are each correct on problems 1-3 (lead-verified).
Oracle number: **16/20 cells + DuckDB 3; 3 ATTACKS backed by measurement.** No paradigm beats tuned C on any cell (Halide is
closest, 1.2-2.5x; Soufflé furthest, 30-200x); the distances and their causes are in GRID.md and MORNING.md.
ATTACKS 1: decode in the flood, 2x the eager graph build (C). 2: huge pages: relocate at the VM's bandwidth bound
(5.4-6.8 ms on 8), and `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` gets it with no code. 3: drop level sync: 111 levels cost
every paradigm; a worklist flood is 1.7x passive OpenMP under overload, 25x default spinning.
Next (M3/M4): problem 4 in two paradigms, kube-apiserver for paradigm cells, second attempts on falls-downs, GraphBLAS.

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
| 4. codegen by shape: emit 136,634 bodies (60.7 MB) from 108,833 byte templates + 7.68 M operands | | fc's tree walker (no separate phase time) | 13-17 (lay out 2.38 M pieces) + 139-147 (emit), load 3.5; lead rerun 16:05Z: 18.4 + 162.4 | p4par (codex 17:27Z, load ~20, 5 interleaved, min/med/max): 1 thread layout 7.8/7.9/10.4 + emit 107/118/150; **8 threads 2.9/4.3/7.8 + 51/66/68** (vs p4 116/145/174 same window). Patching is 7.6% of 1-thread emit: the copy of 2.38 M ~25-byte pieces at ~0.6 GB/s is the cost (per-item, not bandwidth). [results/codex-p4par.md](results/codex-p4par.md) | `ref/p4/check.sh` -> P4 OK (`ref/p4/README.md`) |

**Floors vs this VM.** FLOOR.md's floors imply ~87 GB/s (minio resolve: 119 MB in 1.36 ms); this VM streams **25 GB/s read on
1 thread, 36 GB/s read / 27 GB/s write on 8** (16:16Z, load 5, a sum and a fill over 1 GB). Floors here are ~2.4x lower than the
column says. Per-kernel bandwidth bound for flat 3b: 76 MB symbol bytes + ~33 MB relocations read, 76 MB written -> ~3-6 ms on
8 threads, which p3par on 2 MB pages (5.4-6.8 median) meets.

Tuned and plain flat rows re-measured 2026-11-16T15:56Z at load 0.8, 3 interleaved rounds (`ref/flat/bench.sh`); hlink's resolve/deadcode from `COLD=0 ref/hlink/bench.sh minio build/hlink 3` right after.

**kube-apiserver** (second dataset, extracted 2026-11-16T15:55Z, 2.2 GB of flat files, 13.27 M slots): every plain and
tuned C reference reproduces its oracles (`ref/flat/check.sh data/kube-apiserver`, loaded machine): p1 434 + 88 ms,
p1c 550 ms, p2 66.5 ms (926,486 live), p3a 30.4 ms, p3b 67.8 ms (72.6 MB, 2.82 M relocations); p1par 26.2 ms and
p2par 19.6 ms on 8 threads. Its oracles come from hlink alone so far (MinIO's live list was also checked against fclink).

**Paradigm cells on kube-apiserver** (codex-kube, 17:21-18:16Z, merged 18:30Z; [results/codex-kube.md](results/codex-kube.md)): every P1-P3 cell of Goose, Futhark, Halide, Soufflé and DuckDB, unchanged, reproduces kube's oracles at 1 and 8 threads (lead spot-checked Halide P1 and Goose P2 at 18:28Z). Load was 17-26 for most of the window, so 8-thread kube/MinIO ratios (2-10x) mostly measure the load; the 1-thread ratios, 1.5-2.5x for a dataset ~1.4x MinIO's ids and 1.4x its live set, say no cell has a cost that grows faster than the data except Halide 1a (6.7x on 1 thread: to recheck quiet).

Floors are bottom-up's FLOOR.md model (same CPU model, the 2026-11-13 VM); they count the raw object bytes hlink
reads, so for the flat files they are an upper bound on the floor.

## The paradigms

Added columns (M3): **GraphBLAS** (SuiteSparse 10.5.2, JIT on; lead rebuilt + reran 18:24Z, P1/P2 OK, all files `cmp`). 8 threads warm, min/med/max, load 7-10, tuned C same window: **P2 reach 179/186/213** (p2par 15/20/26: 9x); 1 thread 192-203, i.e. it does not scale (frontier ~7 K is below SuiteSparse's per-call threading threshold: 379 of 382 vxm calls on one thread); 178 synchronous steps (97 BFS + 81 iface levels) in 7 rounds; pull direction 40x slower. **1a** 268/306/339 (18x p1par; the 128-bit key must be hashed into a 60-bit index and checked), **1b** 162/194/203 (21x), **1c** 279/349/392 (24x). P3: not attempted (no scan; the round-up-then-add fold is no monoid). Easy: reach is ~the shortest legible form (masked vxm); forced: the iface rule as its own closure per round. `systems/graphblas/CELLS.md`.

| problem | Goose | Futhark | Halide | Soufflé | DuckDB (added 15:58Z) |
|---|---|---|---|---|---|
| 1. resolve | **P1 OK** (1a/1b/1c). 1a 79-115 / 37-57; 1b 39-57 / 42-67; 1c 97-118 / 39-47 (1 / 8 workers, load 8-13). Forced: shuffle join, one table per worker; impossible: a shared table. | **P1 OK** (1a/1b/1c). 2 MB pages: 1a 79 / **24**, 1b 47 / 18, 1c 111 / 38 (1 / 8, load 5-13; 4 KB 1.2-2.5x slower). Easy: winner as reduce_by_index max; forced: the hash table as rounds of probing. | **P1 OK** (1a/1b/1c). 1a 81-93 / **19.8-27**; 1b 42.7-46 / 9.9-10.9; 1c 122-133 / 33-36.5 (1 / 8; tuned C same window 12.1-20.9 / 7.5-8.9 / 11.7-14.8). Surprise: 1a beats plain C 2.5-3x on 1 core (compact first). Forced: no hash table, so join = rounds of scatter-min + gather. | **P1 OK** (1a/1b/1c x2). 1a 1928-2034 / 636; 1b 5220-5426 / 1080; 1c 1625-1865 / 394-587 (1 / 8 threads, load 3-6): 30-200x C. Easy: join + max aggregate are the rules; forced: B-tree loading of 5.2 M rows. | **P1 OK** (1a/1b/1c). 8 threads: 1a 103-113, 1b 272-375, 1c 128-190 (10-60x tuned C). Easy: the joins are SQL joins; forced: every lookup by dense id is a hash join (SQL has no a[i]). |
| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood over given edges 97 / 58; lazy (own decode) 165 / 90 vs eager with its own decode >= 97 + 47 decode + an unwritten CSR build: **a tie at best, not a win** for ATTACKS 1 (corrected after review: the earlier 'slower than eager' compared unlike work). Cost: 111 levels x ~25 passes each. | **P2 OK** (eager + lazy). 8 thr: lazy 79-90, eager 70-127 (tuned C 25.7-29.1 / 12.7-14.6). Lazy is 1.3-1.5x *slower* than eager in Halide. Forced: no fixpoint (one pipeline call per level, ~100 levels), no worklist; `atomic()` dropped (mutex per element, wrong live sets). | **P2 OK** (eager + lazy). Flood over given edges 1227-1467 / 431. With its own decode: **lazy 2.9-3.2 s vs eager ~6.5 s on 1 thread, 960 ms vs ~1.6 s on 8** (decodes 1.73 M of 9.4 M relocations, 3 rules): ATTACKS 1 holds inside Soufflé; ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: slow as implemented (45 rounds x ~25 statements, hash tables rebuilt each round); Third attempt (M3, one recursive query per method round, `p2rec.sql`; lead rerun 18:22Z P2 OK): 8 threads setup 577-871 + flood 1438-2234 ms, 1.2x the old cell overall. **Falls down after three attempts**: each recursive step rescans all 2.7 M edges; DuckDB has no fast lookup by id (an index pays below ~50 rows, frontiers are 16-24 K). Lazy ties eager. |
| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down: one image array on 8 workers (gather +142-171). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK**. 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a: second attempt `layout_rank.dl` (M3; lead rebuilt from source, P3 OK 18:21Z): the order by a ranked successor chain, no pair counting, no C functor: 1 thread 1930 (median, was 8270), 8 threads 1725/1937/3541 (was 2860/3388/7055 same runs), load 6-22. **No longer falls down**, but ~100x plain C: the 1,763-step unit walk is the limit. 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
| 4. codegen by shape (stretch) | not attempted | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). | not attempted | not attempted | not attempted |

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

## 4. Code generation by shape: the emit costs pieces, not bytes; merge templates by width

**Problem.** Code generation by shape (4): emit fc's 60.7 MB of MinIO function bodies from 108,833 byte templates,
7.68 M operands and 17.6 MB of glue (ref/p4/README.md).

**Before.** The obvious model, from copy-and-patch compilers (Xu and Kjolstad 2021; ideas/copy-and-patch.md): a
template table plus a patch pass; speed is bandwidth, and the table should be keyed by typed shape (SHAPES.md: 4,096
shapes cover 96.7% of nodes).

**Observation.** (a) The plain C emit runs at **0.42 GB/s** (1 core, 145 ms median for 60.7 MB), 60x below this VM's
read bandwidth, and patching holes is only **7.6%** of it (codex-p4par, P4_NOPATCH split): the cost is 2.38 M
pieces of ~25 bytes, each a lookup, a variable-length copy and a branch. 8 threads give 2.2x (51/66/68 ms, load ~20).
(b) The typed shape does not fix the bytes (ref/p4/NOTES.md: 74.6% of nodes in the top 4,096 *byte* templates).
One splitter is purely mechanical: a displacement is disp8 exactly when the operand fits in int8, **0 exceptions in
4,864,669 holes** (lead, results/cp1check.py).

**Attack.** Key templates with the displacement width relaxed (store disp32, shrink when the operand fits): **108,833
-> 66,046 templates; the top 4,096 cover 82.3% of nodes instead of 74.6%**, and the emitter reproduces the oracle byte for
byte (codex-cp, `ref/p4/check-cp.sh`, lead rerun P4 OK). But keep each node's length precomputed: deriving lengths
from operands at layout time made the scan 6.3x slower (7.68 M operand reads instead of 2.38 M lengths). Specialising
the hot templates into straight-line C (Deegen's method) cut instructions 26% and the covered segments' emit 1.67x
(median), but ranges overlap and those templates are only 36% of the emitted bytes; 2.9 MB of generated C.

**What it says about the paradigms.** Futhark expresses the whole problem in 159 lines (lengths by map, places by
scan, holes as a flat map, rel32 by gather, relocations by expand) and is correct on the first shapes, at 3.2x C on 1
core; what it had to force is the copy itself (a map cannot memcpy into a slice; a sequential slice-update loop inside
a map over 64 blocks can, 3-5x cheaper than scattering bytes). **Next:** fewer, larger pieces: fuse a node's segments
with hole-free neighbours and glue into one run at extraction (CP7 supernodes), counted as pieces per byte, and the
Halide cell (running).
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

exec
/bin/bash -lc 'cat README.md LOOP.md; nl -ba GRID.md | tail -n 48; nl -ba ATTACKS.md' in /home/exedev/paradigms-w
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
    15	  not a like-for-like row. Whole link: 537 ms on 1 core, **149 ms on 8 cores** (process, warm).
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
    28	| 4. codegen by shape: emit 136,634 bodies (60.7 MB) from 108,833 byte templates + 7.68 M operands | | fc's tree walker (no separate phase time) | 13-17 (lay out 2.38 M pieces) + 139-147 (emit), load 3.5; lead rerun 16:05Z: 18.4 + 162.4 | p4par (codex 17:27Z, load ~20, 5 interleaved, min/med/max): 1 thread layout 7.8/7.9/10.4 + emit 107/118/150; **8 threads 2.9/4.3/7.8 + 51/66/68** (vs p4 116/145/174 same window). Patching is 7.6% of 1-thread emit: the copy of 2.38 M ~25-byte pieces at ~0.6 GB/s is the cost (per-item, not bandwidth). [results/codex-p4par.md](results/codex-p4par.md) | `ref/p4/check.sh` -> P4 OK (`ref/p4/README.md`) |
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
    42	**Paradigm cells on kube-apiserver** (codex-kube, 17:21-18:16Z, merged 18:30Z; [results/codex-kube.md](results/codex-kube.md)): every P1-P3 cell of Goose, Futhark, Halide, Soufflé and DuckDB, unchanged, reproduces kube's oracles at 1 and 8 threads (lead spot-checked Halide P1 and Goose P2 at 18:28Z). Load was 17-26 for most of the window, so 8-thread kube/MinIO ratios (2-10x) mostly measure the load; the 1-thread ratios, 1.5-2.5x for a dataset ~1.4x MinIO's ids and 1.4x its live set, say no cell has a cost that grows faster than the data except Halide 1a (6.7x on 1 thread: to recheck quiet).
    43	
    44	Floors are bottom-up's FLOOR.md model (same CPU model, the 2026-11-13 VM); they count the raw object bytes hlink
    45	reads, so for the flat files they are an upper bound on the floor.
    46	
    47	## The paradigms
    48	
    49	Added columns (M3): **GraphBLAS** (SuiteSparse 10.5.2, JIT on; lead rebuilt + reran 18:24Z, P1/P2 OK, all files `cmp`). 8 threads warm, min/med/max, load 7-10, tuned C same window: **P2 reach 179/186/213** (p2par 15/20/26: 9x); 1 thread 192-203, i.e. it does not scale (frontier ~7 K is below SuiteSparse's per-call threading threshold: 379 of 382 vxm calls on one thread); 178 synchronous steps (97 BFS + 81 iface levels) in 7 rounds; pull direction 40x slower. **1a** 268/306/339 (18x p1par; the 128-bit key must be hashed into a 60-bit index and checked), **1b** 162/194/203 (21x), **1c** 279/349/392 (24x). P3: not attempted (no scan; the round-up-then-add fold is no monoid). Easy: reach is ~the shortest legible form (masked vxm); forced: the iface rule as its own closure per round. `systems/graphblas/CELLS.md`.
    50	
    51	| problem | Goose | Futhark | Halide | Soufflé | DuckDB (added 15:58Z) |
    52	|---|---|---|---|---|---|
    53	| 1. resolve | **P1 OK** (1a/1b/1c). 1a 79-115 / 37-57; 1b 39-57 / 42-67; 1c 97-118 / 39-47 (1 / 8 workers, load 8-13). Forced: shuffle join, one table per worker; impossible: a shared table. | **P1 OK** (1a/1b/1c). 2 MB pages: 1a 79 / **24**, 1b 47 / 18, 1c 111 / 38 (1 / 8, load 5-13; 4 KB 1.2-2.5x slower). Easy: winner as reduce_by_index max; forced: the hash table as rounds of probing. | **P1 OK** (1a/1b/1c). 1a 81-93 / **19.8-27**; 1b 42.7-46 / 9.9-10.9; 1c 122-133 / 33-36.5 (1 / 8; tuned C same window 12.1-20.9 / 7.5-8.9 / 11.7-14.8). Surprise: 1a beats plain C 2.5-3x on 1 core (compact first). Forced: no hash table, so join = rounds of scatter-min + gather. | **P1 OK** (1a/1b/1c x2). 1a 1928-2034 / 636; 1b 5220-5426 / 1080; 1c 1625-1865 / 394-587 (1 / 8 threads, load 3-6): 30-200x C. Easy: join + max aggregate are the rules; forced: B-tree loading of 5.2 M rows. | **P1 OK** (1a/1b/1c). 8 threads: 1a 103-113, 1b 272-375, 1c 128-190 (10-60x tuned C). Easy: the joins are SQL joins; forced: every lookup by dense id is a hash join (SQL has no a[i]). |
    54	| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood over given edges 97 / 58; lazy (own decode) 165 / 90 vs eager with its own decode >= 97 + 47 decode + an unwritten CSR build: **a tie at best, not a win** for ATTACKS 1 (corrected after review: the earlier 'slower than eager' compared unlike work). Cost: 111 levels x ~25 passes each. | **P2 OK** (eager + lazy). 8 thr: lazy 79-90, eager 70-127 (tuned C 25.7-29.1 / 12.7-14.6). Lazy is 1.3-1.5x *slower* than eager in Halide. Forced: no fixpoint (one pipeline call per level, ~100 levels), no worklist; `atomic()` dropped (mutex per element, wrong live sets). | **P2 OK** (eager + lazy). Flood over given edges 1227-1467 / 431. With its own decode: **lazy 2.9-3.2 s vs eager ~6.5 s on 1 thread, 960 ms vs ~1.6 s on 8** (decodes 1.73 M of 9.4 M relocations, 3 rules): ATTACKS 1 holds inside Soufflé; ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: slow as implemented (45 rounds x ~25 statements, hash tables rebuilt each round); Third attempt (M3, one recursive query per method round, `p2rec.sql`; lead rerun 18:22Z P2 OK): 8 threads setup 577-871 + flood 1438-2234 ms, 1.2x the old cell overall. **Falls down after three attempts**: each recursive step rescans all 2.7 M edges; DuckDB has no fast lookup by id (an index pays below ~50 rows, frontiers are 16-24 K). Lazy ties eager. |
    55	| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down: one image array on 8 workers (gather +142-171). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK**. 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a: second attempt `layout_rank.dl` (M3; lead rebuilt from source, P3 OK 18:21Z): the order by a ranked successor chain, no pair counting, no C functor: 1 thread 1930 (median, was 8270), 8 threads 1725/1937/3541 (was 2860/3388/7055 same runs), load 6-22. **No longer falls down**, but ~100x plain C: the 1,763-step unit walk is the limit. 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
    56	| 4. codegen by shape (stretch) | not attempted | **P4 OK** (5 shapes, c + multicore; lead rebuilt from source and reran 18:12Z). Best `4bf` (blocked copy + patch), 2 MB: 326/374/439 (1) / **161/187/216 (8)** vs p4.c 102-110 / p4par 28-43 same window, load 10-12: 3.2x / 5.3x. 159 lines. Easy: lengths map, places scan, holes as a flat map, rel32 gather, relocations by expand; forced: the copy (a map cannot memcpy into a slice; a sequential slice-update loop inside a map over 64 blocks can, 3-5x cheaper than byte scatter). | not attempted | not attempted | not attempted |
    57	
    58	Halide column merged 17:17Z (lead rebuilt every generator and driver in a copy; P1/P2/P3 OK; `systems/halide/CELLS.md`).
    59	DuckDB column merged 17:13Z (lead reran every check in a copy, pipeline included; `systems/duckdb/CELLS.md`).
    60	Soufflé column merged 16:58Z (lead rebuilt all 8 programs from source; every check OK; `systems/souffle/CELLS.md`).
    61	Futhark column merged 16:42Z (lead rebuilt from source; every cell OK; `systems/futhark/CELLS.md`). Surprise: `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, no code change, makes most Futhark cells 1.2-1.7x faster.
    62	Goose column merged 16:35Z; lead reran every check (P1/P2/P3/pipeline OK, a source rebuild of p2par passes). Long form and lines: `systems/goose/CELLS.md`, `NOTES.md`.
     1	# ATTACKS
     2	
     3	New attacks on the grid's problems (CHARTER.md). Each entry: the problem, the old shape, the new
     4	attack and where it came from, the evidence, what it would take. Before, observation, fix, number.
     5	
     6	## 1. Reach: don't build the graph; decode each reference when the flood reaches it, from columns
     7	
     8	**Problem.** Reach (2): the live set from the roots, over relocations plus the method rules.
     9	
    10	**Before.** Two shapes. hlink floods straight over the raw objects, decoding each live symbol's relocation records
    11	((package index, symbol index) -> canonical id) on the spot: 98-103 ms on 1 core, 22-23 ms on 8 (MinIO). The textbook
    12	data-parallel shape, and what the flat files hand every paradigm, is "decode everything into a canonical CSR, then
    13	flood": the flood alone is fast (p2par 26-30 / 8.6-17 ms), which made it look as if the flat layout had won.
    14	
    15	**Observation.** It hadn't: the flood reads only **28.7%** of the CSR's edges on MinIO (2.39 M of 8.32 M) and **21.0%**
    16	on kube-apiserver (3.27 M of 15.6 M), and a quarter of those repeat a (source, target) pair (580,425 on MinIO counting (kind, target), 821,056 on kube)
    17	(`python3 ref/flat/reachfacts.py data/<dataset>`). Building the graph is 3.5-5x more decoding than reach needs, and
    18	measured end to end (Codex, `ref/flat/p2pipe.c`: decode 9.4 M targets + count/prefix/fill + flood) it loses:
    19	**108.7 ms on 1 thread, 30.1 ms on 8** on MinIO (results/codex-d.md; my rerun 16:04Z: 33.7 ms on 8), slower than hlink.
    20	The CSR build alone (11.7 ms on 8) costs as much as the flood.
    21	
    22	**Attack.** Keep the relocations as columns (`rel_pkg`, `rel_idx`, `rel_type`, one contiguous range per symbol,
    23	`slot_rel`) and decode inside the flood: when a symbol becomes live, walk its range, map (pkg, idx) through its
    24	object's package-base table and the canon array, mark. That is hlink's lazy shape on a columnar layout, parallel
    25	by level (`ref/flat/p2lazy.c`, the flood loop of p2par with ~15 lines changed).
    26	
    27	**Number.** MinIO: **52.6-53.3 ms on 1 thread, 13.6-15.4 ms on 8** (P2 OK; 16:02-16:04Z, load 0.8, interleaved with
    28	p2par). kube-apiserver: 72.7 / 19.9 ms. vs the eager pipeline 108.7 / 30.1: **2x, reproduced independently** by Codex
    29	(16:04-16:05Z, interleaved, medians of minima: MinIO 53.5 vs 110.8 / 18.2 vs 32.8; kube 75.3 vs 196.7 / 23.5 vs 58.5,
    30	i.e. 2.1-2.6x; reviews/done/20261116T1603-codex-attack1.md). What the 2x covers: the same flood, eager decode+CSR vs decode in
    31	the flood; both exclude file mapping, 1a, aux and kinds 1-3 decode and method tables. **Not a claim about hlink**:
    32	hlink's flood timer does work the flat kernels don't (aux and funcdata/pcdata marking, inline-tree parsing, flRef
    33	marking, itab and method-table parsing; dead.c:301-580) and excludes work they include (frontier merging, methods),
    34	so no fair ratio to hlink follows from these numbers (an earlier draft said 1.9x / 1.5x; withdrawn 16:20Z).
    35	
    36	**Caveats.** Both flat pipelines take the funcdata aux edges (704 K on MinIO), the interface kinds 1-3 and the
    37	method tables precomputed from the p2 files, and the 1a canon from its oracle; hlink decodes those inside its
    38	flood too, so the hlink ratio overstates the win by the cost of that work (dumpflat's untuned eager version of
    39	all of it is 133-139 ms; a lazy one should be a fraction). The columnar relocation layout is what the extractor
    40	produced, i.e. a cost hlink does not pay (it reads the object files in place); what the measurement shows is that
    41	the *layout* of the references matters more than pre-decoding them.
    42	
    43	**What it would take / next.** (a) Done for aux (codex-e, results/codex-e.md; `build/p2lazy <data> N --aux` decodes the
    44	1.48 M raw funcdata aux records from new `aux_*` columns inside the flood): +6.9 ms MinIO / +9.3 ms kube on 1 thread
    45	(59.9 -> 66.8, 77.2 -> 86.5 best of 5, 16:12Z), so the lazy flood *with its own aux decode* still beats the eager pipeline
    46	*without* one by 1.6x. Kinds 1-3 and method tables remain precomputed; the hlink comparison stays withdrawn. (b) Dedup per source: measured 16:09Z, a deduped CSR (8.32 M -> 5.28 M edges) floods in 23.9-29.1 ms vs 26.2-32.5 on 1 thread (~8%; 8-thread runs swamped at load 6): duplicates hit cache lines already loaded, so not worth a pass. (c) Ask the paradigms: a lazy decode inside a flood is
    47	a gather inside a fixpoint. Futhark/Halide want the edge list materialized (the eager shape that loses here);
    48	Soufflé's rule `live(t) <- live(s), rel(s, r), target(r, t)` evaluated semi-naively is exactly the lazy shape,
    49	if its join on `rel` runs over the columns rather than a B-tree. That is the kill test for each paradigm's P2 cell:
    50	does it beat 108.7 / 30.1 ms (the eager pipeline) when its own input decode is counted?
    51	
    52	## 2. Relocate: the output's page faults are half the work (2x on 1 core; 8 threads at 1.1-1.4x the floor)
    53	
    54	**Problem.** Relocate (3b): copy 76 MB of symbol bytes into a 109 MB image and patch 2.05 M relocations (MinIO).
    55	
    56	**Before.** The plain C reference callocs the image and writes it: 48-68 ms on 1 core (85-115 in noisy runs).
    57	The FLOOR.md floor for hlink's write phase is 4.92 ms on 8 cores.
    58	
    59	**Observation.** First-touching the image (memset outside the timed region, `P3_PREFAULT=1 build/p3`) costs 45-64 ms,
    60	and the copy + patch that remains is 31-35 ms: on 4 KB pages, **the kernel zeroing and mapping 26,700 pages is
    61	about half of relocate.** (2026-11-16T16:05Z, load ~1-2.)
    62	
    63	**Attack.** Put the output on 2 MB huge pages (53 faults): `P3_THP=1 build/p3` maps the image MAP_HUGETLB (with
    64	`vm.nr_hugepages=512` reserved; this kernel has no transparent huge pages), faults included in the timed region.
    65	
    66	**Number.** `P3_REPS=20` (a fresh output mapping per rep, its faults inside the timed region; input warm), three
    67	interleaved runs each, 2026-11-16T16:13Z, load 2.4-3.0, P3 OK:
    68	
    69	| | 4 KB pages | 2 MB pages |
    70	|---|---|---|
    71	| 1 thread (`build/p3`) | min 48.6, median 49.3-50.3, max 54-56 ms; 19,109 faults/rep | min 23.9-24.2, median 24.4-24.8, max 25-30 ms; 39 faults/rep |
    72	| 8 threads (`build/p3par`) | min 13.4-14.1, median 14.3-15.1, max 21-29 ms; 19,137 faults | min 5.4-6.0, median 6.1-6.8, max 7-12 ms; 155 faults |
    73	
    74	On 8 threads the huge-page relocate is **1.1-1.4x the FLOOR.md floor of 4.92 ms**; on 4 KB pages it is 3x. The ranges
    75	don't overlap at either thread count. The attack is not a new loop: it is the output's page size. Against this VM's measured bandwidth (36 GB/s read, 27 write on 8
    76	threads: 109 MB read + 76 MB written -> ~3-6 ms) the 8-thread huge-page relocate is at the memory bound; nothing is left in the loop.
    77	
    78	**No code needed.** The Futhark builder found that `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` (glibc's malloc on the reserved
    79	2 MB pages) gives Futhark 1.2-1.7x across most cells. On the plain C relocate, unchanged `calloc`, 3 interleaved runs
    80	(16:40Z, load 2.6): **24.2-24.7 min / 24.4-26.6 median ms vs 55.0-55.8 / 55.5-58.9 on 4 KB, faults 39 vs 19,109 per
    81	rep**: the same 2x as the explicit MAP_HUGETLB, from an environment variable. (p3's "4 KB pages" label under the
    82	tunable is wrong: it reports the allocation path, not the page size.) Any paradigm whose runtime allocates through
    83	glibc malloc gets the attack for free, given reserved huge pages (`vm.nr_hugepages`).
    84	The tunable also speeds the hash join: p1compact's join on 1 thread 20.5-21.2 vs 30.1-32.3 ms (3 interleaved, 16:40Z)
    85	-- its tables and records are malloc'd and probed at random, so this is TLB reach, not faults. The floods (p2par, p2lazy)
    86	don't move (within noise): their inputs are mmap'd files, which the tunable doesn't touch.
    87	
    88	**Note for the grid.** hlink with `vm.nr_hugepages=512` vs `0`, 6 interleaved rounds on 8 cores (16:15Z, load 1.9-2.5):
    89	total 193-267 ms (median ~226) vs 205-249 (~225): **within noise**; write phase 17-56 vs 17-23. The earlier noisy
    90	215-229 vs 250-287 did not reproduce, so GRID's hlink numbers (taken with 0 reserved pages) stand. hlink reuses its
    91	previous output file in place ("overwrote the old output in place, zeroed": create 11 ms), so its output faults are
    92	already mostly paid outside write.
    93	
    94	**What it would take.** Quiet 8-thread numbers; then the paradigms' 3b cells should report whether their runtime lets
    95	the output live on huge pages (Futhark's and Halide's buffers, Goose's arenas, DuckDB's blobs), because on this
    96	problem that matters as much as the loop.
    97	
    98	## 3. Reach: the level count is the cost; drop level synchronization
    99	
   100	**Problem.** Reach (2), the same flood as entry 1.
   101	
   102	**Before.** Every data-parallel shape of the flood is level-synchronous: expand the frontier, barrier, swap. MinIO's
   103	reach has **111 levels in 7 rounds** (kube 103). p2par (OpenMP) pays one team barrier per level; Futhark pays ~25
   104	whole-array passes per level (its lazy flood "falls down": 165 / 90 ms, slower than its own eager flood, NOTES.md);
   105	Goose pays a message exchange per superstep and scales only 1.7x on 8 workers; DuckDB iterates 45 SQL rounds.
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
   152	## 4. Code generation by shape: the emit costs pieces, not bytes; merge templates by width
   153	
   154	**Problem.** Code generation by shape (4): emit fc's 60.7 MB of MinIO function bodies from 108,833 byte templates,
   155	7.68 M operands and 17.6 MB of glue (ref/p4/README.md).
   156	
   157	**Before.** The obvious model, from copy-and-patch compilers (Xu and Kjolstad 2021; ideas/copy-and-patch.md): a
   158	template table plus a patch pass; speed is bandwidth, and the table should be keyed by typed shape (SHAPES.md: 4,096
   159	shapes cover 96.7% of nodes).
   160	
   161	**Observation.** (a) The plain C emit runs at **0.42 GB/s** (1 core, 145 ms median for 60.7 MB), 60x below this VM's
   162	read bandwidth, and patching holes is only **7.6%** of it (codex-p4par, P4_NOPATCH split): the cost is 2.38 M
   163	pieces of ~25 bytes, each a lookup, a variable-length copy and a branch. 8 threads give 2.2x (51/66/68 ms, load ~20).
   164	(b) The typed shape does not fix the bytes (ref/p4/NOTES.md: 74.6% of nodes in the top 4,096 *byte* templates).
   165	One splitter is purely mechanical: a displacement is disp8 exactly when the operand fits in int8, **0 exceptions in
   166	4,864,669 holes** (lead, results/cp1check.py).
   167	
   168	**Attack.** Key templates with the displacement width relaxed (store disp32, shrink when the operand fits): **108,833
   169	-> 66,046 templates; the top 4,096 cover 82.3% of nodes instead of 74.6%**, and the emitter reproduces the oracle byte for
   170	byte (codex-cp, `ref/p4/check-cp.sh`, lead rerun P4 OK). But keep each node's length precomputed: deriving lengths
   171	from operands at layout time made the scan 6.3x slower (7.68 M operand reads instead of 2.38 M lengths). Specialising
   172	the hot templates into straight-line C (Deegen's method) cut instructions 26% and the covered segments' emit 1.67x
   173	(median), but ranges overlap and those templates are only 36% of the emitted bytes; 2.9 MB of generated C.
   174	
   175	**What it says about the paradigms.** Futhark expresses the whole problem in 159 lines (lengths by map, places by
   176	scan, holes as a flat map, rel32 by gather, relocations by expand) and is correct on the first shapes, at 3.2x C on 1
   177	core; what it had to force is the copy itself (a map cannot memcpy into a slice; a sequential slice-update loop inside
   178	a map over 64 blocks can, 3-5x cheaper than scattering bytes). **Next:** fewer, larger pieces: fuse a node's segments
   179	with hole-free neighbours and glue into one run at extraction (CP7 supernodes), counted as pieces per byte, and the
   180	Halide cell (running).

exec
/bin/bash -lc "rg -n 'check|oracle|falls|time|session|budget' systems/*/NOTES.md results/second-attempts.md; rg --files systems | rg 'check|run'" in /home/exedev/paradigms-w
 succeeded in 0ms:
results/second-attempts.md:3:Branch `worktree-agent-aa77583b9c2460e4b`. Both new cells match their oracles exactly. Times are ms,
results/second-attempts.md:7:## Soufflé 3a (lay out): attempt 3, `systems/souffle/layout_rank.dl`. No longer falls down
results/second-attempts.md:9:Check: `build/layout_rank -j8 -F $D -D out && build/check_p3 $D $D/oracle/p3_image.bin out/p3_addr.u32` gives P3 OK
results/second-attempts.md:34:  Datalog. Verdict: **slow, not falls down.**
results/second-attempts.md:41:3. `tl(_, _, _, _, c2), c2 != c` in `tlcheck` scanned all of tl for each typelink (also present in attempt 2). It now
results/second-attempts.md:46:Check: `./dk $D 8 sql/load2.sql sql/p2rec.sql && cmp out/p2_live.u32 $D/oracle/p2_live.u32` matches (663,630 live).
results/second-attempts.md:70:Verdict: **falls down** after three attempts. Both reasons are measured:
systems/graphblas/NOTES.md:18:diagnostics (method chosen, threads, time): every profile below comes from it.
systems/graphblas/NOTES.md:25:  wall time is flat in the thread count: best of 7 at load ~7 (18:10Z), 1/2/4/8 threads 192 / 188 / 187 / 173 ms, while
systems/graphblas/NOTES.md:26:  process CPU time grows 192 -> 256 ms (spinning up teams that have nothing to do). bench.sh: 1 thread 192-203 ms,
systems/graphblas/NOTES.md:27:  8 threads 179-213 ms (ranges overlap). A finer grain (`GB_CHUNK=8192`) doubles CPU time (400 ms) for 161-223 ms
systems/graphblas/NOTES.md:36:- **Every cell matched its oracle on the first run** (P2, 1a, builtins, 1b, 1c), and the code reads as the rules.
systems/graphblas/NOTES.md:37:- **A 128-bit key does not fit a GraphBLAS index (60 bits).** Resolve hashes (k1, k2) to 60 bits and checks every
systems/graphblas/NOTES.md:48:| `gb.h` | the flat-file reader (`ref/flat/flat.h`), an error check, iso-bool matrix/vector builders |
systems/graphblas/NOTES.md:58:## Oracle checks
systems/graphblas/NOTES.md:61:    GB_THREADS=8 ./p2gb-jit $D 1 && cmp out/p2_live.u32 $D/oracle/p2_live.u32 && echo P2 ok
systems/graphblas/NOTES.md:62:    GB_THREADS=8 ./p1gb-jit $D 1 && for f in p1_canon p1_builtin p1_reltarget p1c_canon; do cmp out/$f.u32 $D/oracle/$f.u32 && echo ok $f; done
systems/graphblas/NOTES.md:64:Each program also compares in-process (`check32`) and prints `P2 OK` / `P1 OK` (P1 also requires 0 key collisions).
systems/graphblas/NOTES.md:130:hashed to a 60-bit index `h` (four elementwise ops: times, xor, times, shift).
systems/graphblas/NOTES.md:155:**Where the time goes** (1 thread, burble, JIT): the builds (sorts of 0.8-2.6 M tuples, ~50-120 ms each), the
systems/graphblas/NOTES.md:180:oracle check). JIT build. Raw output: `results-bench-20261116T1809Z.txt`. C references in the same window: `p2par`,
systems/goose/NOTES.md:19:2. **Goose's runtime pins the output to 4 KB pages, but a 20-line C sidecar can swap the pages under a builder's stack
systems/goose/NOTES.md:21:   rep 26,731 -> 258-457; 8 workers 32.8-40.0 -> **17.4-23.5 ms**. The runtime reserves each data stack as a NORESERVE
systems/goose/NOTES.md:58:  (outside the timed region; it is why p2lazy's end to end is 731-859 ms).
systems/goose/NOTES.md:91:All checks: `systems/goose/bin/<cell> /home/exedev/paradigms/data/minio [8]` prints `P1 OK` / `P2 OK` / `P3 OK`
systems/goose/NOTES.md:92:(`P3 pipeline OK` with `--pipeline`); each compares its output arrays with the oracle files element by element
systems/goose/NOTES.md:93:(`check32` in flat.goose; the image symbol by symbol against oracle/p3_image.bin). Lines are whole files (flat.goose 150
systems/goose/NOTES.md:107:e2e is one process with one rep, wall from `date +%s%N` around it, including the oracle self-check's reads (~10-20%).
systems/goose/NOTES.md:119:- **2 CSR** easy: `checkfree fn drain` over index-typed columns (`u32 in live`, `u32 upto edge_dst`), validated once by
systems/goose/NOTES.md:120:  `from_bytes` at load: zero bounds checks in the flood. Forced: on 8, owner-partitioned BSP with per-owner inbox types and
systems/goose/NOTES.md:127:  output only through C under the runtime. Impossible: one image written by 8 workers in place (they can't share it);
systems/goose/NOTES.md:170:no destructors, no lifetime annotations. Dynamic data lives on N compiler-assigned "data stacks" (bump pointers,
systems/goose/NOTES.md:172:and references stay valid). Every reference has a static root checked by the compiler. Nested data is inline (arrays
systems/goose/NOTES.md:174:types `u32 in arr` (validated once, indexing then needs no check; `checkfree fn` enforces zero bounds checks).
systems/goose/NOTES.md:180:Claude Fable; design by the author). About 30 k lines of C++ is the site's claim; the checked-out `src/*.h|cpp`
systems/goose/NOTES.md:188:- No interpreter, no LLVM, no direct native backend. Also: formatter, LSP, package fetcher (git tags), `checkfree` and `--bce-lines` diagnostics.
systems/goose/NOTES.md:201:- Binary used: release v2026.4 (published 2026-11-06), `goose v2026.4 (runtime dce889723a23)`, linux-x64 (needs glibc >= 2.35;
systems/goose/NOTES.md:205:- Both `goose run` and `goose build` work with the VM's own gcc and libc headers. First `build` compiles the runtime once ("compiling the runtime for gcc 13.3.0").
systems/goose/NOTES.md:215:(monomorphized), function blocks, index types (`u32 in a`, `u32 upto a`) and `checkfree fn`, `simd fn`, thread_fn + typed
systems/goose/NOTES.md:224:  file/stdin/time/env (no subprocess, no network, no mmap).
systems/goose/NOTES.md:225:- Bounds-check elimination: indirect indexing keeps its check unless the stored values have an index type; `u64`
systems/goose/NOTES.md:231:no pread, no seek, no partial read), decode u32 LE with `binary::read_u32le` (checked, per element; there is no
systems/goose/NOTES.md:233:checksums. `hello/hello8.goose`: same across N workers, chunk copied to each over a typed queue, worker returns
systems/goose/NOTES.md:235:the oracle; `hello/c_ref.c` is a C reference. Both Goose programs print identical checksums to the oracle.
systems/goose/NOTES.md:238:Oracle: `total=107384557821208332 prefix_checksum=2059448577320109362 hist_checksum=10908785116829868540` (all match).
systems/goose/NOTES.md:242:| program | read | decode | scan+hist (+checksum) | send+spawn | workers+collect | total |
systems/goose/NOTES.md:254:and it includes thread spawn). The checksum phase of `hello1` is 17 ms (a second pass over the prefix array).
systems/goose/NOTES.md:267:- Unsigned overflow wraps silently in release builds; `--debug` adds checks. `u32` converts implicitly to `u64`
systems/goose/NOTES.md:274:`/home/exedev/tools/goose/build/goose`, `goose v2026.4-43-g3a0aa2eb (runtime a9ca9ac6fac6)` (43 commits past the v2026.4 tag).
systems/goose/NOTES.md:275:Compiled `hello1.goose` with it: identical checksums, total 487 ms (same as the release binary within noise). The release
systems/goose/NOTES.md:282:validated once at load, and `checkfree fn` proves the flood and relocation loops have no bounds checks, which is the
systems/goose/NOTES.md:283:Zig-compiler "indices not pointers" design with the compiler checking it (the bench `graph_csr` is exactly a CSR flood).
systems/goose/NOTES.md:286:arrays, bounds-check-free only if the slot values are typed as indices (the bench `words` keeps four checks); the 0.74 G/s
systems/goose/NOTES.md:292:against a floor of about 1.2 ms. So the memory model maps well onto layout, resolve and reach as single-thread, bounds-check-free
systems/souffle/NOTES.md:3:Soufflé 2.5, compiled mode (`souffle -g` + g++ -O3), every cell correct against its oracle. Inputs come in through
systems/souffle/NOTES.md:7:has no bytes) and the untimed P2-lazy setup that p2lazy.c also does untimed.
systems/souffle/NOTES.md:36:  faults are 2-5% of the cell. Soufflé's own runtime (B-trees from `new`) has no page-size knob; this kernel has no
systems/souffle/NOTES.md:50:generated main runs once); the warm times are >= 0.3 s, most >= 1 s.
systems/souffle/NOTES.md:73:gcc -O2 -o build/check_p3 check_p3.c; gcc -O2 -o build/prep_lazy prep_lazy.c; build/prep_lazy $D build/lazyfacts
systems/souffle/NOTES.md:74:build/resolve_a -j8 -F $D -D out && cmp out/p1_canon.u32 $D/oracle/p1_canon.u32 && cmp out/p1_builtin.u32 $D/oracle/p1_builtin.u32
systems/souffle/NOTES.md:75:build/resolve_b -j8 -F $D -D out && cmp out/p1_reltarget.u32 $D/oracle/p1_reltarget.u32
systems/souffle/NOTES.md:76:build/resolve_c -j8 -F $D -D out && cmp out/p1c_canon.u32 $D/oracle/p1c_canon.u32      # also resolve_c2
systems/souffle/NOTES.md:77:build/reach -j8 -F $D -D out && cmp out/p2_live.u32 $D/oracle/p2_live.u32
systems/souffle/NOTES.md:78:build/reach_lazy -j8 -F build/lazyfacts -D out && cmp out/p2_live.u32 $D/oracle/p2_live.u32
systems/souffle/NOTES.md:79:build/layout -j8 -F $D -D out && build/check_p3 $D $D/oracle/p3_image.bin out/p3_addr.u32   # 3a
systems/souffle/NOTES.md:80:build/layout_rank -j8 -F $D -D out && build/check_p3 $D $D/oracle/p3_image.bin out/p3_addr.u32   # 3a attempt 3 (P3 OK)
systems/souffle/NOTES.md:81:build/relocate -j8 -F $D -D out && build/check_p3 $D out/p3_image.out                       # 3b
systems/souffle/NOTES.md:110:**2' reach, lazy decode** (reach_lazy.dl 65 [84], plus prep_lazy.c 67, untimed, as p2lazy's setup). See Surprises.
systems/souffle/NOTES.md:112:Found and checked: hlink's walk skips the two relocations after an R_METHODOFF (`r += 2`); on MinIO all 225,760
systems/souffle/NOTES.md:113:skipped ones are themselves R_METHODOFF (prep_lazy checks this), so "plain" is a filter on the type and needs no
systems/souffle/NOTES.md:123:a size that is a multiple of 8 (checked by the rule `tlcheck`, empty). Verdict after two attempts was "falls down on
systems/souffle/NOTES.md:135:tlcheck rewritten (`tl(_, _, _, _, c2), c2 != c` scanned all of tl per typelink: now a class relation). Tried and
systems/souffle/NOTES.md:139:**Verdict: correct, slow; no longer "falls down".** 8 threads do not help: the unit walk is a 1,763-iteration
systems/souffle/NOTES.md:188:`bench.sh`; `check_p3.c`; `prep_lazy.c`; `best.sh` (older single-cell timer).
systems/souffle/NOTES.md:227:Embedded numbers are in-process phase timers (mmap of edge.bin is ~5 ms). Hardware-style context: the raw
systems/souffle/NOTES.md:240:   (linear-time bulk build, BTree.h). 446 ms at 8 threads, and the resulting tree is dense, so eval is faster too
systems/souffle/NOTES.md:254:  GDlog paper observes the same: tuple insertion is 77.8 % of Soufflé runtime on REACH, 16->32 threads gives 7 %.
systems/souffle/NOTES.md:257:- Compile time: ~10 s per generated program with g++; interpreter has no compile step but is similar here (10.1 s,
systems/duckdb/NOTES.md:25:  on a dense id), so 45 rounds of "decode only the frontier" read `canon` 45 times. See ATTACKS below.
systems/duckdb/NOTES.md:37:| `dk.c` (build: `./build.sh`) | the host: `.table NAME ROWS col=file ...` (Arrow view + CREATE TABLE), `col=OFF.u32[f=file,...]` a list column over a CSR, `col=#` a row index, `.cell`, `.repeat/.until` (a loop driven from outside: SQL has no loop), `.dump`, `.image/.blob/.int/.imgout` (the 3b scatter), `.show`; `DK_REPS` (repeat the scripts after `--`), `DK_THP` (image on 2 MB pages), `DK_STMTS` (time per statement over all rounds), `DK_ALLROUNDS` (print every `.repeat` round) |
systems/duckdb/NOTES.md:43:| `sql/load3b.sql load3b_iso.sql p3b.sql` | 3b isolated (oracle addresses, `p3_addr_all`) |
systems/duckdb/NOTES.md:45:| `check3b.py` | the 3b image check (every placed symbol's bytes vs `oracle/p3_image.bin`) |
systems/duckdb/NOTES.md:48:## Oracle checks
systems/duckdb/NOTES.md:51:    ./dk $D 8 sql/load1.sql sql/p1.sql && for f in p1_canon p1_builtin p1_reltarget p1c_canon; do cmp out/$f.u32 $D/oracle/$f.u32 && echo ok $f; done
systems/duckdb/NOTES.md:52:    ./dk $D 8 sql/load2.sql sql/p2prep.sql sql/p2loop.sql && cmp out/p2_live.u32 $D/oracle/p2_live.u32 && echo P2 ok
systems/duckdb/NOTES.md:53:    ./dk $D 8 sql/load2.sql sql/p2rec.sql && cmp out/p2_live.u32 $D/oracle/p2_live.u32 && echo P2 rec ok   # attempt 3
systems/duckdb/NOTES.md:54:    ./dk $D 8 sql/load1.sql sql/p1.sql sql/load2.sql sql/p2prep.sql sql/p2aux.sql sql/p2lazy.sql sql/p2loop.sql && cmp out/p2_live.u32 $D/oracle/p2_live.u32 && echo P2 lazy ok   # (p2eager.sql likewise)
systems/duckdb/NOTES.md:55:    ./dk $D 8 sql/load3.sql sql/p3a.sql && cmp out/p3_addr.u32 $D/oracle/p3_addr.u32 && echo 3a ok
systems/duckdb/NOTES.md:56:    ./dk $D 8 sql/load3b.sql sql/load3b_iso.sql sql/p3b.sql && python3 -I check3b.py $D out/p3_image.bin       # P3b OK
systems/duckdb/NOTES.md:57:    ./dk $D 8 sql/load1.sql sql/p1.sql sql/load3.sql sql/p3a.sql sql/load3b.sql sql/p3link.sql sql/p3b.sql && python3 -I check3b.py $D out/p3_image.bin   # pipeline
systems/duckdb/NOTES.md:60:(DuckDB's own canon and layout, `p3_fixed_*` given) reproduce the oracles exactly.
systems/duckdb/NOTES.md:130:available, and batching levels would need E^k edges. **Verdict: falls down** after three attempts, ~140x tuned C.
systems/halide/NOTES.md:6:wrap them zero-copy as `Halide::Runtime::Buffer`, call the AOT pipelines, check against the oracles), `bench.h`
systems/halide/NOTES.md:10:Build and check (each driver checks every variant it runs against the oracles and ends with `P<n> OK`):
systems/halide/NOTES.md:58:End to end (whole process, `/usr/bin/time`, 5 runs, s): P1 (counts+1a+1b+1c) 0.35-0.40 (1 thr) / 0.13-0.17 (8);
systems/halide/NOTES.md:59:P2 lazy 0.55-0.59 / 0.24-0.25 and eager 0.47-0.51 / 0.22-0.26 (both include ~115 ms of untimed C setup that
systems/halide/NOTES.md:63:Thread pool: Halide's (`HL_NUM_THREADS`); no wait-policy knob. No 8-thread time came out above its 1-thread time
systems/halide/NOTES.md:90:and hands it to the pipeline. Fresh output per run, faults inside the timed region (`p3 DATA 3b`, getrusage):
systems/halide/NOTES.md:94:vs C's 5.4-6.8 on 2 MB pages): the byte-at-a-time stores (below) are now the cost, 4-5x C.
systems/halide/NOTES.md:117:- Forced: the kind folded into the key (kind 2 gets k2 = 2^40); a check output for sizes >= 2^31.
systems/halide/NOTES.md:129:### 3a lay out (`Layout` in gen_p3.cpp, 154 lines; from the earlier session)
systems/halide/NOTES.md:180:build.sh 60. Drivers are loading, wrapping, the level loop (P2), the untimed edge split (P2) and checking.
systems/halide/NOTES.md:195:- Python: `python3 -m venv /home/exedev/tools/halide/venv && venv/bin/pip install halide==22.1.0 numpy` (JIT sanity-checked).
systems/halide/NOTES.md:202:3. `driver.cpp` mmaps the files (MAP_POPULATE), wraps them zero-copy in `Halide::Runtime::Buffer`, calls the AOT functions, checks each result against plain C++, reports best of 5.
systems/halide/NOTES.md:229:- `HL_NUM_THREADS` controls the runtime thread pool (works as expected).
systems/halide/NOTES.md:230:- Compile errors from `.parallel()` are thrown as `Halide::CompileError` at schedule time.
systems/futhark/NOTES.md:5:hand them to the generated library (`futhark_new_*`, which copies), call the entries, time them, and compare
systems/futhark/NOTES.md:6:the outputs with the oracles. Build: `systems/futhark/build.sh` (`YIELD=1` also builds the non-spinning runtime,
systems/futhark/NOTES.md:13:   that is the C level-by-level count; this flood checks methods every level), **1,211 parallel operations**.
systems/futhark/NOTES.md:23:   `row with [a : a + m] = xs[f : f + m]` in a sequential loop compiles to a memcpy (through the runtime's
systems/futhark/NOTES.md:29:1. **2 MB pages for the whole runtime are a 1.2-1.7x win on every cell, with no code change.**
systems/futhark/NOTES.md:31:   runtime gets every array from malloc. Intermediate arrays are allocated per entry call and part of that
systems/futhark/NOTES.md:45:   made general: for a runtime that allocates per operation, the page size is a first-order knob.
systems/futhark/NOTES.md:46:2. **The stock multicore runtime spins, and on this shared machine that costs more than the parallelism wins.**
systems/futhark/NOTES.md:68:arrays element by element with `oracle/p1_canon.u32`, `p1_builtin.u32`, `p1_reltarget.u32`, `p1c_canon.u32`,
systems/futhark/NOTES.md:71:and 1c take 1a's canon from its oracle, 3b takes 3a's addresses and `p3_addr_all`, 2lazy takes 1a's canon; each
systems/futhark/NOTES.md:78:min / median ms. 1 = `c` backend; 8 = `multicore`, stock (spinning) runtime unless marked yield. The 8-thread
systems/futhark/NOTES.md:93:\* 2lazy's e2e includes its untimed-in-warm setup (aux_edges: the funcdata and kind 1-3 rows, 260-530 ms), as
systems/futhark/NOTES.md:94:ref/flat/p2lazy.c also excludes from its flood time.
systems/futhark/NOTES.md:116:- First tuning pass (before these numbers): the method rule checked every one of the 106 K methods every level
systems/futhark/NOTES.md:122:- **Can the runtime put 3b's output on 2 MB pages? Yes, without touching the code**:
systems/futhark/NOTES.md:127:  each time); Futhark's warm reps fault less because the image's memory is recycled (even with `FUT_FRESH=1`,
systems/futhark/NOTES.md:128:  which empties the runtime's free list, so glibc keeps it), so the warm number does not pay the image's first
systems/futhark/NOTES.md:150:(17:40Z, 5 interleaved rounds, load 17-20, 4 KB pages; `systems/futhark/levelfree.sh time`.) Easy: the drain is
systems/futhark/NOTES.md:182:  the expand of rows each level, dedup by scatter-the-index-and-check; the R_USETYPE skip rule as a segmented scan
systems/futhark/NOTES.md:185:  scan is exact against the oracle). Forced: the typelink sort (the library's merge sort is GPU-shaped, 13 ms for
systems/futhark/NOTES.md:221:p3.c 87, drv.h 67 (loading, timing, checking only).
systems/futhark/NOTES.md:225:`FUT_PROFILE=1 build/p2_multicore.exe ... 2> prof.json; python3 prof.py prof.json` sums the runtime's
systems/futhark/NOTES.md:227:`FUT_FRESH=1` (p3) empties the runtime's free list before each rep.
systems/futhark/NOTES.md:231:`p4.fut` (one entry, `emit`) + `p4.c` (driver: loads `data/minio-p4`, times, checks). Rebuilds all 60,733,909 bytes
systems/futhark/NOTES.md:241:It compares the image with `oracle/p4_code.bin` byte for byte (memcmp), the offsets with `oracle/p4_fn_off.u64`,
systems/futhark/NOTES.md:242:the relocations with `oracle/p4_rel_*`, and counts holes with an encoding/width the encoder cannot write (0).
systems/futhark/NOTES.md:244:**The shape.** All in Futhark; nothing but loading and checking in C.
systems/futhark/NOTES.md:248:  A node's segments are items (checked: 1,640,416 = 1,640,416), so `seg_item[nseg_off[r] + s]` (one scatter) gives
systems/futhark/NOTES.md:283:**Where the time goes** (1 core, `PARTS` switches pieces off; cpu/rep, 17:46Z, load ~20, so only the proportions
systems/futhark/NOTES.md:284:mean anything): everything but the copy, patch and relocations (lay out, hole tables, the static check, zeroing
systems/futhark/NOTES.md:292:`--unsafe` (bounds checks off); multicore at 1 thread vs the c backend (equal within noise).
systems/futhark/NOTES.md:296:  (index, byte) pairs: removing the fusion (the check moved to the template table) took the entry 1.1 s -> 0.6 s.
systems/futhark/NOTES.md:312:  loop of slice updates inside (C's loop, 64 times in parallel), which needs the first item of each block (a binary
systems/futhark/NOTES.md:318:tables 25, patch_item 12, copy_blocks 18, relocations 15, entry plumbing ~13); p4.c 122 (loading, timing, checking).
systems/futhark/NOTES.md:384:the low 16 bits (`reduce_by_index`) + checksums.  `driver.c` mmaps a raw little-endian u32 file
systems/futhark/NOTES.md:385:(MAP_POPULATE), hands the pointer to the generated library, runs 3 reps, prints a checksum.
systems/futhark/NOTES.md:386:`build.sh` builds both backends; `run.sh [N]` generates N random u32 (xorshift64*) and times.
systems/futhark/NOTES.md:406:Entry time only (input already resident), ms, 3 reps; upload = the copy in `futhark_new_u32_1d`.
systems/futhark/NOTES.md:422:Baselines from the library code (20 M elements, multicore, entry time only, 2 reps, same day):
systems/futhark/NOTES.md:437:- `futhark_new_u32_1d` copies (no zero-copy wrap of a host pointer on c/multicore); budget the copy
systems/futhark/NOTES.md:440:- Indices are `i64`; bounds checks are on by default (`--unsafe` turns them off)
systems/futhark/NOTES.md:443:- `futhark bench`/`-t` report microseconds of entry time excluding input parsing; our driver measures wall clock
systems/futhark/hello/run.sh
systems/goose/hello/run.sh
systems/duckdb/check3b.py
systems/souffle/check_p3.c
systems/halide/hello/run.sh
systems/souffle/hello/run.sh
systems/souffle/out/layout_tlcheck.u32

exec
/bin/bash -lc 'cat systems/souffle/check_p3.c; cat systems/duckdb/check3b.py; cat ATTACKS.md; cat results/second-attempts.md' in /home/exedev/paradigms-w
 succeeded in 0ms:
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
# check a problem-3b image: every placed symbol's [addr, addr+size) must equal oracle/p3_image.bin (not buckets 13-15)
#   python3 -I check3b.py DATA image.bin
import array, sys
d, img = sys.argv[1], open(sys.argv[2], 'rb').read()
want = open(d + '/oracle/p3_image.bin', 'rb').read()
addr = array.array('I', open(d + '/oracle/p3_addr.u32', 'rb').read())
size = array.array('I', open(d + '/p3_size.u32', 'rb').read())
bucket = open(d + '/p3_bucket.u8', 'rb').read()
base = int([l.split()[1] for l in open(d + '/meta.txt') if l.startswith('imageBase ')][0])
bad = n = 0
for i in range(len(addr)):
    if bucket[i] >= 13: continue
    o = addr[i] - base
    n += size[i]
    if img[o:o + size[i]] != want[o:o + size[i]]:
        bad += 1
        if bad <= 5: print('symbol', i, 'bucket', bucket[i], 'differs')
print('%d bytes checked, %d placed symbols differ' % (n, bad))
print('P3b OK' if bad == 0 else 'P3b FAIL')
sys.exit(bad != 0)
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

## 4. Code generation by shape: the emit costs pieces, not bytes; merge templates by width

**Problem.** Code generation by shape (4): emit fc's 60.7 MB of MinIO function bodies from 108,833 byte templates,
7.68 M operands and 17.6 MB of glue (ref/p4/README.md).

**Before.** The obvious model, from copy-and-patch compilers (Xu and Kjolstad 2021; ideas/copy-and-patch.md): a
template table plus a patch pass; speed is bandwidth, and the table should be keyed by typed shape (SHAPES.md: 4,096
shapes cover 96.7% of nodes).

**Observation.** (a) The plain C emit runs at **0.42 GB/s** (1 core, 145 ms median for 60.7 MB), 60x below this VM's
read bandwidth, and patching holes is only **7.6%** of it (codex-p4par, P4_NOPATCH split): the cost is 2.38 M
pieces of ~25 bytes, each a lookup, a variable-length copy and a branch. 8 threads give 2.2x (51/66/68 ms, load ~20).
(b) The typed shape does not fix the bytes (ref/p4/NOTES.md: 74.6% of nodes in the top 4,096 *byte* templates).
One splitter is purely mechanical: a displacement is disp8 exactly when the operand fits in int8, **0 exceptions in
4,864,669 holes** (lead, results/cp1check.py).

**Attack.** Key templates with the displacement width relaxed (store disp32, shrink when the operand fits): **108,833
-> 66,046 templates; the top 4,096 cover 82.3% of nodes instead of 74.6%**, and the emitter reproduces the oracle byte for
byte (codex-cp, `ref/p4/check-cp.sh`, lead rerun P4 OK). But keep each node's length precomputed: deriving lengths
from operands at layout time made the scan 6.3x slower (7.68 M operand reads instead of 2.38 M lengths). Specialising
the hot templates into straight-line C (Deegen's method) cut instructions 26% and the covered segments' emit 1.67x
(median), but ranges overlap and those templates are only 36% of the emitted bytes; 2.9 MB of generated C.

**What it says about the paradigms.** Futhark expresses the whole problem in 159 lines (lengths by map, places by
scan, holes as a flat map, rel32 by gather, relocations by expand) and is correct on the first shapes, at 3.2x C on 1
core; what it had to force is the copy itself (a map cannot memcpy into a slice; a sequential slice-update loop inside
a map over 64 blocks can, 3-5x cheaper than scattering bytes). **Next:** fewer, larger pieces: fuse a node's segments
with hole-free neighbours and glue into one run at extraction (CP7 supernodes), counted as pieces per byte, and the
Halide cell (running).
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

exec
/bin/bash -lc "rg -n 'check|memcmp|oracle|OK' systems/halide/p3.cpp systems/futhark/p3.c systems/goose/p3*; nl -ba systems/futhark/NOTES.md | sed -n '120,133p'; nl -ba JOURNAL.md | tail -n 24; ls systems/souffle/build/check_p3 data/minio/oracle/p3_image.bin" in /home/exedev/paradigms-w
 succeeded in 0ms:
systems/goose/p3par.goose:42:var addr: u32[>..] = [];        // 3b: the oracle's addresses, so 3b does not depend on 3a (as in ref/flat/p3.c)
systems/goose/p3par.goose:66:    load("oracle/p3_addr.u32", addr);
systems/goose/p3par.goose:344:    var bad = check32("addr", addr, "oracle/p3_addr.u32");
systems/goose/p3par.goose:345:    let oaddr = load_u32("oracle/p3_addr.u32");
systems/goose/p3par.goose:349:    load("oracle/p3_image.bin", want);
systems/goose/p3par.goose:381:    format(note, nbad, " placed symbols differ from oracle/p3_image.bin");
systems/goose/p3par.goose:384:    print(if bad == 0 { "P3 OK" } else { "P3 FAIL" });
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
systems/futhark/p3.c:1:// p3.c: driver for p3.fut. Loads the flat files, calls the entries, times them, checks against the oracles.
systems/futhark/p3.c:2://   p3 DATADIR THREADS REPS CELL [nocheck]     CELL: 3a 3a_loop 3b 3b_bytes
systems/futhark/p3.c:10:	int check = !(argc > 5 && strcmp(argv[5], "nocheck") == 0);
systems/futhark/p3.c:25:		// 3a: radix sort + segmented scan; 3a_loop: sequential cursor loop; 3a_cells, 3a_tl, 3a_radix: pieces (no check)
systems/futhark/p3.c:28:		if (strcmp(cell, "3a_cells") == 0) entry = futhark_entry_piece_cells, check = 0;
systems/futhark/p3.c:29:		if (strcmp(cell, "3a_tl") == 0) entry = futhark_entry_piece_typelinks, check = 0;
systems/futhark/p3.c:30:		if (strcmp(cell, "3a_radix") == 0) entry = futhark_entry_piece_radix, check = 0;
systems/futhark/p3.c:34:			fut_check(entry(ctx, &out, nunits, minFunc, bstart, bmin, bucket, cls, size, unit, sym, tlstr, soff, sbin), cell);
systems/futhark/p3.c:39:		if (check) bad += check32("addr", addr, L, "oracle/p3_addr.u32");
systems/futhark/p3.c:42:		// 3b runs on the oracle's addresses, as ref/flat/p3.c does, so it does not depend on 3a
systems/futhark/p3.c:43:		struct futhark_u32_1d *addr = LOAD(u32, "oracle/p3_addr.u32"), *all = LOAD(u32, "p3_addr_all.u32");
systems/futhark/p3.c:55:		// 3b: chunked copy + patch; 3b_bytes: per-byte copy + patch; 3b_copy, 3b_patch: the halves alone (no check)
systems/futhark/p3.c:58:		if (strcmp(cell, "3b_copy") == 0) entry = futhark_entry_copy_only, check = 0;
systems/futhark/p3.c:59:		if (strcmp(cell, "3b_patch") == 0) entry = futhark_entry_patch_only, check = 0;
systems/futhark/p3.c:66:			if (fresh) fut_check(futhark_context_clear_caches(ctx), "clear_caches");
systems/futhark/p3.c:67:			fut_check(entry(ctx, &out, nimg, imageBase, addr, bucket, doff, dbin, rstart, roff, rsiz, rkind, radd, rtgt, all, fsect,
systems/futhark/p3.c:79:		if (check) {
systems/futhark/p3.c:80:			const u8 *want = FMAP(u8, "oracle/p3_image.bin", NULL), *pb = FMAP(u8, "p3_bucket.u8", NULL);
systems/futhark/p3.c:81:			const u32 *oaddr = FMAP(u32, "oracle/p3_addr.u32", NULL), *psize = FMAP(u32, "p3_size.u32", NULL);
systems/futhark/p3.c:86:				if (memcmp(img + o, want + o, psize[i]) != 0 && nbad++ < 5) printf("  symbol %u (bucket %u) differs\n", i, pb[i]);
systems/futhark/p3.c:88:			printf("image: %ld placed symbols differ from oracle/p3_image.bin\n", nbad);
systems/futhark/p3.c:93:	if (check) printf(bad ? "P%s FAIL\n" : "P%s OK\n", cell);
rg: systems/halide/p3.cpp: No such file or directory (os error 2)
   120	## ATTACKS 2 (relocate on huge pages)
   121	
   122	- **Can the runtime put 3b's output on 2 MB pages? Yes, without touching the code**:
   123	  `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` (with `vm.nr_hugepages=512` reserved here). Futhark exposes no
   124	  allocator hook; the C API offers no way to hand it a buffer (`futhark_new_*` copies).
   125	- **Minor faults of 3b**: per warm rep 4,623-6,833 (4 KB) vs 18-80 (2 MB); per process (load + 1 run) 104,142
   126	  (1 core) / 168,488 (8 threads) vs 2,456 / 2,798. The C reference faults 19,109 per rep (a fresh 109 MB image
   127	  each time); Futhark's warm reps fault less because the image's memory is recycled (even with `FUT_FRESH=1`,
   128	  which empties the runtime's free list, so glibc keeps it), so the warm number does not pay the image's first
   129	  touch; the first rep of each process does.
   130	- **Times**, 5 interleaved process runs each, 3 reps per process, 16:31Z, load 5.8-6.1:
   131	  first rep (fresh image) 1 core 181-190 ms (4 KB) vs 119-150 (2 MB); 8 threads 126-137 vs 52-64; later reps
   132	  1 core 112-126 vs 107-116 (within noise), 8 threads 44-67 vs 29-55.
   133	  3b itself is 2-4.5x the C reference (C 1 core 49 / 24 ms on 4 KB / 2 MB pages): the copy and the patch are
    68	stalled. What remains for M3/M4: problem 4 in two paradigms (0 so far; the C reference and oracle exist), kube-apiserver
    69	for the paradigm cells, second attempts on the falls-downs (DuckDB's single recursive query, Soufflé 3a), quiet-machine
    70	reruns of the 8-thread numbers, and the new columns (GraphBLAS for reach as a semiring BFS).
    71	- 2026-11-16T17:15:35Z triaged M2 skeptic: corrected GRID (DuckDB P2 not yet falls-down; Futhark lazy ties eager-with-decode; Soufflé lazy 2x its eager), ATTACKS 3 scope; the M2 paragraph's 'in Futhark and Halide lazy is slower than eager' holds for Halide only.
    72	- 2026-11-16T17:18:19Z M3/M4 launched: 5 Opus builders (Halide P4, Futhark P4, DuckDB P2 + Soufflé 3a second attempts, GraphBLAS, level-free floods) + Codex kube-apiserver runs.
    73	- 2026-11-16T17:19:05Z frontier histogram (ref/flat/p2levels.c, results/levels.md): 81 of 111 levels < 4K frontier, 6% of work; hybrid wide-parallel / sequential-tail flood suggested.
    74	- 2026-11-16T17:19:34Z relocation histogram (results/relkinds.*): 8 (kind,width) pairs, rel32 56%, top 100 targets = 30% of 2.05 M.
    75	- 2026-11-16T17:20:30Z Codex: codex-p4par (tuned C P4) launched in paradigms-c; codex-kube relaunched with full access (sandbox blocked the systemd bus).
    76	- 2026-11-16T17:23:23Z p2hyb (sequential tail flood): 111 -> 12 barriers but ~44% of pops go sequential; times noise at load 19.
    77	- 2026-11-16T17:28:24Z prior art: ideas/copy-and-patch.md (8 ideas for P4 with kill tests); CP1/CP4 queued for Codex.
    78	- 2026-11-16T17:28:49Z verified CP1 premise: disp width == fits-int8, 0 exceptions in 4,864,669 holes (results/cp1check.*).
    79	
    80	- 2026-11-16T17:24Z (codex-p4par): implement P4 OpenMP local scans + chunk-base addition, contiguous function emission balanced by item count, private relocation vectors concatenated in deterministic range order, explicit 2 MB output with fallback. Add warm in-process repeats to serial comparator, oracle check at 1/8 threads, and reproducible interleaved benchmark/count scripts. Oracle passes independently including fallback and constrained team; measurements underway in results/codex-p4par-runs.log.
    81	- 2026-11-16T17:29Z (codex-p4par): five interleaved P4_REPS=64 runs, passive OpenMP, 12 GiB cap, load 14.67–22.45. Serial vs 8-thread median layout 6.945 vs 4.283 ms (overlapping ranges: within noise), emit 145.367 vs 65.553 ms (2.22x, non-overlapping). Every oracle passes; final constrained-team/fallback check passes. Count 60,733,909 copied bytes, 6,333,835 integer/jump patches plus 1,346,646 symbol holes; logical bandwidth scale 5.932–11.349 ms. No-patch delta 7.999 ms at one thread; eight-thread diagnostic inverted under noise, no defensible fraction. Full results and logs: results/codex-p4par.md. This tunes the existing C P4 reference, not a new paradigm cell.
    82	- 2026-11-16T17:30:31Z merged codex-p4par: tuned C P4 (ref/p4/p4par.c); lead reran ref/p4/check.sh: P4 OK at 1 and 8 threads; emit 8 threads 51-68 ms vs 116-174 serial (load ~20); patching only 7.6% of emit.
    83	
    84	2026-11-16 CP1/CP4: build decoder-backed disp32-normalized templates and generated top-4096 emitters on codex-cp. CP1 compresses 108,833 to 66,046 keys (82.252% top-4096 node coverage); full oracle matches. Measure the added operand-dependent length pass and CP4's covered-only control against their explicit kill thresholds; preserve child-segment boundaries separately from normalized template identity.
    85	2026-11-16T17:43Z CP1/CP4 measurements complete: 35 interleaved oracle-passing runs. CP1 66,046 keys / 82.252% top-4096 coverage passes counting, but layout median 42.708 ms vs p4 6.805 ms (6.28x) kills runtime relaxation. CP4 covered medians 46.611/27.865 ms (1.67x) overlap in range: within noise, not an established threshold pass; full 1-thread median 110.532 ms misses 80 ms. Perf shows 26% fewer instructions than p4par, no branch-miss reduction. Full report results/codex-cp.md; independent review + SIB/RIP synthetic and negative-offset tests retained.
    86	- 2026-11-16T17:48:24Z merged codex-cp: CP1 template count 108,833 -> 66,046 (82.3% top-4K) but its operand-derived layout 6.3x slower; CP4 generated emitter 1.67x on covered segments, within noise; both P4 OK (lead rerun).
    87	- 2026-11-16T17:59:57Z merged levelfree builder: level count is not Futhark's cost (45 levels; per-level passes are); Goose async 1.3-1.5x; lead reran both checks (P2 OK). ATTACKS 3 updated.
    88	- 2026-11-16T18:10:45Z merged Futhark P4 (lead rebuilt + reran c/multicore: P4bf OK): 3.2x C on 1 core, ~5x on 8; slice-update loop inside a map = bulk copy.
    89	- 2026-11-16T18:20:04Z merged second attempts (Soufflé 3a no longer falls down: 1.9 s ranked chain; DuckDB P2 falls down after 3) and GraphBLAS column (P1/P2 OK, 9-24x tuned C, does not scale: frontiers below its threading threshold); lead reran every check.
    90	- 2026-11-16T18:20:53Z merged codex-kube: all 5 paradigms' P1-P3 cells correct on kube-apiserver; 1-thread scaling 1.5-2.5x; Halide 1a 6.7x flagged.
    91	- 2026-11-16T18:21:18Z ATTACKS 4: emit costs pieces not bytes (0.42 GB/s, patch 7.6%); width-relaxed templates 108,833 -> 66,046.
data/minio/oracle/p3_image.bin
systems/souffle/build/check_p3

codex
The current files have already withdrawn several earlier verdicts. I found a remaining warm-versus-fresh timing mismatch and the deferred 6.7× Halide scaling anomaly; I’m checking whether any current correctness claim has a faulty oracle.
exec
/bin/bash -lc "rg --files systems/halide | head -25; cat systems/halide/CELLS.md; rg -n 'check|memcmp|OK|oracle' systems/halide/driver*; cat systems/futhark/drv.h; sed -n '330,390p' systems/goose/p3par.goose; rg -n 'time left|usage|session|budget|remaining|defer|deadline' JOURNAL.md" in /home/exedev/paradigms-w
 exited 1 in 32ms:
systems/halide/IDEAS.md
systems/halide/hello/mkdata.cpp
systems/halide/hello/driver.cpp
systems/halide/hello/gen.cpp
systems/halide/hello/run.sh
systems/halide/hello/build.sh
systems/halide/CELLS.md
systems/halide/NOTES.md
systems/halide/bench.sh
systems/halide/cells/gen_p3.cpp
systems/halide/cells/p2.cpp
systems/halide/cells/build.sh
systems/halide/cells/bench.h
systems/halide/cells/gen_p1.cpp
systems/halide/cells/p3.cpp
systems/halide/cells/gen_p2.cpp
systems/halide/cells/p1.cpp
systems/halide/cells/join.h
# Halide cells (for GRID.md), MinIO, 2026-11-16T17:00-17:10Z, load 1.8-7.3. ms: per-iteration min-max of 5 runs >= 1 s.
# Check: build with systems/halide/cells/build.sh; B=systems/halide/cells/build; D=data/minio (absolute path).

**1. resolve** — correct: P1 OK (`HL_NUM_THREADS=8 $B/p1 $D e2e 1`: canon, builtin, reltarget, p1c canon, all 0 differ).
1a join 81-93 / **19.8-27** (1 / 8 thr, warm) · 1b targets 42.7-46 / **9.9-10.9** · 1c content join 122-133 / **33-36.5**
· counts 10 / 2 · e2e process 0.35-0.40 s / 0.13-0.17 s · ~390 lines (gen_p1 219 + join.h 60 + driver 80 + counts).
Easy: winners as atomic max updates, targets as a ragged gather. Forced: no hash table, so the join is rounds of
scatter-min + gather (join.h); scans are serial, the parallel compaction is hand-blocked; sizes need a separate
pipeline. Impossible: none. Surprise: 1a beats plain C 2.5-3x on 1 core (compact first, small tables), not tuned C.

**2. reach** — correct: P2 OK (`HL_NUM_THREADS=8 $B/p2 $D e2e 1`; 30/30 parallel runs). lazy decode in the flood
417-465 / **79-90** · eager over p2_edge 312-332 / **70-127** · e2e 0.55-0.59 / 0.24-0.25 s (incl. 115 ms untimed C
edge split) · gen_p2 241 + driver 125 lines. Easy: each rule one select. Forced: level loop in C++ (no fixpoint),
implicit frontier + block lists (no worklist), 3 iterations per edge (one site per iteration), a byte per fact with
racy constant stores (atomic() on such an update = a mutex array per call, 3.5 s, and lost symbols). Impossible:
worklist, fixpoint, test-and-set. ATTACKS 1: lazy is expressible but does not beat the eager C pipeline (30.1 ms);
lazy is slower than eager in Halide.

**3a. lay out** — correct: `$B/p3 $D 3a` (addr 0 differ). 53-58 / **20-23** · in e2e 22-31 · 154 lines.
Easy: sizes, per-unit sums, alignment walk. Forced: two-site cursor bump as a phase RVar; merge sort with merge-path
binary search + extern comparator (no sort). Impossible: none.

**3b. relocate** — correct: `$B/p3 $D 3b` (image 0 differ, 0 errors). warm 78-82 / **24.9-27**; fresh output 4 KB
pages 113-119 (19,109 faults) / 42.7-45 (19,187); **2 MB pages 84.6-88 (39 faults) / 27-30.8 (170)** · e2e (3a+3b)
0.21-0.25 / 0.10-0.12 s · 106 lines. Easy: ragged copy, 8-kind select chain. Forced: values stored byte by byte with
scratch bytes for unwritten ones (a non-innermost where is not a loop bound); chunked parallel. ATTACKS 2: yes, the
caller maps the output MAP_HUGETLB; faults 19 K -> 39, 1.3-1.6x faster, but 4-5x C on 2 MB pages.
rg: systems/halide/driver*: IO error for operation on systems/halide/driver*: No such file or directory (os error 2)
// drv.h: what every Futhark driver shares. The drivers only load the flat files (mmap, via ref/flat/flat.h),
// hand them to the generated library, time the entry points and check the outputs; all compute is in .fut.
// Include after the generated header.
#include "../../ref/flat/flat.h"
#include <sys/resource.h>

static struct futhark_context *ctx;
static int threads = 0, reps = 5;

static void fut_init(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "/home/exedev/paradigms/data/minio";
	threads = argc > 2 ? atoi(argv[2]) : 0;
	reps = argc > 3 ? atoi(argv[3]) : 5;
	struct futhark_context_config *cfg = futhark_context_config_new();
#ifdef MULTICORE
	if (threads > 0) futhark_context_config_set_num_threads(cfg, threads);
#endif
	const char *tune = getenv("FUT_TUNING");
	if (tune && *tune) {
		FILE *f = fopen(tune, "r");
		char k[256];
		long v;
		while (f && fscanf(f, "%255[^=]=%ld\n", k, &v) == 2)
			if (futhark_context_config_set_tuning_param(cfg, k, v)) fprintf(stderr, "tuning: no %s\n", k);
		if (f) fclose(f);
	}
	if (getenv("FUT_PROFILE")) futhark_context_config_set_profiling(cfg, 1);
	ctx = futhark_context_new(cfg);
}
// FUT_PROFILE=1: the runtime's per-kernel report (JSON) to stderr
static void fut_report(void) {
	if (getenv("FUT_PROFILE")) {
		char *r = futhark_context_report(ctx);
		fputs(r, stderr);
		free(r);
	}
}
static void fut_check(int err, const char *what) {
	if (err) {
		fprintf(stderr, "%s: %s\n", what, futhark_context_get_error(ctx));
		exit(1);
	}
}
#define SYNC() fut_check(futhark_context_sync(ctx), "sync")
// a flat file as a Futhark array (copied into Futhark's memory: futhark_new_* copies on c/multicore)
#define LOADER(T, CT)                                                                                        \
	static struct futhark_##T##_1d *load_##T(const char *file) {                                             \
		size_t n;                                                                                            \
		const void *p = fmap(file, &n);                                                                      \
		struct futhark_##T##_1d *a = futhark_new_##T##_1d(ctx, (const CT *)p, (int64_t)(n / sizeof(CT)));   \
		if (n) munmap((void *)p, n);                                                                         \
		return a;                                                                                            \
	}
#define LOAD(T, file) load_##T(file)

static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
// time `body` reps times; prints every time, then min / median / max
#define TIMEIT(name, body)                                                                                   \
	do {                                                                                                     \
		double ts_[1000];                                                                                    \
		int n_ = reps < 1000 ? reps : 1000;                                                                  \
		struct rusage ru0_, ru1_;                                                                            \
		printf("%-14s", name);                                                                               \
		getrusage(RUSAGE_SELF, &ru0_);                                                                       \
		for (int rep_ = 0; rep_ < n_; rep_++) {                                                              \
			double t0_ = now();                                                                              \
			body;                                                                                            \
			SYNC();                                                                                          \
			ts_[rep_] = (now() - t0_) * 1e3;                                                                 \
			printf(" %.1f", ts_[rep_]);                                                                      \
		}                                                                                                    \
		getrusage(RUSAGE_SELF, &ru1_);                                                                       \
		qsort(ts_, n_, sizeof(double), cmpd);                                                                \
		printf("  | min %.2f med %.2f max %.2f ms (threads %d) minflt/rep %ld cpu/rep %.1f ms\n", ts_[0], ts_[n_ / 2],       \
		       ts_[n_ - 1], threads, (ru1_.ru_minflt - ru0_.ru_minflt) / n_,                                          \
		       ((ru1_.ru_utime.tv_sec - ru0_.ru_utime.tv_sec + ru1_.ru_stime.tv_sec - ru0_.ru_stime.tv_sec) * 1e3 +     \
		        (ru1_.ru_utime.tv_usec - ru0_.ru_utime.tv_usec + ru1_.ru_stime.tv_usec - ru0_.ru_stime.tv_usec) * 1e-3) / n_); \
	} while (0)
    let f0 = minor_faults(false);
    let t2b = clock();
    let mhb = fresh_pages(img, meta("image_bytes"), huge);
    img.resize(meta("image_bytes"), 0);
    let t2c = clock();
    for j in W { send(j, Go { go: true }); }
    for j in W {
        let s = qget<Seg>();
        img[s.at..s.at + s.bytes.len].move_from(s.bytes[..]);
    }
    let t3 = clock();
    let gflt = minor_faults(false) - f0;
    for id in ids { thread_wait(id); }
    let t4 = clock();
    var bad = check32("addr", addr, "oracle/p3_addr.u32");
    let oaddr = load_u32("oracle/p3_addr.u32");
    let pbucket = load_u8("p3_bucket.u8");
    let psize = load_u32("p3_size.u32");
    var want: u8[>..] = [];
    load("oracle/p3_image.bin", want);
    let imageBase = meta("imageBase");
    var nbad = 0;
    for i in Lm {
        guard pbucket[i] < bBSS else { continue; }
        let o = oaddr[i] - imageBase;
        let n = psize[i] as i64;
        if img[o..o + n] != want[o..o + n] {
            if nbad < 5 { print("  symbol ", i, " (bucket ", pbucket[i], ") differs"); }
            nbad++;
        }
    }
    var note: u8[>..] = [];
    format(note, "p3par goose (", W, " workers): ");
    report(note, "spawn+load", t0, t1);
    report(note, "3a", t1, t2);
    format(note, "3b distributed (", if huge { "2 MB" } else { "4 KB" }, " pages, x", reps, ") ");
    sort(times) { x, y => x < y };
    format_flt(note, times[0], 1);
    format(note, " / ");
    format_flt(note, times[times.len / 2], 1);
    format(note, " / ");
    format_flt(note, times[times.len - 1], 1);
    format(note, " ms min/median/max (slowest worker ");
    format_flt(note, wmax, 1);
    format(note, " ms, ", flt, " minor faults/rep in workers, ", hb, " bytes on huge pages)  ");
    report(note, "gather: image alloc", t2b, t2c);
    report(note, "segments into it", t2c, t3);
    format(note, "(", gflt, " faults in the process, ", mhb, " image bytes on huge pages)  ");
    format(note, "warm 3a + best 3b distributed ");
    format_flt(note, ms(t1, t2) + times[0], 1);
    format(note, " ms  ");
    format(note, nbad, " placed symbols differ from oracle/p3_image.bin");
    print(note);
    bad += nbad;
    print(if bad == 0 { "P3 OK" } else { "P3 FAIL" });
    if bad != 0 { exit(1); }
}

exec
/bin/bash -lc "cat systems/halide/cells/p3.cpp; rg -n 'check32' ref/flat/flat.h; sed -n '65,100p' systems/futhark/p3.c; ls systems/halide/cells/build/p3 systems/goose/bin/p3 systems/futhark/build/p3_c.exe; nl -ba ATTACKS.md | tail -n 75" in /home/exedev/paradigms-w
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
                printf("3b %-16s fresh %s " STAT_FMT ", %ld minor faults/run\n", relocs[v].name, huge ? "2MB" : "4KB", STAT_ARGS(sf), faults / n);
            }
        }
    }
    printf(bad ? "P3 FAIL\n" : "P3 OK\n");
    fflush(stdout);
    return bad != 0;
}
71:static long check32(const char *what, const u32 *got, size_t n, const char *oracle) {
			if (out) futhark_free_opaque_tup2_arr1d_u8_i64(ctx, out);
			if (fresh) fut_check(futhark_context_clear_caches(ctx), "clear_caches");
			fut_check(entry(ctx, &out, nimg, imageBase, addr, bucket, doff, dbin, rstart, roff, rsiz, rkind, radd, rtgt, all, fsect,
			                unreach, mapinit), cell);
		});
		struct futhark_u8_1d *fimg;
		int64_t nerr;
		futhark_project_opaque_tup2_arr1d_u8_i64_0(ctx, &fimg, out);
		futhark_project_opaque_tup2_arr1d_u8_i64_1(ctx, &nerr, out);
		u8 *img = malloc(nimg);
		double tc = now();
		futhark_values_u8_1d(ctx, fimg, img);
		SYNC();
		printf("copy out %.1f ms, %ld errors\n", (now() - tc) * 1e3, (long)nerr);
		if (check) {
			const u8 *want = FMAP(u8, "oracle/p3_image.bin", NULL), *pb = FMAP(u8, "p3_bucket.u8", NULL);
			const u32 *oaddr = FMAP(u32, "oracle/p3_addr.u32", NULL), *psize = FMAP(u32, "p3_size.u32", NULL);
			long nbad = 0;
			for (u32 i = 0; i < L; i++) {
				if (pb[i] >= 13) continue;
				size_t o = oaddr[i] - imageBase;
				if (memcmp(img + o, want + o, psize[i]) != 0 && nbad++ < 5) printf("  symbol %u (bucket %u) differs\n", i, pb[i]);
			}
			printf("image: %ld placed symbols differ from oracle/p3_image.bin\n", nbad);
			bad += nbad + (nerr != 0);
		}
	}
	fut_report();
	if (check) printf(bad ? "P%s FAIL\n" : "P%s OK\n", cell);
	return bad != 0;
}
systems/futhark/build/p3_c.exe
systems/goose/bin/p3
systems/halide/cells/build/p3
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
   152	## 4. Code generation by shape: the emit costs pieces, not bytes; merge templates by width
   153	
   154	**Problem.** Code generation by shape (4): emit fc's 60.7 MB of MinIO function bodies from 108,833 byte templates,
   155	7.68 M operands and 17.6 MB of glue (ref/p4/README.md).
   156	
   157	**Before.** The obvious model, from copy-and-patch compilers (Xu and Kjolstad 2021; ideas/copy-and-patch.md): a
   158	template table plus a patch pass; speed is bandwidth, and the table should be keyed by typed shape (SHAPES.md: 4,096
   159	shapes cover 96.7% of nodes).
   160	
   161	**Observation.** (a) The plain C emit runs at **0.42 GB/s** (1 core, 145 ms median for 60.7 MB), 60x below this VM's
   162	read bandwidth, and patching holes is only **7.6%** of it (codex-p4par, P4_NOPATCH split): the cost is 2.38 M
   163	pieces of ~25 bytes, each a lookup, a variable-length copy and a branch. 8 threads give 2.2x (51/66/68 ms, load ~20).
   164	(b) The typed shape does not fix the bytes (ref/p4/NOTES.md: 74.6% of nodes in the top 4,096 *byte* templates).
   165	One splitter is purely mechanical: a displacement is disp8 exactly when the operand fits in int8, **0 exceptions in
   166	4,864,669 holes** (lead, results/cp1check.py).
   167	
   168	**Attack.** Key templates with the displacement width relaxed (store disp32, shrink when the operand fits): **108,833
   169	-> 66,046 templates; the top 4,096 cover 82.3% of nodes instead of 74.6%**, and the emitter reproduces the oracle byte for
   170	byte (codex-cp, `ref/p4/check-cp.sh`, lead rerun P4 OK). But keep each node's length precomputed: deriving lengths
   171	from operands at layout time made the scan 6.3x slower (7.68 M operand reads instead of 2.38 M lengths). Specialising
   172	the hot templates into straight-line C (Deegen's method) cut instructions 26% and the covered segments' emit 1.67x
   173	(median), but ranges overlap and those templates are only 36% of the emitted bytes; 2.9 MB of generated C.
   174	
   175	**What it says about the paradigms.** Futhark expresses the whole problem in 159 lines (lengths by map, places by
   176	scan, holes as a flat map, rel32 by gather, relocations by expand) and is correct on the first shapes, at 3.2x C on 1
   177	core; what it had to force is the copy itself (a map cannot memcpy into a slice; a sequential slice-update loop inside
   178	a map over 64 blocks can, 3-5x cheaper than scattering bytes). **Next:** fewer, larger pieces: fuse a node's segments
   179	with hole-free neighbours and glue into one run at extraction (CP7 supernodes), counted as pieces per byte, and the
   180	Halide cell (running).

exec
/bin/bash -lc 'HL_NUM_THREADS=8 systems/halide/cells/build/p3 data/minio 3b' in /home/exedev/paradigms-w
exec
/bin/bash -lc "nl -ba systems/halide/cells/p3.cpp | sed -n '130,163p'; nl -ba JOURNAL.md | head -n 8; nl -ba CHARTER.md | sed -n '70,103p'" in /home/exedev/paradigms-w
 succeeded in 0ms:
   130	            printf("3b %-16s warm        " STAT_FMT "\n", relocs[v].name, STAT_ARGS(s));
   131	            // fresh: a new zeroed mapping per run, its page faults inside the timed region (ATTACKS.md 2): 4 KB
   132	            // pages (mmap anonymous) and 2 MB pages (MAP_HUGETLB: needs vm.nr_hugepages >= 53)
   133	            for (int huge = 0; huge < 2; huge++) {
   134	                size_t len = (nimg + 8 + (2u << 20) - 1) & ~(size_t)((2u << 20) - 1);
   135	                long faults = 0, n = 0;
   136	                bool ok = true;
   137	                Stat sf = timeRuns([&] {
   138	                    struct rusage r0, r1;
   139	                    getrusage(RUSAGE_SELF, &r0);
   140	                    void *m = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | (huge ? MAP_HUGETLB : 0), -1, 0);
   141	                    if (m == MAP_FAILED) { ok = false; return; }
   142	                    Buffer<u8, 1> im((u8 *)m, (int)nimg + 8);
   143	                    relocs[v].fn(bucket, addr, doff, dbin, rstart, roff, rsiz, rkind, radd, rtgt, addrAll, sect, imageBase, unreach, mapinit, im, nerr);
   144	                    munmap(m, len);
   145	                    getrusage(RUSAGE_SELF, &r1);
   146	                    faults += r1.ru_minflt - r0.ru_minflt, n++;
   147	                });
   148	                if (!ok) { printf("3b %-16s %s: mmap failed (huge pages reserved?)\n", relocs[v].name, huge ? "2 MB" : "4 KB"); continue; }
   149	                printf("3b %-16s fresh %s " STAT_FMT ", %ld minor faults/run\n", relocs[v].name, huge ? "2MB" : "4KB", STAT_ARGS(sf), faults / n);
   150	            }
   151	        }
   152	    }
   153	    printf(bad ? "P3 FAIL\n" : "P3 OK\n");
   154	    fflush(stdout);
   155	    return bad != 0;
   156	}
     1	# JOURNAL (append-only: one line per commit, a paragraph at each milestone)
     2	- 2026-11-16T15:31:42Z M0 data: MinIO frozen (1,311 archives + main.a), hlink copied to ref/hlink with -dumpflat, flat files + oracles for P1-P3 (ref/flat/README.md); plain 1-core C references reproduce every oracle exactly (ref/flat/check.sh). Installs merged: Futhark 0.29.1, Halide 21, Soufflé 2.5, Goose v2026.4 (systems/*/NOTES.md).
     3	- 2026-11-16T15:34:24Z M0: GRID.md C column (hlink phases on this VM, plain flat C references), IDEAS.md index, four Opus cell builders launched (ideas/BUILDER.md).
     4	- 2026-11-16T15:36:04Z ref/flat/p2par.c: level-synchronous OpenMP flood on the flat CSR, full rules, P2 OK: 33.4 ms 1 thread, 12.1 ms 8 threads (load 14: provisional), 111 levels in 7 rounds; hlink's flood is 98-103 / 22-23 ms on raw objects. Problem 4 data/oracle handed to an Opus agent.
     5	- 2026-11-16T15:37:00Z ref/flat/p1par.c: partitioned join (256 partitions, per-partition tables) + parallel gather, P1 OK: 72.7 ms 1 thread, 19.2 ms 8 threads (load 7, provisional) vs plain 230 ms; hlink resolve 29-33 ms on raw objects.
     6	- 2026-11-16T15:54:54Z triage M0 skeptic: flat README contract fixes (name offsets, p1c_mark meaning, rank bit 30, id 0, p3_addr_all leak); stats.c note
     7	- 2026-11-16T15:57:39Z GRID: tuned flat C column at load 0.8 (p1par 13.9-16.8 ms, p2par 13.0-17.0 ms on 8 threads vs hlink resolve 24, floods 22-23); kube-apiserver flat files + all C refs OK; dumpflat decode-to-CSR timed 133-139 ms; Codex: p3 pipeline + 1c par (codex-c), p2pipe eager decode (codex-d)
     8	- 2026-11-16T15:58:28Z new column DuckDB (Opus builder, same brief): the north star names databases; DuckDB is installed (zero install cost), wildcard's top-2, and is the only vectorized-SQL/morsel-parallel engine in the grid (Soufflé is compiled Datalog, a different axis). Started before M2 because 4 builders + 2 Codex jobs leave the lead idle.
    70	The grid is a starting frame, not a cage. Leave room to be surprised in both directions.
    71	
    72	- **A surprising result jumps the queue.** A paradigm beating hlink, a paradigm failing for a
    73	  reason nobody predicted, a problem that turns out to be the wrong question: chase it before
    74	  filling routine cells, and put it at the top of STATUS.md.
    75	- **Negative results are results**, and often the most useful ones. A cell is "falls down"
    76	  only after two serious attempts, each using the system's own idioms and tuning (Halide's
    77	  schedules, Soufflé's indexes and parallel mode, Futhark's backends), with the reason written
    78	  down. A toy that proves nothing is not an attempt.
    79	- **Add a column when the evidence points at one.** If prior-art reading or a cell's failure
    80	  points at a system not listed (Differential Dataflow, BQN, DuckDB, ISPC, Taichi, a research
    81	  prototype from a paper), add it and say why in JOURNAL.md.
    82	- **Abuse a paradigm if that's where the idea is.** Using Halide on a graph or Datalog for
    83	  layout is fair, and the place it strains is a finding.
    84	
    85	## Measuring on a noisy machine
    86	
    87	The VM shares its host with other VMs, and the run's own builders compile in the background,
    88	so a single millisecond timing means little. Don't trust one, and don't stall on noise either.
    89	
    90	- **Speed here is about orders of magnitude.** The grid asks whether a paradigm is at 2x, 10x or
    91	  100x of the reference and the floor, and why. A 5% difference is not a finding; don't spend
    92	  effort resolving it.
    93	- **Make the work meaty.** Repeat a kernel inside one process until a run takes at least a
    94	  second, and report time per iteration; or use the bigger dataset (kube-apiserver), or scale
    95	  the data up (MinIO's data several times over, with renumbered IDs). Then report the per-unit
    96	  number next to the floor.
    97	- **Repeat and report the range.** At least five runs, interleaved with whatever it's compared
    98	  against, in the same window. Report min, median and max. A difference counts only when the
    99	  ranges don't overlap; otherwise write "within noise" and move on.
   100	- **Count the mechanism when you can.** Bytes touched, passes over the data, cache misses
   101	  (`perf stat` if it works here), instructions. A count explains a time and doesn't drift.
   102	- **Never wait for a quiet machine.** Note the load (`uptime`) next to a measurement. Pin a
   103	  benchmark to cores the builders aren't using (`taskset`) if the comparison needs it, or take

exec
/bin/bash -lc "cat systems/halide/cells/bench.h; cat systems/halide/cells/build/p3_variants.h | tail -n 22; nl -ba ATTACKS.md | sed -n '98,107p'; nl -ba systems/goose/NOTES.md | sed -n '12,35p'" in /home/exedev/paradigms-w
 succeeded in 0ms:
// bench.h: timing on a noisy machine (CHARTER.md): repeat the kernel in-process until one run takes >= 1 s,
// five runs, report per-iteration min / median / max in ms.
#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>
struct Stat { double mn, md, mx; int k; };
static Stat timeRuns(std::function<void()> f, int runs = 5, double minRun = 1.0) {
    double t = now();
    f();
    double one = now() - t;
    int k = std::max(1, (int)std::ceil(minRun / std::max(one, 1e-6)));
    std::vector<double> v;
    for (int r = 0; r < runs; r++) {
        t = now();
        for (int i = 0; i < k; i++) f();
        v.push_back((now() - t) / k * 1e3);
    }
    std::sort(v.begin(), v.end());
    return {v[0], v[runs / 2], v[runs - 1], k};
}
#define STAT_FMT "min %8.2f  med %8.2f  max %8.2f ms (x%d)"
#define STAT_ARGS(s) (s).mn, (s).md, (s).mx, (s).k
#include "reloc_f0_pf_v0.h"
#include "reloc_f0_pf_v32.h"
#include "reloc_f0_pt_v0.h"
#include "reloc_f0_pt_v32.h"
static struct { const char *name; decltype(&layout_c0_w0) fn; } layouts[] = {
    {"layout_c0_w0", layout_c0_w0},
    {"layout_c0_w1", layout_c0_w1},
    {"layout_c1_w0", layout_c1_w0},
    {"layout_c1_w1", layout_c1_w1},
    {"layout_c2_w0", layout_c2_w0},
    {"layout_c2_w1", layout_c2_w1},
    {"layout_c3_w0", layout_c3_w0},
    {"layout_c3_w1", layout_c3_w1},
};
static const int nLayouts = sizeof layouts / sizeof layouts[0];
static struct { const char *name; decltype(&reloc_f0_pf_v0) fn; } relocs[] = {
    {"reloc_f0_pf_v0", reloc_f0_pf_v0},
    {"reloc_f0_pf_v32", reloc_f0_pf_v32},
    {"reloc_f0_pt_v0", reloc_f0_pt_v0},
    {"reloc_f0_pt_v32", reloc_f0_pt_v32},
};
static const int nRelocs = sizeof relocs / sizeof relocs[0];
    98	## 3. Reach: the level count is the cost; drop level synchronization
    99	
   100	**Problem.** Reach (2), the same flood as entry 1.
   101	
   102	**Before.** Every data-parallel shape of the flood is level-synchronous: expand the frontier, barrier, swap. MinIO's
   103	reach has **111 levels in 7 rounds** (kube 103). p2par (OpenMP) pays one team barrier per level; Futhark pays ~25
   104	whole-array passes per level (its lazy flood "falls down": 165 / 90 ms, slower than its own eager flood, NOTES.md);
   105	Goose pays a message exchange per superstep and scales only 1.7x on 8 workers; DuckDB iterates 45 SQL rounds.
   106	
   107	**Observation.** On this shared 8-core VM the barrier is not a constant: with OpenMP's default spinning wait, p2par's
    12	## Surprises
    13	
    14	1. **The shared-nothing tax is the gather, not the compute.** 3b on 8 workers builds its image in 17-24 ms (2 MB pages)
    15	   when each worker keeps its address range; gathering the 109 MB into one array in main costs **142-171 ms** more
    16	   (two copies through the queue plus main's page faults: 57-83 k faults). Same in 1b: 9.4 M targets computed in
    17	   parallel and sent to main take 42-67 ms on 8 workers vs 39-57 ms on 1 thread. The workloads parallelize; moving
    18	   their outputs between isolated programs is 5-8x the compute.
    19	2. **Goose's runtime pins the output to 4 KB pages, but a 20-line C sidecar can swap the pages under a builder's stack
    20	   top for hugetlb pages, and it halves 3b** (ATTACKS 2): 1 thread 86.7-98.5 ms -> **40.8-42.2 ms**, minor faults per
    21	   rep 26,731 -> 258-457; 8 workers 32.8-40.0 -> **17.4-23.5 ms**. The runtime reserves each data stack as a NORESERVE
    22	   4 KB mapping and madvises MADV_NOHUGEPAGE on its first 2 MB; the language has no word for page size.
    23	3. **P1's first repetition is 2.5x its later ones** (1a 213 ms once vs 79-115 ms in reps 2-5): the 200 MB open-addressing
    24	   table (8 M groups x 24 B) and the 1c table are first-touched in rep 1; later reps reuse the same stack pages (a
    25	   resizable cleared and regrown on the same data stack never returns its pages). The warm P1 numbers below are those
    26	   later reps; the C references' single-run numbers include their table faults. One run of p1 (all of 1a+1b+1c, first
    27	   touch included) is 560 ms of compute (16:15Z, load 2).
    28	
    29	## ATTACKS 3 (no supersteps: is the level count the cost?)
    30	
    31	Full write-up with counts and both windows: results/levelfree.md. `p2async.goose` keeps p2par's owner partition and
    32	p2rules.goose's local rules and drops the supersteps: a worker floods its part to a local fixpoint (or until
    33	`P2FLUSH` ids wait), posts ids straight into the owners' inboxes and any new global fact (signature, method name,
    34	reflect) to every worker, takes what has arrived with `qpoll`, and goes on; idle, it reports its per-peer sent and
    35	received batch counts to main and blocks on its inbox. Main stops it when every latest report is idle and

exec
/bin/bash -lc "nl -ba systems/halide/cells/p3.cpp | sed -n '121,131p'; nl -ba STATUS.md; rg -n '6.7|Halide|1a' results/codex-kube.md | tail -n 12; git status --short" in /home/exedev/paradigms-w
 succeeded in 0ms:
   121	            bad += check32("addr", addr.data(), L, "oracle/p3_addr.u32") != 0;
   122	        }
   123	        memcpy(addr.data(), FMAP(u32, "oracle/p3_addr.u32", NULL), 4 * (size_t)L);
   124	        for (int v = 0; v < nRelocs; v++) {
   125	            if (mode != "all" && mode != "3b" && mode != relocs[v].name) continue;
   126	            // warm: the image buffer is reused (its pages are already mapped)
   127	            memset(imgp, 0, nimg);
   128	            Stat s = timeRuns([&] { runReloc(v); });
   129	            bad += checkImage();
   130	            printf("3b %-16s warm        " STAT_FMT "\n", relocs[v].name, STAT_ARGS(s));
   131	            // fresh: a new zeroed mapping per run, its page faults inside the timed region (ATTACKS.md 2): 4 KB
     1	# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)
     2	
     3	2026-11-16T17:22Z. **M1 + M2 done**: Goose, Futhark, Halide, Soufflé and DuckDB are each correct on problems 1-3 (lead-verified).
     4	Oracle number: **16/20 cells + DuckDB 3; 3 ATTACKS backed by measurement.** No paradigm beats tuned C on any cell (Halide is
     5	closest, 1.2-2.5x; Soufflé furthest, 30-200x); the distances and their causes are in GRID.md and MORNING.md.
     6	ATTACKS 1: decode in the flood, 2x the eager graph build (C). 2: huge pages: relocate at the VM's bandwidth bound
     7	(5.4-6.8 ms on 8), and `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` gets it with no code. 3: drop level sync: 111 levels cost
     8	every paradigm; a worklist flood is 1.7x passive OpenMP under overload, 25x default spinning.
     9	Next (M3/M4): problem 4 in two paradigms, kube-apiserver for paradigm cells, second attempts on falls-downs, GraphBLAS.
    10	
    11	## Decisions for Dan
    12	- None. (vm.nr_hugepages is 1536 now; not persistent across a reboot.)
224:| 1a resolve_t | 8 | OK | 79.41 / 84.46 / 94.32 | 19.8–27 | 3.61x |
230:| 3a layout_c0_w1 | 8 | OK | 46.76 / 47.80 / 48.37 | — | — |
243:| 3b reloc_f0_pt_v0 warm | 8 | OK | 62.16 / 66.76 / 72.31 | — | — |
274:DuckDB checks ran **2026-11-16 17:21:30–17:25:56 UTC**, benchmark **17:25:56–18:15:01 UTC**. Sampled one-minute uptime load over the check/benchmark window ranged **2.43–25.72**; benchmark start `23.13, 14.90, 8.32`, end `3.76, 5.66, 9.93` (1/5/15-minute loads). [Sampled uptime log](codex-kube-duckdb-load.log), [checks/raw summary](codex-kube-duckdb-raw.txt), [five-round benchmark](codex-kube-duckdb-bench.txt). The long benchmark overlapped Goose, Halide/Futhark, Soufflé, short C references, and other builders during portions of its window. Every one of its 22 warm rows and ten e2e rows has five rounds.
285:| 1a | OK / OK | 7068.2 / 10490.7 / 13011.5 | 2860.3 / 3438.0 / 4699.4 | 1928–2034 / 636 | 5.30× / 5.41× |
286:| 1b | OK / OK | 13002.9 / 17545.4 / 23584.9 | 4252.4 / 6378.8 / 8177.3 | 5220–5426 / 1080 | 3.30× / 5.91× |
291:| 3a | OK / OK | 15344.3 / 17146.8 / 38350.5 | 4558.8 / 6677.2 / 8814.9 | 7222–7589 / 1339 | 2.32× / 4.99× |
298:| 1a / e2e | 11196.1 / 15063.0 / 17857.4 | 7431.4 / 7500.2 / 10343.1 |
303:| 2-lazy / e2e | 9107.0 / 10761.5 / 13481.1 | 6181.5 / 9951.2 / 25657.4 |
313:| 1a | OK / OK | 877.5 / 991.5 / 1499.9 | 225.9 / 411.2 / 590.1 | — / 103–113 | — / 3.81× |
317:| 2loop | OK / OK | 5004.1 / 6767.4 / 9075.5 | 2767.2 / 3687.5 / 5904.1 | — / 1965–2354 | — / 1.71× |
330:| P1-P3 / e2e | 12946.5 / 14437.6 / 18607.5 | 3535.2 / 5739.1 / 9433.7 |
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
Halide’s relocation checker validates the reused buffer, but unmaps each fresh 4 KB/2 MB output without checking it. The command is running and has reported zero differences for the warm buffers. I found no JOURNAL passage that declines work because of time, usage, or session ending.
 succeeded in 66072ms:
threads=8
image: 0 placed symbols differ from oracle/p3_image.bin, 0 relocation errors
3b reloc_f0_pf_v0   warm        min    84.75  med    97.79  max   117.08 ms (x11)
3b reloc_f0_pf_v0   fresh 4KB min   123.96  med   144.53  max   176.72 ms (x7), 19109 minor faults/run
3b reloc_f0_pf_v0   fresh 2MB min   101.47  med   149.20  max   159.43 ms (x10), 39 minor faults/run
image: 0 placed symbols differ from oracle/p3_image.bin, 0 relocation errors
3b reloc_f0_pf_v32  warm        min    99.70  med   127.49  max   132.10 ms (x8)
3b reloc_f0_pf_v32  fresh 4KB min   131.94  med   150.59  max   227.39 ms (x6), 19109 minor faults/run
3b reloc_f0_pf_v32  fresh 2MB min    88.55  med   106.89  max   112.87 ms (x12), 39 minor faults/run
image: 0 placed symbols differ from oracle/p3_image.bin, 0 relocation errors
3b reloc_f0_pt_v0   warm        min    55.43  med    57.72  max    60.00 ms (x21)
3b reloc_f0_pt_v0   fresh 4KB min    92.49  med    98.02  max   105.42 ms (x9), 19176 minor faults/run
3b reloc_f0_pt_v0   fresh 2MB min    63.63  med    72.66  max    79.67 ms (x11), 105 minor faults/run
image: 0 placed symbols differ from oracle/p3_image.bin, 0 relocation errors
3b reloc_f0_pt_v32  warm        min    49.68  med    58.31  max    62.95 ms (x19)
3b reloc_f0_pt_v32  fresh 4KB min    92.68  med   103.61  max   110.32 ms (x11), 19171 minor faults/run
3b reloc_f0_pt_v32  fresh 2MB min    55.99  med    66.67  max    69.88 ms (x17), 94 minor faults/run
P3 OK

codex
1. **Halide P3b’s fresh-output variants are unchecked.** [GRID.md:55](/home/exedev/paradigms-w/GRID.md:55) claims P3 OK, including fresh huge-page output. The [driver:143](/home/exedev/paradigms-w/systems/halide/cells/p3.cpp:143) unmaps those outputs without comparing them; only the reused buffer is checked. I ran `HL_NUM_THREADS=8 systems/halide/cells/build/p3 data/minio 3b`: exit **0**, `P3 OK`. That validates the warm buffers, not the fresh ones.

2. **Futhark lazy reach’s “falls down” verdict survives incorrectly in [ATTACKS.md:104](/home/exedev/paradigms-w/ATTACKS.md:104).** Its **165/90 ms** includes decoding; the eager flood excludes it. [GRID.md:54](/home/exedev/paradigms-w/GRID.md:54) already corrects this: eager costs at least **97 + 47 ms plus an unwritten CSR build** on one thread. This is an unequal-work comparison, not demonstrated paradigm failure. The earlier Soufflé layout verdict has also been withdrawn.

3. **Goose P1 compares recycled pages against C’s first-touch allocation.** [NOTES.md:23](/home/exedev/paradigms-w/systems/goose/NOTES.md:23) explicitly records **213 ms first run versus 79–115 ms later**, with the latter used in GRID while C includes table faults. Futhark P3b has the same mismatch: recycled output versus C’s fresh mapping ([NOTES.md:125](/home/exedev/paradigms-w/systems/futhark/NOTES.md:125)).

4. **Halide resolve’s kube scaling anomaly remains uninvestigated.** [GRID.md:42](/home/exedev/paradigms-w/GRID.md:42) reports **6.7×** one-thread slowdown for roughly **1.4×** the IDs, then says “to recheck quiet.” It is absent from STATUS’s priorities. Chase it with interleaved dataset runs and counts of probing rounds/table occupancy; waiting for quiet contradicts the charter.

5. **No entire ATTACKS entry lacks measurements; entry 3’s generalization does.** “The level count is the cost” remains unsupported for Halide/DuckDB and contradicted by Futhark: **45→15 levels, 1,211→437 parallel operations**, yet performance stayed within noise ([ATTACKS.md:126](/home/exedev/paradigms-w/ATTACKS.md:126)). Measure oracle-equivalent batched or asynchronous variants against their originals, counting passes and synchronization, with five interleaved runs.

6. **No qualifying JOURNAL passage found.** I read all **91 lines**, including the recent tail. None declines or shrinks work because of time, usage, or a session ending.

No files edited.
tokens used
58,648
1. **Halide P3b’s fresh-output variants are unchecked.** [GRID.md:55](/home/exedev/paradigms-w/GRID.md:55) claims P3 OK, including fresh huge-page output. The [driver:143](/home/exedev/paradigms-w/systems/halide/cells/p3.cpp:143) unmaps those outputs without comparing them; only the reused buffer is checked. I ran `HL_NUM_THREADS=8 systems/halide/cells/build/p3 data/minio 3b`: exit **0**, `P3 OK`. That validates the warm buffers, not the fresh ones.

2. **Futhark lazy reach’s “falls down” verdict survives incorrectly in [ATTACKS.md:104](/home/exedev/paradigms-w/ATTACKS.md:104).** Its **165/90 ms** includes decoding; the eager flood excludes it. [GRID.md:54](/home/exedev/paradigms-w/GRID.md:54) already corrects this: eager costs at least **97 + 47 ms plus an unwritten CSR build** on one thread. This is an unequal-work comparison, not demonstrated paradigm failure. The earlier Soufflé layout verdict has also been withdrawn.

3. **Goose P1 compares recycled pages against C’s first-touch allocation.** [NOTES.md:23](/home/exedev/paradigms-w/systems/goose/NOTES.md:23) explicitly records **213 ms first run versus 79–115 ms later**, with the latter used in GRID while C includes table faults. Futhark P3b has the same mismatch: recycled output versus C’s fresh mapping ([NOTES.md:125](/home/exedev/paradigms-w/systems/futhark/NOTES.md:125)).

4. **Halide resolve’s kube scaling anomaly remains uninvestigated.** [GRID.md:42](/home/exedev/paradigms-w/GRID.md:42) reports **6.7×** one-thread slowdown for roughly **1.4×** the IDs, then says “to recheck quiet.” It is absent from STATUS’s priorities. Chase it with interleaved dataset runs and counts of probing rounds/table occupancy; waiting for quiet contradicts the charter.

5. **No entire ATTACKS entry lacks measurements; entry 3’s generalization does.** “The level count is the cost” remains unsupported for Halide/DuckDB and contradicted by Futhark: **45→15 levels, 1,211→437 parallel operations**, yet performance stayed within noise ([ATTACKS.md:126](/home/exedev/paradigms-w/ATTACKS.md:126)). Measure oracle-equivalent batched or asynchronous variants against their originals, counting passes and synchronization, with five interleaved runs.

6. **No qualifying JOURNAL passage found.** I read all **91 lines**, including the recent tail. None declines or shrinks work because of time, usage, or a session ending.

No files edited.
