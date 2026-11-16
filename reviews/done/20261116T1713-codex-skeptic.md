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
session id: 01a1e559-afeb-7722-907a-f8d1e07a2a13
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
I’ll read the repository’s review notes and trace the claims to their checks and measurements without editing files.
exec
/bin/bash -lc "pwd; rg --files -g 'CHARTER.md' -g 'PLAN.md' -g 'STATUS.md' -g 'GRID.md' -g 'ATTACKS.md' -g 'JOURNAL.md' -g 'CLAUDE.md' -g 'AGENTS.md'" in /home/exedev/paradigms-w
 succeeded in 0ms:
/home/exedev/paradigms-w
PLAN.md
CHARTER.md
JOURNAL.md
STATUS.md
GRID.md
ATTACKS.md

exec
/bin/bash -lc 'cat CHARTER.md PLAN.md STATUS.md GRID.md ATTACKS.md; tail -n 180 JOURNAL.md' in /home/exedev/paradigms-w
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

Now (2026-11-16T16:45Z, lead session paradigms-46): M1/M2 in flight. Merged: Goose, Futhark (GRID columns), codex-e/f/g.
**A /clear or session end kills the session's subagents**: still running inside paradigms-46: builders souffle
(.claude/worktrees/agent-a04daa..), halide (agent-a45ed3..), duckdb (agent-a66651..); if the session is gone, relaunch
each with ideas/RESUME.md (prompt = "You are the <sys> cell builder ... worktree <path> ... follow ideas/RESUME.md").
When a builder replies: rebuild its cells from source, rerun every check, merge its branch, fill its GRID column by table
position (the C table has rows with the same names: edit only the rows under "## The paradigms").
Codex: codex-h (paradigms-e, results/codex-h.md) splits p1compact's two changes (dense single pass vs partition-local
candidates) to explain its 1.5x. Then: M1 milestone pass (LOOP.md "At each milestone": oracle number, GRID, JOURNAL
paragraph, skeptic reviewer from a second clone, STATUS, MORNING) once Soufflé/Halide/DuckDB merge.
Open threads: ATTACKS 3 quiet-machine run (p2async vs p2par passive, nothing else running); Futhark's
glibc.malloc.hugetlb=2 tunable tried on the C refs and the other paradigms.
# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T16:23Z. Campaign 1, M0 done, M1/M2 in flight: five builders resumed (Soufflé, Futhark, Halide, Goose, DuckDB); 0/20 cells merged yet.
**2 ATTACKS entries backed by measurement.** 1: reach, decoding relocation columns inside the flood is 2x the eager decode+CSR+flood
(52.6 / 13.6-15.4 vs 108.7 / 30.1 ms; Codex reproduced 2.1-2.6x); the hlink ratio is withdrawn (not equivalent work).
2: relocate on 2 MB output pages, 24 vs 49 ms on 1 thread and **5.4-6.8 ms on 8 = 1.1-1.4x the 4.92 ms floor** (14 on 4 KB pages).
Next lead hypothesis (Codex testing): resolve's join is bound by 4 passes over the dense 83 MB key array, of which 15% is keyed.
VM bandwidth measured: 25 GB/s read on 1 thread, 36 on 8 (FLOOR.md assumes more; floors here are optimistic by ~2x).

## Decisions for Dan
- None. (PLAN.md's campaign text had been truncated by an agent's Now-line edit; restored from git at 16:22Z.)
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
| 3a. lay out | 0.60 | 85 / 23-25 (layout phase, includes pclntab sizing and the typelink sort) | 8-19 | | `build/p3 data/minio` -> P3 OK |
| 3b. relocate | 4.92 (write) | 102-104 / 15 (write phase, includes pclntab and GC bitmaps) | 59-67 (76 MB, 2.05 M relocations); P3_REPS=20 16:13Z: 48.6-56 on 4 KB pages, **23.9-30** on 2 MB pages | p3par 8 threads: 13.4-29 on 4 KB, **5.4-12 (median 6.1-6.8) on 2 MB** (ATTACKS 2) | same; `build/p3 data/minio --pipeline` runs 3a -> 3b from its own addresses -> P3 pipeline OK |
| 4. codegen by shape: emit 136,634 bodies (60.7 MB) from 108,833 byte templates + 7.68 M operands | | fc's tree walker (no separate phase time) | 13-17 (lay out 2.38 M pieces) + 139-147 (emit), load 3.5; lead rerun 16:05Z: 18.4 + 162.4 | | `ref/p4/check.sh` -> P4 OK (`ref/p4/README.md`) |

**Floors vs this VM.** FLOOR.md's floors imply ~87 GB/s (minio resolve: 119 MB in 1.36 ms); this VM streams **25 GB/s read on
1 thread, 36 GB/s read / 27 GB/s write on 8** (16:16Z, load 5, a sum and a fill over 1 GB). Floors here are ~2.4x lower than the
column says. Per-kernel bandwidth bound for flat 3b: 76 MB symbol bytes + ~33 MB relocations read, 76 MB written -> ~3-6 ms on
8 threads, which p3par on 2 MB pages (5.4-6.8 median) meets.

Tuned and plain flat rows re-measured 2026-11-16T15:56Z at load 0.8, 3 interleaved rounds (`ref/flat/bench.sh`); hlink's resolve/deadcode from `COLD=0 ref/hlink/bench.sh minio build/hlink 3` right after.

**kube-apiserver** (second dataset, extracted 2026-11-16T15:55Z, 2.2 GB of flat files, 13.27 M slots): every plain and
tuned C reference reproduces its oracles (`ref/flat/check.sh data/kube-apiserver`, loaded machine): p1 434 + 88 ms,
p1c 550 ms, p2 66.5 ms (926,486 live), p3a 30.4 ms, p3b 67.8 ms (72.6 MB, 2.82 M relocations); p1par 26.2 ms and
p2par 19.6 ms on 8 threads. Its oracles come from hlink alone so far (MinIO's live list was also checked against fclink).

Floors are bottom-up's FLOOR.md model (same CPU model, the 2026-11-13 VM); they count the raw object bytes hlink
reads, so for the flat files they are an upper bound on the floor.

## The paradigms

| problem | Goose | Futhark | Halide | Soufflé | DuckDB (added 15:58Z) |
|---|---|---|---|---|---|
| 1. resolve | **P1 OK** (1a/1b/1c). 1a 79-115 / 37-57; 1b 39-57 / 42-67; 1c 97-118 / 39-47 (1 / 8 workers, load 8-13). Forced: shuffle join, one table per worker; impossible: a shared table. | **P1 OK** (1a/1b/1c). 2 MB pages: 1a 79 / **24**, 1b 47 / 18, 1c 111 / 38 (1 / 8, load 5-13; 4 KB 1.2-2.5x slower). Easy: winner as reduce_by_index max; forced: the hash table as rounds of probing. | **P1 OK** (1a/1b/1c). 1a 81-93 / **19.8-27**; 1b 42.7-46 / 9.9-10.9; 1c 122-133 / 33-36.5 (1 / 8; tuned C same window 12.1-20.9 / 7.5-8.9 / 11.7-14.8). Surprise: 1a beats plain C 2.5-3x on 1 core (compact first). Forced: no hash table, so join = rounds of scatter-min + gather. | **P1 OK** (1a/1b/1c x2). 1a 1928-2034 / 636; 1b 5220-5426 / 1080; 1c 1625-1865 / 394-587 (1 / 8 threads, load 3-6): 30-200x C. Easy: join + max aggregate are the rules; forced: B-tree loading of 5.2 M rows. | **P1 OK** (1a/1b/1c). 8 threads: 1a 103-113, 1b 272-375, 1c 128-190 (10-60x tuned C). Easy: the joins are SQL joins; forced: every lookup by dense id is a hash join (SQL has no a[i]). |
| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood 97 / 58; lazy 165 / 90. Falls down on ATTACKS 1 (two attempts): lazy is slower than its own eager flood; 111 levels x ~25 passes each. | **P2 OK** (eager + lazy). 8 thr: lazy 79-90, eager 70-127 (tuned C 25.7-29.1 / 12.7-14.6). Lazy is 1.3-1.5x *slower* than eager in Halide. Forced: no fixpoint (one pipeline call per level, ~100 levels), no worklist; `atomic()` dropped (mutex per element, wrong live sets). | **P2 OK** (eager + lazy). 1227-1467 / 431; lazy 960 on 8 (decodes 1.73 M of 9.4 M relocations, 3 rules), 2x its eager, ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: **falls down** on speed (2 attempts; 45 rounds x ~25 statements at 1-10 ms each). Lazy ties eager. A single recursive query: 425 ms for plain edges. |
| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down: one image array on 8 workers (gather +142-171). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK**. 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a 7222-7589 on 1 thread, 1339 on 8: **falls down** (no sort in Datalog; typelink order by pair counting, 2 attempts). 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
| 4. codegen by shape (stretch) | not attempted | not attempted | not attempted | not attempted | not attempted |

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

**What it says about the paradigms.** None of the grid's data-parallel systems can say "a fixpoint without levels":
Futhark and Halide have no worklist; Soufflé's semi-naive evaluation is level-synchronous by construction; DuckDB's
recursive CTE iterates rounds; Goose has the queues but no shared visited set. The C worklist is ~200 lines of
mechanism (stealing, credits) for ~15 lines of rules. A build farm's machines are shared like this VM, so robustness
under load is part of the speed. **Next:** a quiet-machine run (does async lose anything with nothing else running?),
and whether a level-free flood also cuts Futhark's/Goose's cost (fewer, larger rounds: e.g. expand several levels per
superstep, a "delta-stepping" of the flood).
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

exec
/bin/bash -lc 'cat GRID.md; cat systems/{futhark,halide,souffle,duckdb,goose}/NOTES.md' in /home/exedev/paradigms-w
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
| 3a. lay out | 0.60 | 85 / 23-25 (layout phase, includes pclntab sizing and the typelink sort) | 8-19 | | `build/p3 data/minio` -> P3 OK |
| 3b. relocate | 4.92 (write) | 102-104 / 15 (write phase, includes pclntab and GC bitmaps) | 59-67 (76 MB, 2.05 M relocations); P3_REPS=20 16:13Z: 48.6-56 on 4 KB pages, **23.9-30** on 2 MB pages | p3par 8 threads: 13.4-29 on 4 KB, **5.4-12 (median 6.1-6.8) on 2 MB** (ATTACKS 2) | same; `build/p3 data/minio --pipeline` runs 3a -> 3b from its own addresses -> P3 pipeline OK |
| 4. codegen by shape: emit 136,634 bodies (60.7 MB) from 108,833 byte templates + 7.68 M operands | | fc's tree walker (no separate phase time) | 13-17 (lay out 2.38 M pieces) + 139-147 (emit), load 3.5; lead rerun 16:05Z: 18.4 + 162.4 | | `ref/p4/check.sh` -> P4 OK (`ref/p4/README.md`) |

**Floors vs this VM.** FLOOR.md's floors imply ~87 GB/s (minio resolve: 119 MB in 1.36 ms); this VM streams **25 GB/s read on
1 thread, 36 GB/s read / 27 GB/s write on 8** (16:16Z, load 5, a sum and a fill over 1 GB). Floors here are ~2.4x lower than the
column says. Per-kernel bandwidth bound for flat 3b: 76 MB symbol bytes + ~33 MB relocations read, 76 MB written -> ~3-6 ms on
8 threads, which p3par on 2 MB pages (5.4-6.8 median) meets.

Tuned and plain flat rows re-measured 2026-11-16T15:56Z at load 0.8, 3 interleaved rounds (`ref/flat/bench.sh`); hlink's resolve/deadcode from `COLD=0 ref/hlink/bench.sh minio build/hlink 3` right after.

**kube-apiserver** (second dataset, extracted 2026-11-16T15:55Z, 2.2 GB of flat files, 13.27 M slots): every plain and
tuned C reference reproduces its oracles (`ref/flat/check.sh data/kube-apiserver`, loaded machine): p1 434 + 88 ms,
p1c 550 ms, p2 66.5 ms (926,486 live), p3a 30.4 ms, p3b 67.8 ms (72.6 MB, 2.82 M relocations); p1par 26.2 ms and
p2par 19.6 ms on 8 threads. Its oracles come from hlink alone so far (MinIO's live list was also checked against fclink).

Floors are bottom-up's FLOOR.md model (same CPU model, the 2026-11-13 VM); they count the raw object bytes hlink
reads, so for the flat files they are an upper bound on the floor.

## The paradigms

| problem | Goose | Futhark | Halide | Soufflé | DuckDB (added 15:58Z) |
|---|---|---|---|---|---|
| 1. resolve | **P1 OK** (1a/1b/1c). 1a 79-115 / 37-57; 1b 39-57 / 42-67; 1c 97-118 / 39-47 (1 / 8 workers, load 8-13). Forced: shuffle join, one table per worker; impossible: a shared table. | **P1 OK** (1a/1b/1c). 2 MB pages: 1a 79 / **24**, 1b 47 / 18, 1c 111 / 38 (1 / 8, load 5-13; 4 KB 1.2-2.5x slower). Easy: winner as reduce_by_index max; forced: the hash table as rounds of probing. | **P1 OK** (1a/1b/1c). 1a 81-93 / **19.8-27**; 1b 42.7-46 / 9.9-10.9; 1c 122-133 / 33-36.5 (1 / 8; tuned C same window 12.1-20.9 / 7.5-8.9 / 11.7-14.8). Surprise: 1a beats plain C 2.5-3x on 1 core (compact first). Forced: no hash table, so join = rounds of scatter-min + gather. | **P1 OK** (1a/1b/1c x2). 1a 1928-2034 / 636; 1b 5220-5426 / 1080; 1c 1625-1865 / 394-587 (1 / 8 threads, load 3-6): 30-200x C. Easy: join + max aggregate are the rules; forced: B-tree loading of 5.2 M rows. | **P1 OK** (1a/1b/1c). 8 threads: 1a 103-113, 1b 272-375, 1c 128-190 (10-60x tuned C). Easy: the joins are SQL joins; forced: every lookup by dense id is a hash join (SQL has no a[i]). |
| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood 97 / 58; lazy 165 / 90. Falls down on ATTACKS 1 (two attempts): lazy is slower than its own eager flood; 111 levels x ~25 passes each. | **P2 OK** (eager + lazy). 8 thr: lazy 79-90, eager 70-127 (tuned C 25.7-29.1 / 12.7-14.6). Lazy is 1.3-1.5x *slower* than eager in Halide. Forced: no fixpoint (one pipeline call per level, ~100 levels), no worklist; `atomic()` dropped (mutex per element, wrong live sets). | **P2 OK** (eager + lazy). 1227-1467 / 431; lazy 960 on 8 (decodes 1.73 M of 9.4 M relocations, 3 rules), 2x its eager, ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: **falls down** on speed (2 attempts; 45 rounds x ~25 statements at 1-10 ms each). Lazy ties eager. A single recursive query: 425 ms for plain edges. |
| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down: one image array on 8 workers (gather +142-171). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. | **P3 OK**. 3a 20-23 on 8 (C 8.5-9 on 1 core). 3b 24.9-27 reused output, 27-30.8 fresh on 2 MB (faults 19,187 -> 170); the byte-at-a-time copy is the cost, 4-5x C. | **P3 OK**. 3a 7222-7589 on 1 thread, 1339 on 8: **falls down** (no sort in Datalog; typelink order by pair counting, 2 attempts). 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
| 4. codegen by shape (stretch) | not attempted | not attempted | not attempted | not attempted | not attempted |

Halide column merged 17:17Z (lead rebuilt every generator and driver in a copy; P1/P2/P3 OK; `systems/halide/CELLS.md`).
DuckDB column merged 17:13Z (lead reran every check in a copy, pipeline included; `systems/duckdb/CELLS.md`).
Soufflé column merged 16:58Z (lead rebuilt all 8 programs from source; every check OK; `systems/souffle/CELLS.md`).
Futhark column merged 16:42Z (lead rebuilt from source; every cell OK; `systems/futhark/CELLS.md`). Surprise: `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, no code change, makes most Futhark cells 1.2-1.7x faster.
Goose column merged 16:35Z; lead reran every check (P1/P2/P3/pipeline OK, a source rebuild of p2par passes). Long form and lines: `systems/goose/CELLS.md`, `NOTES.md`.
# Futhark: problems 1-3 on MinIO (cells), then the install notes

Futhark 0.29.1, `c` backend (sequential) for 1 core, `multicore` backend for 8. All compute is in `p1.fut`,
`p2.fut`, `p3.fut` (+ `msort.fut`); the C drivers (`p1.c p2.c p3.c`, shared `drv.h`) only mmap the flat files,
hand them to the generated library (`futhark_new_*`, which copies), call the entries, time them, and compare
the outputs with the oracles. Build: `systems/futhark/build.sh` (`YIELD=1` also builds the non-spinning runtime,
`build/*_mcy.exe`). Rerun every timing: `systems/futhark/bench.sh` (`HUGE=1` for 2 MB pages).

## Surprises

1. **2 MB pages for the whole runtime are a 1.2-1.7x win on every cell, with no code change.**
   `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` makes glibc back its large mallocs with MAP_HUGETLB, and the Futhark
   runtime gets every array from malloc. Intermediate arrays are allocated per entry call and part of that
   memory goes back to the kernel between calls, so with 4 KB pages each warm rep pays 16,589 (1a) to 33,183 (1c) minor faults;
   with huge pages 18-67. Interleaved, 3 rounds, 1 core, 16:33Z, load 5.3-6.3 (min-max of the 3 rounds' medians):

   | cell | 4 KB pages, ms (faults/rep) | 2 MB pages, ms (faults/rep) |
   |---|---|---|
   | 1a | 143-158 (16,589) | 89-111 (38) |
   | 1b_scan | 74-94 (19,285) | 47-48 (37) |
   | 1c | 197-225 (33,183) | 115-133 (67) |
   | 2 | 122-157 (9,453) | 103-119 (24) |
   | 2lazy | 196-224 (3,785) | 167-200 (18) |

   End to end it matters more (the uploads of the inputs fault too): 3b process 0.59-0.61 s -> 0.38-0.42 s on 1
   core, 0.41-0.44 -> 0.17-0.22 s on 8 (104,142 -> 2,456 faults per process). This is ATTACKS 2's observation
   made general: for a runtime that allocates per operation, the page size is a first-order knob.
2. **The stock multicore runtime spins, and on this shared machine that costs more than the parallelism wins.**
   The join loop of every parallel operation and the scans' decoupled look-back busy-wait; Futhark has no
   wait-policy knob. `YIELD=1 build.sh` patches the generated C (sed, 7-26 `sched_yield()` per program) so a
   waiting thread yields. Interleaved P2 at 8 threads, load 10-12 (16:21Z): eager flood medians 156-310 ms
   spinning vs 120-297 yielding; lazy 235-459 vs 106-181. Under load 13 (bench at 16:30Z) neither helps:
   8-thread P2 is slower than 1 core. At 111 levels x ~25 parallel operations per level, the flood is ~2,800
   barriers, and each one waits for the slowest descheduled thread. Read all 8-thread numbers below as
   upper bounds measured on an oversubscribed machine.
3. **The binary search a data-parallel program reaches for first is the slow part.** "Which row owns item j" as
   a binary search per item (1b, 3b) vs a scatter of row starts + a max-scan: 1b 455-471 -> 47-103 ms on 1 core;
   3b 177-183 -> 111-120 ms (interleaved, 16:31Z, load 10). 9.4 M searches of 23 steps over a 20 MB offset
   array are cache misses; the scan is two streaming passes.

## Correctness (all OK, 2026-11-16, MinIO)

    cd systems/futhark && ./build.sh
    D=/home/exedev/paradigms/data/minio
    build/p1_c.exe $D 1 1 1a; build/p1_c.exe $D 1 1 1b_scan; build/p1_c.exe $D 1 1 1c    # -> P1a OK, P1b_scan OK, P1c OK
    build/p2_c.exe $D 1 1 2; build/p2_c.exe $D 1 1 2lazy                                  # -> P2 OK, P2lazy OK
    build/p3_c.exe $D 1 1 3a; build/p3_c.exe $D 1 1 3b                                    # -> P3a OK, P3b OK

(args: DATADIR THREADS REPS CELL; the same with `_multicore.exe $D 8 ...`.) Each driver compares the output
arrays element by element with `oracle/p1_canon.u32`, `p1_builtin.u32`, `p1_reltarget.u32`, `p1c_canon.u32`,
`p2_live.u32`, `p3_addr.u32`, and every placed symbol's bytes with `p3_image.bin`; 3b also counts relocation
errors (0). Variants 1b (binary search), 2lazy_look, 3a_loop, 3b_bytes also pass. As in the C references, 1b
and 1c take 1a's canon from its oracle, 3b takes 3a's addresses and `p3_addr_all`, 2lazy takes 1a's canon; each
cell stands alone.

## Timings

`bench.sh`, 2026-11-16T16:26-16:30Z (load 5.5 rising to 13.2) and `HUGE=1 bench.sh` 16:32-16:33Z (load 4.9-7.0),
MinIO. warm = entry only, inputs resident, repeated in one process (>= ~1 s); e2e = whole process, 5 runs.
min / median ms. 1 = `c` backend; 8 = `multicore`, stock (spinning) runtime unless marked yield. The 8-thread
rows were taken at load 5-13 on 8 cores: they measure the machine as much as Futhark (Surprise 2).

| cell | 1 core warm | 1 core e2e | 8 thr warm | 8 thr e2e | 1 core warm, 2 MB | 8 thr warm, 2 MB | 8 thr e2e, 2 MB | plain C 1 core / tuned C 8 |
|---|---|---|---|---|---|---|---|---|
| 1a join by name | 152 / 178 | 275 / 308 | 60 / 84 (yield 61 / 77) | 208 / 248 | 79 / 88 | 24 / 28 | 68 / 70 | 195 / 8.6-10.5 |
| 1b targets (scan) | 76 / 103 | 215 / 226 | 59 / 71 | 211 / 235 | 47 / 49 | 18 / 24 | 64 / 71 | 44 / 5.3-6.3 |
| 1b targets (binary search) | 459 / 471 | 628 / 645 | 86 / 96 | 267 / 300 | 455 / 465 | 70 / 83 | 136 / 142 | |
| 1c content join | 202 / 210 | 332 / 349 | 127 / 165 (yield 107 / 144) | 267 / 268 | 111 / 115 | 38 / 46 | 83 / 87 | 57-61 / 7.2-8.9 |
| 2 reach (p2_edge CSR) | 140 / 188 | 303 / 349 | 155 / 259 | 347 / 519 | 97 / 105 | 58 / 102 | 135 / 142 | 39-43 / 13-17 |
| 2lazy (decode in flood) | 303 / 346 | 996 / 1343 * | 241 / 412 | 1171 / 1499 * | 165 / 173 | 90 / 137 | 362 / 440 * | p2lazy 52.6 / 13.6-15.4 |
| 3a layout (scan) | 22 / 25 | 117 / 137 | 10 / 31 | 98 / 128 | 22 / 22 | 12 / 30 | 25 / 37 | 8-19 / - |
| 3a layout (loop) | 14 / 21 | 40 / 40 | 8.5 / 12 (yield 7.3 / 10.5) | 36 / 50 | 13 / 14 | 14 / 17 | 49 / 56 | |
| 3b relocate | 108 / 119 | 342 / 368 | 40 / 48 | 286 / 332 | 107 / 109 | 41 / 50 | 114 / 119 | 48.6-56 (4 KB) 23.9 (2 MB) / 13.4-14 (4 KB) 5.4-6.8 (2 MB) |

\* 2lazy's e2e includes its untimed-in-warm setup (aux_edges: the funcdata and kind 1-3 rows, 260-530 ms), as
ref/flat/p2lazy.c also excludes from its flood time.

Best 8-core warm (min, 2 MB pages): 1a 24, 1b 18, 1c 38, 2 58, 2lazy 90, 3a 7.3-12, 3b 41 ms.

## ATTACKS 1 (reach counting its own decode)

- **Can Futhark express "decode a reference when the flood reaches it"? Yes.** `reach_lazy`'s `edges` function
  expands each frontier source's relocation range (`expand` over `slot_rel`) and maps the decode
  (package-base table, then canon) over the expanded relocations, inside the level loop; the graph is never
  materialized. The R_USETYPE skip rule (type 26 skips the next two) becomes a segmented scan of a 3-state
  machine's transition functions (function composition is associative), or a bounded look-back
  (`2lazy_look`, the second attempt: not faster, 108-130 vs 96-118 ms at 8 threads, 16:21Z).
- **Does it beat the eager C pipeline (108.7 ms 1 thread / 30.1 ms 8)? No.** 1 core: lazy 165-200 ms (2 MB
  pages) / 196-224 (4 KB). The eager Futhark pipeline would be its own 1b (47-48 ms) + a CSR build (not written)
  + the flood over p2_edge (97-119 ms), > 144 ms: also above 108.7. 8 threads: lazy 90-137 ms median, eager flood
  58-102, at load 5-7.
- **Why.** Futhark's flood is level-synchronous: 111 levels, each ~25 data-parallel operations (filters,
  expands = scan + scatter + segmented iota, scatters, the dedup), each a full pass with its own allocation. At
  1 core the eager flood costs ~0.9 ms per level against C's 0.4; the lazy flood adds an expand and a
  segmented scan per level, so in Futhark lazy *loses* to eager (165 vs 97 ms) where in C it wins. What makes
  the lazy shape cheap in C (a sequential walk of a row, decode and mark in the same loop) is a nested,
  irregular loop with writes to shared state, which Futhark must flatten into separate passes.
- First tuning pass (before these numbers): the method rule checked every one of the 106 K methods every level
  (`filter call (iota M)`, the largest item in the profile); now a pending list holds the candidate methods
  that have not fired.

## ATTACKS 2 (relocate on huge pages)

- **Can the runtime put 3b's output on 2 MB pages? Yes, without touching the code**:
  `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` (with `vm.nr_hugepages=512` reserved here). Futhark exposes no
  allocator hook; the C API offers no way to hand it a buffer (`futhark_new_*` copies).
- **Minor faults of 3b**: per warm rep 4,623-6,833 (4 KB) vs 18-80 (2 MB); per process (load + 1 run) 104,142
  (1 core) / 168,488 (8 threads) vs 2,456 / 2,798. The C reference faults 19,109 per rep (a fresh 109 MB image
  each time); Futhark's warm reps fault less because the image's memory is recycled (even with `FUT_FRESH=1`,
  which empties the runtime's free list, so glibc keeps it), so the warm number does not pay the image's first
  touch; the first rep of each process does.
- **Times**, 5 interleaved process runs each, 3 reps per process, 16:31Z, load 5.8-6.1:
  first rep (fresh image) 1 core 181-190 ms (4 KB) vs 119-150 (2 MB); 8 threads 126-137 vs 52-64; later reps
  1 core 112-126 vs 107-116 (within noise), 8 threads 44-67 vs 29-55.
  3b itself is 2-4.5x the C reference (C 1 core 49 / 24 ms on 4 KB / 2 MB pages): the copy and the patch are
  scatters of (index, byte) pairs (one per byte of data, 8 per relocation), which is how a scatter must be
  shaped in Futhark: there is no memcpy of a run into a slice of a shared array from inside a map.

## Per cell: easy / forced / impossible

- **1a** (49 lines incl. the hash table). Easy: the winner rule as `reduce_by_index u64.max` over a packed
  `rank << 32 | ~id`. Forced: the hash table itself (no hash maps in the language; built by rounds of linear
  probing where `reduce_by_index i64.max` claims empty slots, no atomics). Impossible: nothing.
- **1b** (18 lines for the scan version). Easy: the decode is a pure map. Forced: each relocation's owner slot
  must be computed (scatter + max-scan); the first shape, a binary search per relocation, was 6-10x slower.
- **1c** (14 lines, reusing 1a's table). Easy, same shape as 1a with a different packed key.
- **2** (74 lines eager, +63 lazy). Easy: each rule is a scatter of `true` into a flag array (all monotone, so
  races are benign). Forced: the frontier as a getter function (a function argument cannot be size-polymorphic),
  the expand of rows each level, dedup by scatter-the-index-and-check; the R_USETYPE skip rule as a segmented scan
  of state-transition functions. Impossible: a cheap lazy decode (see ATTACKS 1).
- **3a** (86 lines + 26 msort). Easy: layout as a scan of "align then add" maps, which compose (adv_then; the
  scan is exact against the oracle). Forced: the typelink sort (the library's merge sort is GPU-shaped, 13 ms for
  33 K keys; a CPU-shaped bottom-up merge sort is 2 ms). The sequential cursor loop (`3a_loop`, in-place
  updates under uniqueness types) is as fast as the data-parallel scan here: 360 K symbols is too small.
- **3b** (60 lines). Easy: patch values as a `match` over kinds. Forced: copying bytes as a scatter of
  (index, byte) pairs in 64-byte chunks (per-byte binary search, `3b_bytes`, 480 ms); no bulk copy. Impossible:
  choosing the output's page size from the language (done from outside, via glibc).

## Core code (excerpts)

The layout walk as a scan (p3.fut): an advance is `x -> rup(x + c, a) + d`; two compose into one.

```futhark
def adv_then ((c1, a1, d1): adv) ((c2, a2, d2): adv) : adv =
  let e = d1 + c2
  in if a2 <= a1 then (c1, a1, rup e a2 + d2) else (c1 + rup e a1, a2, d2)
...
  let ends = segmented_scan adv_then adv_id (map (\i -> i % (nunits * nc) == 0) (iota ncells)) steps
```

The lazy reach's edges of a frontier (p2.fut): expand the rows, decode where followed.

```futhark
    let rs = expand (\c -> if decodes c then i64.u32 (slot_rel[c + 1] - slot_rel[c]) else 0)
                    (\c j -> (c, i64.u32 slot_rel[c] + j)) proc
    let fol = followed (map (\(c, r) -> r == i64.u32 slot_rel[c]) rs) (map (\(_, r) -> rel_type[r]) rs)
    let rt = map2 (\(c, r) f -> if f then decode canon builtin pkg_off pkg (i64.u32 slot_obj[c]) rel_pkg[r] rel_idx[r] else 0) rs fol
```

The 1a winner (p1.fut):

```futhark
  let value g = u64.u32 rank[g] << 32 | u64.u32 (!(u32.i64 g))
  let win = reduce_by_index (replicate m 0u64) u64.max 0 (drop W slot :> [nn]i64) (map value named)
```

Lines (non-blank, non-comment): p1.fut 103, p2.fut 137, p3.fut 168, msort.fut 26; drivers p1.c 65, p2.c 57,
p3.c 87, drv.h 67 (loading, timing, checking only).

## Tools

`FUT_PROFILE=1 build/p2_multicore.exe ... 2> prof.json; python3 prof.py prof.json` sums the runtime's
per-operation report by source location. `FUT_TUNING=file` sets tuning parameters (`name=value` lines).
`FUT_FRESH=1` (p3) empties the runtime's free list before each rep.

# Install notes (Futhark on this VM)

## Install (done 2026-11-16)

```
mkdir -p /home/exedev/tools/dl && cd /home/exedev/tools/dl
curl -sLO https://github.com/diku-dk/futhark/releases/download/v0.29.1/futhark-0.29.1-linux-x86_64.tar.xz
mkdir -p ../futhark && tar xf futhark-0.29.1-linux-x86_64.tar.xz -C ../futhark --strip-components=1
ln -sf /home/exedev/tools/futhark/bin/futhark ~/.local/bin/futhark     # ~/.local/bin is on PATH
```

- Version: `Futhark 0.29.1` (git 06a06af), prebuilt static binary, no Haskell toolchain needed.
- Backends used: `c` (sequential) and `multicore` (pthreads), both just need gcc (gcc present at /usr/bin/gcc).
  No GPU here, so cuda/opencl/hip are irrelevant.
- Also on disk: `/home/exedev/tools/fb/futhark-benchmarks-master` (futhark-benchmarks tarball, includes
  `lib/github.com/diku-dk/{segmented,sorts,containers,...}`), scratch benches in `/home/exedev/tools/fut-scratch`.

## Hello world: `hello/`

`hello.fut`: entry `hello (xs: [n]u32) : [3]u64` = inclusive prefix sum (scan, u64) + 65,536-bin histogram of
the low 16 bits (`reduce_by_index`) + checksums.  `driver.c` mmaps a raw little-endian u32 file
(MAP_POPULATE), hands the pointer to the generated library, runs 3 reps, prints a checksum.
`build.sh` builds both backends; `run.sh [N]` generates N random u32 (xorshift64*) and times.

How to call from C with raw arrays (the path for MinIO's columns):

```
futhark multicore --library -o hello hello.fut      # -> hello.c hello.h (+ hello.json manifest)
gcc -O3 -march=native -DMULTICORE -I. driver.c hello.c -lm -lpthread
```
```c
struct futhark_context_config *cfg = futhark_context_config_new();
futhark_context_config_set_num_threads(cfg, 8);                      // multicore only
struct futhark_context *ctx = futhark_context_new(cfg);
struct futhark_u32_1d *a = futhark_new_u32_1d(ctx, mmap_ptr, n);     // COPIES the data
struct futhark_u64_1d *out;
futhark_entry_hello(ctx, &out, a); futhark_context_sync(ctx);
futhark_values_u64_1d(ctx, out, host_buf);                           // copy out (also async: sync after)
```

## Timings (50 M random u32 = 200 MB; 8 cores; `date -u` = Mon Nov 16 15:22:41 UTC 2026)

Entry time only (input already resident), ms, 3 reps; upload = the copy in `futhark_new_u32_1d`.

| backend | threads | upload copy | run 0 | run 1 | run 2 |
|---|---|---|---|---|---|
| c | 1 (sequential) | 118.7 | 229.3 | 237.0 | 218.5 |
| multicore | 1 | 118.4 | 539.6 | 557.0 | 573.5 |
| multicore | 8 | 120.5 | 180.0 | 159.0 | 186.2 |

All three agree: sum=107369272683654617 ck_scan=3657172974595629368 ck_hist=1638382427737.

Reading: 8 threads give only 1.3-1.4x over the sequential C backend (and 3.3x over multicore at 1 thread).
Hardware floor for this work is ~200 MB read at ~90 GB/s = a few ms per pass, so Futhark is >30x off
the memory floor even in the best case. The multicore 1-thread penalty (2.4x vs `c`) is the cost of the
chunked scan (two passes) plus scheduler. Fused map/scan/reduce is not the problem; scan and
reduce_by_index with 65,536 bins (per-thread subhistograms, then merge) dominate (not profiled further).

Baselines from the library code (20 M elements, multicore, entry time only, 2 reps, same day):

| program | 1 thread | 8 threads |
|---|---|---|
| `radix_sort_int 32 u32.get_bit` (diku-dk/sorts), 20 M u32 | 1.95 s, 1.86 s | 0.71 s, 0.68 s |
| `containers/array` hash `dedup`, 20 M i32 (~4 M distinct) | 1.79 s, 1.74 s | 0.81 s, 1.12 s |

That is ~30 M keys/s sorted, ~20 M/s hash-inserted: two orders of magnitude below the 0.74 G random loads/s
the machine does. Sort-based and hash-based joins in stock Futhark libraries are not competitive; any Futhark
entry in the grid should be written with scan/scatter/reduce_by_index over flat columns, not these libs.

## Gotchas

- Tuple results of an entry become an opaque struct (`futhark_opaque_tup3_...`) in 0.29; the simple route is
  to return one array (`[3]u64`) or a record and use the generated projections.
- `futhark_new_u32_1d` copies (no zero-copy wrap of a host pointer on c/multicore); budget the copy
  (~120 ms for 200 MB here, ~1.7 GB/s; cause not investigated).
  Do inputs once, keep arrays resident, call entries repeatedly.
- Indices are `i64`; bounds checks are on by default (`--unsafe` turns them off)
  and may matter in gathers; untested here.
- Library mode needs `-lm -lpthread`; multicore `-DMULTICORE` is only our driver's guard around set_num_threads.
- `futhark bench`/`-t` report microseconds of entry time excluding input parsing; our driver measures wall clock
  around entry + sync.
- Threads default = number of cores; override with config API or `--num-threads` in the generated executable.
- `reduce_by_index` on multicore with a large bin count (65,536) is the likely cost; with up to ~4 M bins
  (symbol slots 5.2 M) the per-thread subhistogram strategy (O(H * T) memory) would hurt; test before using.
- `/tmp`-style reminder: `build/` holds the 200 MB data file and is gitignored.
# Halide: the grid cells (problems 1, 2, 3)

Halide 21.0.0, C++ AOT generators (install notes at the end). Branch `worktree-agent-a45ed3bb4a5a5b2a6`.
Code: `systems/halide/cells/`: `gen_p1.cpp gen_p2.cpp gen_p3.cpp` (the Halide algorithms and schedules),
`join.h` (a hash join in Halide, used by 1a, 1c and P2), `p1.cpp p2.cpp p3.cpp` (drivers: mmap the flat files,
wrap them zero-copy as `Halide::Runtime::Buffer`, call the AOT pipelines, check against the oracles), `bench.h`
(timing), `build.sh` (one `.a` per schedule variant). Timings: `systems/halide/bench.sh` (one line per cell,
threads, warm/e2e).

Build and check (each driver checks every variant it runs against the oracles and ends with `P<n> OK`):

    systems/halide/cells/build.sh                      # ~1 min: generators, every variant, drivers
    B=systems/halide/cells/build; D=/home/exedev/paradigms/data/minio
    HL_NUM_THREADS=8 $B/p1 $D e2e 1                    # P1 OK: canon, builtin, reltarget, canon after 1c
    HL_NUM_THREADS=8 $B/p2 $D e2e 1                    # P2 OK: live set (lazy decode, parallel)
    HL_NUM_THREADS=8 $B/p3 $D e2e 0 2                  # P3 OK: 3a addresses, 3b image
    systems/halide/bench.sh                            # all timings

Threads are Halide's own pool (`HL_NUM_THREADS`). Its idle workers spin for a short while, then sleep on a
condition variable; there is no wait-policy knob, so the 8-thread numbers are with Halide's default (the RESUME.md
load warning: no 100x collapse was seen, see the ranges).


## Surprises

- **1a (join by name) in Halide beats the plain C reference 2.5-3x on one core** (81-93 vs 231-249 ms, same window,
  17:00-17:10Z, load 2-5), with no hash table: the join is rounds of scatter-min + gather (`join.h`). The plain C
  probes a 4-entry-per-slot table (125 MB) for 5.2 M slots; the Halide version first compacts the 789 K keyed slots
  (a scan) and hashes only those, into tables of 2^20 down to 2^13 buckets. It does not beat the tuned C (p1par
  1 thread 74-77 for 1a+1b; Halide 1a+1b 124-139), so the win is the shape (compact, then small tables), not
  Halide.
- **A lazy flood is slower than an eager one in Halide** (1.3-1.5x on 1 thread, 417-465 vs 312-332 ms), the
  opposite of C (p2lazy 52.6 vs p2pipe 108.7). Halide's cost is per op (every op is a full iteration of a select
  chain with three phases), and the lazy rows walk every relocation of a live symbol, followed or not, while the
  eager CSR holds only the followed ones; the decode itself (six dependent loads) is the smaller part.
- **atomic() on a scatter that reads other sites is a mutex per element**: Halide allocates a mutex array the size
  of the Func on every call (`halide_mutex_array_create`), which made the parallel flood 3.4-3.6 s (vs 0.3 s
  serial), and the first byte layout was still WRONG in parallel (2-2,106 live symbols lost, from reading uii
  twice in one iteration). Both are fixed by a layout where every write is a constant store (below).

## Results (MinIO; ms; per-iteration min / median / max over 5 runs of >= 1 s, in-process, inputs mmapped and
warm; bench.sh 2026-11-16T17:00-17:06Z, load 1.8-7.3; 3b at 8 threads re-measured 17:09Z after the chunked
schedule; C references interleaved 17:10Z, load 4.1-4.9, OMP_WAIT_POLICY=passive)

| cell | Halide 1 thread | Halide 8 threads | plain C 1 core (same window) | tuned C 8 threads |
|---|---|---|---|---|
| counts (sizes for 1a/1c) | 9.9-10.4 | 1.6-2.6 | | |
| 1a join by name | 81-93 | **19.8-27.2** | 231-249 | 12.1-20.9 (p1par join) |
| 1b relocation targets (9.4 M) | 42.7-46.3 | **9.9-10.9** | 47-59 | 7.5-8.9 |
| 1c content-hash join | 122-133 | **33.2-36.5** | 63-78 | 11.7-14.8 |
| 2 reach, lazy decode in the flood | 417-465 | **79-90** | p2lazy 1 thr 59-77 | p2lazy 25.7-29.1 |
| 2 reach, eager (p2_edge as given) | 312-332 | **70-127** (med 90) | p2 39-58 | p2par 12.7-14.6 |
| 3a lay out | 53-58 (c3_w0) | **20.0-23.0** (c3_w1) | 8.5-9.0 | (none) |
| 3b relocate, warm output | 78-82 (f0_pf_v0) | **24.9-27.0** (f0_pt_v32) | | |
| 3b, fresh output, 4 KB pages | 113-119; 19,109 faults | 42.7-45.4; 19,187 faults | 55-61 (calloc) | 23.4-25.7 (p3par, calloc) |
| 3b, fresh output, 2 MB pages | 84.6-88.1; 39 faults | **27.0-30.8**; 170 faults | ATTACKS 2: 23.9-30 | ATTACKS 2: 5.4-12 |

End to end (whole process, `/usr/bin/time`, 5 runs, s): P1 (counts+1a+1b+1c) 0.35-0.40 (1 thr) / 0.13-0.17 (8);
P2 lazy 0.55-0.59 / 0.24-0.25 and eager 0.47-0.51 / 0.22-0.26 (both include ~115 ms of untimed C setup that
splits the non-relocation edges off p2_edge, as p2lazy's setup does); P3 (3a+3b, calloc'd output) 0.21-0.25 /
0.10-0.12 (in-process: load 6-20, 3a 22-31, 3b 36-47 ms at 8 threads).

Thread pool: Halide's (`HL_NUM_THREADS`); no wait-policy knob. No 8-thread time came out above its 1-thread time
in these runs, so the spinning-barrier collapse in RESUME.md's load warning was not seen; the 8-thread eager P2
range (70-127) is the widest and is probably load.

## ATTACKS 1 (reach): can Halide decode a reference when the flood reaches it?

Yes, it can express it, and it costs more than it saves. The flood level (`Flood`, `lazy=true`) walks a live
symbol's own `rel_*` range and maps (package, index) -> slot -> canon inside the op loop, exactly p2lazy's shape;
nothing is materialized. Counting its own decode it does **not** beat the eager C pipeline: 79-90 ms on 8 threads
vs p2pipe's 30.1 (and p2lazy's 13.6-29), 417-465 on 1 vs 108.7. What Halide forces instead of the graph:
- the level loop is C++ (a pipeline is a DAG; it cannot run to a fixpoint): 93-111 calls, one per level;
- a worklist is impossible: the frontier is implicit (a byte per fact; "needs expanding" = live and not expanded,
  or an interface type not expanded as one), visited through a list of 16-id blocks that each level writes;
- an update writes one site per iteration, so each op is three iterations (the target's block, its IFACE, its
  LIVE), and the pieces a symbol's ops share must be a Func computed per symbol (`info`), indexed by the loop's
  own variables (indexed by the id, bounds inference sizes it for every block the id could be in);
- a `where` becomes a loop bound only on the innermost RVar that has one; anywhere else it is an `if` re-tested
  every iteration and never hoisted when it reads the state: the first version iterated every symbol's whole row
  every level (2.7 s).
The eager variant over `p2_edge` (no decode) is faster in Halide (312-332 / 70-127) because the lazy rows are
longer (every relocation, followed or not). Halide's eager P2 counting its own 1b decode would add 10 ms (1b) and
a CSR build Halide would do as a histogram + scan + scatter; not built, since the eager flood alone is already
above the eager C pipeline's 30.1.

## ATTACKS 2 (relocate): huge pages

Yes: a Halide output buffer is whatever memory the caller wraps, so `p3.cpp` maps the 3b image MAP_HUGETLB (2 MB)
and hands it to the pipeline. Fresh output per run, faults inside the timed region (`p3 DATA 3b`, getrusage):
- 1 thread: 4 KB pages 113-119 ms with **19,109 minor faults/run**; 2 MB pages 84.6-88.1 ms with **39**.
- 8 threads: 4 KB 42.7-45.4 ms, 19,187 faults; 2 MB **27.0-30.8 ms, 170 faults**; warm (reused, prefaulted) 24.9-27.0.
So huge pages remove the 15-30 ms of faulting, as in C; what remains is Halide's copy + patch (25 ms at 8 threads
vs C's 5.4-6.8 on 2 MB pages): the byte-at-a-time stores (below) are now the cost, 4-5x C.

## Per cell

### 1a join by name (`Resolve` in gen_p1.cpp, `join.h`; 59 + 30 (compaction) + 60 (join) + 23 (counts) lines)
- Easy: the winner per group is an associative update (`win(rep) = max(win(rep), rank << 32 | ~id)`); `atomic()`
  parallelizes it, `rfactor` the counts. Canon and builtins are pure gathers.
- Forced: no hash table (a probe is a data-dependent loop, an update writes one site): the join is rounds of
  scatter-min (claim a bucket) + gather (compare keys), 8 rounds unrolled in the generator, with an output that
  must be 0 (entries that found no group). A scan (compaction of the keyed slots) has no parallel form in Halide:
  the parallel schedule is a different, hand-blocked algorithm (block scans, serial scan of block totals,
  scatter). Sizes computed by a pipeline cannot size a later stage of the same pipeline: a separate `Counts`
  pipeline runs first. Every RVar split for parallelism runs a tail past the end that must be clamped by hand.
- Impossible: nothing.

### 1b relocation targets (`RelTarget`, 33 lines)
- Easy: a ragged RDom (relocation in slot, slot), its `where` is the innermost loop bound; parallel over chunks of
  slots with allow_race_conditions (distinct relocations).
- Forced: `unsafe_promise_clamped` on every data-dependent index (else "accessed in an unbounded way").
- Impossible: nothing.

### 1c content-hash join (`ContentJoin`, 56 lines + join.h)
- Easy: the same join; the winner is one u64 max of (mark 1) << 63 | size << 32 | ~id.
- Forced: the kind folded into the key (kind 2 gets k2 = 2^40); a check output for sizes >= 2^31.
- Slower than C 2x (1 thr) / 2.5x (8): 1.29 M entries through 8 rounds, each round a full pass.

### 2 reach (`Init`, `Flood`, `Methods`, `Prep` in gen_p2.cpp, 241 lines; p2.cpp's level loop)
- Easy: each rule is one select of which byte to set; the sig lookup is join.h again (`Prep`).
- Forced: everything in the ATTACKS 1 list. Attempt 1 (packed bits per symbol, compare-and-swap via `atomic()`):
  correct serially (306 ms lazy / 193 eager after tuning) but Halide refuses atomic() when the written site depends
  on the Func's values and, once the sites came from the inputs only, implemented it with a mutex array per call
  (3.4-3.6 s at 1 or 8 threads) and lost live symbols in parallel. Attempt 2 (this one): one byte per fact,
  constant stores only, `allow_race_conditions`: 30/30 parallel runs correct (17:00Z), 79-90 ms at 8 threads.
- Impossible: a worklist, a loop to a fixpoint, a test-and-set; the frontier must be implicit.

### 3a lay out (`Layout` in gen_p3.cpp, 154 lines; from the earlier session)
- Easy: occupied sizes, per-unit sums, the block alignment walk as RDom updates.
- Forced: "read the cursor, store the address, bump the cursor" is two sites, so a phase RVar over one Func
  whose sites are [cursors, addresses]; the typelink sort is a 16-stage merge sort with merge-path binary search
  (Halide has no sort), with an extern C comparator for ties beyond 24 bytes of type string.
- 2.5x the plain C on 8 threads (20-23 vs 8.5-9 on one C core); 6x on one thread.

### 3b relocate (`Relocate`, 106 lines)
- Easy: the copy is a ragged RDom (byte, symbol); the patch value is one select chain over the 8 kinds.
- Forced: a value is written byte by byte (8 iterations, unrolled) because the image is u8 and an update writes
  one site; bytes a relocation does not write go to 8 scratch bytes past the image, because a `where` on the byte
  loop makes the relocation loop's `where` an `if` inside a loop to 2^24 (the first version ran 2^24 iterations
  per symbol). Parallel over chunks of 512 symbols (`parallel(rc.y)` alone is one task per symbol).
- A second form (relocations in equal chunks, each finding its symbol by a 20-step binary search) was dropped:
  slower than the first even when it ran.

## Code excerpts

The hash join (join.h), one round:

    Func claim(...);                       // bucket -> the smallest entry index that hashes there
    RDom rj(0, M);
    rj.where(creates(rj) != 0 && prev(rj) < 0);   // entries still looking for their group
    claim(h) = M;
    claim(bucket(rj)) = min(claim(bucket(rj)), rj);
    Expr own = claim(bucket(j));
    Expr hit = own < M && k1(own) == k1(j) && k2(own) == k2(j);
    rep(j) = select(prev(j) >= 0, prev(j), hit, own, -1);
    // schedule: claim.update().atomic(), split + parallel over chunks of rj

The flood's rule for a plain edge (gen_p2.cpp), as which bytes to set:

    Expr setIface = uii && bit(flt, fGoType) && bit(flt, fDef) && !ifaceT;
    Expr setLive  = plainOp && !liveT;
    Expr push     = setLive || setIface;                  // its block goes to the next level's list
    Expr site0 = select(push, SUMN + ti / BLK, ..., scr);   // phase 0
    Expr site1 = select(setIface, L.IFACE + ti, scr);        // phase 1
    Expr site2 = select(setLive || setIface, L.LIVE + ti, scr); // phase 2
    st(select(ph == 0, site0, ph == 1, site1, site2)) = u8(1);

The patch (gen_p3.cpp): 

    Expr v = select(kind == 1 || kind == 2, cast<int64_t>(t2) + add,
                    kind == 4 && ta == 0, Expr((int64_t)0xFFFFFFFFll),
                    kind == 3 || kind == 4, cast<int64_t>(t2 - sb) + add,
                    kind >= 5 && kind <= 7, select(tg == 0, add - pc, cast<int64_t>(t2) + add - pc),
                    add);
    image(select(write && rr.x < nbytes, P - imageBase + rr.x, NI + rr.x)) = cast<uint8_t>(v >> (8 * rr.x));

## Lines of code (non-blank, non-comment)
join.h 60, gen_p1.cpp 219, p1.cpp 80, gen_p2.cpp 241, p2.cpp 125, gen_p3.cpp 274, p3.cpp 141, bench.h 22,
build.sh 60. Drivers are loading, wrapping, the level loop (P2), the untimed edge split (P2) and checking.


# Install and hello-world (from the installer)

### Install (done, 2026-11-16)
- C++ API + AOT generators: official release tarball **Halide 21.0.0**. v21 is the newest tag that ships
  binaries; the v22.x GitHub releases have NO attached tarballs, only PyPI wheels.
  ```
  mkdir -p /home/exedev/tools/halide && cd /home/exedev/tools/halide
  curl -sL -o h21.tar.gz https://github.com/halide/Halide/releases/download/v21.0.0/Halide-21.0.0-x86-64-linux-b629c80de18f1534ec71fddd8b567aa7027a0876.tar.gz
  mkdir v21 && tar xzf h21.tar.gz -C v21 --strip-components=1     # 665 MB; include/ lib/ bin/ share/
  ```
  Host target detected by `v21/bin/get_host_target`: x86-64-linux, avx2, avx512 (skylake/cannonlake), zen4, fma, f16c.
  No cmake on the VM; g++ 13.3 is enough (see build.sh).
- Python: `python3 -m venv /home/exedev/tools/halide/venv && venv/bin/pip install halide==22.1.0 numpy` (JIT sanity-checked).
  Note: C++ AOT is 21.0.0, Python is 22.1.0.
- Data: `/home/exedev/tools/halide/data/{idx.u32,table.u32}` (raw LE u32; idx = 50M values in [0,2^24), table = 2^24 values; made by hello/mkdata.cpp).

### AOT/generator path (hello/)
1. `gen.cpp` defines three `Generator`s (gather, hist, scan) and links `libHalide.so` + `libHalide_GenGen.a` (supplies `main`; the tarball has no GenGen.cpp).
2. `./gen -g NAME -f FUNC -o . target=host variant=K` emits `FUNC.a` + `FUNC.h` (one invocation per variant).
3. `driver.cpp` mmaps the files (MAP_POPULATE), wraps them zero-copy in `Halide::Runtime::Buffer`, calls the AOT functions, checks each result against plain C++, reports best of 5.
`hello/build.sh` then `hello/run.sh` reproduce everything (data generated on first run). Raw output: `hello/run.out.txt`.

### Timings (run 2026-11-16T15:25:36Z, best of 5, N=50,000,000 u32, table 64 MB, 2^20 bins; ms)
| pipeline | 1 thread | 8 threads |
|---|---|---|
| (a) gather out[i]=table[idx[i]] | 488 | 91 (68 in an earlier run) |
| (b) hist, RDom scatter, serial update (default) | 66 | 65 |
| (b) hist, `update().atomic().parallel(r)` | 698 | 1584 |
| (b) hist, `rfactor` private hists + merge, parallel over 8 chunks | 150 | 184 |
| (c) scan, serial RDom `out(r)=out(r-1)+in(r)` | 46 | 44 |
| (c) scan, hand-blocked 3-phase, parallel | 244 | 60 |
| plain C++ gather / hist / scan (1 thread) | 416 / 61 / 20 | n/a |

All Halide outputs verified equal to the C++ reference. Gather: 8 threads give 5-7x (DRAM random-load bound, about 0.55-0.75 G/s).

### What Halide does with these
- **Gather**: pure definition, trivial, parallel over chunks, matches C++ and scales. A data-dependent index REQUIRES `clamp` (else "Buffer may be accessed in an unbounded way"). Halide's model is that it must bound every access footprint.
- **Scatter / histogram**: an RDom update `h(f(in(r))) += 1`. Default is serial. `parallel(r)` is refused with "may introduce a race condition" (tested). Options: `atomic()` (correct but 10-25x slower here; slower with more threads), or `rfactor` (private partial histograms per chunk; did not beat serial for 2^20 bins; I tried one schedule only, untuned, so this is not a verdict). Halide has no sort, radix, or partition primitive.
- **Scan**: no scan primitive. A scan is an RDom update with a loop-carried dependence; `parallel` on it is refused (same race error, tested), and `rfactor` needs an associative reduction, which a scan is not as a whole. The only route is a hand-written blocked scan as separate Funcs (what variant 1 does: in-block scan parallel over blocks, serial scan of block totals, add offsets). That makes about 3 passes over memory: 60 ms at 8 threads vs a 20 ms single-thread C++ loop, so no win at 50M u32 (the serial scan already moves ~9 GB/s).
- Scatter output bounds cannot be inferred; the extent is pinned with `set_bounds`.

### Gotchas
- v22.x has no binary tarball; use v21.0.0 or the pip wheel.
- Arithmetic with `GeneratorParam<int>` and `0u` is ambiguous as Expr; use `cast<uint32_t>(0)`. `RDom(0, buf.dim(0))` does not compile; use `.extent()`.
- `Var`s used in `schedule()` must be the same Vars as in `generate()` (make them generator members), else "could not find split dimension".
- A uint32 index into a Buffer needs an explicit `cast<int>`; no implicit unsigned-to-signed.
- `HL_NUM_THREADS` controls the runtime thread pool (works as expected).
- Compile errors from `.parallel()` are thrown as `Halide::CompileError` at schedule time.
# Soufflé column: problems 1-3 on MinIO

Soufflé 2.5, compiled mode (`souffle -g` + g++ -O3), every cell correct against its oracle. Inputs come in through
`flatio.h` (C++, I/O only): build.sh rewrites the generated reader/writer calls so binary flat-file columns are
mmap'd, turned into tuples in parallel, sorted per index and bulk-loaded into Soufflé's B-trees (`btree_set::load`).
The rules are plain Datalog; nothing in C computes an answer, except the 3b image assembly (copying bytes; Soufflé
has no bytes) and the untimed P2-lazy setup that p2lazy.c also does untimed.

## Surprises

- **Lazy decode is expressible and wins against Soufflé's own eager shape, but not against C (ATTACKS 1).** The rule
  `plain(s, t) :- live(s), source(s), rel(s, p, i, ty), plainty(ty), slotobj(s, o), objpkg(o, p, b), canon(b + i, t)`
  is evaluated semi-naively, so only live sources' relocations are decoded (1.73 M `plain` tuples of 9.4 M
  relocations). Warm: 2.9-3.2 s on 1 thread, 0.96-1.6 s on 8, vs 1b's eager decode 5.2-5.4 / 1.08-1.30 s plus the
  flood over the pre-decoded CSR 1.23-1.47 / 0.43-0.58 s. The C pipelines: eager 108.7 / 30.1 ms, lazy 52.6 /
  13.6-15.4 ms. So Soufflé is ~30x / ~35-50x the *eager* C pipeline and does not approach the lazy one. And the
  laziness stops at the rules: the engine joins only over B-trees, so the 9.4 M relocation columns are still loaded
  into a B-tree index first (6.15 M distinct tuples, ~1.5 s of the 2.2 s load at 1 thread): Soufflé materializes the
  *input* even when the rules don't materialize the graph. e2e: lazy 4.6-5.0 s / 2.3-3.1 s vs eager 1b + 2 =
  7.4 + 2.6 s / 2.9 + 1.3 s.
- **The output's page size is irrelevant to the Soufflé 3b cell (ATTACKS 2).** The image lives in flatio's writer
  (C), where MAP_HUGETLB works: assembly 53.5-96.9 ms (19,108 minor faults) on 4 KB pages vs 20.0-34.7 ms (39 faults)
  on 2 MB, 1 thread; 18.3-31.6 vs 7.8-16.2 ms (150-172 faults) on 8 (FLATIO_IMG_REPS=20, three interleaved runs each,
  16:29-16:31Z, load 8.7-11). But the rules computing the 2.05 M patch values take 1.05-1.12 s / 0.32-0.45 s: the
  faults are 2-5% of the cell. Soufflé's own runtime (B-trees from `new`) has no page-size knob; this kernel has no
  THP.
- **8 threads with OpenMP's default (spinning) wait policy lost 10-25x under load**: reach_lazy -j8 took 38 s vs
  4.2 s at -j1 (16:17Z, load ~11). With `OMP_WAIT_POLICY=passive` it is 1.5 s. Every 8-thread number below is passive.
- **A sum over "the earlier members of my group" is fast when the group key is an index prefix**: Soufflé turns
  `y = sum o : { occ(b, c, u, j, o), j < i }` into a range scan of a (b, c, u, j) index; layout's 327 K non-typelink
  addresses (Σ n²/2 = 108 M tuples visited) take 56 ms on 8 threads. Comparing *symbols* (strings) in the same shape
  is 80x slower per pair: 4.6 s for the 33 K typelinks.

## Timings

`bench.sh` (5 interleaved rounds, min / median / max ms), 2026-11-16T16:37Z, load 2.9-6.2; full output in
`bench-20261116T1637Z.txt`. warm = flatio's "compute" (rules only; inputs already in the B-trees, output writing
excluded). e2e = process start to exit. 8 threads: `OMP_WAIT_POLICY=passive`. Each warm run is one evaluation (the
generated main runs once); the warm times are >= 0.3 s, most >= 1 s.

| cell | 1 thread warm | 1 thread e2e | 8 threads warm | 8 threads e2e | plain C 1 core / tuned C 8 |
|---|---|---|---|---|---|
| 1a join by name (resolve_a) | 1928 / 1996 / 2034 | 3136 / 3206 / 3295 | 636 / 680 / 754 | 1722 / 1736 / 1888 | 194-203 / 8.6-10.5 |
| 1b relocation targets (resolve_b) | 5220 / 5263 / 5426 | 7371 / 7542 / 7657 | 1080 / 1206 / 1301 | 2669 / 2875 / 2972 | 43-44 / 5.3-6.3 |
| 1c content-hash join (resolve_c) | 1625 / 1664 / 1865 | 2805 / 2879 / 3127 | 587 / 615 / 718 | 1566 / 1610 / 1761 | 57-61 / 7.2-8.9 |
| 1c, rules over the slot columns (resolve_c2) | 1013 / 1055 / 1069 | 3086 / 3184 / 3256 | 394 / 408 / 437 | 1936 / 2015 / 2107 | |
| 2 reach (reach, pre-decoded CSR) | 1227 / 1317 / 1467 | 2571 / 2713 / 2921 | 431 / 462 / 579 | 1242 / 1294 / 1429 | 39-43 / 13-17 |
| 2' reach, lazy decode (reach_lazy) | 2905 / 3124 / 3214 | 4637 / 4797 / 4982 | 960 / 1069 / 1610 | 2312 / 2478 / 3081 | 52.6 / 13.6-15.4 (p2lazy) |
| 3a layout (layout) | 7222 / 7337 / 7589 | 7367 / 7480 / 7717 | 1339 / 1625 / 1784 | 1474 / 1774 / 1933 | 8-19 / - |
| 3b relocate (relocate; rules) | 1054 / 1079 / 1115 | 2086 / 2109 / 2139 | 320 / 366 / 451 | 1205 / 1302 / 1374 | 59-67 / 13.4-29 |
| 3b image assembly (flatio, C, 4 KB pages) | 54.2 / 54.4 / 55.9 | | 18.0 / 26.1 / 28.5 | | |

Orders of magnitude: resolve 10-120x the plain C on 1 core (1b worst) and 70-200x the tuned C on 8; reach 30x / 30x; layout
~500x / ~100x the plain C; relocate's rules ~17x / ~25x (only the C assembly is near C).

## Checks (each must print nothing / P3 OK)

```
cd systems/souffle; D=/home/exedev/paradigms/data/minio; for p in resolve_a resolve_b resolve_c resolve_c2 reach layout relocate reach_lazy; do ./build.sh $p.dl; done
gcc -O2 -o build/check_p3 check_p3.c; gcc -O2 -o build/prep_lazy prep_lazy.c; build/prep_lazy $D build/lazyfacts
build/resolve_a -j8 -F $D -D out && cmp out/p1_canon.u32 $D/oracle/p1_canon.u32 && cmp out/p1_builtin.u32 $D/oracle/p1_builtin.u32
build/resolve_b -j8 -F $D -D out && cmp out/p1_reltarget.u32 $D/oracle/p1_reltarget.u32
build/resolve_c -j8 -F $D -D out && cmp out/p1c_canon.u32 $D/oracle/p1c_canon.u32      # also resolve_c2
build/reach -j8 -F $D -D out && cmp out/p2_live.u32 $D/oracle/p2_live.u32
build/reach_lazy -j8 -F build/lazyfacts -D out && cmp out/p2_live.u32 $D/oracle/p2_live.u32
build/layout -j8 -F $D -D out && build/check_p3 $D $D/oracle/p3_image.bin out/p3_addr.u32   # 3a
build/relocate -j8 -F $D -D out && build/check_p3 $D out/p3_image.out                       # 3b
```
`bench.sh` runs all of these each round and complains on stderr if one fails.

## Per cell

Lines: rules + declarations, without comments (the file's total in brackets). Shared: flatio.h 385 lines of C++
(I/O only), build.sh 19.

**1a join by name** (resolve_a.dl, 37 [51]). Easy: groups, "a kind-1 candidate joins only an existing group"
(a join with `group`), winner = `max rank` then `min id` aggregates, the debug-reference rule as negation.
Forced: none. Impossible: none. The 128-bit key is two columns.

**1b relocation targets** (resolve_b.dl, 25 [35]). Easy: the package-table gather is a join on (object, entry);
the special package indices are arithmetic. Forced: `npkg` is a count aggregate per object; "no target -> 0" needs
a helper relation and negation. Slow: 9.4 M relocations x 4 joins, each a B-tree probe and an insert (5.2 s / 1.1 s).

**1c content-hash join** (resolve_c.dl 26 [42]; resolve_c2.dl 23 [31]). Easy: the winner order as aggregates
(`max size` among reached copies, then `min id`). Forced: aggregates run once per tuple of the body before them, so
each group must first be its own relation (`group2(k1) :- ...` then `size2(k1, s) :- group2(k1), s = max ...`);
written as `copy2(k1, _, 1, _), s = max ...` it ran once per copy. Two shapes: copy relations (resolve_c), or the
rules straight over one 5-column `slot` relation (resolve_c2: 37% less compute, but more indexes to bulk-load, so
e2e is a wash).

**2 reach** (reach.dl 47 [60]; reach_literal.dl 39 [50], README's rules written literally). Easy: every rule of
deadcode.go is one Datalog rule, including the conditional method rule; mutual recursion of live/iface/sig/cand is
just a stratum. Forced: joining the static parts (type-to-type edges, a type's methods) once before the flood.
Semi-naive evaluation inserts every live tuple into `live`, `delta_live` and `new_live` B-trees per iteration.

**2' reach, lazy decode** (reach_lazy.dl 65 [84], plus prep_lazy.c 67, untimed, as p2lazy's setup). See Surprises.
Easy: the decode is three rules (ordinary package / special package / builtin), body-joined with `live(s)`.
Found and checked: hlink's walk skips the two relocations after an R_METHODOFF (`r += 2`); on MinIO all 225,760
skipped ones are themselves R_METHODOFF (prep_lazy checks this), so "plain" is a filter on the type and needs no
walk. Forced: the aux edges (funcdata) come pre-split from p2_edge, as in p2lazy.

**3a layout** (layout.dl 45 [66]). Datalog has no scan or sort. Forced: (1) every running sum is an aggregate over
the earlier members of a group (`sum o : { occ(b, c, u, j, o), j < i }`, a range scan, O(n²) per group but fast,
56 ms); (2) the walk over units is a recursion, one unit per iteration (1,763 iterations, ~0.25 s); (3) typelinks:
the sort by type string is "the sum of the sizes of the typelinks before me", O(n²) comparisons. Attempt 1 compared
symbols (Soufflé compares symbols as strings): 28 s / 5.0 s. Attempt 2 adds an 8-byte big-endian prefix of each
type string as a number (loader column `p3_str.key8`) so most pairs are a numeric range: 7.2 s / 1.3 s, still 95% of
the cell. Also forced: the typelink round-up only commutes with the sum because every MinIO typelink is class 3 with
a size that is a multiple of 8 (checked by the rule `tlcheck`, empty). Verdict: correct but ~500x C: **falls down on
speed** (two attempts; the O(n²) "rank by counting" is what Datalog without sort has).

**3b relocate** (relocate.dl 31 [40]). Easy: each relocation kind is one or two rules; the section base is a `max`
aggregate over 8 values; errors are a relation. Forced: the image itself (bytes) is assembled by flatio's writer in
C (`image=` directive): copy each symbol's bytes, write each patch's low bytes. Attempt 1 materialized `site`
(rel x addr x target address): 5.2 s / 1.5 s; attempt 2 inlines it (`.decl site(...) inline`): 2.5 s / 0.74 s at
load 11 (1.05 / 0.32 s at load 3).

## Code excerpts

1c, the content-hash join straight over the slot columns (resolve_c2.dl):
```
group2(k1) :- slot(_, 2, 1, k1, _).
size2(k1, s) :- group2(k1), s = max ss : { slot(_, 2, 1, k1, ss) }.
win2(k1, w, s) :- size2(k1, s), w = min g : { slot(g, 2, 1, k1, s) }.
moved(g, w) :- win2(k1, w, _), slot(g, 2, 1, k1, _), g != w.
moved(g, w) :- win2(k1, w, ws), slot(g, 2, 2, k1, s), s <= ws.
final(g, c) :- canon(g, c), !moved(g, _).
```
2', decoding a relocation when the flood reaches its symbol (reach_lazy.dl):
```
plain(s, t) :- live(s), source(s), rel(s, p, i, ty), plainty(ty), p < 2147483643,
               slotobj(s, o), objpkg(o, p, b), b != 0xFFFFFFFF, canon(b + i, t), t != 0.
live(t)    :- plain(_, t).
iface(t)   :- iface(s), gotype(s), plain(s, t), gotype(t).
```
3a, a symbol's address (layout.dl):
```
va(b, u + 1, x + n) :- ubase(b, u, x), ub(u, b, _, n), meta("nunits", nu), u + 1 < nu.
cbase(b, c, u, x + y) :- ubc(u, b, c, _), ubase(b, u, x), y = sum z : { ubc(u, b, c2, z), c2 > c }.
addr(i, x + y) :- occ(b, c, u, i, _), cbase(b, c, u, x), y = sum o : { occ(b, c, u, j, o), j < i }.
```

## Files

`*.dl` the cells; `flatio.h` the binary column reader/writer (inputs `IO=flat cols=... [csr=] [index=|pos=]`,
`.str`/`.key8` string-table columns, `meta=true`; outputs `cols=`, `image=`, `exitafter=`); `build.sh`;
`bench.sh`; `check_p3.c`; `prep_lazy.c`; `best.sh` (older single-cell timer).

## Install, hello-world, timings, gotchas (installer notes)

Measured 2026-11-16 15:27 UTC (`date -u`), Ubuntu 24.04.5, 8 cores, g++ 13.3. Everything reproducible with
`hello/build.sh` then `hello/run.sh`.

### Install (done)

```
mkdir -p ~/tools/souffle-dl && cd ~/tools/souffle-dl
curl -sLO https://github.com/souffle-lang/souffle/releases/download/2.5/x86_64-ubuntu-2404-souffle-2.5-Linux.deb
sudo apt-get install -y ./x86_64-ubuntu-2404-souffle-2.5-Linux.deb     # deps: g++, libffi-dev, libncurses5-dev, libsqlite3-dev, mcpp, zlib1g-dev
```
`souffle --version`: **2.5** (latest release as of today; options enabled: ffi openmp ncurses sqlite zlib).
Headers in /usr/include/souffle (used for the embedding build). Installed system-wide in /usr (not ~/tools/souffle).
Verified: interpreter (`souffle -j8 reach.dl`), compiled (`souffle -o prog -j8`, then `./prog -j1|-j8`),
and `souffle -g x.cpp` code generation + own `main` (embedding).

### Hello world: reach from 100 roots

`hello/gen.cpp`: 2 M nodes, 10 M uniformly random edges (seed 42), 100 random roots. Writes `edge.bin`
(80 MB, u32 pairs), `edge.facts` (149 MB TSV), `root.facts`. Rules (`reach.dl`):
`reach(x):-root(x). reach(y):-reach(x),edge(x,y).` + count. Result **1,985,956** reached (all variants agree).
After set-dedup the relation has 9,999,989 distinct edges.

Timings, wall clock, ms (one run each, noise ~5%):

| path | load | eval | total |
|---|---|---|---|
| interpreter, TSV, -j8 | | | 10,117 |
| compiled, TSV `.input IO=file`, -j1 | 9,527 (load-only program) | ~4,340 (diff) | 13,863 |
| compiled, TSV, -j8 | 9,365 | ~1,310 (diff) | 10,679 |
| embedded, `Relation::insert(tuple)` public API, serial | 4,812 | 2,717 | 7,529 |
| embedded, btree insert w/ per-thread hint ctx, 8 thr | 919 | 2,999 | 3,918 |
| embedded, sort pairs + `btree_set::load` bulk, 1 thr | 943 | 2,507 | 3,450 |
| **embedded, parallel sort + `btree_set::load`, 8 thr** | **446** | 2,636 | **3,082** |

"diff" = reach program minus load-only program; includes the extra index build and is only indicative.
Embedded numbers are in-process phase timers (mmap of edge.bin is ~5 ms). Hardware-style context: the raw
80 MB read is ~1 ms at DRAM speed, so even the best path is ~450x its read floor on load and a 10 M-edge
BFS taking 2.5 s is ~4 M edges/s, orders of magnitude under the 0.74 G random loads/s the brief quotes.

#### Fastest fact-input path
1. TSV `.input IO=file` is single-threaded text parsing: ~9.4 s for 10 M rows (~1 us/row), and the same at -j8.
   Dominates everything. (Not tried: IO=sqlite, gz, splitting TSV into N relations. SQLite is strictly more work.)
2. C++ embedding API: `souffle -g prog.cpp prog.dl`, then own main with `#include "prog.cpp"` and
   `-D__EMBEDDED_SOUFFLE__` (skips the generated main). Public `Relation::insert` = 4.8 s (virtual call + tuple
   copy + cold btree insert), not much better than text.
3. Fastest: bypass the wrapper. `#define private public` before including the generated file exposes
   `rel_edge_<hash>->ind_0` (a `btree_set<Tuple<u32,2>>`); build a sorted unique `vector<Tuple>` from the
   mmap'd binary (parallel `__gnu_parallel::sort` on packed u64 keys) and `ind_0 = btree_set::load(begin,end)`
   (linear-time bulk build, BTree.h). 446 ms at 8 threads, and the resulting tree is dense, so eval is faster too
   (2.3-2.6 s vs 2.7-3.1 s for random-order insertion). Needs a one-index relation; with several indexes
   (auto-index selection) each `ind_k` must be loaded in its own sort order. Fragile: hash suffixes in member
   names (`rel_edge_04d25f5060e9043b`) change if the .dl changes; grep after `souffle -g`.
4. Not tried: a custom IO via functor / `IO=` plugin; and a functor-based edge lookup (`@edges(x)`) would
   skip the relation entirely but gives up Soufflé's joins.

### Gotchas
- `souffle -g` drops input relations with no `.input` and no rules (warning "No rules/facts defined"); keep a
  dummy `.input ... IO=file` and call `runAll("", "", /*performIO=*/false, false)` to skip file IO.
- `count` yields signed `number`; declaring the result `unsigned` is a type error.
- `.input` without `-F` looks in cwd; `-D-` prints outputs to stdout.
- **-j8 barely helps reachability**: semi-naive BFS runs one round per frontier level; small frontiers do not
  parallelise, and the embedded eval is 2.6 s at both -j1 and -j8. Loading (parallel insert) is what scales.
  GDlog paper observes the same: tuple insertion is 77.8 % of Soufflé runtime on REACH, 16->32 threads gives 7 %.
- Semi-naive keeps `reach`, `delta_reach`, `new_reach` as separate btrees and re-inserts every discovered
  node; a bitmap flood would touch each edge once. See IDEAS.md.
- Compile time: ~10 s per generated program with g++; interpreter has no compile step but is similar here (10.1 s,
  all of it load).
- Output relation `total` must be read via `getRelation("total")` iterator when performIO is off.
# DuckDB (vectorized, morsel-parallel SQL)

DuckDB v1.5.5 (libduckdb from the GitHub release, `~/tools/duckdb/{duckdb.h,libduckdb.so}`; the CLI is
`/usr/local/bin/duckdb`, same version). Every problem's logic is SQL in `sql/`; `dk.c` is a C host that only does I/O:
it shows the flat files to DuckDB as Arrow arrays (zero-copy over the mmaps), runs the scripts with a clock per
statement and per `.cell`, and writes results out as raw arrays or into an image.

## Surprises

- **The CSRs are Arrow lists, for free.** `slot_rel.u32` + `rel_*.u32` is exactly Arrow's list layout (offsets +
  child arrays), so dk hands DuckDB `srel.rels`, `sym.out`, `sym.meths`, `obj.bases`, `sym3b.rels` as
  `LIST(STRUCT(...))` columns with no copy, and `unnest(rels, recursive := true)` turns a symbol into its
  relocations without a join. That replaced an as-of join (1b: 6.4 s -> 2.1 s on 1 thread) and a bucketed range join
  (P2 prep: 16.9 s -> 0.2 s on 1 thread). The first version of both was "SQL over flat tables"; the nested type is
  what made them reasonable.
- **Decoding in the flood does not win in SQL (ATTACKS 1).** In C the lazy flood is 2x the eager pipeline. In DuckDB
  the lazy loop (2loop-lazy) costs about as much more than the eager loop as the eager decode costs: each round's
  decode is a hash join against the 5.2 M-row `canon` table, rebuilt or rescanned every round (no persistent index
  on a dense id), so 45 rounds of "decode only the frontier" read `canon` 45 times. See ATTACKS below.
- **A dense-id lookup is a hash join.** SQL has no `a[i]`. 9 M probes into a 5 M-row table cost 1.3 s of CPU
  (~140 ns per probe, 0.22-0.29 s on 8 threads, CLI test 16:20Z at load 12); the C gather is a few ns per element.
  This one fact explains most of the distance to C in every problem. A `LIST` lookup (`l[i+1]` on a 5 M-element
  list cross-joined in) is worse: 5.3 s.
- **`prlimit --as` segfaults DuckDB at any limit** (6G, 12G, 32G, 100G): it reserves address space up front. Cap
  memory with `systemd-run --user --scope -p MemoryMax=8G` instead (bench.sh does).

## Layout

| file | what |
|---|---|
| `dk.c` (build: `./build.sh`) | the host: `.table NAME ROWS col=file ...` (Arrow view + CREATE TABLE), `col=OFF.u32[f=file,...]` a list column over a CSR, `col=#` a row index, `.cell`, `.repeat/.until` (a loop driven from outside: SQL has no loop), `.dump`, `.image/.blob/.int/.imgout` (the 3b scatter), `.show`; `DK_REPS` (repeat the scripts after `--`), `DK_THP` (image on 2 MB pages), `DK_STMTS` (time per statement over all rounds) |
| `sql/load1.sql p1.sql` | P1: 1a, 1b, 1c |
| `sql/load2.sql p2prep.sql p2loop.sql` | P2 from the p2_* files |
| `sql/p2aux.sql p2eager.sql p2lazy.sql` | ATTACKS 1: P2 from the relocation columns (after P1), eager or lazy decode |
| `sql/load3.sql p3a.sql` | 3a |
| `sql/load3b.sql load3b_iso.sql p3b.sql` | 3b isolated (oracle addresses, `p3_addr_all`) |
| `sql/p3link.sql` | 3a -> 3b pipeline: addresses from DuckDB's own 3a through its own 1c canon |
| `check3b.py` | the 3b image check (every placed symbol's bytes vs `oracle/p3_image.bin`) |
| `bench.sh` | every timing, one line per (cell, threads, warm/e2e) |

## Oracle checks

    cd systems/duckdb && ./build.sh && D=/home/exedev/paradigms/data/minio
    ./dk $D 8 sql/load1.sql sql/p1.sql && for f in p1_canon p1_builtin p1_reltarget p1c_canon; do cmp out/$f.u32 $D/oracle/$f.u32 && echo ok $f; done
    ./dk $D 8 sql/load2.sql sql/p2prep.sql sql/p2loop.sql && cmp out/p2_live.u32 $D/oracle/p2_live.u32 && echo P2 ok
    ./dk $D 8 sql/load1.sql sql/p1.sql sql/load2.sql sql/p2prep.sql sql/p2aux.sql sql/p2lazy.sql sql/p2loop.sql && cmp out/p2_live.u32 $D/oracle/p2_live.u32 && echo P2 lazy ok   # (p2eager.sql likewise)
    ./dk $D 8 sql/load3.sql sql/p3a.sql && cmp out/p3_addr.u32 $D/oracle/p3_addr.u32 && echo 3a ok
    ./dk $D 8 sql/load3b.sql sql/load3b_iso.sql sql/p3b.sql && python3 -I check3b.py $D out/p3_image.bin       # P3b OK
    ./dk $D 8 sql/load1.sql sql/p1.sql sql/load3.sql sql/p3a.sql sql/load3b.sql sql/p3link.sql sql/p3b.sql && python3 -I check3b.py $D out/p3_image.bin   # pipeline

All pass (2026-11-16T16:40Z-16:47Z): 1a, 1b, 1c, P2 (three ways), 3a, 3b isolated, and the P1 -> 3a -> 3b pipeline
(DuckDB's own canon and layout, `p3_fixed_*` given) reproduce the oracles exactly.

## Timings

`bench.sh` (2026-11-16T16:49Z-17:09Z, load 3.0-6.0, five interleaved rounds of 1 and 8 threads; warm = median of
5 in-process reps per round, then min/median/max over the rounds; e2e = the whole process; raw output in
`results-bench-20261116T1649Z.txt`). ms. C references from GRID.md (plain C 1 core warm; tuned C 8 threads).

| cell | 1 thread warm | 8 threads warm | 1 thread e2e | 8 threads e2e | plain C 1 core | tuned C 8 thr | DuckDB / tuned C (8) |
|---|---|---|---|---|---|---|---|
| 1a | 319-337-355 | **103-107-113** | P1: 4188-4228-4386 | P1: 1112-1122-1278 | 194-203 | 8.6-10.5 | ~11x |
| 1b | 1405-1449-1470 | **272-314-375** | (in P1) | (in P1) | 43-44 | 5.3-6.3 | ~50x |
| 1c | 563-595-632 | **128-165-190** | (in P1) | (in P1) | 57-61 | 7.2-8.9 | ~18x |
| 2 (prep + loop) | 207-212-251 + 2915-3206-3492 | **103-103-116 + 1965-2273-2354** | 3463-3625-3875 | 2367-2554-2567 | 39-43 | 13.0-17.0 | ~150x |
| 3a | 304-315-322 | **217-246-263** | 408-415-439 | 325-335-448 | 8-19 | (none) | ~15-30x plain C |
| 3b (4 KB pages) | 789-827-1142 | **339-362-368** | 1583-1637-1786 | 687-750-956 | 48.6-56 | 13.4-29 | ~25x |
| 3b (2 MB pages) | 813-832-937 | **309-321-332** | | | 23.9-30 | 5.4-12 | ~50x |
| P1 -> 3a -> 3b pipeline | | | 5962-6138-6436 | 2017-2090-2765 | | | |

e2e includes DuckDB start, the Arrow views and the copy into DuckDB tables (P1 load ~0.3-0.6 s), and writing the
output arrays / image. Load (`CREATE TABLE AS FROM arrow view`) is not in the warm numbers.

Lines (non-blank, non-comment SQL): 1a 18, 1b 17, 1c 16 (p1.sql 56 + load 5); P2 48 + prep 8 + load 3;
ATTACKS 1 aux 16, eager 12, lazy 20; 3a 35 + load 3; 3b 32 + load 3; pipeline link 9. Host: dk.c 628 lines of C
(I/O only: Arrow views, the statement runner, the scatter of 3b's writes).

## Per cell

**1a (join by name).** Easy: two GROUP BY max over a UNION ALL, a SEMI JOIN for "a candidate joins only an existing
group", a LEFT JOIN for the canonical ids. The 128-bit key is read as Arrow `decimal128` = DuckDB HUGEINT, so
equality and hashing are on the raw bits. Forced: nothing. 18 lines.

**1b (relocation targets).** Easy once the relocations are a list per slot (`unnest`), the package table a list
per object (`generate_subscripts` gives the index). Forced: a LEFT JOIN `ON rel.pkg = BUILTIN AND bc.i = rel.idx`
is planned as a block-wise nested-loop join (6.8 s of CPU); `ON bc.i = CASE WHEN rel.pkg = BUILTIN THEN rel.idx END`
is a hash join. Two hash joins against 5 M and 9 M rows are the whole cost. 17 lines.

**1c (content-hash join).** Easy: `arg_min(struct, (mark, -size, id))` per group is the winner rule as written. 16 lines.

**2 (reach).** Forced. SQL has no loop: dk's `.repeat/.until` reruns a block of statements until the frontier is
empty (45 rounds), i.e. semi-naive evaluation driven from outside, one temp table per relation. 48 lines for the
rules + 8 prep. Every round rebuilds hash tables for `live`, `iface`, `uii` and the frontier joins; there is no
persistent index, so a round costs ~25 statements x (1-10 ms) regardless of how small the frontier is. Two tuned
attempts: (1) edges by a bucketed range join, 16.9 s prep; (2) edges as lists + the rules' flag tests as small
relations (`gt`, `rf`) instead of joins with the 5.2 M symbols. A recursive CTE cannot express it (the method
rules need the global sig/named/reflect sets, and a recursive CTE may reference itself once).

**3a (layout).** Mostly easy: occupied sizes, per-(unit, bucket, class) sums and every in-class position are window
sums. Forced: "va = round_up(va, align); va += n" over the units is a fold, not a prefix sum (a round-up does not
compose). Tried three ways: a Jacobi fixpoint over the paddings (prefix-sum with guessed paddings, recompute,
repeat): exact but 1,121 rounds for the units; a recursive CTE over the unit index: 1,322 iterations, 340 ms on 1
thread and 620 ms on 8 (the scheduler's per-iteration cost); `list_reduce` with a list accumulator (a fold in
DuckDB's lambdas, O(n^2) list copies but n = 1,322): 65-95 ms. Typelinks (33 K) use the fixpoint: 1 round on MinIO
because every typelink size is a multiple of its alignment, exact in general. 35 lines.

**3b (relocate).** SQL computes every write: the symbols' bytes as (pos, blob) rows and every relocation as
(pos, width, value) from one CASE over the kinds; dk copies them in (`.blob`, `.int`: a memcpy per row, the only
thing SQL can't say, "store at byte offset"). The section base is a CASE ladder. 32 lines. Isolated and pipeline
both exact.

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
# Goose: the cells (problems 1-3 on MinIO), then install notes

Cells measured 2026-11-16T16:22-16:25Z under **load 8-13** (all builders running): every number below is a range
over 5 interleaved rounds of `systems/goose/bench.sh` (min / median / max), each round repeating its kernel in-process
(P1_REPS=5, P2REPS=10-20, P3_REPS=20) and taking the min of its reps. Treat them as provisional; the lead can rerun
`systems/goose/bench.sh` on a quiet machine. Goose's queues block on a pthread condition variable (no spin-wait), so
the 8-worker numbers have no wait policy to set (RESUME.md's load warning does not apply as such; a wake-up per
message round still costs more under load). Build: `goose build X.goose -o bin/X` (release v2026.4, gcc 13.3 -O2).
Memory cap: `systemd-run --user --scope -p MemoryMax=8G` (Goose reserves 2 GB of address space per data stack, so
`prlimit --as=6G` makes it die at start with "cannot reserve data stack address space").

## Surprises

1. **The shared-nothing tax is the gather, not the compute.** 3b on 8 workers builds its image in 17-24 ms (2 MB pages)
   when each worker keeps its address range; gathering the 109 MB into one array in main costs **142-171 ms** more
   (two copies through the queue plus main's page faults: 57-83 k faults). Same in 1b: 9.4 M targets computed in
   parallel and sent to main take 42-67 ms on 8 workers vs 39-57 ms on 1 thread. The workloads parallelize; moving
   their outputs between isolated programs is 5-8x the compute.
2. **Goose's runtime pins the output to 4 KB pages, but a 20-line C sidecar can swap the pages under a builder's stack
   top for hugetlb pages, and it halves 3b** (ATTACKS 2): 1 thread 86.7-98.5 ms -> **40.8-42.2 ms**, minor faults per
   rep 26,731 -> 258-457; 8 workers 32.8-40.0 -> **17.4-23.5 ms**. The runtime reserves each data stack as a NORESERVE
   4 KB mapping and madvises MADV_NOHUGEPAGE on its first 2 MB; the language has no word for page size.
3. **P1's first repetition is 2.5x its later ones** (1a 213 ms once vs 79-115 ms in reps 2-5): the 200 MB open-addressing
   table (8 M groups x 24 B) and the 1c table are first-touched in rep 1; later reps reuse the same stack pages (a
   resizable cleared and regrown on the same data stack never returns its pages). The warm P1 numbers below are those
   later reps; the C references' single-run numbers include their table faults. One run of p1 (all of 1a+1b+1c, first
   touch included) is 560 ms of compute (16:15Z, load 2).

## ATTACKS 1 (reach, counting the decode)

- **Expressible: yes, trivially.** The flood's drain loop decodes a live symbol's relocations from rel_pkg/rel_idx/
  rel_type through its object's package-base table and canon right where it pops the symbol (p2lazy.goose `drain` and
  `target`, p2rules.goose for workers). Goose is imperative; nothing forces the graph to be materialized. What is
  forced: the p2 files mix relocation edges with aux edges, so load "cuts" the relocation edges out of each row
  (outside the timed region; it is why p2lazy's end to end is 731-859 ms).
- **Numbers** (interleaved with the C references, 16:24-16:25Z, load 7.5-8.5): 1 thread Goose lazy **70-84 ms** vs C
  p2lazy 61-71 (1 outlier 229) in the same window, vs the eager C pipeline 108.7 (quiet). 8 workers Goose lazy **39-59 ms**
  vs C p2lazy 21-31 (OMP_WAIT_POLICY=passive) and eager C 30.1 (quiet). So: it beats the eager C pipeline on 1 thread,
  and is near the lazy C one there (1.1-1.2x); on 8 workers it does not beat even the eager C pipeline.
- **Against Goose's own eager shape** it wins at both widths: eager = 1b decode (39-57 ms 1 thread, 42-67 on 8) + a CSR
  build (not written: the extractor's p2 files are that CSR) + the CSR flood (41-48 ms 1 thread, 22-32 on 8), so >= 80 /
  64 ms before the CSR build, vs 74-79 / 37-49 lazy.
- Why 8 workers lose to C: the flood (lazy or not) scales only ~1.7x from 1 to 8 workers (CSR 41 -> 22-32; lazy 66 -> 38-48,
  16:16Z): 10 bulk-synchronous supersteps with W x W inbox messages per step, each a condition-variable wake-up, and each
  worker drains only 9-20 ms of its 37-47 ms. Lazy costs ~1.7x the CSR flood at every W (three columns read per
  relocation instead of one, plus the canon gather into a per-worker 20 MB copy: shared-nothing gives every worker its
  own canon, 160 MB on 8, where C's threads share one).

## ATTACKS 2 (relocate on huge pages)

Yes, but under the language: `flatio.c` `fresh_pages(builder, n, huge)` mmaps MAP_FIXED|MAP_HUGETLB over the 2 MB-
aligned part of the next n bytes above the builder's stack top (the head below the first 2 MB boundary stays on 4 KB
pages, hence 258-457 faults rather than C's 39), then Goose's own `img.resize(n, 0)` writes the image there. With
`huge=false` it maps fresh 4 KB pages, so every rep pays its faults (P3_REPS, like ref/flat/p3.c). `minor_faults()` is
getrusage. Interleaved 16:24-16:25Z (load ~8), 1 thread P3_REPS=20 min per run:

| | 4 KB pages | 2 MB pages |
|---|---|---|
| C `build/p3` | 56.4-60.3 ms, 19,109 faults/rep | 24.5-26.0 ms, 39 faults/rep |
| Goose `bin/p3` | 86.7-98.5 ms (bench 16:22Z), 26,731 faults/rep | 39.3-43.2 ms, 258-457 faults/rep |
| Goose `bin/p3par` 8 workers, image distributed | 32.8-40.0 ms, 26,736 faults/rep in workers | 17.4-23.5 ms, 824-1,336 faults/rep |

Goose stays 1.6x C at either page size: `img.resize(n, 0)` writes 109 MB of zeros C's mmap gets free, and `put` stores a
relocation byte by byte (the language has no unaligned u32/u64 store into a u8 array).

## The cells

All checks: `systems/goose/bin/<cell> /home/exedev/paradigms/data/minio [8]` prints `P1 OK` / `P2 OK` / `P3 OK`
(`P3 pipeline OK` with `--pipeline`); each compares its output arrays with the oracle files element by element
(`check32` in flat.goose; the image symbol by symbol against oracle/p3_image.bin). Lines are whole files (flat.goose 150
and par.goose 36 shared; flatio.c 102 is the C escape: file reads, page remapping, getrusage).

| cell | 1 thread warm | 8 workers warm | 1 / 8 e2e | lines |
|---|---|---|---|---|
| 1a join by name | 79-115 (first rep 213) | 37-57 | 1a+1b+1c: 852-1066 / 518-687 | p1 195, p1par 411 |
| 1b relocation targets | 39-57 | 42-67 (incl. sending 9.4 M targets to main) | (above) | |
| 1c content-hash join | 97-118 | 39-47 | (above) | |
| 2 reach, CSR flood | 41-49 (p2par W=1; p2 single run 53) | 22-32 | 205-269 / 225-316 | p2 187, p2par 241 + p2rules 341 |
| 2' reach, lazy decode | 74-79 | 37-49 | 731-859 / 411-807 | p2lazy 286, p2par lazy mode |
| 3a layout | 15-26 | 17-31 | 3a+3b: 338-392 / 595-730 | p3 251, p3par 386 |
| 3b relocate, 4 KB | 87-99 | 33-40 distributed; +142-171 gathered | (above) | |
| 3b relocate, 2 MB | 41-42 | 17-24 distributed | | |

e2e is one process with one rep, wall from `date +%s%N` around it, including the oracle self-check's reads (~10-20%).
3a on 8 workers is no faster than 1 thread: main's serial scan over 366 k (unit, bucket, class) counts and the typelink
sort, plus three message rounds, are most of it.

### Per cell: easy / forced / impossible

- **1a** easy: open-addressing table of inline `Group {Key, val}` structs, winner rule as `rank << 32 | ~id` max. Forced:
  on 8 workers a shuffle join; a worker's inbox must be a distinct type (`Mail<u8[j+1], M>`, an 8-arm match) because
  queues are per type. Impossible: one shared table (no shared memory).
- **1b** easy: a gather per relocation through the object's package slice. Forced: on 8, an allgather of canon (every
  worker gets all 21 MB) and shipping targets to main. Impossible: nothing.
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
  batched inboxes and BSP; the lazy variant). Reason: per-superstep message rounds; Goose has no shared-memory
  `par for` (a design note only).

### Core code excerpt (p2lazy.goose, the lazy part of the flood)

```
        if decodes(c) {
            // the lazy part: this symbol's relocations, decoded now
            var skip = 0;     // type 26 hides the next two relocations
            for r in slot_rel[c]..slot_rel[c as i64 + 1] {
                if skip > 0 { skip--; continue; }
                let ty = rel_type[r];
                if ty == 26 { skip = 2; continue; }
                if plain(ty) {
                    let t = target(c, r);
                    if t != 0 { plain_edge(t as Sym, uii, work); }
                }
            }
        }
```

and 3b's loop (p3.goose): `img[base..base + bytes.len].move_from(bytes);` then per relocation
`match rkind[r] { 1, 2 => { v = ta + radd[r]; } 3, 4 => { ... v = (ta - sect[k]) + radd[r]; } 5, 6, 7 => { v = ta + radd[r] - (P + sz); } ... }`
and `put(img, base + off, v, sz)`.

## Install, status, hello-world, judgment (installer, 15:24Z)

Measured 2026-11-16 (`date -u`: Mon Nov 16 15:24 UTC 2026), Linux x86_64, 8 cores, gcc 13.3.0, glibc 2.39.

## What Goose is

A new (2026) memory-safe systems language by Wouter van Oortmerssen (Lobster's author). Site
https://aardappel.github.io/goose/ , repo https://github.com/aardappel/goose (Apache 2.0). Design: no heap, no GC, no RC,
no destructors, no lifetime annotations. Dynamic data lives on N compiler-assigned "data stacks" (bump pointers,
freed at scope exit with one store; at most one resizable value live per stack, always on top, so growth never moves
and references stay valid). Every reference has a static root checked by the compiler. Nested data is inline (arrays
of strings are one block; variable-size enums). Narrow links: `T&<u32>` self-relative, `T&<u32 in pool>`, and index
types `u32 in arr` (validated once, indexing then needs no check; `checkfree fn` enforces zero bounds checks).
Threads: each `thread_fn` is a separate program with its own memory; flat values move through typed queues by memcpy.
Compiles to one C file (whole-program, monomorphizing); `goose run` JITs it with built-in TinyCC, `goose build` calls a
system C compiler (gcc here, -O2).

The README states it was "produced almost entirely with AI, from a human language design" (compiler implementation by
Claude Fable; design by the author). About 30 k lines of C++ is the site's claim; the checked-out `src/*.h|cpp`
is 74 k lines.

## Is there a compiler / interpreter / transpiler

- A compiler that transpiles Goose to C (`goose -o out.c file.goose`) and then either
  - runs it in-process through TinyCC (`goose file.goose`, no optimization, no threads, extern C only for `<math.h>`), or
  - builds an executable with the machine's C compiler (`goose build file.goose -o exe`, `-O2`).
- No interpreter, no LLVM, no direct native backend. Also: formatter, LSP, package fetcher (git tags), `checkfree` and `--bce-lines` diagnostics.

## Install (what I did)

Exact commands (also `systems/goose/hello/build.sh`, `build_from_source.sh`):

```
mkdir -p /home/exedev/tools/goose && cd /home/exedev/tools/goose
curl -sL -o rel.tgz https://github.com/aardappel/goose/releases/download/v2026.4/goose-v2026.4-linux-x64.tar.gz
mkdir rel && tar xzf rel.tgz -C rel          # -> rel/goose-v2026.4-linux-x64/goose
git clone https://github.com/aardappel/goose src   # HEAD 3a0aa2eb (2026-11-15), docs and benchmarks read from here
```

- Binary used: release v2026.4 (published 2026-11-06), `goose v2026.4 (runtime dce889723a23)`, linux-x64 (needs glibc >= 2.35;
  this VM has 2.39). `goose toolchain` finds gcc 13.3.0. Release bundles gfx/audio/ui/physics.
- Source build: needs cmake >= 3.27 (not installed on the VM). I used a pip venv (`cmakevenv`, cmake 4.4.5) and
  `-DGOOSE_BUILTIN_LIBS=none`. See "Source build" below for the result.
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

exec
/bin/bash -lc "rg --files systems; cat ref/p4/check.sh; cat LOOP.md; rg -n 'session|time left|usage|budget|defer|remaining|skip|toy' JOURNAL.md systems/*/NOTES.md" in /home/exedev/paradigms-w
 succeeded in 0ms:
systems/futhark/drv.h
systems/futhark/fbin.py
systems/futhark/p1.c
systems/futhark/IDEAS.md
systems/futhark/prof.py
systems/futhark/msort.fut
systems/futhark/p1.fut
systems/futhark/p3.fut
systems/futhark/hello/hello.fut
systems/futhark/hello/gen.c
systems/futhark/hello/driver.c
systems/futhark/hello/run.sh
systems/futhark/hello/build.sh
systems/futhark/CELLS.md
systems/futhark/p2.c
systems/futhark/build.sh
systems/futhark/NOTES.md
systems/futhark/bench.sh
systems/futhark/p3.c
systems/futhark/p2.fut
systems/futhark/lib/sorts/merge_sort.fut
systems/futhark/lib/sorts/radix_sort.fut
systems/futhark/lib/segmented/segmented.fut
systems/goose/p1.build
systems/goose/flatio.c
systems/goose/IDEAS.md
systems/goose/p1par.build
systems/goose/scratch/loadtest.build
systems/goose/scratch/mail.goose
systems/goose/scratch/thr.goose
systems/goose/scratch/loadtest.goose
systems/goose/par.goose
systems/souffle/bench-20261116T1637Z.txt
systems/souffle/resolve_b.dl
systems/souffle/IDEAS.md
systems/souffle/best.sh
systems/souffle/resolve_c2.dl
systems/souffle/check_p3.c
systems/goose/p2.build
systems/goose/p3par.build
systems/goose/p1par.goose
systems/goose/p2lazy.goose
systems/goose/p2par.goose
systems/goose/bench.sh
systems/goose/flat.goose
systems/goose/p2lazy.build
systems/goose/p2par.build
systems/goose/p3par.goose
systems/goose/CELLS.md
systems/goose/p2.goose
systems/goose/NOTES.md
systems/goose/p3.goose
systems/goose/p2rules.goose
systems/goose/p1.goose
systems/goose/p3.build
systems/goose/hello/run.sh
systems/goose/hello/build.sh
systems/goose/hello/build_from_source.sh
systems/goose/hello/hello1.goose
systems/goose/hello/c_ref.c
systems/goose/hello/hello8.goose
systems/goose/hello/gen.c
systems/souffle/reach_lazy.dl
systems/souffle/reach_literal.dl
systems/souffle/reach.dl
systems/souffle/flatio.h
systems/souffle/relocate.dl
systems/souffle/bench.sh
systems/souffle/layout.dl
systems/souffle/build.sh
systems/souffle/prep_lazy.c
systems/souffle/NOTES.md
systems/souffle/CELLS.md
systems/souffle/resolve_c.dl
systems/souffle/resolve_a.dl
systems/duckdb/IDEAS.md
systems/duckdb/CELLS.md
systems/duckdb/build.sh
systems/duckdb/NOTES.md
systems/duckdb/dk.c
systems/duckdb/results-bench-20261116T1649Z.txt
systems/duckdb/check3b.py
systems/duckdb/bench.sh
systems/halide/IDEAS.md
systems/souffle/hello/reach_c.cpp
systems/souffle/hello/load_c.cpp
systems/souffle/hello/gen.cpp
systems/souffle/hello/run.sh
systems/souffle/hello/build.sh
systems/souffle/hello/load.dl
systems/souffle/hello/reach.dl
systems/souffle/hello/reach_emb.dl
systems/souffle/hello/embed.cpp
systems/duckdb/sql/p2lazy.sql
systems/duckdb/sql/p1.sql
systems/duckdb/sql/p2prep.sql
systems/duckdb/sql/load3.sql
systems/duckdb/sql/x_plainclosure.sql
systems/duckdb/sql/load3b.sql
systems/duckdb/sql/p3link.sql
systems/duckdb/sql/p2eager.sql
systems/duckdb/sql/load2.sql
systems/duckdb/sql/load3b_iso.sql
systems/duckdb/sql/p3a.sql
systems/duckdb/sql/p3b.sql
systems/duckdb/sql/load1.sql
systems/duckdb/sql/p2aux.sql
systems/halide/bench.sh
systems/duckdb/sql/p2loop.sql
systems/halide/NOTES.md
systems/halide/CELLS.md
systems/halide/hello/mkdata.cpp
systems/halide/hello/driver.cpp
systems/halide/hello/gen.cpp
systems/halide/hello/run.sh
systems/halide/hello/build.sh
systems/halide/cells/gen_p3.cpp
systems/halide/cells/p2.cpp
systems/halide/cells/build.sh
systems/halide/cells/bench.h
systems/halide/cells/gen_p1.cpp
systems/halide/cells/p3.cpp
systems/halide/cells/gen_p2.cpp
systems/halide/cells/p1.cpp
systems/halide/cells/join.h
#!/usr/bin/env bash
# Build the plain C reference for problem 4 and check it against the oracle:  ref/p4/check.sh [data/minio-p4]
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd); d=${1:-/home/exedev/paradigms/data/minio-p4}
mkdir -p "$here/build"
gcc -O2 -march=native -o "$here/build/p4" "$here/ref/p4/p4.c"
prlimit --as=12884901888 "$here/build/p4" "$d"
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
systems/souffle/NOTES.md:98:Found and checked: hlink's walk skips the two relocations after an R_METHODOFF (`r += 2`); on MinIO all 225,760
systems/souffle/NOTES.md:99:skipped ones are themselves R_METHODOFF (prep_lazy checks this), so "plain" is a filter on the type and needs no
systems/souffle/NOTES.md:194:   `-D__EMBEDDED_SOUFFLE__` (skips the generated main). Public `Relation::insert` = 4.8 s (virtual call + tuple
systems/souffle/NOTES.md:204:   skip the relation entirely but gives up Soufflé's joins.
systems/souffle/NOTES.md:208:  dummy `.input ... IO=file` and call `runAll("", "", /*performIO=*/false, false)` to skip file IO.
systems/goose/NOTES.md:55:getrusage. Interleaved 16:24-16:25Z (load ~8), 1 thread P3_REPS=20 min per run:
systems/goose/NOTES.md:71:and par.goose 36 shared; flatio.c 102 is the C escape: file reads, page remapping, getrusage).
systems/goose/NOTES.md:121:            var skip = 0;     // type 26 hides the next two relocations
systems/goose/NOTES.md:123:                if skip > 0 { skip--; continue; }
systems/goose/NOTES.md:125:                if ty == 26 { skip = 2; continue; }
systems/halide/NOTES.md:90:and hands it to the pipeline. Fresh output per run, faults inside the timed region (`p3 DATA 3b`, getrusage):
systems/halide/NOTES.md:129:### 3a lay out (`Layout` in gen_p3.cpp, 154 lines; from the earlier session)
systems/futhark/NOTES.md:85:  materialized. The R_USETYPE skip rule (type 26 skips the next two) becomes a segmented scan of a 3-state
systems/futhark/NOTES.md:129:  the expand of rows each level, dedup by scatter-the-index-and-check; the R_USETYPE skip rule as a segmented scan
systems/futhark/NOTES.md:249:- `futhark_new_u32_1d` copies (no zero-copy wrap of a host pointer on c/multicore); budget the copy

exec
/bin/bash -lc 'cat systems/souffle/check_p3.c; cat systems/{souffle,duckdb,halide}/CELLS.md; cat ref/p4/p4.c' in /home/exedev/paradigms-w
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
# Soufflé cells (MinIO; for GRID.md)

Times: ms, min / median / max of 5 interleaved runs, 2026-11-16T16:37Z, load 2.9-6.2 (`systems/souffle/bench.sh`).
warm = rules only (inputs bulk-loaded into B-trees); e2e = process. 8 threads with `OMP_WAIT_POLICY=passive`.
Checks: NOTES.md "Checks" (cmp against the oracles / check_p3 -> P3 OK). Lines: rules + decls without comments;
flatio.h (385 lines C++, I/O only) shared.

**1a join by name** — correct (`cmp out/p1_canon.u32 oracle/p1_canon.u32`, builtin too) · 1 thr warm 1928-2034,
e2e 3136-3295 · 8 thr warm 636-754, e2e 1722-1888 · 37 lines · easy: groups + max/min aggregates; forced: nothing.

**1b relocation targets** — correct (`cmp p1_reltarget.u32`) · 1 thr warm 5220-5426, e2e 7371-7657 · 8 thr warm
1080-1301, e2e 2669-2972 · 25 lines · easy: package-table gather as a join; forced: count aggregate for npkg,
negation for "no target -> 0"; slow (9.4 M x 4 B-tree joins), ~120x plain C.

**1c content-hash join** — correct (`cmp p1c_canon.u32`) · copy relations: 1 thr warm 1625-1865, e2e 2805-3127 ·
8 thr warm 587-718, e2e 1566-1761; rules over the slot columns: warm 1013-1069 / 394-437, e2e 3086-3256 / 1936-2107 ·
26 / 23 lines · easy: winner order as aggregates; forced: each group its own relation before an aggregate.

**2 reach** — correct (`cmp p2_live.u32`) · 1 thr warm 1227-1467, e2e 2571-2921 · 8 thr warm 431-579, e2e 1242-1429
· 47 lines · easy: every deadcode.go rule is one Datalog rule, incl. the conditional method rule; forced: static
joins hoisted out of the flood. ~30x C.

**2' reach with its own relocation decode (ATTACKS 1)** — correct · lazy (decode in the flood rule): 1 thr warm
2905-3214, e2e 4637-4982 · 8 thr warm 960-1610, e2e 2312-3081 · 65 lines · eager in Soufflé = 1b + 2: warm
~6.5 s / ~1.6 s. Lazy is expressible (semi-naive evaluation decodes only live sources: 1.73 M of 9.4 M) and beats
Soufflé's eager shape ~2x, but is ~30x / ~35-50x the eager C pipeline (108.7 / 30.1); and the relocation columns are
still bulk-loaded into a B-tree (the engine joins only over indexes).

**3a layout** — correct (`check_p3 $D oracle/p3_image.bin out/p3_addr.u32` -> 0 differ) · 1 thr warm 7222-7589,
e2e 7367-7717 · 8 thr warm 1339-1784, e2e 1474-1933 · 45 lines · forced: running sums as range aggregates over
earlier group members, unit walk as 1,763-iteration recursion, typelink sort as O(n²) "sum of sizes before me"
(numeric 8-byte prefix + string compare: 28 s -> 7.2 s); **falls down on speed** (~500x C; no sort/scan in Datalog).

**3b relocate** — correct (`check_p3 $D out/p3_image.out` -> P3 OK) · rules: 1 thr warm 1054-1115, e2e 2086-2139 ·
8 thr warm 320-451, e2e 1205-1374 · image assembly (C writer): 54-56 / 18-29 ms on 4 KB pages; on 2 MB pages
(ATTACKS 2) 20.0-34.7 / 7.8-16.2 ms, 39 / 150-172 minor faults vs 19,108 · 31 lines · easy: one rule per kind;
forced: bytes (copy + patch) in C; `site` inlined (5.2 s -> 2.5 s at load 11).
# DuckDB cells (for GRID.md)

MinIO. bench.sh 2026-11-16T16:49Z-17:09Z, load 3-6, five interleaved rounds; ranges min-median-max ms. Default
scheduler (no spin/passive knob). Check commands: NOTES.md "Oracle checks".

**1a. join by name** — correct (`cmp out/p1_canon.u32`, `p1_builtin.u32` vs oracle) · warm 1 thr 319-337-355 /
8 thr **103-107-113**; e2e (all of P1) 4188-4228 / 1112-1122 · 18 lines SQL · easy: GROUP BY max over a UNION ALL,
SEMI JOIN for candidates; the 128-bit key is a HUGEINT via Arrow decimal128 · forced: nothing · impossible: nothing.

**1b. relocation targets** — correct (`cmp out/p1_reltarget.u32`) · warm 1405-1449-1470 / **272-314-375** · 17 lines ·
easy: the CSRs as Arrow list columns, `unnest` · forced: an equi-join written as `ON bc.i = CASE ... END` (the
natural `ON pkg = B AND i = idx` LEFT JOIN plans as a nested-loop join, 6.8 s CPU); an as-of join version was 6.4 s
· impossible: an array lookup by dense id (every lookup is a hash join, ~50x C).

**1c. content-hash join** — correct (`cmp out/p1c_canon.u32`) · warm 563-595-632 / **128-165-190** · 16 lines ·
easy: `arg_min(struct, (mark, -size, id))` is the winner rule · forced/impossible: nothing.

**2. reach** — correct (`cmp out/p2_live.u32`; also from relocations, eager and lazy) · warm prep + loop
207 + 2915-3206-3492 / **103 + 1965-2273-2354**; e2e 3463-3625 / 2367-2554 · 48 + 8 lines · easy: each rule is one
INSERT ... ANTI JOIN · forced: the loop (45 rounds) is driven from outside (dk `.repeat/.until`); flag tests as
small relations · impossible: a fast round (no index persists across statements: ~25 statements x 1-10 ms per
round); a recursive CTE (the method rules need global sets). ~150x tuned C. ATTACKS 1: eager decode+flood ~3.56 s,
lazy 3.44-3.54-3.69 s on 8 threads (tie; 1.2x on 1 thread): SQL expresses decode-on-reach but each round's decode
is a hash join against all of canon, so it does not pay; ~100x the eager C pipeline (30.1 ms).

**3a. lay out** — correct (`cmp out/p3_addr.u32`) · warm 304-315-322 / **217-246-263**; e2e 408-415 / 325-335 ·
35 lines · easy: occupied sizes and in-class positions are window sums · forced: the unit chain
(round up, then add) is a fold: `list_reduce` with a list accumulator (65-95 ms; a recursive CTE was 340-620 ms, a
padding fixpoint 1,121 rounds); typelinks by the padding fixpoint (1 round on MinIO) · impossible: nothing.

**3b. relocate** — correct (`check3b.py` -> P3b OK; also as P1 -> 3a -> 3b pipeline from DuckDB's own canon and
addresses, e2e 5962-6138 / 2017-2090) · warm 789-827-1142 / **339-362-368** on 4 KB pages, 813-832 / **309-321-332**
on 2 MB pages; e2e 1583-1637 / 687-750 · 32 lines · easy: every write as rows (pos, bytes) / (pos, width, value),
one CASE over the kinds · forced: the store at byte offset is dk's (a memcpy per row) · impossible: DuckDB's own
buffers on huge pages; ATTACKS 2: the image's faults drop 21,870 -> 4,101, ~10% at 8 threads, because the
target-address hash join, not the faults, is the cost.
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
// p4.c: problem 4 (code generation by shape) from the flat files, plainly, on one core: every function body
// fc's tree walker wrote, rebuilt from byte templates and operands. Checks itself against oracle/p4_code.bin
// (byte for byte) and the relocations against oracle/p4_rel_* (as a sorted list).
//   ref/p4/p4 data/minio-p4 [out.bin]
#include "../flat/flat.h"

enum { encInt = 1, encRel32 = 2, encReloc = 3 };

typedef struct {
	u64 off;
	i64 add;
	u32 sym;
	u16 type;
	u8 siz;
} Rel;
static int cmpRel(const void *a, const void *b) {
	const Rel *x = a, *y = b;
	if (x->off != y->off) return x->off < y->off ? -1 : 1;
	if (x->type != y->type) return x->type < y->type ? -1 : 1;
	if (x->sym != y->sym) return x->sym < y->sym ? -1 : 1;
	if (x->add != y->add) return x->add < y->add ? -1 : 1;
	return (int)x->siz - (int)y->siz;
}

int main(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "data/minio-p4";
	u32 nfn = (u32)metaGet("nfn"), nitem = (u32)metaGet("nitem"), nrel = (u32)metaGet("nrel");
	u64 ncode = metaGet("code_bytes");
	const u32 *fnItem = FMAP(u32, "fn_item.u32", NULL), *fnMarkOff = FMAP(u32, "fn_mark_off.u32", NULL);
	const u32 *markOff = FMAP(u32, "mark_off.u32", NULL), *markSym = FMAP(u32, "mark_sym.u32", NULL);
	const u16 *markType = FMAP(u16, "mark_type.u16", NULL);
	const i64 *markAdd = FMAP(i64, "mark_add.i64", NULL);
	const u8 *itemKind = FMAP(u8, "item_kind.u8", NULL);
	const u32 *itemRef = FMAP(u32, "item_ref.u32", NULL), *itemSeg = FMAP(u32, "item_seg.u32", NULL);
	const u64 *glueOff = FMAP(u64, "glue.off.u64", NULL);
	const u8 *glueBin = FMAP(u8, "glue.bin", NULL);
	const u32 *glueRelOff = FMAP(u32, "glue_rel_off.u32", NULL), *grelOff = FMAP(u32, "grel_off.u32", NULL);
	const u8 *grelSiz = FMAP(u8, "grel_siz.u8", NULL);
	const u16 *grelType = FMAP(u16, "grel_type.u16", NULL);
	const u32 *grelSym = FMAP(u32, "grel_sym.u32", NULL);
	const i64 *grelAdd = FMAP(i64, "grel_add.i64", NULL);
	const u32 *nodeTmpl = FMAP(u32, "node_tmpl.u32", NULL), *nodeOp = FMAP(u32, "node_op.u32", NULL);
	const i64 *opVal = FMAP(i64, "op_val.i64", NULL), *opAux = FMAP(i64, "op_aux.i64", NULL);
	const u64 *tmplOff = FMAP(u64, "tmpl.off.u64", NULL);
	const u8 *tmplBin = FMAP(u8, "tmpl.bin", NULL);
	const u32 *tmplSegOff = FMAP(u32, "tmpl_seg_off.u32", NULL), *tsegEnd = FMAP(u32, "tseg_end.u32", NULL);
	const u32 *tmplHoleOff = FMAP(u32, "tmpl_hole_off.u32", NULL), *holeAt = FMAP(u32, "hole_at.u32", NULL);
	const u8 *holeW = FMAP(u8, "hole_w.u8", NULL), *holeEnc = FMAP(u8, "hole_enc.u8", NULL);
	const u16 *holeRtype = FMAP(u16, "hole_rtype.u16", NULL);

	// ---- 1. the length of every item (a glue run, or one segment of a node's template), then where it goes
	double t0 = now();
	u64 *at = malloc(8 * ((size_t)nitem + 1));
	u64 pos = 0;
	for (u32 i = 0; i < nitem; i++) {
		at[i] = pos;
		u32 r = itemRef[i];
		if (itemKind[i] == 0) {
			pos += glueOff[r + 1] - glueOff[r];
		} else {
			u32 t = nodeTmpl[r], s = itemSeg[i], b = tmplSegOff[t];
			pos += tsegEnd[b + s] - (s ? tsegEnd[b + s - 1] : 0);
		}
	}
	at[nitem] = pos;
	double t1 = now();

	// ---- 2. emit: copy each item's bytes, patch the holes of node segments
	u8 *code = malloc(pos ? pos : 1);
	Rel *rel = malloc(sizeof(Rel) * ((size_t)nrel + 1));
	u64 nr = 0, nerr = 0;
	for (u32 f = 0; f < nfn; f++) {
		u64 fstart = at[fnItem[f]];
		for (u32 m = fnMarkOff[f]; m < fnMarkOff[f + 1]; m++)
			rel[nr++] = (Rel){fstart + markOff[m], markAdd[m], markSym[m], markType[m], 0};
		for (u32 i = fnItem[f]; i < fnItem[f + 1]; i++) {
			u32 r = itemRef[i];
			u8 *P = code + at[i];
			if (itemKind[i] == 0) {
				memcpy(P, glueBin + glueOff[r], glueOff[r + 1] - glueOff[r]);
				for (u32 k = glueRelOff[r]; k < glueRelOff[r + 1]; k++)
					rel[nr++] = (Rel){at[i] + grelOff[k], grelAdd[k], grelSym[k], grelType[k], grelSiz[k]};
				continue;
			}
			u32 t = nodeTmpl[r], s = itemSeg[i], b = tmplSegOff[t];
			u32 lo = s ? tsegEnd[b + s - 1] : 0, hi = tsegEnd[b + s];
			memcpy(P, tmplBin + tmplOff[t] + lo, hi - lo);
			// the template's holes in [lo, hi): they are sorted by offset
			u32 h0 = tmplHoleOff[t], h1 = tmplHoleOff[t + 1];
			while (h0 < h1) {
				u32 mid = (h0 + h1) / 2;
				if (holeAt[mid] < lo) h0 = mid + 1; else h1 = mid;
			}
			for (u32 h = h0; h < tmplHoleOff[t + 1] && holeAt[h] < hi; h++) {
				u32 o = nodeOp[r] + (h - tmplHoleOff[t]);
				u64 hp = at[i] + holeAt[h] - lo;
				i64 v = opVal[o];
				switch (holeEnc[h]) {
				case encInt:
					switch (holeW[h]) {
					case 1: code[hp] = (u8)v; break;
					case 2: { u16 x = (u16)v; memcpy(code + hp, &x, 2); break; }
					case 4: { u32 x = (u32)v; memcpy(code + hp, &x, 4); break; }
					case 8: memcpy(code + hp, &v, 8); break;
					default: nerr++;
					}
					break;
				case encRel32: { // a jump to (item, offset) of the same function
					i32 x = (i32)((i64)(at[v] + opAux[o]) - (i64)(hp + 4));
					memcpy(code + hp, &x, 4);
					break;
				}
				case encReloc:
					if (nr < nrel) rel[nr] = (Rel){hp, opAux[o], (u32)v, holeRtype[h], holeW[h]};
					nr++;
					break;
				default: nerr++;
				}
			}
		}
	}
	double t2 = now();
	qsort(rel, nr < nrel ? nr : nrel, sizeof(Rel), cmpRel);
	double t3 = now();
	printf("p4 (1 core): lay out %.1f ms, emit %.1f ms (%llu bytes, %u items), sort relocations %.1f ms (%llu); %llu errors\n",
	       (t1 - t0) * 1e3, (t2 - t1) * 1e3, (unsigned long long)pos, nitem, (t3 - t2) * 1e3, (unsigned long long)nr,
	       (unsigned long long)nerr);

	// ---- check
	long bad = nerr != 0;
	size_t m;
	const u8 *want = FMAP(u8, "oracle/p4_code.bin", &m);
	const u64 *wfn = FMAP(u64, "oracle/p4_fn_off.u64", NULL);
	if (m != pos || pos != ncode) {
		printf("code: %llu bytes, oracle %zu\n", (unsigned long long)pos, m);
		bad++;
	} else if (memcmp(code, want, pos) != 0) {
		long nf = 0;
		for (u32 f = 0; f < nfn; f++)
			if (at[fnItem[f]] != wfn[f] || memcmp(code + wfn[f], want + wfn[f], wfn[f + 1] - wfn[f]) != 0)
				if (nf++ < 5) printf("  function %u differs\n", f);
		printf("code: %ld functions differ\n", nf);
		bad++;
	}
	printf("code: %llu bytes in %u functions, %s oracle/p4_code.bin\n", (unsigned long long)pos, nfn, bad ? "NOT equal to" : "equal to");
	const u64 *wo = FMAP(u64, "oracle/p4_rel_off.u64", NULL);
	const u8 *wsiz = FMAP(u8, "oracle/p4_rel_siz.u8", NULL);
	const u16 *wtype = FMAP(u16, "oracle/p4_rel_type.u16", NULL);
	const u32 *wsym = FMAP(u32, "oracle/p4_rel_sym.u32", NULL);
	const i64 *wadd = FMAP(i64, "oracle/p4_rel_add.i64", NULL);
	Rel *w = malloc(sizeof(Rel) * ((size_t)nrel + 1));
	for (u32 i = 0; i < nrel; i++) w[i] = (Rel){wo[i], wadd[i], wsym[i], wtype[i], wsiz[i]};
	qsort(w, nrel, sizeof(Rel), cmpRel);
	long rbad = nr != nrel;
	for (u32 i = 0; i < nrel && i < nr; i++)
		if (cmpRel(&rel[i], &w[i]) != 0 && rbad++ < 5) printf("  relocation %u differs (offset %llu)\n", i, (unsigned long long)rel[i].off);
	printf("relocations: %llu, oracle %u, %ld differ\n", (unsigned long long)nr, nrel, rbad);
	bad += rbad;
	if (argc > 2) {
		FILE *f = fopen(argv[2], "wb");
		fwrite(code, 1, pos, f);
		fclose(f);
	}
	printf(bad ? "P4 FAIL\n" : "P4 OK\n");
	return bad != 0;
}

exec
/bin/bash -lc "rg -n 'check|oracle|OK|memcmp' systems/halide/cells/p3.cpp systems/souffle/flatio.h systems/duckdb/dk.c; ls build /home/exedev/paradigms/build; sed -n '100,155p' systems/souffle/NOTES.md; cat systems/souffle/layout.dl" in /home/exedev/paradigms-w
 succeeded in 0ms:
systems/souffle/flatio.h:290:// oracle/p3_addr.u32; not buckets 13-15), write each patch's low `width` bytes little-endian, write NAME.
systems/souffle/flatio.h:298:	std::string an = opt(d, "addr").empty() ? "oracle/p3_addr.u32" : opt(d, "addr");
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
systems/halide/cells/p3.cpp:153:    printf(bad ? "P3 FAIL\n" : "P3 OK\n");
/home/exedev/paradigms/build:
hlink
p1
p1compact
p1onlyA
p1onlyB
p1par
p2
p2async
p2lazy
p2par
p2pipe
p3
p3a_par
p3par
stats
ls: cannot access 'build': No such file or directory
walk. Forced: the aux edges (funcdata) come pre-split from p2_edge, as in p2lazy.

**3a layout** (layout.dl 45 [66]). Datalog has no scan or sort. Forced: (1) every running sum is an aggregate over
the earlier members of a group (`sum o : { occ(b, c, u, j, o), j < i }`, a range scan, O(n²) per group but fast,
56 ms); (2) the walk over units is a recursion, one unit per iteration (1,763 iterations, ~0.25 s); (3) typelinks:
the sort by type string is "the sum of the sizes of the typelinks before me", O(n²) comparisons. Attempt 1 compared
symbols (Soufflé compares symbols as strings): 28 s / 5.0 s. Attempt 2 adds an 8-byte big-endian prefix of each
type string as a number (loader column `p3_str.key8`) so most pairs are a numeric range: 7.2 s / 1.3 s, still 95% of
the cell. Also forced: the typelink round-up only commutes with the sum because every MinIO typelink is class 3 with
a size that is a multiple of 8 (checked by the rule `tlcheck`, empty). Verdict: correct but ~500x C: **falls down on
speed** (two attempts; the O(n²) "rank by counting" is what Datalog without sort has).

**3b relocate** (relocate.dl 31 [40]). Easy: each relocation kind is one or two rules; the section base is a `max`
aggregate over 8 values; errors are a relation. Forced: the image itself (bytes) is assembled by flatio's writer in
C (`image=` directive): copy each symbol's bytes, write each patch's low bytes. Attempt 1 materialized `site`
(rel x addr x target address): 5.2 s / 1.5 s; attempt 2 inlines it (`.decl site(...) inline`): 2.5 s / 0.74 s at
load 11 (1.05 / 0.32 s at load 3).

## Code excerpts

1c, the content-hash join straight over the slot columns (resolve_c2.dl):
```
group2(k1) :- slot(_, 2, 1, k1, _).
size2(k1, s) :- group2(k1), s = max ss : { slot(_, 2, 1, k1, ss) }.
win2(k1, w, s) :- size2(k1, s), w = min g : { slot(g, 2, 1, k1, s) }.
moved(g, w) :- win2(k1, w, _), slot(g, 2, 1, k1, _), g != w.
moved(g, w) :- win2(k1, w, ws), slot(g, 2, 2, k1, s), s <= ws.
final(g, c) :- canon(g, c), !moved(g, _).
```
2', decoding a relocation when the flood reaches its symbol (reach_lazy.dl):
```
plain(s, t) :- live(s), source(s), rel(s, p, i, ty), plainty(ty), p < 2147483643,
               slotobj(s, o), objpkg(o, p, b), b != 0xFFFFFFFF, canon(b + i, t), t != 0.
live(t)    :- plain(_, t).
iface(t)   :- iface(s), gotype(s), plain(s, t), gotype(t).
```
3a, a symbol's address (layout.dl):
```
va(b, u + 1, x + n) :- ubase(b, u, x), ub(u, b, _, n), meta("nunits", nu), u + 1 < nu.
cbase(b, c, u, x + y) :- ubc(u, b, c, _), ubase(b, u, x), y = sum z : { ubc(u, b, c2, z), c2 > c }.
addr(i, x + y) :- occ(b, c, u, i, _), cbase(b, c, u, x), y = sum o : { occ(b, c, u, j, o), j < i }.
```

## Files

`*.dl` the cells; `flatio.h` the binary column reader/writer (inputs `IO=flat cols=... [csr=] [index=|pos=]`,
`.str`/`.key8` string-table columns, `meta=true`; outputs `cols=`, `image=`, `exitafter=`); `build.sh`;
`bench.sh`; `check_p3.c`; `prep_lazy.c`; `best.sh` (older single-cell timer).

## Install, hello-world, timings, gotchas (installer notes)

Measured 2026-11-16 15:27 UTC (`date -u`), Ubuntu 24.04.5, 8 cores, g++ 13.3. Everything reproducible with
`hello/build.sh` then `hello/run.sh`.

### Install (done)

// Problem 3a, layout (ref/flat/README.md): every placed symbol's address. Datalog has no scan, so every running
// sum is an aggregate over the earlier members of its group, and the walk over units is a recursion, one unit
// per iteration.

.decl meta(k: symbol, v: unsigned)
.input meta(IO=flat, meta="true")
.decl sym(i: unsigned, b: unsigned, c: unsigned, size: unsigned, u: unsigned)   // placed symbol i, in list order
.input sym(IO=flat, index="true", cols="p3_bucket.u8,p3_class.u8,p3_size.u32,p3_unit.u32")
.decl id(i: unsigned, g: unsigned)
.input id(IO=flat, index="true", cols="p3_sym.u32")
.decl tlstr(i: unsigned, s: unsigned)
.input tlstr(IO=flat, index="true", cols="p3_tlstr.u32")
.decl str(s: unsigned, k: unsigned, t: symbol)         // a type string: its first 8 bytes as a number, itself
.input str(IO=flat, index="true", cols="p3_str.key8,p3_str.str")
.decl bstart(b: unsigned, a: unsigned)
.input bstart(IO=flat, index="true", cols="p3_bucket_start.u32")
.decl bmin(b: unsigned, a: unsigned)
.input bmin(IO=flat, index="true", cols="p3_bucket_minalign.u32")

#define RND(v, a) (((v) + (a) - 1) band bnot ((a) - 1))

// occupied size: rounded up to the alignment; text at least minFunc
.decl occ(b: unsigned, c: unsigned, u: unsigned, i: unsigned, o: unsigned)
occ(b, c, u, i, RND(s, 1 bshl c)) :- sym(i, b, c, z, u), b != 6, b != 1, b != 2, s = z.
occ(b, c, u, i, RND(s, 1 bshl c)) :- sym(i, b, c, z, u), (b = 1 ; b = 2), meta("minFunc", m), s = max(z, m).

// bytes per (unit, bucket, class); per (unit, bucket) with bytes: the total and the largest class with bytes
.decl ubc(u: unsigned, b: unsigned, c: unsigned, n: unsigned)
ubc(u, b, c, n) :- occ(b, c, u, _, _), n = sum o : { occ(b, c, u, _, o) }.
.decl ub(u: unsigned, b: unsigned, top: unsigned, n: unsigned)
ub(u, b, t, n) :- ubc(u, b, _, z), z > 0, t = max c : { ubc(u, b, c, zz), zz > 0 }, n = sum zz : { ubc(u, b, _, zz) }.

// the walk over the units of each bucket: va(b, u, a) is the address before unit u
.decl va(b: unsigned, u: unsigned, a: unsigned)
.decl ubase(b: unsigned, u: unsigned, a: unsigned)
va(b, 0, a) :- bstart(b, a), b >= 1, b != 6.
ubase(b, u, RND(v, al)) :- va(b, u, v), ub(u, b, t, _), bmin(b, m), al = max(m, 1 bshl t).
ubase(b, u, v) :- va(b, u, v), !ub(u, b, _, _).
va(b, u + 1, x + n) :- ubase(b, u, x), ub(u, b, _, n), meta("nunits", nu), u + 1 < nu.
va(b, u + 1, x) :- ubase(b, u, x), !ub(u, b, _, _), meta("nunits", nu), u + 1 < nu.

// a class's first address: the unit's base plus the bytes of the larger classes; a symbol's: plus the earlier
// symbols of its class
.decl cbase(b: unsigned, c: unsigned, u: unsigned, a: unsigned)
cbase(b, c, u, x + y) :- ubc(u, b, c, _), ubase(b, u, x), y = sum z : { ubc(u, b, c2, z), c2 > c }.
.decl addr(i: unsigned, a: unsigned)
addr(i, x + y) :- occ(b, c, u, i, _), cbase(b, c, u, x), y = sum o : { occ(b, c, u, j, o), j < i }.

// typelinks: sorted by type string, then id; va = round_up(va, 2^class), then += size. Every typelink here has
// the same class and a size that is a multiple of it, so the address is the rounded start plus the sizes of
// the typelinks before it (checked: `tlcheck` must be empty). "Before": a smaller 8-byte prefix (a numeric range
// of an index), or the same prefix and a smaller string (symbols compare as strings), or the same string and a
// smaller id. (Comparing the symbols alone, 33 K x 33 K string compares, took 4.6 s on 8 threads.)
.decl tl(i: unsigned, k: unsigned, t: symbol, g: unsigned, size: unsigned, c: unsigned)
tl(i, k, t, g, z, c) :- sym(i, 6, c, z, _), tlstr(i, s), str(s, k, t), id(i, g).
.decl tlcheck(i: unsigned)
tlcheck(i) :- tl(i, _, _, _, z, c), (tl(_, _, _, _, _, c2), c2 != c ; z band ((1 bshl c) - 1) != 0).
.decl tlstart(a: unsigned)
tlstart(RND(a, 1 bshl c)) :- bstart(6, a), tl(_, _, _, _, _, c).
addr(i, x + y1 + y2 + y3) :- tl(i, k, t, g, _, _), tlstart(x),
                             y1 = sum z : { tl(_, k2, _, _, z, _), k2 < k },
                             y2 = sum z : { tl(_, k, t2, _, z, _), t2 < t },
                             y3 = sum z : { tl(_, k, t, g2, z, _), g2 < g }.

.output tlcheck(IO=flat, cols="layout_tlcheck.u32", exitafter="2")
.output addr(IO=flat, cols="-,p3_addr.u32", exitafter="2")

codex
The lazy-versus-eager claims omit work on the eager side: Futhark’s eager CSR build is unwritten, and Soufflé’s full eager pipeline is slower than its lazy version. I’m also checking whether the “OK” commands compare generated output with the oracle.
exec
/bin/bash -lc "cat systems/duckdb/check3b.py; rg -n 'check|oracle|OK|bad|diff' systems/goose/p3*.goose systems/futhark/p3.c; cat systems/duckdb/sql/p2loop.sql; rg -n 'index|recursive|USING|falls|attempt' systems/duckdb/NOTES.md; ls /home/exedev/paradigms/systems/{goose,halide/cells,futhark,souffle}/build" in /home/exedev/paradigms-w
 exited 2 in 0ms:
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
systems/futhark/p3.c:1:// p3.c: driver for p3.fut. Loads the flat files, calls the entries, times them, checks against the oracles.
systems/futhark/p3.c:2://   p3 DATADIR THREADS REPS CELL [nocheck]     CELL: 3a 3a_loop 3b 3b_bytes
systems/futhark/p3.c:10:	int check = !(argc > 5 && strcmp(argv[5], "nocheck") == 0);
systems/futhark/p3.c:12:	long bad = 0;
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
systems/futhark/p3.c:82:			long nbad = 0;
systems/futhark/p3.c:86:				if (memcmp(img + o, want + o, psize[i]) != 0 && nbad++ < 5) printf("  symbol %u (bucket %u) differs\n", i, pb[i]);
systems/futhark/p3.c:88:			printf("image: %ld placed symbols differ from oracle/p3_image.bin\n", nbad);
systems/futhark/p3.c:89:			bad += nbad + (nerr != 0);
systems/futhark/p3.c:93:	if (check) printf(bad ? "P%s FAIL\n" : "P%s OK\n", cell);
systems/futhark/p3.c:94:	return bad != 0;
systems/goose/p3par.goose:42:var addr: u32[>..] = [];        // 3b: the oracle's addresses, so 3b does not depend on 3a (as in ref/flat/p3.c)
systems/goose/p3par.goose:66:    load("oracle/p3_addr.u32", addr);
systems/goose/p3par.goose:344:    var bad = check32("addr", addr, "oracle/p3_addr.u32");
systems/goose/p3par.goose:345:    let oaddr = load_u32("oracle/p3_addr.u32");
systems/goose/p3par.goose:349:    load("oracle/p3_image.bin", want);
systems/goose/p3par.goose:351:    var nbad = 0;
systems/goose/p3par.goose:357:            if nbad < 5 { print("  symbol ", i, " (bucket ", pbucket[i], ") differs"); }
systems/goose/p3par.goose:358:            nbad++;
systems/goose/p3par.goose:381:    format(note, nbad, " placed symbols differ from oracle/p3_image.bin");
systems/goose/p3par.goose:383:    bad += nbad;
systems/goose/p3par.goose:384:    print(if bad == 0 { "P3 OK" } else { "P3 FAIL" });
systems/goose/p3par.goose:385:    if bad != 0 { exit(1); }
systems/goose/p3.goose:3:// relocations (ref/flat/README.md; ref/flat/p3.c is the plain C form). Checks itself against oracle/p3_addr.u32
systems/goose/p3.goose:4:// and every placed symbol's bytes in oracle/p3_image.bin.
systems/goose/p3.goose:6:// --pipeline: 3b relocates against this program's own 3a addresses, spread to every id through oracle/p1c_canon.u32
systems/goose/p3.goose:161:    var bad = check32("addr", addr, "oracle/p3_addr.u32");
systems/goose/p3.goose:162:    // 3b's addresses: the oracle's (so that 3b does not depend on 3a), or this program's own (--pipeline)
systems/goose/p3.goose:173:        let canon = load_u32("oracle/p1c_canon.u32");
systems/goose/p3.goose:176:        load("oracle/p3_addr.u32", oaddr);
systems/goose/p3.goose:199:    load("oracle/p3_image.bin", want);
systems/goose/p3.goose:200:    var nbad = 0;
systems/goose/p3.goose:208:            if nbad < 5 { print("  symbol ", i, " (id ", psym[i], ", bucket ", pbucket[i], ") differs"); }
systems/goose/p3.goose:209:            nbad++;
systems/goose/p3.goose:226:    format(note, "; ", nbytes, " bytes, ", rstart[L], " relocations, ", nerr, " errors; ", nbad,
systems/goose/p3.goose:227:           " placed symbols differ from oracle/p3_image.bin");
systems/goose/p3.goose:231:    bad += nbad + (if nerr != 0 { 1 } else { 0 });
systems/goose/p3.goose:233:    print(what, if bad == 0 { " OK" } else { " FAIL" });
systems/goose/p3.goose:234:    if bad != 0 { exit(1); }
-- Problem 2, reach, as an iterated frontier (semi-naive evaluation driven from outside: dk's .repeat).
-- Each round joins only what is new: dl, the symbols that became live last round, and du, the live types that
-- became "used in interface" (iface + gotype + def) last round. The rules are ref/flat/README.md's.
-- The edges come from two views over dl and du, out_dl and out_du (kind, dst): p2prep.sql reads them from the
-- p2_edge CSR, p2eager.sql from relocations decoded up front, p2lazy.sql decodes a symbol's relocations when it
-- becomes live (ATTACKS 1).
.cell 2loop
CREATE OR REPLACE TEMP TABLE live AS SELECT DISTINCT id FROM root WHERE id <> 0;
CREATE OR REPLACE TEMP TABLE iface AS SELECT id FROM sym WHERE fl & 16 <> 0;
-- the flags the rules test, as small relations: a round never scans the 5.2 M symbols
CREATE OR REPLACE TEMP TABLE gt AS SELECT id FROM sym WHERE fl & 9 = 9;      -- defined go types
CREATE OR REPLACE TEMP TABLE rf AS SELECT id FROM sym WHERE fl & 4 <> 0;     -- callers of reflect's Method
CREATE OR REPLACE TEMP TABLE uii (id UINTEGER);           -- types already handled as used-in-interface
CREATE OR REPLACE TEMP TABLE sigs (x UINTEGER);           -- interface methods some live code calls
CREATE OR REPLACE TEMP TABLE named (n UINTEGER);          -- method names called by name
CREATE OR REPLACE TEMP TABLE refl (b INTEGER);            -- one row once reflect's Method is live
CREATE OR REPLACE TEMP TABLE cand AS SELECT * FROM meth LIMIT 0;  -- methods of used-in-interface types
CREATE OR REPLACE TEMP TABLE called (m UINTEGER);
CREATE OR REPLACE TEMP TABLE dl AS FROM live;
CREATE OR REPLACE TEMP TABLE du AS
SELECT id FROM dl SEMI JOIN gt USING (id) SEMI JOIN iface USING (id);
INSERT INTO uii FROM du;
.repeat
-- what the new live symbols name
CREATE OR REPLACE TEMP TABLE out_l AS FROM out_dl;
INSERT INTO sigs SELECT DISTINCT dst FROM out_l ANTI JOIN sigs ON sigs.x = out_l.dst WHERE kind = 2;
INSERT INTO named SELECT DISTINCT dst FROM out_l ANTI JOIN named ON named.n = out_l.dst WHERE kind = 3;
INSERT INTO refl SELECT 1 WHERE NOT EXISTS (FROM refl) AND EXISTS (FROM dl SEMI JOIN rf USING (id));
-- the new used-in-interface types: their methods become candidates
INSERT INTO cand SELECT meth.* FROM meth JOIN du ON meth.typ = du.id;
-- new iface marks: converted to an interface by live code, a gotype a used-in-interface type points at, a method's type
CREATE OR REPLACE TEMP TABLE ni AS
SELECT DISTINCT id FROM (
  SELECT dst AS id FROM out_l WHERE kind = 1
  UNION ALL
  SELECT e.dst FROM out_du e SEMI JOIN gt ON gt.id = e.dst WHERE e.kind = 0
  UNION ALL
  SELECT mtyp FROM meth JOIN du ON meth.typ = du.id)
ANTI JOIN iface USING (id) WHERE id <> 0;
INSERT INTO iface FROM ni;
-- candidate methods something can now call
CREATE OR REPLACE TEMP TABLE call AS
SELECT c.* FROM cand c ANTI JOIN called USING (m)
LEFT JOIN (SELECT DISTINCT sig.name, sig.typ FROM sigs JOIN sig USING (x)) ls ON ls.name = c.name AND ls.typ = c.mtyp
WHERE ls.name IS NOT NULL OR c.name IN (SELECT n FROM named) OR (c.exported <> 0 AND EXISTS (FROM refl));
INSERT INTO called SELECT m FROM call;
-- new live symbols: plain targets of new live code, and what called methods keep
CREATE OR REPLACE TEMP TABLE nl AS
SELECT DISTINCT id FROM (SELECT dst AS id FROM out_l WHERE kind = 0 UNION ALL SELECT unnest([mtyp, ifn, tfn]) FROM call)
ANTI JOIN live USING (id) WHERE id <> 0;
INSERT INTO live FROM nl;
-- next round's frontiers
CREATE OR REPLACE TEMP TABLE du AS
SELECT DISTINCT id FROM (FROM nl UNION ALL FROM ni) SEMI JOIN gt USING (id)
SEMI JOIN live USING (id) SEMI JOIN iface USING (id) ANTI JOIN uii USING (id);
INSERT INTO uii FROM du;
CREATE OR REPLACE TEMP TABLE dl AS FROM nl;
.until SELECT (SELECT count(*) FROM dl) + (SELECT count(*) FROM du);
.nocell
.show SELECT count(*) FROM live;
.dump out/p2_live.u32 SELECT id FROM live ORDER BY id;
12:  `LIST(STRUCT(...))` columns with no copy, and `unnest(rels, recursive := true)` turns a symbol into its
18:  decode is a hash join against the 5.2 M-row `canon` table, rebuilt or rescanned every round (no persistent index
31:| `dk.c` (build: `./build.sh`) | the host: `.table NAME ROWS col=file ...` (Arrow view + CREATE TABLE), `col=OFF.u32[f=file,...]` a list column over a CSR, `col=#` a row index, `.cell`, `.repeat/.until` (a loop driven from outside: SQL has no loop), `.dump`, `.image/.blob/.int/.imgout` (the 3b scatter), `.show`; `DK_REPS` (repeat the scripts after `--`), `DK_THP` (image on 2 MB pages), `DK_STMTS` (time per statement over all rounds) |
85:per object (`generate_subscripts` gives the index). Forced: a LEFT JOIN `ON rel.pkg = BUILTIN AND bc.i = rel.idx`
94:persistent index, so a round costs ~25 statements x (1-10 ms) regardless of how small the frontier is. Two tuned
95:attempts: (1) edges by a bucketed range join, 16.9 s prep; (2) edges as lists + the rules' flag tests as small
96:relations (`gt`, `rf`) instead of joins with the 5.2 M symbols. A recursive CTE cannot express it (the method
97:rules need the global sig/named/reflect sets, and a recursive CTE may reference itself once).
102:repeat): exact but 1,121 rounds for the units; a recursive CTE over the unit index: 1,322 iterations, 340 ms on 1
143:ANTI JOIN live USING (id) WHERE id <> 0;
150:CREATE OR REPLACE TEMP VIEW rel_dl AS SELECT id, o, unnest(rels, recursive := true) FROM rsrc SEMI JOIN dl USING (id);
153:UNION ALL SELECT unnest(out, recursive := true) FROM adjx JOIN dl ON adjx.src = dl.id;
162:FROM blk JOIN bucket USING (b) GROUP BY b
174:  cell (1.3x for 3a, 1.4x for the P2 loop, 3-5x for the big joins of 1a/1b/1c/3b), except the abandoned recursive CTE
ls: cannot access '/home/exedev/paradigms/systems/goose/build': No such file or directory
ls: cannot access '/home/exedev/paradigms/systems/halide/cells/build': No such file or directory
ls: cannot access '/home/exedev/paradigms/systems/futhark/build': No such file or directory
ls: cannot access '/home/exedev/paradigms/systems/souffle/build': No such file or directory

exec
/bin/bash -lc "cat ref/flat/flat.h; sed -n '330,390p' systems/goose/p3par.goose; sed -n '1,90p' systems/souffle/NOTES.md; rg -n 'check|oracle|OK' ref/flat/p{1,2,3}.c; nl -ba ATTACKS.md | tail -n 50; nl -ba JOURNAL.md | tail -n 24" in /home/exedev/paradigms-w
 succeeded in 0ms:
// flat.h: read the flat files (data/README.md) by mmap. Header-only; every reference program includes it.
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;

static const char *flatDir;
static size_t lastSize;
// fmap: the whole file, read-only; *n = its size in bytes
static const void *fmap(const char *name, size_t *n) {
	char p[4096];
	snprintf(p, sizeof p, "%s/%s", flatDir, name);
	int fd = open(p, O_RDONLY);
	if (fd < 0) { perror(p); exit(2); }
	struct stat st;
	fstat(fd, &st);
	lastSize = (size_t)st.st_size;
	if (n) *n = lastSize;
	if (lastSize == 0) { close(fd); return ""; }
	void *m = mmap(NULL, lastSize, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
	if (m == MAP_FAILED) { perror(p); exit(2); }
	close(fd);
	// FLAT_HUGE=1: copy files of >= 2 MB onto 2 MB pages (MAP_HUGETLB, needs vm.nr_hugepages; falls back to the file
	// mapping): page-cache mappings are 4 KB, so a random walk over a big column misses the TLB (ATTACKS 2)
	static int huge = -1;
	if (huge < 0) huge = getenv("FLAT_HUGE") && atoi(getenv("FLAT_HUGE"));
	if (huge && lastSize >= (2u << 20)) {
		size_t hn = (lastSize + (2u << 20) - 1) & ~(size_t)((2u << 20) - 1);
		void *h = mmap(NULL, hn, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
		if (h != MAP_FAILED) { memcpy(h, m, lastSize); munmap(m, lastSize); return h; }
	}
	return m;
}
#define FMAP(T, name, cnt) ((const T *)fmap(name, cnt))
static u64 metaGet(const char *key) {
	char p[4096], k[128];
	unsigned long long v;
	snprintf(p, sizeof p, "%s/meta.txt", flatDir);
	FILE *f = fopen(p, "r");
	if (!f) { perror(p); exit(2); }
	while (fscanf(f, "%127s %llu", k, &v) == 2)
		if (strcmp(k, key) == 0) { fclose(f); return v; }
	fprintf(stderr, "meta: no %s\n", key);
	exit(2);
}
static double now(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec * 1e-9;
}
static inline u64 mix64(u64 h) {
	h ^= h >> 30;
	h *= 0xBF58476D1CE4E5B9ull;
	h ^= h >> 27;
	h *= 0x94D049BB133111EBull;
	return h ^ (h >> 31);
}
// compare n u32s with an oracle file; prints the first few differences; returns the number that differ
static long check32(const char *what, const u32 *got, size_t n, const char *oracle) {
	size_t m;
	const u32 *want = FMAP(u32, oracle, &m);
	if (m != n * 4) { printf("%s: size %zu, oracle %zu\n", what, n, m / 4); return -1; }
	long bad = 0;
	for (size_t i = 0; i < n; i++)
		if (got[i] != want[i] && bad++ < 5) printf("  %s[%zu] = %u, oracle %u\n", what, i, got[i], want[i]);
	printf("%s: %zu entries, %ld differ from %s\n", what, n, bad, oracle);
	return bad;
}
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
# Soufflé column: problems 1-3 on MinIO

Soufflé 2.5, compiled mode (`souffle -g` + g++ -O3), every cell correct against its oracle. Inputs come in through
`flatio.h` (C++, I/O only): build.sh rewrites the generated reader/writer calls so binary flat-file columns are
mmap'd, turned into tuples in parallel, sorted per index and bulk-loaded into Soufflé's B-trees (`btree_set::load`).
The rules are plain Datalog; nothing in C computes an answer, except the 3b image assembly (copying bytes; Soufflé
has no bytes) and the untimed P2-lazy setup that p2lazy.c also does untimed.

## Surprises

- **Lazy decode is expressible and wins against Soufflé's own eager shape, but not against C (ATTACKS 1).** The rule
  `plain(s, t) :- live(s), source(s), rel(s, p, i, ty), plainty(ty), slotobj(s, o), objpkg(o, p, b), canon(b + i, t)`
  is evaluated semi-naively, so only live sources' relocations are decoded (1.73 M `plain` tuples of 9.4 M
  relocations). Warm: 2.9-3.2 s on 1 thread, 0.96-1.6 s on 8, vs 1b's eager decode 5.2-5.4 / 1.08-1.30 s plus the
  flood over the pre-decoded CSR 1.23-1.47 / 0.43-0.58 s. The C pipelines: eager 108.7 / 30.1 ms, lazy 52.6 /
  13.6-15.4 ms. So Soufflé is ~30x / ~35-50x the *eager* C pipeline and does not approach the lazy one. And the
  laziness stops at the rules: the engine joins only over B-trees, so the 9.4 M relocation columns are still loaded
  into a B-tree index first (6.15 M distinct tuples, ~1.5 s of the 2.2 s load at 1 thread): Soufflé materializes the
  *input* even when the rules don't materialize the graph. e2e: lazy 4.6-5.0 s / 2.3-3.1 s vs eager 1b + 2 =
  7.4 + 2.6 s / 2.9 + 1.3 s.
- **The output's page size is irrelevant to the Soufflé 3b cell (ATTACKS 2).** The image lives in flatio's writer
  (C), where MAP_HUGETLB works: assembly 53.5-96.9 ms (19,108 minor faults) on 4 KB pages vs 20.0-34.7 ms (39 faults)
  on 2 MB, 1 thread; 18.3-31.6 vs 7.8-16.2 ms (150-172 faults) on 8 (FLATIO_IMG_REPS=20, three interleaved runs each,
  16:29-16:31Z, load 8.7-11). But the rules computing the 2.05 M patch values take 1.05-1.12 s / 0.32-0.45 s: the
  faults are 2-5% of the cell. Soufflé's own runtime (B-trees from `new`) has no page-size knob; this kernel has no
  THP.
- **8 threads with OpenMP's default (spinning) wait policy lost 10-25x under load**: reach_lazy -j8 took 38 s vs
  4.2 s at -j1 (16:17Z, load ~11). With `OMP_WAIT_POLICY=passive` it is 1.5 s. Every 8-thread number below is passive.
- **A sum over "the earlier members of my group" is fast when the group key is an index prefix**: Soufflé turns
  `y = sum o : { occ(b, c, u, j, o), j < i }` into a range scan of a (b, c, u, j) index; layout's 327 K non-typelink
  addresses (Σ n²/2 = 108 M tuples visited) take 56 ms on 8 threads. Comparing *symbols* (strings) in the same shape
  is 80x slower per pair: 4.6 s for the 33 K typelinks.

## Timings

`bench.sh` (5 interleaved rounds, min / median / max ms), 2026-11-16T16:37Z, load 2.9-6.2; full output in
`bench-20261116T1637Z.txt`. warm = flatio's "compute" (rules only; inputs already in the B-trees, output writing
excluded). e2e = process start to exit. 8 threads: `OMP_WAIT_POLICY=passive`. Each warm run is one evaluation (the
generated main runs once); the warm times are >= 0.3 s, most >= 1 s.

| cell | 1 thread warm | 1 thread e2e | 8 threads warm | 8 threads e2e | plain C 1 core / tuned C 8 |
|---|---|---|---|---|---|
| 1a join by name (resolve_a) | 1928 / 1996 / 2034 | 3136 / 3206 / 3295 | 636 / 680 / 754 | 1722 / 1736 / 1888 | 194-203 / 8.6-10.5 |
| 1b relocation targets (resolve_b) | 5220 / 5263 / 5426 | 7371 / 7542 / 7657 | 1080 / 1206 / 1301 | 2669 / 2875 / 2972 | 43-44 / 5.3-6.3 |
| 1c content-hash join (resolve_c) | 1625 / 1664 / 1865 | 2805 / 2879 / 3127 | 587 / 615 / 718 | 1566 / 1610 / 1761 | 57-61 / 7.2-8.9 |
| 1c, rules over the slot columns (resolve_c2) | 1013 / 1055 / 1069 | 3086 / 3184 / 3256 | 394 / 408 / 437 | 1936 / 2015 / 2107 | |
| 2 reach (reach, pre-decoded CSR) | 1227 / 1317 / 1467 | 2571 / 2713 / 2921 | 431 / 462 / 579 | 1242 / 1294 / 1429 | 39-43 / 13-17 |
| 2' reach, lazy decode (reach_lazy) | 2905 / 3124 / 3214 | 4637 / 4797 / 4982 | 960 / 1069 / 1610 | 2312 / 2478 / 3081 | 52.6 / 13.6-15.4 (p2lazy) |
| 3a layout (layout) | 7222 / 7337 / 7589 | 7367 / 7480 / 7717 | 1339 / 1625 / 1784 | 1474 / 1774 / 1933 | 8-19 / - |
| 3b relocate (relocate; rules) | 1054 / 1079 / 1115 | 2086 / 2109 / 2139 | 320 / 366 / 451 | 1205 / 1302 / 1374 | 59-67 / 13.4-29 |
| 3b image assembly (flatio, C, 4 KB pages) | 54.2 / 54.4 / 55.9 | | 18.0 / 26.1 / 28.5 | | |

Orders of magnitude: resolve 10-120x the plain C on 1 core (1b worst) and 70-200x the tuned C on 8; reach 30x / 30x; layout
~500x / ~100x the plain C; relocate's rules ~17x / ~25x (only the C assembly is near C).

## Checks (each must print nothing / P3 OK)

```
cd systems/souffle; D=/home/exedev/paradigms/data/minio; for p in resolve_a resolve_b resolve_c resolve_c2 reach layout relocate reach_lazy; do ./build.sh $p.dl; done
gcc -O2 -o build/check_p3 check_p3.c; gcc -O2 -o build/prep_lazy prep_lazy.c; build/prep_lazy $D build/lazyfacts
build/resolve_a -j8 -F $D -D out && cmp out/p1_canon.u32 $D/oracle/p1_canon.u32 && cmp out/p1_builtin.u32 $D/oracle/p1_builtin.u32
build/resolve_b -j8 -F $D -D out && cmp out/p1_reltarget.u32 $D/oracle/p1_reltarget.u32
build/resolve_c -j8 -F $D -D out && cmp out/p1c_canon.u32 $D/oracle/p1c_canon.u32      # also resolve_c2
build/reach -j8 -F $D -D out && cmp out/p2_live.u32 $D/oracle/p2_live.u32
build/reach_lazy -j8 -F build/lazyfacts -D out && cmp out/p2_live.u32 $D/oracle/p2_live.u32
build/layout -j8 -F $D -D out && build/check_p3 $D $D/oracle/p3_image.bin out/p3_addr.u32   # 3a
build/relocate -j8 -F $D -D out && build/check_p3 $D out/p3_image.out                       # 3b
```
`bench.sh` runs all of these each round and complains on stderr if one fails.

## Per cell

Lines: rules + declarations, without comments (the file's total in brackets). Shared: flatio.h 385 lines of C++
(I/O only), build.sh 19.

**1a join by name** (resolve_a.dl, 37 [51]). Easy: groups, "a kind-1 candidate joins only an existing group"
(a join with `group`), winner = `max rank` then `min id` aggregates, the debug-reference rule as negation.
Forced: none. Impossible: none. The 128-bit key is two columns.

**1b relocation targets** (resolve_b.dl, 25 [35]). Easy: the package-table gather is a join on (object, entry);
the special package indices are arithmetic. Forced: `npkg` is a count aggregate per object; "no target -> 0" needs
a helper relation and negation. Slow: 9.4 M relocations x 4 joins, each a B-tree probe and an insert (5.2 s / 1.1 s).

**1c content-hash join** (resolve_c.dl 26 [42]; resolve_c2.dl 23 [31]). Easy: the winner order as aggregates
(`max size` among reached copies, then `min id`). Forced: aggregates run once per tuple of the body before them, so
each group must first be its own relation (`group2(k1) :- ...` then `size2(k1, s) :- group2(k1), s = max ...`);
written as `copy2(k1, _, 1, _), s = max ...` it ran once per copy. Two shapes: copy relations (resolve_c), or the
rules straight over one 5-column `slot` relation (resolve_c2: 37% less compute, but more indexes to bulk-load, so
e2e is a wash).

ref/flat/p3.c:2:// oracle/p3_addr.u32 (3a) and the bytes of every placed symbol in oracle/p3_image.bin (3b).
ref/flat/p3.c:91:	// ---- 3b. isolated oracle addresses, or our layout propagated through final 1c canon.
ref/flat/p3.c:93:	const u8 *want = FMAP(u8, "oracle/p3_image.bin", &nimg);
ref/flat/p3.c:97:		const u32 *canon = FMAP(u32, "oracle/p1c_canon.u32", NULL);
ref/flat/p3.c:112:		oaddr = FMAP(u32, "oracle/p3_addr.u32", NULL);
ref/flat/p3.c:218:	printf("p3b (%d threads): copy + relocate %.1f ms, %llu bytes, %u relocations, %ld errors; checksum %016llx\n", nthreads(), (t3 - t2) * 1e3,
ref/flat/p3.c:220:	printf("image: %ld placed symbols differ from oracle/p3_image.bin\n", nbad);
ref/flat/p3.c:226:	bad += check32("addr", addr, L, "oracle/p3_addr.u32");
ref/flat/p3.c:228:	// which no relocation reads; the image is the oracle.)
ref/flat/p3.c:230:	if (pipeline) printf(bad ? "P3 pipeline FAIL\n" : "P3 pipeline OK\n");
ref/flat/p3.c:231:	else printf(bad ? "P3 FAIL\n" : "P3 OK\n");
ref/flat/p2.c:2:// oracle/p2_live.u32. The rules are data/README.md's (they are cmd/link's deadcode.go):
ref/flat/p2.c:128:	long bad = check32("live", list, nlive, "oracle/p2_live.u32");
ref/flat/p2.c:129:	printf(bad ? "P2 FAIL\n" : "P2 OK\n");
ref/flat/p1.c:2:// say everything resolve needs. Checks itself against oracle/p1_canon.u32, oracle/p1_builtin.u32,
ref/flat/p1.c:3:// oracle/p1_reltarget.u32 and (the content-hash join, 1c) oracle/p1c_canon.u32.
ref/flat/p1.c:93:	long bad = check32("canon", canon, (size_t)N + 1, "oracle/p1_canon.u32");
ref/flat/p1.c:94:	bad += check32("builtin", builtin, nb, "oracle/p1_builtin.u32");
ref/flat/p1.c:95:	bad += check32("reltarget", tgt, nrel, "oracle/p1_reltarget.u32");
ref/flat/p1.c:103:	bad += check32("canon after 1c", canon2, (size_t)N + 1, "oracle/p1c_canon.u32");
ref/flat/p1.c:104:	printf(bad ? "P1 FAIL\n" : "P1 OK\n");
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
   126	**What it says about the paradigms.** None of the grid's data-parallel systems can say "a fixpoint without levels":
   127	Futhark and Halide have no worklist; Soufflé's semi-naive evaluation is level-synchronous by construction; DuckDB's
   128	recursive CTE iterates rounds; Goose has the queues but no shared visited set. The C worklist is ~200 lines of
   129	mechanism (stealing, credits) for ~15 lines of rules. A build farm's machines are shared like this VM, so robustness
   130	under load is part of the speed. **Next:** a quiet-machine run (does async lose anything with nothing else running?),
   131	and whether a level-free flood also cuts Futhark's/Goose's cost (fewer, larger rounds: e.g. expand several levels per
   132	superstep, a "delta-stepping" of the flood).
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

exec
/bin/bash -lc '/home/exedev/paradigms/build/p3 /home/exedev/paradigms/data/minio --pipeline' in /home/exedev/paradigms-w
 succeeded in 129ms:
p3a (1 core): layout 15.6 ms (360315 symbols, 33261 sorted by type string)
p3b (1 threads): copy + relocate 63.5 ms, 76359886 bytes, 2053801 relocations, 0 errors; checksum 065963d1b7a64b22
image: 0 placed symbols differ from oracle/p3_image.bin
addr: 360315 entries, 0 differ from oracle/p3_addr.u32
P3 pipeline OK

exec
/bin/bash -lc "nl -ba systems/futhark/NOTES.md | sed -n '80,117p'; nl -ba systems/duckdb/NOTES.md | sed -n '88,101p'; nl -ba systems/halide/NOTES.md | head -n 40; nl -ba systems/souffle/CELLS.md | sed -n '20,42p'; git status --short" in /home/exedev/paradigms-w
 succeeded in 0ms:
    80	## ATTACKS 1 (reach counting its own decode)
    81	
    82	- **Can Futhark express "decode a reference when the flood reaches it"? Yes.** `reach_lazy`'s `edges` function
    83	  expands each frontier source's relocation range (`expand` over `slot_rel`) and maps the decode
    84	  (package-base table, then canon) over the expanded relocations, inside the level loop; the graph is never
    85	  materialized. The R_USETYPE skip rule (type 26 skips the next two) becomes a segmented scan of a 3-state
    86	  machine's transition functions (function composition is associative), or a bounded look-back
    87	  (`2lazy_look`, the second attempt: not faster, 108-130 vs 96-118 ms at 8 threads, 16:21Z).
    88	- **Does it beat the eager C pipeline (108.7 ms 1 thread / 30.1 ms 8)? No.** 1 core: lazy 165-200 ms (2 MB
    89	  pages) / 196-224 (4 KB). The eager Futhark pipeline would be its own 1b (47-48 ms) + a CSR build (not written)
    90	  + the flood over p2_edge (97-119 ms), > 144 ms: also above 108.7. 8 threads: lazy 90-137 ms median, eager flood
    91	  58-102, at load 5-7.
    92	- **Why.** Futhark's flood is level-synchronous: 111 levels, each ~25 data-parallel operations (filters,
    93	  expands = scan + scatter + segmented iota, scatters, the dedup), each a full pass with its own allocation. At
    94	  1 core the eager flood costs ~0.9 ms per level against C's 0.4; the lazy flood adds an expand and a
    95	  segmented scan per level, so in Futhark lazy *loses* to eager (165 vs 97 ms) where in C it wins. What makes
    96	  the lazy shape cheap in C (a sequential walk of a row, decode and mark in the same loop) is a nested,
    97	  irregular loop with writes to shared state, which Futhark must flatten into separate passes.
    98	- First tuning pass (before these numbers): the method rule checked every one of the 106 K methods every level
    99	  (`filter call (iota M)`, the largest item in the profile); now a pending list holds the candidate methods
   100	  that have not fired.
   101	
   102	## ATTACKS 2 (relocate on huge pages)
   103	
   104	- **Can the runtime put 3b's output on 2 MB pages? Yes, without touching the code**:
   105	  `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` (with `vm.nr_hugepages=512` reserved here). Futhark exposes no
   106	  allocator hook; the C API offers no way to hand it a buffer (`futhark_new_*` copies).
   107	- **Minor faults of 3b**: per warm rep 4,623-6,833 (4 KB) vs 18-80 (2 MB); per process (load + 1 run) 104,142
   108	  (1 core) / 168,488 (8 threads) vs 2,456 / 2,798. The C reference faults 19,109 per rep (a fresh 109 MB image
   109	  each time); Futhark's warm reps fault less because the image's memory is recycled (even with `FUT_FRESH=1`,
   110	  which empties the runtime's free list, so glibc keeps it), so the warm number does not pay the image's first
   111	  touch; the first rep of each process does.
   112	- **Times**, 5 interleaved process runs each, 3 reps per process, 16:31Z, load 5.8-6.1:
   113	  first rep (fresh image) 1 core 181-190 ms (4 KB) vs 119-150 (2 MB); 8 threads 126-137 vs 52-64; later reps
   114	  1 core 112-126 vs 107-116 (within noise), 8 threads 44-67 vs 29-55.
   115	  3b itself is 2-4.5x the C reference (C 1 core 49 / 24 ms on 4 KB / 2 MB pages): the copy and the patch are
   116	  scatters of (index, byte) pairs (one per byte of data, 8 per relocation), which is how a scatter must be
   117	  shaped in Futhark: there is no memcpy of a run into a slice of a shared array from inside a map.
    88	
    89	**1c (content-hash join).** Easy: `arg_min(struct, (mark, -size, id))` per group is the winner rule as written. 16 lines.
    90	
    91	**2 (reach).** Forced. SQL has no loop: dk's `.repeat/.until` reruns a block of statements until the frontier is
    92	empty (45 rounds), i.e. semi-naive evaluation driven from outside, one temp table per relation. 48 lines for the
    93	rules + 8 prep. Every round rebuilds hash tables for `live`, `iface`, `uii` and the frontier joins; there is no
    94	persistent index, so a round costs ~25 statements x (1-10 ms) regardless of how small the frontier is. Two tuned
    95	attempts: (1) edges by a bucketed range join, 16.9 s prep; (2) edges as lists + the rules' flag tests as small
    96	relations (`gt`, `rf`) instead of joins with the 5.2 M symbols. A recursive CTE cannot express it (the method
    97	rules need the global sig/named/reflect sets, and a recursive CTE may reference itself once).
    98	
    99	**3a (layout).** Mostly easy: occupied sizes, per-(unit, bucket, class) sums and every in-class position are window
   100	sums. Forced: "va = round_up(va, align); va += n" over the units is a fold, not a prefix sum (a round-up does not
   101	compose). Tried three ways: a Jacobi fixpoint over the paddings (prefix-sum with guessed paddings, recompute,
     1	# Halide: the grid cells (problems 1, 2, 3)
     2	
     3	Halide 21.0.0, C++ AOT generators (install notes at the end). Branch `worktree-agent-a45ed3bb4a5a5b2a6`.
     4	Code: `systems/halide/cells/`: `gen_p1.cpp gen_p2.cpp gen_p3.cpp` (the Halide algorithms and schedules),
     5	`join.h` (a hash join in Halide, used by 1a, 1c and P2), `p1.cpp p2.cpp p3.cpp` (drivers: mmap the flat files,
     6	wrap them zero-copy as `Halide::Runtime::Buffer`, call the AOT pipelines, check against the oracles), `bench.h`
     7	(timing), `build.sh` (one `.a` per schedule variant). Timings: `systems/halide/bench.sh` (one line per cell,
     8	threads, warm/e2e).
     9	
    10	Build and check (each driver checks every variant it runs against the oracles and ends with `P<n> OK`):
    11	
    12	    systems/halide/cells/build.sh                      # ~1 min: generators, every variant, drivers
    13	    B=systems/halide/cells/build; D=/home/exedev/paradigms/data/minio
    14	    HL_NUM_THREADS=8 $B/p1 $D e2e 1                    # P1 OK: canon, builtin, reltarget, canon after 1c
    15	    HL_NUM_THREADS=8 $B/p2 $D e2e 1                    # P2 OK: live set (lazy decode, parallel)
    16	    HL_NUM_THREADS=8 $B/p3 $D e2e 0 2                  # P3 OK: 3a addresses, 3b image
    17	    systems/halide/bench.sh                            # all timings
    18	
    19	Threads are Halide's own pool (`HL_NUM_THREADS`). Its idle workers spin for a short while, then sleep on a
    20	condition variable; there is no wait-policy knob, so the 8-thread numbers are with Halide's default (the RESUME.md
    21	load warning: no 100x collapse was seen, see the ranges).
    22	
    23	
    24	## Surprises
    25	
    26	- **1a (join by name) in Halide beats the plain C reference 2.5-3x on one core** (81-93 vs 231-249 ms, same window,
    27	  17:00-17:10Z, load 2-5), with no hash table: the join is rounds of scatter-min + gather (`join.h`). The plain C
    28	  probes a 4-entry-per-slot table (125 MB) for 5.2 M slots; the Halide version first compacts the 789 K keyed slots
    29	  (a scan) and hashes only those, into tables of 2^20 down to 2^13 buckets. It does not beat the tuned C (p1par
    30	  1 thread 74-77 for 1a+1b; Halide 1a+1b 124-139), so the win is the shape (compact, then small tables), not
    31	  Halide.
    32	- **A lazy flood is slower than an eager one in Halide** (1.3-1.5x on 1 thread, 417-465 vs 312-332 ms), the
    33	  opposite of C (p2lazy 52.6 vs p2pipe 108.7). Halide's cost is per op (every op is a full iteration of a select
    34	  chain with three phases), and the lazy rows walk every relocation of a live symbol, followed or not, while the
    35	  eager CSR holds only the followed ones; the decode itself (six dependent loads) is the smaller part.
    36	- **atomic() on a scatter that reads other sites is a mutex per element**: Halide allocates a mutex array the size
    37	  of the Func on every call (`halide_mutex_array_create`), which made the parallel flood 3.4-3.6 s (vs 0.3 s
    38	  serial), and the first byte layout was still WRONG in parallel (2-2,106 live symbols lost, from reading uii
    39	  twice in one iteration). Both are fixed by a layout where every write is a constant store (below).
    40	
    20	· 47 lines · easy: every deadcode.go rule is one Datalog rule, incl. the conditional method rule; forced: static
    21	joins hoisted out of the flood. ~30x C.
    22	
    23	**2' reach with its own relocation decode (ATTACKS 1)** — correct · lazy (decode in the flood rule): 1 thr warm
    24	2905-3214, e2e 4637-4982 · 8 thr warm 960-1610, e2e 2312-3081 · 65 lines · eager in Soufflé = 1b + 2: warm
    25	~6.5 s / ~1.6 s. Lazy is expressible (semi-naive evaluation decodes only live sources: 1.73 M of 9.4 M) and beats
    26	Soufflé's eager shape ~2x, but is ~30x / ~35-50x the eager C pipeline (108.7 / 30.1); and the relocation columns are
    27	still bulk-loaded into a B-tree (the engine joins only over indexes).
    28	
    29	**3a layout** — correct (`check_p3 $D oracle/p3_image.bin out/p3_addr.u32` -> 0 differ) · 1 thr warm 7222-7589,
    30	e2e 7367-7717 · 8 thr warm 1339-1784, e2e 1474-1933 · 45 lines · forced: running sums as range aggregates over
    31	earlier group members, unit walk as 1,763-iteration recursion, typelink sort as O(n²) "sum of sizes before me"
    32	(numeric 8-byte prefix + string compare: 28 s -> 7.2 s); **falls down on speed** (~500x C; no sort/scan in Datalog).
    33	
    34	**3b relocate** — correct (`check_p3 $D out/p3_image.out` -> P3 OK) · rules: 1 thr warm 1054-1115, e2e 2086-2139 ·
    35	8 thr warm 320-451, e2e 1205-1374 · image assembly (C writer): 54-56 / 18-29 ms on 4 KB pages; on 2 MB pages
    36	(ATTACKS 2) 20.0-34.7 / 7.8-16.2 ms, 39 / 150-172 minor faults vs 19,108 · 31 lines · easy: one rule per kind;
    37	forced: bytes (copy + patch) in C; `site` inlined (5.2 s -> 2.5 s at load 11).
?? data

codex
1. **I found no confirmed unchecked “correct” cell.** The inspected checkers compare actual arrays or image bytes. I ran `/home/exedev/paradigms/build/p3 /home/exedev/paradigms/data/minio --pipeline`: **360,315 addresses checked, 0 differing symbols, 2,053,801 relocations, 0 errors; `P3 pipeline OK`**. The comparison is in [p3.c](/home/exedev/paradigms-w/ref/flat/p3.c:218). Paradigm binaries were absent from the documented build directories; rebuilding would violate your no-edit instruction.

2. **DuckDB P2’s paradigm-wide “falls down” verdict is premature.** Its two attempts change edge representation, while retaining **45 rounds × roughly 25 statements**, rebuilding hash tables every round. No persistent-index experiment is documented; “there is no persistent index” describes this implementation. Measure indexed persistent relations and inspect their query plans before generalizing. See [NOTES.md](/home/exedev/paradigms-w/systems/duckdb/NOTES.md:91). The measured implementation is slow; that broader conclusion remains unproved.

3. **Futhark’s lazy-versus-eager comparison leaves out eager work.** **165 ms lazy versus 97 ms eager** compares decoding-plus-flood with a predecoded flood. Its notes admit eager also needs **47–48 ms decoding plus an unwritten CSR build**. Therefore “lazy loses” is unestablished: [NOTES.md](/home/exedev/paradigms-w/systems/futhark/NOTES.md:88). Separately, Futhark’s warm relocation recycles output memory while C faults a fresh image—explicitly acknowledged at [line 107](/home/exedev/paradigms-w/systems/futhark/NOTES.md:107).

4. **Soufflé’s lazy-decode win is obscured in GRID.** The grid calls **960 ms** lazy “2x its eager,” using the **431 ms flood alone**. Its complete eager estimate is approximately **1.6 seconds on eight threads**, and **6.5 seconds versus 2.9–3.2 seconds lazy on one thread**. This supports the lazy-decode idea despite poor absolute speed. The notes recognize it, but the grid reverses the message; separating B-tree loading from avoided decoding deserves follow-up: [CELLS.md](/home/exedev/paradigms-w/systems/souffle/CELLS.md:23).

5. **None of ATTACKS entries 1–3 is wholly unmeasured.** Entry **3’s generalization across paradigms** is unmeasured: C async versus passive is **19.9 versus 21.4 ms ambient—within noise**, and **89.4 versus 152.7 ms under overload**. That establishes overload robustness, not a universal level-synchronization bottleneck. Measure batched-level Futhark/Goose implementations against their originals, counting passes, queue operations and barriers alongside interleaved timings: [ATTACKS.md](/home/exedev/paradigms-w/ATTACKS.md:116).

6. **No matching JOURNAL passage found.** Its recent entries do not decline or shrink work because of time, usage or session ending. “**Quiet claim untested**” appears in [JOURNAL.md](/home/exedev/paradigms-w/JOURNAL.md:36), but supplies no such excuse. I would not characterize it as one.
tokens used
66,198
1. **I found no confirmed unchecked “correct” cell.** The inspected checkers compare actual arrays or image bytes. I ran `/home/exedev/paradigms/build/p3 /home/exedev/paradigms/data/minio --pipeline`: **360,315 addresses checked, 0 differing symbols, 2,053,801 relocations, 0 errors; `P3 pipeline OK`**. The comparison is in [p3.c](/home/exedev/paradigms-w/ref/flat/p3.c:218). Paradigm binaries were absent from the documented build directories; rebuilding would violate your no-edit instruction.

2. **DuckDB P2’s paradigm-wide “falls down” verdict is premature.** Its two attempts change edge representation, while retaining **45 rounds × roughly 25 statements**, rebuilding hash tables every round. No persistent-index experiment is documented; “there is no persistent index” describes this implementation. Measure indexed persistent relations and inspect their query plans before generalizing. See [NOTES.md](/home/exedev/paradigms-w/systems/duckdb/NOTES.md:91). The measured implementation is slow; that broader conclusion remains unproved.

3. **Futhark’s lazy-versus-eager comparison leaves out eager work.** **165 ms lazy versus 97 ms eager** compares decoding-plus-flood with a predecoded flood. Its notes admit eager also needs **47–48 ms decoding plus an unwritten CSR build**. Therefore “lazy loses” is unestablished: [NOTES.md](/home/exedev/paradigms-w/systems/futhark/NOTES.md:88). Separately, Futhark’s warm relocation recycles output memory while C faults a fresh image—explicitly acknowledged at [line 107](/home/exedev/paradigms-w/systems/futhark/NOTES.md:107).

4. **Soufflé’s lazy-decode win is obscured in GRID.** The grid calls **960 ms** lazy “2x its eager,” using the **431 ms flood alone**. Its complete eager estimate is approximately **1.6 seconds on eight threads**, and **6.5 seconds versus 2.9–3.2 seconds lazy on one thread**. This supports the lazy-decode idea despite poor absolute speed. The notes recognize it, but the grid reverses the message; separating B-tree loading from avoided decoding deserves follow-up: [CELLS.md](/home/exedev/paradigms-w/systems/souffle/CELLS.md:23).

5. **None of ATTACKS entries 1–3 is wholly unmeasured.** Entry **3’s generalization across paradigms** is unmeasured: C async versus passive is **19.9 versus 21.4 ms ambient—within noise**, and **89.4 versus 152.7 ms under overload**. That establishes overload robustness, not a universal level-synchronization bottleneck. Measure batched-level Futhark/Goose implementations against their originals, counting passes, queue operations and barriers alongside interleaved timings: [ATTACKS.md](/home/exedev/paradigms-w/ATTACKS.md:116).

6. **No matching JOURNAL passage found.** Its recent entries do not decline or shrink work because of time, usage or session ending. “**Quiet claim untested**” appears in [JOURNAL.md](/home/exedev/paradigms-w/JOURNAL.md:36), but supplies no such excuse. I would not characterize it as one.
