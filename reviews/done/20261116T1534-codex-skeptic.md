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
session id: 01a1e4fe-ef69-75f2-80a3-ae0e5be4348c
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
Skip praise. Also: check ref/flat/README.md against ref/hlink/dumpflat.c and ref/flat/p1.c p2.c p3.c: is any rule in the README wrong or missing, or does a reference pass its oracle only because the dump precomputed the answer (e.g. an input file that leaks the output)?
codex
I’ll read the review notes, trace the oracles and flat-file references, and run checks that don’t require editing files.
exec
/bin/bash -lc "pwd; rg --files -g 'CHARTER.md' -g 'PLAN.md' -g 'STATUS.md' -g 'GRID.md' -g 'ATTACKS.md' -g 'JOURNAL.md' -g 'CLAUDE.md' -g 'README.md'" in /home/exedev/paradigms-w
 succeeded in 0ms:
/home/exedev/paradigms-w
GRID.md
CHARTER.md
PLAN.md
ATTACKS.md
ref/flat/README.md
JOURNAL.md
STATUS.md
README.md

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
  timestamps from `date -u`. The host is noisy: compare configurations interleaved in the same
  window.

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

Now: M1/M2 in flight. Four Opus builders (worktrees, branches worktree-agent-*) are building Soufflé, Futhark, Halide, Goose cells for problems 1-3 per ideas/BUILDER.md; merge each branch when it reports (check its CELLS.md, rerun its oracle check and bench.sh on a quiet machine), paste cells into GRID.md. Codex is writing ref/flat/stats.c (BFS levels, degree and reloc-kind histograms -> results/minio.stats.txt). Next for the lead: problem 4 data (fc's shapes from simd-lang: find the tool behind SHAPES.md's 4,096 shapes / 96.7%), kube-apiserver freeze + extraction (`link/freeze.sh` from bottom-up with kubernetes source), first ATTACKS.md entries from the cells.

# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T15:40Z. Campaign 1, **M0 reached**: MinIO's flat files and oracles for problems 1-3, checked by plain C
references that reproduce every oracle exactly; hlink here: 149 ms whole link on 8 cores. Oracle number: 0/20 cells, 0 attacks.
In flight (M1/M2): Opus builders for Soufflé, Futhark, Halide, Goose, each doing problems 1-3.
Early surprise to chase: a plain 1-core worklist flood over the flat (pre-decoded) edges takes 41-46 ms; hlink's tuned flood
over raw objects takes 98-103 ms on 1 core. The decode, not the graph walk, is most of reach.

## Decisions for Dan
- None yet.
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

| problem | floor (8 cores, FLOOR.md model) | hlink 1 core / 8 cores | plain C, flat, 1 core warm | check |
|---|---|---|---|---|
| 1a+1b. resolve: join by name, relocation targets | 1.36 (resolve) | 126-136 / 29-33 (resolve phase) | 186-206 (join) + 43-58 (9.4 M targets) | `build/p1 data/minio` -> P1 OK |
| 1c. content-hash join | (in resolve) | 37-39 / 8-9 (inside deadcode) | 310-380 (untuned: a 400 MB table) | same |
| 2. reach | 2.78 | 98-103 / 22-23 floods + 7 / 1.5 methods | 41-46 | `build/p2 data/minio` -> P2 OK |
| 3a. lay out | 0.60 | 85 / 23-25 (layout phase, includes pclntab sizing and the typelink sort) | 8-19 | `build/p3 data/minio` -> P3 OK |
| 3b. relocate | 4.92 (write) | 102-104 / 15 (write phase, includes pclntab and GC bitmaps) | 59-67 (76 MB, 2.05 M relocations) | same |
| 4. codegen by shape | | | | |

Floors are bottom-up's FLOOR.md model (same CPU model, the 2026-11-13 VM); they count the raw object bytes hlink
reads, so for the flat files they are an upper bound on the floor.

## The paradigms

| problem | Goose | Futhark | Halide | Soufflé |
|---|---|---|---|---|
| 1. resolve | | | | |
| 2. reach | | | | |
| 3. lay out + relocate | | | | |
| 4. codegen by shape (stretch) | | | | |
# ATTACKS

New attacks on the grid's problems (CHARTER.md). Each entry: the problem, the old shape, the new
attack and where it came from, the evidence, what it would take. Before, observation, fix, number.
# JOURNAL (append-only: one line per commit, a paragraph at each milestone)
- 2026-11-16T15:31:42Z M0 data: MinIO frozen (1,311 archives + main.a), hlink copied to ref/hlink with -dumpflat, flat files + oracles for P1-P3 (ref/flat/README.md); plain 1-core C references reproduce every oracle exactly (ref/flat/check.sh). Installs merged: Futhark 0.29.1, Halide 21, Soufflé 2.5, Goose v2026.4 (systems/*/NOTES.md).
- 2026-11-16T15:34:24Z M0: GRID.md C column (hlink phases on this VM, plain flat C references), IDEAS.md index, four Opus cell builders launched (ideas/BUILDER.md).

exec
/bin/bash -lc 'cat README.md LOOP.md ref/flat/README.md ref/flat/check.sh; nl -ba ref/flat/p1.c; nl -ba ref/flat/p2.c; nl -ba ref/flat/p3.c' in /home/exedev/paradigms-w
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
# The flat files

Every system in the grid reads these and nothing else: no system parses Go objects. They are written by
`ref/flat/extract.sh <workload>` into `data/<workload>/` (git-ignored, about 1.1 GB for MinIO), by a copy of
bottom-up's hlink (`ref/hlink`, from bottom-up `774e37c`) with one added file, `ref/hlink/dumpflat.c`, that writes
hlink's own state at the phase boundaries. The link it dumps from is unchanged (its output is byte-identical to
the stock hlink's) and runs (`--version`).

`ref/flat/check.sh` builds three plain single-core C programs (`ref/flat/p1.c`, `p2.c`, `p3.c`) that read only
these files and reproduce every oracle exactly: that is the proof that the files say everything a problem needs.
Read them next to this file: they are the executable form of the rules below.

## Conventions

- Raw little-endian arrays, no header. The suffix is the element type: `.u8 .u16 .u32 .i32 .i64 .u64`;
  `.u64x2` is pairs of u64 (a 128-bit key: k1 then k2).
- `meta.txt`: `key value` lines (counts and a few addresses). MinIO: `nslots 5205427`, `nrel 9403582`, ...
- **Ids.** A symbol slot has a dense id `1..nslots` (0 is "none"). Ids above `nslots` (up to `nids-1`) are symbols
  the linker makes; they only appear in problem 3's address table. Per-slot arrays have `nslots+1` entries.
- **CSR.** `X_off.u32` / `X_start.u32` with `n+1` (or `nslots+2`) entries: the rows of item `i` are
  `[off[i], off[i+1])`.
- **Strings.** `P.off.u64` (count+1 offsets) and `P.bin` (bytes): string `i` is `bin[off[i]:off[i+1]]`.
  Strings are interned per table, so equal ids mean equal strings. String 0 is "".

## Common

| file | what |
|---|---|
| `slot_obj.u32` | per slot: its object (0..nobjs-1). Objects are in hlink's load order; an object's slots are contiguous |
| `name.off.u64`, `name.bin` | per slot (index = id): its symbol name (not interned; content-addressed symbols mostly have none) |

## Problem 1: resolve

Each object's slots are, in order: definitions referenced by package index (kind 1), content-addressed definitions
with a 64-bit hash (2) and with a longer hash (3), definitions by name (4), references by name to symbols defined
elsewhere (5).

| file | what |
|---|---|
| `p1_kind.u8` | per slot: 1..5 as above |
| `p1_namekey.u64x2` | per slot: the 128-bit key of (name, ABI) if the slot takes part in the join by name, else (0,0). (k1 is never 0 for a real key.) Kinds 4 and 5 take part; kind 1 slots with a key are *candidates* (below); static, unnamed and debug-info symbols have no key |
| `p1_rank.u32` | per definition (kinds 1, 4): its rank. bit 31: not DUPOK; bit 30: has content (code or data bytes); bit 29: a definition; bits 0-27: min(size, 2^28-1). References: 0 |
| `p1_flags.u8` | per slot: bit 0: the record says UsedInIface; bit 1: a reference made only by debug information |
| `p1_want.u64x2` | keys of names the linker itself looks up (builtins, `main.main`, the entry, `-X` names...): they take part in the join with value 0 and no slot |
| `p1_builtin.u64x2` | the key of builtin `i` (`nbuiltin` of them), for references with package index "builtin" |
| `slot_rel.u32` | CSR (`nslots+2`): the relocations of slot `g` are `[slot_rel[g], slot_rel[g+1])` (definitions only) |
| `rel_off.i32 rel_siz.u8 rel_type.u16 rel_add.i64` | per relocation: offset in the symbol, width, cmd/internal/objabi type (bit 15 = weak), addend |
| `rel_pkg.u32 rel_idx.u32` | per relocation: the target as the object names it: (package index, symbol index) |
| `obj_pkg_off.u32`, `obj_pkg.u32` | CSR per object: `npkg+5` bases. Entry `p < npkg`: the id base of that imported package (0xFFFFFFFF: not linked); then 5 entries for the special package indices Self, Builtin, Hashed, Hashed64, None (= 2147483643..2147483647): the base of this object's own block of that kind |
| `p1_hashkey.u64x2` | per content-addressed slot: kind 2: (the 8 bytes, the symbol's size); kind 3: (hash bytes 0-7, hash bytes 8-11) |
| `p1c_mark.u8` | per content-addressed slot: 1 if the flood (problem 2) reached it, 2 if it was not reached but a relocation the flood does not follow names it, else 0 |

**1a. The join by name.** Group the slots with a key (kinds 4 and 5) and the wanted keys by key. A kind-1 candidate
joins a group only if its key is already there (it never creates a group). The winner of a group is the maximum
of `rank << 32 | ~id` (highest rank, then lowest id); every member's canonical id is the winner. Every other slot
is its own canonical id, except a kind-5 slot with `p1_flags` bit 1, whose canonical id is 0. A group whose
winner is a reference stands for an undefined symbol. Builtin `i` resolves to its key's winner if that is a
definition (rank bit 29), else 0. Oracle: `oracle/p1_canon.u32` (per slot), `oracle/p1_builtin.u32`.

**1b. Relocation targets.** For each relocation of slot `g` in object `o`: with `sp = rel_pkg - 2147483643`,
`t = sp < 5 ? npkg + sp : rel_pkg`; if `t < npkg+5` and `base = obj_pkg[o][t] != 0xFFFFFFFF`, the target slot is
`builtin[rel_idx]` when `sp == 1`, else `base + rel_idx`; then its canonical id (1a). Otherwise 0.
Oracle: `oracle/p1_reltarget.u32` (per relocation, in `slot_rel` order).

**1c. The content-hash join** (it runs after the flood in hlink, over what the flood reached: `p1c_mark`).
Among the slots with mark 1 or 2, group kind 2 by k1, kind 3 by (k1, k2). Winner: a mark-1 copy beats a mark-2
one; then for kind 2 the largest size (k2), then the lowest id; for kind 3 the lowest id. If the winner has mark
2 the group is not in the output and nothing changes. Otherwise every member's canonical id becomes the winner,
except a mark-2 kind-2 copy larger than the winner (it keeps itself). Oracle: `oracle/p1c_canon.u32` (the
final canonical id of every slot).

## Problem 2: reach

The graph after 1a (content-addressed copies are still themselves: the flood treats each copy as its own symbol).
Ids are canonical slot ids.

| file | what |
|---|---|
| `p2_flags.u8` | per slot: 1 def (a definition, not an undefined symbol), 2 text, 4 reflect (calls reflect's Method), 8 gotype, 16 iface (converted to an interface, known from resolve), 32 has something to follow |
| `p2_edge_off.u32`, `p2_edge_dst.u32`, `p2_edge_kind.u8` | CSR per source (only canonical definitions with flag 32 have rows). Kind 0 plain: a relocation or funcdata the flood follows, dst = target id. Kind 1 useiface: dst is a type converted to an interface (an itab already replaced by its type). Kind 2 ifacemethod: dst = a signature id (an interface method some live code may call). Kind 3 namedmethod: dst = a string id (a method name called by reflection-like generic code) |
| `p2_sig_name.u32`, `p2_sig_type.u32` | per signature id: (method name string id, function type id) |
| `p2_meth_off.u32` + `p2_meth_name.u32 p2_meth_mtyp.u32 p2_meth_ifn.u32 p2_meth_tfn.u32 p2_meth_exported.u8` | CSR per type: its methods: name string id, the method's function type, the interface-call entry, the direct entry (ids, 0 if none), and whether the name is exported |
| `p2_str.off.u64`, `p2_str.bin` | the method name strings |
| `p2_roots.u32` | the entry points (entry symbol, init tasks in order, a few runtime symbols) |

Rules (cmd/link's deadcode.go, as hlink does them; `s` ranges over live defined symbols):

    live(r)              for every root r
    live(t)              <- live(s), plain(s,t)
    iface(t)             <- flag 16 on t
    iface(t)             <- live(s), useiface(s,t)
    iface(t)             <- live(s), iface(s), gotype(s), plain(s,t), gotype(t), def(t)
    sig(x)               <- live(s), ifacemethod(s,x)
    named(n)             <- live(s), namedmethod(s,n)
    reflect              <- live(s), reflect(s)
    iface(m.mtyp), cand(m) <- live(s), iface(s), gotype(s), method(s,m)
    live(m.mtyp), live(m.ifn), live(m.tfn)
                         <- cand(m), (reflect and m.exported) or sig(m.name, m.mtyp) or named(m.name)

`sig(m.name, m.mtyp)` means some live signature id has that (name, type) pair. Oracle: `oracle/p2_live.u32`, the
sorted ids of every live symbol (663,630 for MinIO: this counts each content-addressed copy and undefined symbol;
after 1c, 360,376 canonical symbols are live: `oracle/live_names.txt`, which equals fclink's and the stock linker's
live list).

## Problem 3: lay out and relocate

The placed symbols: live canonical definitions that go into a bucket (a part of a section), in hlink's unit
order (a unit is an object, or a run of 8192 slots of a big one) and then id order.

| file | what |
|---|---|
| `p3_sym.u32` | per placed symbol: its id |
| `p3_bucket.u8` | 1 text, 2 text FIPS, 3 rodata, 4 rodata FIPS, 5 go:func, 6 typelink, 7 type, 8 itab, 9 noptrdata, 10 noptrdata FIPS, 11 data, 12 data FIPS, 13 bss, 14 noptrbss, 15 coverage counters |
| `p3_class.u8` | log2 of its alignment |
| `p3_size.u32`, `p3_unit.u32` | its size; its unit |
| `p3_tlstr.u32` + `p3_str.*` | typelink bucket only: its type string (string id) |
| `p3_bucket_start.u32`, `p3_bucket_minalign.u32` | per bucket (16 entries): its start address and minimum alignment |
| `p3_data.off.u64`, `p3_data.bin` | per placed symbol: its bytes (may be shorter than its size: the rest is zero; bss buckets: none) |
| `p3_rel_start.u32` + `p3_rel_off.u32 p3_rel_siz.u8 p3_rel_kind.u8 p3_rel_add.i64 p3_rel_tgt.u32` | CSR per placed symbol: its relocations that write bytes: offset, width (1, 2, 4, 8), kind (below), addend, target slot id |
| `p3_addr_all.u32` | per id (`nids`): the final address of what it resolves to (0: not in the output) |

**3a. Layout.** occupied(i) = size rounded up to 2^class, where a text symbol (buckets 1, 2) is at least `minFunc`
(16) bytes. For each bucket other than typelink: `va = start`; for each unit in order: if the unit has bytes in
this bucket, `va = round_up(va, max(minalign, 2^top))` with `top` its largest class with bytes; then for each class
from 12 down to 0: the class's symbols in list order get `va, va + occupied, ...`. Typelinks: sort by type string
(bytewise, then shorter first), then id; `va = start`; each gets `va = round_up(va, 2^class)` and then
`va += size` (not occupied). Oracle: `oracle/p3_addr.u32` (per placed symbol).

**3b. Relocate.** Copy each placed symbol's bytes to `image[addr - imageBase]` (not buckets 13-15) and apply its
relocations, `P = addr + off`, `T = p3_addr_all[tgt]`:

| kind | value written (`siz` bytes, little-endian) |
|---|---|
| 1 addr | `T + add`; T = 0 with tgt != 0 is an error |
| 2 addr, weak | as 1, but T = 0 with tgt != 0 means T = `addr_unreachableMethod` (meta) |
| 3 addroff | `T - sectbase(T) + add`; nothing if T = 0. sectbase: the largest of `lay_text, lay_rodata, lay_pclntab, lay_types, lay_noptrdata, lay_data, lay_bss, lay_noptrbss` (meta) not above T |
| 4 methodoff | as 3, but T = 0 writes 0xFFFFFFFF |
| 5 call, 7 pcrel | `T + add - (P + siz)`; tgt = 0: `add - (P + siz)`; T = 0 with tgt != 0 is an error |
| 6 call, weak | as 5, but T = 0 with tgt != 0 means T = `addr_mapinitnoop` (meta) |
| 8 const | `add` (TLS and size relocations folded in) |

Oracle: `oracle/p3_image.bin` is hlink's whole output file (file offset = address - imageBase); every placed
symbol's `[addr, addr+size)` must match it. hlink also writes the pclntab, GC bitmaps, type links, moduledata
and ELF headers; those are not part of problem 3.

## What the extraction did for the systems (so a timing is read fairly)

The flat files are not raw object files: they have names hashed into 128-bit keys (P1), references decoded
into canonical ids and itabs resolved to their types (P2), method tables decoded from type descriptors (P2), and
relocation types normalised and size relocations folded into constants (P3). hlink does that work inside its
phases; a system here does not. Compare a system's time with the plain C references on the same files first,
and with hlink's phase times only with that caveat.
#!/usr/bin/env bash
# Build the plain C references and check each problem against its oracle:  ref/flat/check.sh [data/minio]
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd); d=${1:-$here/data/minio}
mkdir -p "$here/build"
for p in p1 p2 p3; do gcc -O2 -march=native -o "$here/build/$p" "$here/ref/flat/$p.c"; done
ok=0
for p in p1 p2 p3; do "$here/build/$p" "$d" || ok=1; done
exit $ok
     1	// p1.c: problem 1 (resolve) from the flat files, plainly, on one core: the reference that shows the flat files
     2	// say everything resolve needs. Checks itself against oracle/p1_canon.u32, oracle/p1_builtin.u32,
     3	// oracle/p1_reltarget.u32 and (the content-hash join, 1c) oracle/p1c_canon.u32.
     4	//   ref/flat/p1 data/minio
     5	#include "flat.h"
     6	
     7	#define rankDef (1u << 29)
     8	#define PkgIdxSelf 2147483643u
     9	
    10	typedef struct {
    11		u64 k1, k2, val; // val = rank << 32 | ~id: the maximum is the winner
    12	} ent;
    13	static ent *tab;
    14	static u64 mask;
    15	static ent *find(u64 k1, u64 k2, int add) {
    16		for (u64 h = mix64(k1) & mask;; h = (h + 1) & mask) {
    17			if (tab[h].k1 == 0) {
    18				if (!add) return NULL;
    19				tab[h].k1 = k1, tab[h].k2 = k2, tab[h].val = 0;
    20				return &tab[h];
    21			}
    22			if (tab[h].k1 == k1 && tab[h].k2 == k2) return &tab[h];
    23		}
    24	}
    25	
    26	int main(int argc, char **argv) {
    27		flatDir = argc > 1 ? argv[1] : "data/minio";
    28		u32 N = (u32)metaGet("nslots"), nobjs = (u32)metaGet("nobjs"), nb = (u32)metaGet("nbuiltin");
    29		size_t nw, nrel;
    30		const u8 *kind = FMAP(u8, "p1_kind.u8", NULL), *flags = FMAP(u8, "p1_flags.u8", NULL);
    31		const u64 *key = FMAP(u64, "p1_namekey.u64x2", NULL), *want = FMAP(u64, "p1_want.u64x2", &nw);
    32		const u64 *bkey = FMAP(u64, "p1_builtin.u64x2", NULL);
    33		const u32 *rank = FMAP(u32, "p1_rank.u32", NULL), *slotRel = FMAP(u32, "slot_rel.u32", NULL);
    34		const u32 *slotObj = FMAP(u32, "slot_obj.u32", NULL), *pkgOff = FMAP(u32, "obj_pkg_off.u32", NULL);
    35		const u32 *pkg = FMAP(u32, "obj_pkg.u32", NULL);
    36		const u32 *rPkg = FMAP(u32, "rel_pkg.u32", &nrel), *rIdx = FMAP(u32, "rel_idx.u32", NULL);
    37		nrel /= 4;
    38		nw /= 16;
    39		const u64 *hkey = FMAP(u64, "p1_hashkey.u64x2", NULL);
    40		const u8 *hmark = FMAP(u8, "p1c_mark.u8", NULL);
    41	
    42		double t0 = now();
    43		// 1a. the join by name: the linker's wanted names, then every definition and reference by name
    44		u64 sz = 1;
    45		while (sz < 4 * (u64)(N / 4 + nw)) sz <<= 1;
    46		tab = calloc(sz, sizeof(ent));
    47		mask = sz - 1;
    48		for (size_t i = 0; i < nw; i++) find(want[2 * i], want[2 * i + 1], 1);
    49		u32 *canon = malloc(4 * ((size_t)N + 1));
    50		canon[0] = 0;
    51		for (u32 g = 1; g <= N; g++) {
    52			canon[g] = g;
    53			if ((flags[g] & 2)) canon[g] = 0; // a reference that only debug information makes
    54			if ((kind[g] != 4 && kind[g] != 5) || key[2 * (size_t)g] == 0) continue;
    55			ent *e = find(key[2 * (size_t)g], key[2 * (size_t)g + 1], 1);
    56			u64 v = (u64)rank[g] << 32 | (u32)~g;
    57			if (v > e->val) e->val = v;
    58		}
    59		// definitions by package index join only a name that is already there
    60		for (u32 g = 1; g <= N; g++) {
    61			if (kind[g] != 1 || key[2 * (size_t)g] == 0) continue;
    62			ent *e = find(key[2 * (size_t)g], key[2 * (size_t)g + 1], 0);
    63			if (!e) continue;
    64			u64 v = (u64)rank[g] << 32 | (u32)~g;
    65			if (v > e->val) e->val = v;
    66		}
    67		for (u32 g = 1; g <= N; g++) {
    68			if (kind[g] == 2 || kind[g] == 3 || key[2 * (size_t)g] == 0) continue;
    69			ent *e = find(key[2 * (size_t)g], key[2 * (size_t)g + 1], 0);
    70			if (e) canon[g] = ~(u32)e->val;
    71		}
    72		u32 *builtin = calloc(nb, 4);
    73		for (u32 i = 0; i < nb; i++) {
    74			ent *e = find(bkey[2 * i], bkey[2 * i + 1], 0);
    75			if (e && e->val && ((e->val >> 32) & rankDef)) builtin[i] = ~(u32)e->val;
    76		}
    77		double t1 = now();
    78		// 1b. every relocation's target: a package-indexed reference is a read of the object's package table
    79		u32 *tgt = malloc(4 * nrel);
    80		for (u32 g = 1; g <= N; g++) {
    81			u32 o = slotObj[g];
    82			const u32 *pb = pkg + pkgOff[o];
    83			u32 npk = pkgOff[o + 1] - pkgOff[o] - 5;
    84			for (u32 r = slotRel[g]; r < slotRel[g + 1]; r++) {
    85				u32 p = rPkg[r], idx = rIdx[r], sp = p - PkgIdxSelf, t = sp < 5 ? npk + sp : p, s = 0;
    86				if (t < npk + 5 && pb[t] != 0xFFFFFFFFu) s = sp == 1 ? (idx < nb ? builtin[idx] : 0) : pb[t] + idx;
    87				tgt[r] = s ? canon[s] : 0;
    88			}
    89		}
    90		double t2 = now();
    91		printf("p1 (1 core): join by name %.1f ms, relocation targets %.1f ms\n", (t1 - t0) * 1e3, (t2 - t1) * 1e3);
    92		long bad = check32("canon", canon, (size_t)N + 1, "oracle/p1_canon.u32");
    93		bad += check32("builtin", builtin, nb, "oracle/p1_builtin.u32");
    94		bad += check32("reltarget", tgt, nrel, "oracle/p1_reltarget.u32");
    95	
    96		// 1c. the content-hash join over the copies the flood reached (mark 1) or names without following (2)
    97		double t3 = now();
    98		typedef struct {
    99			u64 k1, k2;
   100			u32 win;
   101		} hent;
   102		u64 hs = 1;
   103		while (hs < 4 * (u64)(N / 2)) hs <<= 1;
   104		hent *ht = calloc(hs, sizeof(hent));
   105		u64 hm = hs - 1;
   106		// a group: the same kind (short or long hash) and key; short: the largest copy wins, then the lowest id;
   107		// long: the lowest id; a copy marked 2 never beats one marked 1
   108	#define HFIND(g, e)                                                                                         \
   109		do {                                                                                                    \
   110			u64 k1 = hkey[2 * (size_t)(g)], k2 = kind[g] == 2 ? 0 : hkey[2 * (size_t)(g) + 1] | (1ull << 63);  \
   111			for (u64 h = mix64(k1 ^ k2) & hm;; h = (h + 1) & hm) {                                             \
   112				if (ht[h].win == 0) {                                                                           \
   113					ht[h].k1 = k1, ht[h].k2 = k2;                                                               \
   114					e = &ht[h];                                                                                 \
   115					break;                                                                                      \
   116				}                                                                                               \
   117				if (ht[h].k1 == k1 && ht[h].k2 == k2) {                                                         \
   118					e = &ht[h];                                                                                 \
   119					break;                                                                                      \
   120				}                                                                                               \
   121			}                                                                                                   \
   122		} while (0)
   123		for (u32 g = 1; g <= N; g++) {
   124			if ((kind[g] != 2 && kind[g] != 3) || hmark[g] == 0) continue;
   125			hent *e;
   126			HFIND(g, e);
   127			if (e->win == 0) { e->win = g; continue; }
   128			u32 w = e->win;
   129			int wr = hmark[w] == 2, gr = hmark[g] == 2;
   130			int better;
   131			if (wr != gr) better = wr; // a reached copy beats one only named
   132			else if (kind[g] == 2) better = hkey[2 * (size_t)g + 1] > hkey[2 * (size_t)w + 1]; // larger; ties: lower id (g > w)
   133			else better = 0;
   134			if (better) e->win = g;
   135		}
   136		u32 *canon2 = malloc(4 * ((size_t)N + 1));
   137		memcpy(canon2, canon, 4 * ((size_t)N + 1));
   138		for (u32 g = 1; g <= N; g++) {
   139			if ((kind[g] != 2 && kind[g] != 3) || hmark[g] == 0) continue;
   140			hent *e;
   141			HFIND(g, e);
   142			u32 w = e->win;
   143			if (hmark[w] == 2 || w == g) continue; // nothing reached the class: not in the output
   144			if (kind[g] == 2 && hmark[g] == 2 && hkey[2 * (size_t)g + 1] > hkey[2 * (size_t)w + 1]) continue;
   145			canon2[g] = w;
   146		}
   147		double t4 = now();
   148		printf("p1c (1 core): content-hash join %.1f ms\n", (t4 - t3) * 1e3);
   149		bad += check32("canon after 1c", canon2, (size_t)N + 1, "oracle/p1c_canon.u32");
   150		printf(bad ? "P1 FAIL\n" : "P1 OK\n");
   151		return bad != 0;
   152	}
     1	// p2.c: problem 2 (reach) from the flat files, plainly, on one core, with a worklist. Checks itself against
     2	// oracle/p2_live.u32. The rules are data/README.md's (they are cmd/link's deadcode.go):
     3	//   live(r)  for every root r
     4	//   live(t)  <- live(s), plain(s,t)
     5	//   iface(t) <- live(s), iface(s), gotype(s), plain(s,t), gotype(t), def(t)
     6	//   iface(t) <- live(s), useiface(s,t)
     7	//   sig(x)   <- live(s), ifacemethod(s,x);   named(n) <- live(s), namedmethod(s,n);   reflect <- live(s), reflect(s)
     8	//   iface(m.mtyp) and markable(m) <- live(s), iface(s), gotype(s), method(s,m)
     9	//   live(m.mtyp), live(m.ifn), live(m.tfn) <- markable(m), (reflect & exported(m) | sig(m.name, m.mtyp) | named(m.name))
    10	// (only defined symbols have edges; edges are given only for them).
    11	//   ref/flat/p2 data/minio
    12	#include "flat.h"
    13	
    14	enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16 };
    15	
    16	static u8 *live, *iface, *ifaceDone;
    17	static u32 *stack;
    18	static size_t sp;
    19	static inline void mark(u32 c) {
    20		if (c && !live[c]) {
    21			live[c] = 1;
    22			stack[sp++] = c;
    23		}
    24	}
    25	static inline void setIface(u32 c) {
    26		if (c && !iface[c]) {
    27			iface[c] = 1;
    28			if (live[c]) stack[sp++] = c;
    29		}
    30	}
    31	
    32	int main(int argc, char **argv) {
    33		flatDir = argc > 1 ? argv[1] : "data/minio";
    34		u32 N = (u32)metaGet("nids"), nsl = (u32)metaGet("nslots");
    35		size_t nroots, nsig, nstr, nmeth;
    36		const u8 *fl = FMAP(u8, "p2_flags.u8", NULL), *ek = FMAP(u8, "p2_edge_kind.u8", NULL);
    37		const u32 *eoff = FMAP(u32, "p2_edge_off.u32", NULL), *ed = FMAP(u32, "p2_edge_dst.u32", NULL);
    38		const u32 *sigName = FMAP(u32, "p2_sig_name.u32", &nsig), *sigType = FMAP(u32, "p2_sig_type.u32", NULL);
    39		const u32 *moff = FMAP(u32, "p2_meth_off.u32", NULL), *mName = FMAP(u32, "p2_meth_name.u32", &nmeth);
    40		const u32 *mTyp = FMAP(u32, "p2_meth_mtyp.u32", NULL), *mIfn = FMAP(u32, "p2_meth_ifn.u32", NULL), *mTfn = FMAP(u32, "p2_meth_tfn.u32", NULL);
    41		const u8 *mExp = FMAP(u8, "p2_meth_exported.u8", NULL);
    42		const u32 *roots = FMAP(u32, "p2_roots.u32", &nroots);
    43		FMAP(u64, "p2_str.off.u64", &nstr);
    44		nroots /= 4, nsig /= 4, nmeth /= 4, nstr = nstr / 8 - 1;
    45	
    46		double t0 = now();
    47		live = calloc(N, 1), iface = calloc(N, 1), ifaceDone = calloc(N, 1);
    48		stack = malloc(4 * (size_t)N * 3);
    49		for (u32 g = 1; g <= nsl; g++) iface[g] = (fl[g] & fIface) != 0;
    50		u8 *sigSeen = calloc(nsig, 1), *named = calloc(nstr + 1, 1);
    51		// sig ids are (name, type) pairs; a method's (name, mtyp) is looked up in a hash of the live sigs
    52		u64 hs = 1;
    53		while (hs < 4 * nsig + 64) hs <<= 1;
    54		u64 *sigTab = calloc(hs, 8);
    55		u32 *markable = malloc(4 * (nmeth + 1)), nm = 0;
    56		int reflectSeen = 0, rounds = 0;
    57		for (size_t i = 0; i < nroots; i++) mark(roots[i]);
    58		for (;;) {
    59			rounds++;
    60			while (sp) {
    61				u32 c = stack[--sp];
    62				if (c > nsl || !(fl[c] & fDef)) continue;
    63				if (fl[c] & fReflect) reflectSeen = 1;
    64				int uii = (fl[c] & fGoType) && iface[c];
    65				if (uii) {
    66					if (ifaceDone[c]) continue;
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
     1	// p3.c: problem 3 (lay out, then relocate) from the flat files, plainly, on one core. Checks itself against
     2	// oracle/p3_addr.u32 (3a) and the bytes of every placed symbol in oracle/p3_image.bin (3b).
     3	//   ref/flat/p3 data/minio [out.img]
     4	#include "flat.h"
     5	
     6	enum { bText = 1, bTextFIPS = 2, bTypelink = 6, bBSS = 13, bNoptrBSS = 14, bCov = 15, nBuckets = 16, nClasses = 13 };
     7	static inline u32 rnd(u32 v, u32 a) { return (v + a - 1) & ~(a - 1); }
     8	
     9	static const u64 *soff;
    10	static const u8 *sbin;
    11	static const u32 *tlstr, *psym;
    12	static int cmpTL(const void *a, const void *b) {
    13		u32 x = *(const u32 *)a, y = *(const u32 *)b;
    14		u64 xo = soff[tlstr[x]], xn = soff[tlstr[x] + 1] - xo, yo = soff[tlstr[y]], yn = soff[tlstr[y] + 1] - yo;
    15		u64 n = xn < yn ? xn : yn;
    16		int c = n ? memcmp(sbin + xo, sbin + yo, n) : 0;
    17		if (c) return c;
    18		if (xn != yn) return xn < yn ? -1 : 1;
    19		return psym[x] < psym[y] ? -1 : psym[x] > psym[y];
    20	}
    21	
    22	int main(int argc, char **argv) {
    23		flatDir = argc > 1 ? argv[1] : "data/minio";
    24		u32 L = (u32)metaGet("p3_placed"), nunits = (u32)metaGet("nunits"), minFunc = (u32)metaGet("minFunc");
    25		u32 imageBase = (u32)metaGet("imageBase");
    26		psym = FMAP(u32, "p3_sym.u32", NULL);
    27		const u8 *pb = FMAP(u8, "p3_bucket.u8", NULL), *pcl = FMAP(u8, "p3_class.u8", NULL);
    28		const u32 *psize = FMAP(u32, "p3_size.u32", NULL), *punit = FMAP(u32, "p3_unit.u32", NULL);
    29		tlstr = FMAP(u32, "p3_tlstr.u32", NULL);
    30		soff = FMAP(u64, "p3_str.off.u64", NULL);
    31		sbin = FMAP(u8, "p3_str.bin", NULL);
    32		const u32 *bstart = FMAP(u32, "p3_bucket_start.u32", NULL), *bmin = FMAP(u32, "p3_bucket_minalign.u32", NULL);
    33	
    34		// ---- 3a. addresses
    35		double t0 = now();
    36		u32 *addr = calloc(L, 4);
    37		// bytes per (unit, bucket, class), then each unit's block of each bucket in unit order, classes from the largest
    38		// alignment down; inside a class the symbols in list order
    39		u32 *csize = calloc((size_t)nunits * nBuckets * nClasses, 4);
    40	#define CS(u, b, c) csize[((size_t)(u) * nBuckets + (b)) * nClasses + (c)]
    41		u32 *occ = malloc(4 * (size_t)L);
    42		for (u32 i = 0; i < L; i++) {
    43			u32 s = psize[i], b = pb[i];
    44			if ((b == bText || b == bTextFIPS) && s < minFunc) s = minFunc;
    45			occ[i] = rnd(s, 1u << pcl[i]);
    46			if (b != bTypelink) CS(punit[i], b, pcl[i]) += occ[i];
    47		}
    48		for (u32 b = 1; b < nBuckets; b++) {
    49			if (b == bTypelink) continue;
    50			u32 va = bstart[b];
    51			for (u32 u = 0; u < nunits; u++) {
    52				int top = -1;
    53				for (int c = nClasses - 1; c >= 0; c--)
    54					if (CS(u, b, c)) { top = c; break; }
    55				if (top >= 0) va = rnd(va, bmin[b] > (1u << top) ? bmin[b] : 1u << top);
    56				for (int c = nClasses - 1; c >= 0; c--) {
    57					u32 n = CS(u, b, c);
    58					CS(u, b, c) = va; // from here on: the class's next address
    59					va += n;
    60				}
    61			}
    62		}
    63		u32 ntl = 0;
    64		u32 *tl = malloc(4 * (size_t)L);
    65		for (u32 i = 0; i < L; i++) {
    66			if (pb[i] == bTypelink) { tl[ntl++] = i; continue; }
    67			addr[i] = CS(punit[i], pb[i], pcl[i]);
    68			CS(punit[i], pb[i], pcl[i]) += occ[i];
    69		}
    70		qsort(tl, ntl, 4, cmpTL);
    71		u32 va = bstart[bTypelink];
    72		for (u32 k = 0; k < ntl; k++) {
    73			u32 i = tl[k];
    74			va = rnd(va, 1u << pcl[i]);
    75			addr[i] = va;
    76			va += psize[i];
    77		}
    78		double t1 = now();
    79		printf("p3a (1 core): layout %.1f ms (%u symbols, %u sorted by type string)\n", (t1 - t0) * 1e3, L, ntl);
    80		long bad = check32("addr", addr, L, "oracle/p3_addr.u32");
    81	
    82		// ---- 3b. copy and relocate, with the addresses the oracle gives (so 3b does not depend on 3a)
    83		size_t nimg;
    84		const u8 *want = FMAP(u8, "oracle/p3_image.bin", &nimg);
    85		const u32 *oaddr = FMAP(u32, "oracle/p3_addr.u32", NULL), *all = FMAP(u32, "p3_addr_all.u32", NULL);
    86		const u64 *doff = FMAP(u64, "p3_data.off.u64", NULL);
    87		const u8 *dbin = FMAP(u8, "p3_data.bin", NULL);
    88		const u32 *rstart = FMAP(u32, "p3_rel_start.u32", NULL), *roff = FMAP(u32, "p3_rel_off.u32", NULL), *rtgt = FMAP(u32, "p3_rel_tgt.u32", NULL);
    89		const u8 *rsiz = FMAP(u8, "p3_rel_siz.u8", NULL), *rkind = FMAP(u8, "p3_rel_kind.u8", NULL);
    90		const i64 *radd = FMAP(i64, "p3_rel_add.i64", NULL);
    91		u32 sect[8] = {(u32)metaGet("lay_text"), (u32)metaGet("lay_rodata"), (u32)metaGet("lay_pclntab"), (u32)metaGet("lay_types"),
    92		               (u32)metaGet("lay_noptrdata"), (u32)metaGet("lay_data"), (u32)metaGet("lay_bss"), (u32)metaGet("lay_noptrbss")};
    93		u32 unreach = (u32)metaGet("addr_unreachableMethod"), mapinit = (u32)metaGet("addr_mapinitnoop");
    94		u8 *img = calloc(nimg, 1);
    95		long nerr = 0;
    96		double t2 = now();
    97		for (u32 i = 0; i < L; i++) {
    98			u32 b = pb[i];
    99			if (b == bBSS || b == bNoptrBSS || b == bCov) continue;
   100			u32 a = oaddr[i];
   101			u8 *P = img + (a - imageBase);
   102			memcpy(P, dbin + doff[i], doff[i + 1] - doff[i]);
   103			for (u32 r = rstart[i]; r < rstart[i + 1]; r++) {
   104				u32 tg = rtgt[r], ta = all[tg], off = roff[r], sz = rsiz[r];
   105				i64 v;
   106				switch (rkind[r]) {
   107				case 2: if (ta == 0 && tg != 0) ta = unreach; /* fallthrough */
   108				case 1:
   109					if (ta == 0 && tg != 0) { nerr++; continue; }
   110					v = (i64)ta + radd[r];
   111					break;
   112				case 4: if (ta == 0) { memset(P + off, 0xFF, 4); continue; } /* fallthrough */
   113				case 3: {
   114					if (ta == 0) continue;
   115					int k = 7;
   116					while (k > 0 && ta < sect[k]) k--;
   117					v = (i64)(ta - sect[k]) + radd[r];
   118					break;
   119				}
   120				case 6: if (ta == 0 && tg != 0) ta = mapinit; /* fallthrough */
   121				case 5:
   122				case 7:
   123					if (ta == 0 && tg != 0) { nerr++; continue; }
   124					v = tg == 0 ? radd[r] - (i64)(a + off + sz) : (i64)ta + radd[r] - (i64)(a + off + sz);
   125					break;
   126				case 8: v = radd[r]; break;
   127				default: nerr++; continue;
   128				}
   129				switch (sz) {
   130				case 8: memcpy(P + off, &v, 8); break;
   131				case 4: { u32 x = (u32)v; memcpy(P + off, &x, 4); break; }
   132				case 2: { u16 x = (u16)v; memcpy(P + off, &x, 2); break; }
   133				case 1: P[off] = (u8)v; break;
   134				default: nerr++;
   135				}
   136			}
   137		}
   138		double t3 = now();
   139		long nbad = 0;
   140		u64 nbytes = 0, sum = 0;
   141		for (u32 i = 0; i < L; i++) {
   142			u32 b = pb[i];
   143			if (b == bBSS || b == bNoptrBSS || b == bCov) continue;
   144			size_t o = oaddr[i] - imageBase, n = psize[i];
   145			nbytes += n;
   146			for (size_t k = 0; k < n; k++) sum = sum * 1099511628211ull + img[o + k];
   147			if (memcmp(img + o, want + o, n) != 0 && nbad++ < 5) printf("  symbol %u (id %u, bucket %u) differs\n", i, psym[i], b);
   148		}
   149		printf("p3b (1 core): copy + relocate %.1f ms, %llu bytes, %u relocations, %ld errors; checksum %016llx\n", (t3 - t2) * 1e3,
   150		       (unsigned long long)nbytes, rstart[L], nerr, (unsigned long long)sum);
   151		printf("image: %ld placed symbols differ from oracle/p3_image.bin\n", nbad);
   152		if (argc > 2) {
   153			FILE *f = fopen(argv[2], "wb");
   154			fwrite(img, 1, nimg, f);
   155			fclose(f);
   156		}
   157		bad += nbad + (nerr != 0);
   158		printf(bad ? "P3 FAIL\n" : "P3 OK\n");
   159		return bad != 0;
   160	}

exec
/bin/bash -lc 'nl -ba ref/hlink/dumpflat.c; rg --files systems results build data/minio | head -n 100' in /home/exedev/paradigms-w
 succeeded in 0ms:
     1	// dumpflat.c (paradigms repo; not part of bottom-up's hlink): -dumpflat DIR writes the flat files that every
     2	// system of the paradigm grid reads, and the oracles, from hlink's own state at the phase boundaries.
     3	// data/README.md (written by ref/flat/readme.sh) says what each file is. Snapshots: after resolve (canon and
     4	// flags, before the content-hash join), at the end of the flood (the marks, before the content-hash join);
     5	// everything else is read at the end of the link.
     6	
     7	static u32 *flatCanon1; // canon after resolve (content-addressed copies are still themselves)
     8	static u8 *flatFL1;     // FL after resolve
     9	static u8 *flatFL2;     // FL at the end of the flood
    10	
    11	static void flatSnapResolve(void) {
    12		flatCanon1 = malloc((size_t)nids * 4);
    13		memcpy(flatCanon1, canon, (size_t)nids * 4);
    14		flatFL1 = malloc(nids);
    15		for (u32 g = 0; g < nids; g++) flatFL1[g] = FL(g);
    16	}
    17	static void flatSnapFlood(void) {
    18		flatFL2 = malloc(nids);
    19		for (u32 g = 0; g < nids; g++) flatFL2[g] = FL(g);
    20	}
    21	
    22	static const char *flatDir;
    23	static void wfile(const char *name, const void *p, size_t n) {
    24		char path[4096];
    25		snprintf(path, sizeof path, "%s/%s", flatDir, name);
    26		FILE *f = fopen(path, "wb");
    27		if (!f) fatalf("dumpflat: %s: %s", path, strerror(errno));
    28		if (n && fwrite(p, 1, n, f) != n) fatalf("dumpflat: write %s", path);
    29		fclose(f);
    30	}
    31	#define WVEC(name, v) wfile(name, (v).a, (v).n * sizeof *(v).a)
    32	static FILE *metaF;
    33	static void meta(const char *k, unsigned long long v) { fprintf(metaF, "%s %llu\n", k, v); }
    34	
    35	// a quiet oref: 0 for anything hlink would report as an error
    36	static u32 orefQ(const object *o, u32 pkg, u32 idx) {
    37		u32 sp = pkg - PkgIdxSelf;
    38		u32 t = sp < 5 ? o->npkg + sp : pkg;
    39		if (t >= o->npkg + 5) return 0;
    40		u32 b = o->pkgBase[t];
    41		if (b == badBase) return 0;
    42		if (sp == 1) return idx < NBUILTIN ? builtins[idx] : 0;
    43		return b + idx;
    44	}
    45	static inline u32 c1(u32 g) { return g < nids ? flatCanon1[g] : 0; }
    46	
    47	// interned strings
    48	typedef VEC(u64) vecu64;
    49	typedef struct {
    50		strmap m;
    51		vecu8 bin;
    52		vecu64 off;
    53	} strtab;
    54	static void stInit(strtab *t) {
    55		smInit(&t->m, 1 << 16);
    56		u64 z = 0;
    57		VPUSH(t->off, z); // string 0 is ""
    58		u64 e = 0;
    59		VPUSH(t->off, e);
    60		bool added;
    61		smPut(&t->m, (str){"", 0}, 0, &added);
    62	}
    63	static u32 stId(strtab *t, str s) {
    64		bool added;
    65		long *v = smPut(&t->m, s, (long)(t->off.n - 1), &added);
    66		if (added) {
    67			for (u32 i = 0; i < s.n; i++) VPUSH(t->bin, (u8)s.p[i]);
    68			u64 e = t->bin.n;
    69			VPUSH(t->off, e);
    70		}
    71		return (u32)*v;
    72	}
    73	static void stWrite(strtab *t, const char *prefix) {
    74		char a[256];
    75		snprintf(a, sizeof a, "%s.off.u64", prefix);
    76		WVEC(a, t->off);
    77		snprintf(a, sizeof a, "%s.bin", prefix);
    78		WVEC(a, t->bin);
    79	}
    80	
    81	static void dumpFlat(void) {
    82		flatDir = flagDumpFlat;
    83		mkdir(flatDir, 0777);
    84		char mp[4096];
    85		snprintf(mp, sizeof mp, "%s/meta.txt", flatDir);
    86		metaF = fopen(mp, "w");
    87		u32 N = nslots;
    88		meta("nslots", N);
    89		meta("nids", nids);
    90		meta("nobjs", (u64)nobjs);
    91		meta("nunits", (u64)nunits);
    92		meta("imageBase", imageBase);
    93	
    94		// ---------------------------------------------------------------- common: slots
    95		u32 *slotObj = calloc(N + 1, 4);
    96		vecu64 nameOff = {0};
    97		vecu8 nameBin = {0};
    98		u64 z = 0;
    99		VPUSH(nameOff, z); // slot 0
   100		for (int i = 0; i < nobjs; i++) {
   101			object *o = objs[i];
   102			for (u32 j = 0; j < o->total; j++) {
   103				u32 g = o->base + j;
   104				slotObj[g] = (u32)i;
   105				str n = symNameOf(o, osym(o, j));
   106				for (u32 k = 0; k < n.n; k++) VPUSH(nameBin, (u8)n.p[k]);
   107				u64 e = nameBin.n;
   108				VPUSH(nameOff, e);
   109			}
   110		}
   111		u64 e0 = nameBin.n;
   112		VPUSH(nameOff, e0);
   113		wfile("slot_obj.u32", slotObj, (size_t)(N + 1) * 4);
   114		WVEC("name.off.u64", nameOff);
   115		WVEC("name.bin", nameBin);
   116	
   117		// ---------------------------------------------------------------- P1: resolve
   118		u8 *kind = calloc(N + 1, 1), *p1f = calloc(N + 1, 1), *hmark = calloc(N + 1, 1);
   119		u64 *nkey = calloc(2 * (size_t)(N + 1), 8), *hkey = calloc(2 * (size_t)(N + 1), 8);
   120		u32 *rank = calloc(N + 1, 4);
   121		u32 *slotRel = calloc(N + 2, 4);
   122		VEC(i32) relOff = {0};
   123		vecu8 relSiz = {0};
   124		VEC(u16) relType = {0};
   125		VEC(i64) relAdd = {0};
   126		vecu32 relPkg = {0}, relIdx = {0}, relTgt = {0};
   127		u32 *objBase = calloc((size_t)nobjs, 4), *objPkgOff = calloc((size_t)nobjs + 1, 4);
   128		vecu32 objPkg = {0};
   129		long nNamed = 0, nPkgCand = 0;
   130		for (int i = 0; i < nobjs; i++) {
   131			object *o = objs[i];
   132			objBase[i] = o->base;
   133			objPkgOff[i] = (u32)objPkg.n;
   134			for (u32 t = 0; t < o->npkg + 5; t++) VPUSH(objPkg, o->pkgBase[t]);
   135			u32 h64Lo = o->ndef, hLo = o->ndef + o->nh64, npLo = hLo + o->nh;
   136			const u8 *h64 = o->b + o->off[BlkHash64], *hh = o->b + o->off[BlkHash];
   137			for (u32 j = 0; j < o->total; j++) {
   138				u32 g = o->base + j;
   139				const u8 *s = osym(o, j);
   140				slotRel[g] = (u32)relOff.n;
   141				u8 k = j < h64Lo ? 1 : j < hLo ? 2 : j < npLo ? 3 : j < o->nalldef ? 4 : 5;
   142				kind[g] = k;
   143				bool dwarf = isDwarfKind(symType(s));
   144				if (symUsedInIface(s)) p1f[g] |= 1;
   145				if (k == 5 && dwarf) p1f[g] |= 2;
   146				bool named = false;
   147				if (k == 1 || k == 4) named = !dwarf && symABI(s) != SymABIstatic && symNameLen(s) != 0;
   148				if (k == 5) named = !dwarf;
   149				if (named) {
   150					key128 kk = nameKey128(o->b + ld32(s + 4), symNameLen(s), symABI(s));
   151					nkey[2 * (size_t)g] = kk.k1;
   152					nkey[2 * (size_t)g + 1] = kk.k2;
   153					if (k == 1) nPkgCand++;
   154					else nNamed++;
   155				}
   156				if (k == 1 || k == 4) rank[g] = rankOf(o, j, s) & ~rankIface;
   157				if (k == 2) {
   158					hkey[2 * (size_t)g] = ld64(h64 + (size_t)(j - h64Lo) * 8);
   159					hkey[2 * (size_t)g + 1] = symSiz(s);
   160				} else if (k == 3) {
   161					const u8 *h = hh + (size_t)(j - hLo) * HashSize;
   162					hkey[2 * (size_t)g] = ld64(h);
   163					hkey[2 * (size_t)g + 1] = ld32(h + 8);
   164				}
   165				if (k == 2 || k == 3) {
   166					u8 f = flatFL2[g];
   167					hmark[g] = (f & hashedFlags) ? 1 : (f & flRef) ? 2 : 0;
   168				}
   169				if (j < o->nalldef) {
   170					u32 nr;
   171					relocp rs = oRelocs(o, j, &nr);
   172					for (u32 r = 0; r < nr; r++) {
   173						relocp rl = RELOC(rs, r);
   174						VPUSH(relOff, rOff(rl));
   175						VPUSH(relSiz, rSiz(rl));
   176						VPUSH(relType, rType(rl));
   177						VPUSH(relAdd, rAdd(rl));
   178						VPUSH(relPkg, rPkg(rl));
   179						VPUSH(relIdx, rIdx(rl));
   180						u32 tg = orefQ(o, rPkg(rl), rIdx(rl));
   181						VPUSH(relTgt, c1(tg));
   182					}
   183				}
   184			}
   185		}
   186		objPkgOff[nobjs] = (u32)objPkg.n;
   187		slotRel[N + 1] = (u32)relOff.n;
   188		slotRel[0] = 0;
   189		meta("nrel", relOff.n);
   190		meta("p1_named", (u64)nNamed);
   191		meta("p1_pkgdef_candidates", (u64)nPkgCand);
   192		wfile("p1_kind.u8", kind, N + 1);
   193		wfile("p1_flags.u8", p1f, N + 1);
   194		wfile("p1_namekey.u64x2", nkey, 16 * (size_t)(N + 1));
   195		wfile("p1_rank.u32", rank, 4 * (size_t)(N + 1));
   196		wfile("p1_hashkey.u64x2", hkey, 16 * (size_t)(N + 1));
   197		wfile("p1c_mark.u8", hmark, N + 1);
   198		wfile("slot_rel.u32", slotRel, 4 * (size_t)(N + 2));
   199		WVEC("rel_off.i32", relOff);
   200		WVEC("rel_siz.u8", relSiz);
   201		WVEC("rel_type.u16", relType);
   202		WVEC("rel_add.i64", relAdd);
   203		WVEC("rel_pkg.u32", relPkg);
   204		WVEC("rel_idx.u32", relIdx);
   205		wfile("obj_base.u32", objBase, 4 * (size_t)nobjs);
   206		wfile("obj_pkg_off.u32", objPkgOff, 4 * ((size_t)nobjs + 1));
   207		WVEC("obj_pkg.u32", objPkg);
   208		{
   209			// the names the linker joins without a slot of its own (resolve's wantName calls, in order)
   210			VEC(u64) want = {0};
   211	#define WANT(nm, abi)                          \
   212		do {                                       \
   213			key128 kk = keyOfStr(cstr(nm), (abi)); \
   214			VPUSH(want, kk.k1);                    \
   215			VPUSH(want, kk.k2);                    \
   216		} while (0)
   217			for (int i = 0; i < NBUILTIN; i++) WANT(builtinNames[i].name, (u16)builtinNames[i].abi);
   218			for (size_t i = 0; i < sizeof linkerNames / sizeof linkerNames[0]; i++) {
   219				WANT(linkerNames[i], 0);
   220				WANT(linkerNames[i], 1);
   221			}
   222			for (size_t i = 0; i < strdata.n; i++) WANT(strdata.a[i].name, 0);
   223			WANT(flagEntry, 0);
   224			WANT(flagEntry, 1);
   225			WVEC("p1_want.u64x2", want);
   226			VEC(u64) bk = {0};
   227			for (int i = 0; i < NBUILTIN; i++) {
   228				key128 kk = keyOfStr(cstr(builtinNames[i].name), (u16)builtinNames[i].abi);
   229				VPUSH(bk, kk.k1);
   230				VPUSH(bk, kk.k2);
   231			}
   232			WVEC("p1_builtin.u64x2", bk);
   233			meta("nbuiltin", NBUILTIN);
   234	#undef WANT
   235		}
   236		mkdir((snprintf(mp, sizeof mp, "%s/oracle", flatDir), mp), 0777);
   237		wfile("oracle/p1_canon.u32", flatCanon1, 4 * (size_t)(N + 1));
   238		WVEC("oracle/p1_reltarget.u32", relTgt);
   239		wfile("oracle/p1c_canon.u32", canon, 4 * (size_t)(N + 1));
   240		{
   241			u32 *bt = calloc(NBUILTIN, 4);
   242			memcpy(bt, builtins, 4 * NBUILTIN);
   243			wfile("oracle/p1_builtin.u32", bt, 4 * NBUILTIN);
   244		}
   245	
   246		// ---------------------------------------------------------------- P2: reach
   247		strtab S;
   248		memset(&S, 0, sizeof S);
   249		stInit(&S);
   250		u8 *nf = calloc(N + 1, 1);
   251		u32 *eoff = calloc(N + 2, 4), *moff = calloc(N + 2, 4);
   252		vecu32 edst = {0}, sigName = {0}, sigType = {0}, mName = {0}, mTyp = {0}, mIfn = {0}, mTfn = {0};
   253		vecu8 ekind = {0}, mExp = {0};
   254		u32map sigs; // (name id, type) -> sig id: key = index+1 in a side table, found by linear probing on a hash
   255		umInit(&sigs, 1 << 12);
   256		long nMethErr = 0;
   257		vecu32 tmp = {0};
   258		for (u32 g = 1; g <= N; g++) {
   259			u8 f1 = flatFL1[g];
   260			u8 sf = SF(g);
   261			bool def = (f1 & stUndef) == 0 && kind[g] != 5; // a canonical reference is an undefined symbol
   262			u8 fl = (def ? 1 : 0) | (sf & sfText ? 2 : 0) | (sf & sfReflect ? 4 : 0) | (sf & sfGoType ? 8 : 0) | (f1 & stUsedInIface ? 16 : 0) |
   263			        (sf & (sfHasRel | sfText | sfReflect) ? 32 : 0);
   264			if (kind[g] == 5) fl = (f1 & stUsedInIface ? 16 : 0); // references carry no record flags
   265			nf[g] = fl;
   266			eoff[g] = (u32)edst.n;
   267			moff[g] = (u32)mName.n;
   268			if (!def || flatCanon1[g] != g || (fl & 32) == 0) continue;
   269			u32 j;
   270			object *o = objOf(g, &j);
   271			if (j >= o->nalldef) continue;
   272			u32 nr;
   273			relocp rs = oRelocs(o, j, &nr);
   274			tmp.n = 0;
   275	#define EMIT(k, d)                                     \
   276		do {                                               \
   277			u8 k_ = (k);                                   \
   278			u32 d_ = (d);                                  \
   279			VPUSH(ekind, k_);                              \
   280			VPUSH(edst, d_);                               \
   281		} while (0)
   282			for (u32 i = 0; i < nr; i++) {
   283				relocp rl = RELOC(rs, i);
   284				u16 t = rType(rl);
   285				if (t & R_WEAK) continue;
   286				switch (t) {
   287				case R_METHODOFF:
   288					if (i + 2 >= nr) {
   289						nMethErr++;
   290						i = nr;
   291						continue;
   292					}
   293					VPUSH(tmp, i);
   294					i += 2;
   295					continue;
   296				case R_USETYPE:
   297				case R_INITORDER: continue;
   298				case R_USEIFACE: {
   299					u32 rsym = c1(orefQ(o, rPkg(rl), rIdx(rl)));
   300					if (!isDef(rsym)) continue;
   301					if (SF(rsym) & sfItab) {
   302						u32 j2, nr2;
   303						object *o2 = objOf(rsym, &j2);
   304						relocp rs2 = oRelocs(o2, j2, &nr2);
   305						relocp rl2 = relocAt(rs2, nr2, 8);
   306						if (!rl2) continue;
   307						rsym = c1(orefQ(o2, rPkg(rl2), rIdx(rl2)));
   308						if (!isDef(rsym)) continue;
   309					}
   310					EMIT(1, rsym);
   311					continue;
   312				}
   313				case R_USEIFACEMETHOD: {
   314					u32 rsym = c1(orefQ(o, rPkg(rl), rIdx(rl)));
   315					if (isDef(rsym)) {
   316						u32 j2, nr2;
   317						object *o2 = objOf(rsym, &j2);
   318						relocp rr = oRelocs(o2, j2, &nr2);
   319						i32 off = (i32)rAdd(rl);
   320						u32 nm = stId(&S, nameAt(o2, rr, nr2, off));
   321						relocp rl2 = relocAt(rr, nr2, off + 4);
   322						u32 ty = rl2 ? c1(orefQ(o2, rPkg(rl2), rIdx(rl2))) : 0;
   323						// sig id: search the side table
   324						u32 key = (u32)(mix64((u64)nm << 32 | ty) | 1);
   325						int *v;
   326						u32 id = 0;
   327						for (;; key = key * 2654435761u | 1) {
   328							v = umFind(&sigs, key);
   329							if (!v) break;
   330							if (sigName.a[*v] == nm && sigType.a[*v] == ty) {
   331								id = (u32)*v;
   332								break;
   333							}
   334						}
   335						if (!v) {
   336							id = (u32)sigName.n;
   337							VPUSH(sigName, nm);
   338							VPUSH(sigType, ty);
   339							umPut(&sigs, key, (int)id);
   340						}
   341						EMIT(2, id);
   342					}
   343					continue;
   344				}
   345				case R_USENAMEDMETHOD: {
   346					bytes d = dataOf(c1(orefQ(o, rPkg(rl), rIdx(rl))));
   347					EMIT(3, stId(&S, (str){(const char *)d.p, d.n}));
   348					continue;
   349				}
   350				}
   351				u32 rsym = c1(orefQ(o, rPkg(rl), rIdx(rl)));
   352				if (rsym == 0) continue;
   353				EMIT(0, rsym);
   354			}
   355			if (sf & sfText) {
   356				u32 na;
   357				auxp as = oAuxs(o, j, &na);
   358				for (u32 i = 0; i < na; i++) {
   359					auxp a = AUX(as, i);
   360					if (aType(a) == AuxFuncdata) {
   361						u32 cc = c1(orefQ(o, aPkg(a), aIdx(a)));
   362						if (cc != 0) EMIT(0, cc);
   363					}
   364				}
   365			}
   366			if (tmp.n != 0) {
   367				bytes p = oData(o, j);
   368				if (p.n <= ABI_TFlagOff8 || (p.p[ABI_TFlagOff8] & ABI_TFlagUncommon) == 0) {
   369					nMethErr++;
   370					continue;
   371				}
   372				u8 tk = typeKind(p.p);
   373				int off = tk < ABI_NKIND ? abiRTypeSize8[tk] : 48;
   374				u32 mcount = ld16(p.p + off + 4);
   375				u32 mo = ld32(p.p + off + 8);
   376				if (mcount != tmp.n) {
   377					nMethErr++;
   378					continue;
   379				}
   380				for (size_t i = 0; i < tmp.n; i++) {
   381					u32 ri = tmp.a[i];
   382					i32 at = (i32)(off + mo + 16 * i);
   383					str name;
   384					if (ri > 0 && rOff(RELOC(rs, ri - 1)) == at) {
   385						relocp rp = RELOC(rs, ri - 1);
   386						name = nameData(dataOf(c1(orefQ(o, rPkg(rp), rIdx(rp)))));
   387					} else {
   388						name = nameAt(o, rs, nr, at);
   389					}
   390					u32 tg[3];
   391					for (int d = 0; d < 3; d++) {
   392						relocp rd = RELOC(rs, ri + d);
   393						tg[d] = c1(orefQ(o, rPkg(rd), rIdx(rd)));
   394					}
   395					u32 nm = stId(&S, name);
   396					u8 ex = isExported(name);
   397					VPUSH(mName, nm);
   398					VPUSH(mTyp, tg[0]);
   399					VPUSH(mIfn, tg[1]);
   400					VPUSH(mTfn, tg[2]);
   401					VPUSH(mExp, ex);
   402				}
   403			}
   404	#undef EMIT
   405		}
   406		eoff[N + 1] = (u32)edst.n;
   407		moff[N + 1] = (u32)mName.n;
   408		eoff[0] = moff[0] = 0;
   409		meta("p2_edges", edst.n);
   410		meta("p2_methods", mName.n);
   411		meta("p2_sigs", sigName.n);
   412		meta("p2_strings", S.off.n - 1);
   413		meta("p2_method_errors", (u64)nMethErr);
   414		wfile("p2_flags.u8", nf, N + 1);
   415		wfile("p2_edge_off.u32", eoff, 4 * (size_t)(N + 2));
   416		WVEC("p2_edge_dst.u32", edst);
   417		WVEC("p2_edge_kind.u8", ekind);
   418		WVEC("p2_sig_name.u32", sigName);
   419		WVEC("p2_sig_type.u32", sigType);
   420		wfile("p2_meth_off.u32", moff, 4 * (size_t)(N + 2));
   421		WVEC("p2_meth_name.u32", mName);
   422		WVEC("p2_meth_mtyp.u32", mTyp);
   423		WVEC("p2_meth_ifn.u32", mIfn);
   424		WVEC("p2_meth_tfn.u32", mTfn);
   425		WVEC("p2_meth_exported.u8", mExp);
   426		stWrite(&S, "p2_str");
   427		{
   428			vecu32 roots = {0};
   429	#define R(c)                                \
   430		do {                                    \
   431			u32 c_ = (c);                       \
   432			if (c_ != 0) VPUSH(roots, c1(c_));  \
   433		} while (0)
   434			R(lookupC(flagEntry, 0));
   435			R(lookupC(flagEntry, 1));
   436			R(lookupC("runtime.unreachableMethod", 0));
   437			R(lookupC("runtime.unreachableMethod", 1));
   438			R(lookupC("runtime.mapinitnoop", 1));
   439			R(lookupC("runtime.firstmoduledata", 0));
   440			for (size_t i = 0; i < mainInittasks.n; i++) R(mainInittasks.a[i]);
   441			for (size_t i = 0; i < runtimeInittasks.n; i++) R(runtimeInittasks.a[i]);
   442	#undef R
   443			WVEC("p2_roots.u32", roots);
   444			vecu32 live = {0};
   445			for (u32 g = 1; g < nids; g++)
   446				if (flatFL2[g] & stLive) VPUSH(live, g);
   447			WVEC("oracle/p2_live.u32", live);
   448			meta("p2_live", live.n);
   449		}
   450	
   451		// ---------------------------------------------------------------- P3: layout and relocate
   452		vecu32 psym = {0}, psize = {0}, punit = {0}, ptl = {0}, rOffv = {0}, prel = {0}, rTgt = {0}, paddr = {0};
   453		vecu8 pb = {0}, pcl = {0}, rSizv = {0}, rKind = {0};
   454		VEC(i64) rAddv = {0};
   455		vecu64 doff = {0};
   456		vecu8 dbin = {0};
   457		strtab T;
   458		memset(&T, 0, sizeof T);
   459		stInit(&T);
   460		long nUnsup = 0;
   461		VPUSH(doff, z);
   462		for (long ui = 0; ui < nunits; ui++) {
   463			unit *u = &units[ui];
   464			object *o = u->o;
   465			for (u32 j = u->lo; j < MIN(u->hi, o->nalldef); j++) {
   466				u32 g = o->base + j;
   467				u32 st = state[g];
   468				if ((st & (stLive | stPC | stNameNeeded)) == 0 || canon[g] != g) continue;
   469				int b = (int)(st >> stBucketShift & 31);
   470				if (b == bNone) continue;
   471				const u8 *s = osym(o, j);
   472				u32 cl = b == bTypelink ? (u32)__builtin_ctz(MAX(8, symAlign(s))) : (st >> stClassShft & 15);
   473				u32 siz = symSiz(s);
   474				u32 a = addr[g];
   475				u8 b8 = (u8)b, cl8 = (u8)cl;
   476				u32 uix = (u32)ui;
   477				VPUSH(psym, g);
   478				VPUSH(pb, b8);
   479				VPUSH(pcl, cl8);
   480				VPUSH(psize, siz);
   481				VPUSH(punit, uix);
   482				VPUSH(paddr, a);
   483				u32 tls_ = b == bTypelink ? stId(&T, typeString(g)) : 0;
   484				VPUSH(ptl, tls_);
   485				u32 rstart = (u32)rOffv.n;
   486				VPUSH(prel, rstart);
   487				if (b != bBSS && b != bNoptrBSS && b != bCov) {
   488					bytes d = oData(o, j);
   489					for (u32 k = 0; k < d.n; k++) VPUSH(dbin, d.p[k]);
   490					u32 nr;
   491					relocp rs = oRelocs(o, j, &nr);
   492					for (u32 i = 0; i < nr; i++) {
   493						relocp rl = RELOC(rs, i);
   494						u32 rsiz = rSiz(rl);
   495						if (rsiz == 0) continue;
   496						u16 t = rType(rl);
   497						bool weak = (t & R_WEAK) != 0;
   498						t &= (u16)~R_WEAK;
   499						u32 tg = orefQ(o, rPkg(rl), rIdx(rl));
   500						i64 ad = rAdd(rl);
   501						u8 kk;
   502						switch (t) {
   503						case R_ADDR: kk = weak ? 2 : 1; break;
   504						case R_ADDROFF: kk = 3; break;
   505						case R_METHODOFF: kk = 4; break;
   506						case R_CALL: kk = weak ? 6 : 5; break;
   507						case R_PCREL: kk = 7; break;
   508						case R_TLS_LE: kk = 8; ad = -8 + ad; break;
   509						case R_CONST: kk = 8; break;
   510						case R_SIZE: {
   511							u32 c = canon[tg];
   512							i64 v = 0;
   513							if (isDef(c)) {
   514								u32 j2;
   515								object *o2 = objOf(c, &j2);
   516								v = symSiz(osym(o2, j2));
   517							}
   518							kk = 8;
   519							ad = v + ad;
   520							break;
   521						}
   522						default: kk = 0; nUnsup++; break;
   523						}
   524						u32 ro = (u32)rOff(rl);
   525						u8 rs8 = (u8)rsiz;
   526						VPUSH(rOffv, ro);
   527						VPUSH(rSizv, rs8);
   528						VPUSH(rKind, kk);
   529						VPUSH(rAddv, ad);
   530						VPUSH(rTgt, tg);
   531					}
   532				}
   533				u64 de = dbin.n;
   534				VPUSH(doff, de);
   535			}
   536		}
   537		u32 rend = (u32)rOffv.n;
   538		VPUSH(prel, rend);
   539		meta("p3_placed", psym.n);
   540		meta("p3_rel", rOffv.n);
   541		meta("p3_rel_unsupported", (u64)nUnsup);
   542		meta("p3_data_bytes", dbin.n);
   543		meta("p3_typelinks", (u64)ntls);
   544		meta("p3_unitSlots", unitSlots);
   545		meta("lay_text", lay.text);
   546		meta("lay_rodata", lay.rodata);
   547		meta("lay_pclntab", lay.pclntab);
   548		meta("lay_types", lay.types);
   549		meta("lay_noptrdata", lay.noptrdata);
   550		meta("lay_data", lay.data);
   551		meta("lay_bss", lay.bss);
   552		meta("lay_noptrbss", lay.noptrbss);
   553		meta("lay_end", lay.end);
   554		meta("addr_unreachableMethod", unreachableMethodAddr);
   555		meta("addr_mapinitnoop", mapinitnoopAddr);
   556		meta("minFunc", minFunc);
   557		flatBucketStart[bTypelink] = lay.types + 8;
   558		flatBucketMinAlign[bTypelink] = 8;
   559		WVEC("p3_sym.u32", psym);
   560		WVEC("p3_bucket.u8", pb);
   561		WVEC("p3_class.u8", pcl);
   562		WVEC("p3_size.u32", psize);
   563		WVEC("p3_unit.u32", punit);
   564		WVEC("p3_tlstr.u32", ptl);
   565		stWrite(&T, "p3_str");
   566		wfile("p3_bucket_start.u32", flatBucketStart, sizeof flatBucketStart);
   567		wfile("p3_bucket_minalign.u32", flatBucketMinAlign, sizeof flatBucketMinAlign);
   568		WVEC("p3_data.off.u64", doff);
   569		WVEC("p3_data.bin", dbin);
   570		WVEC("p3_rel_start.u32", prel);
   571		WVEC("p3_rel_off.u32", rOffv);
   572		WVEC("p3_rel_siz.u8", rSizv);
   573		WVEC("p3_rel_kind.u8", rKind);
   574		WVEC("p3_rel_add.i64", rAddv);
   575		WVEC("p3_rel_tgt.u32", rTgt);
   576		wfile("p3_addr_all.u32", addr, 4 * (size_t)nids);
   577		WVEC("oracle/p3_addr.u32", paddr);
   578		wfile("oracle/p3_image.bin", out, (size_t)outSize);
   579		meta("image_bytes", (u64)outSize);
   580		fclose(metaF);
   581		fprintf(stderr, "dumpflat: wrote %s\n", flatDir);
   582	}
rg: build: No such file or directory (os error 2)
results/minio.hlink.tsv
systems/futhark/IDEAS.md
systems/futhark/hello/hello.fut
systems/futhark/hello/gen.c
systems/futhark/hello/driver.c
systems/futhark/hello/run.sh
systems/futhark/hello/build.sh
systems/futhark/NOTES.md
systems/halide/IDEAS.md
systems/halide/hello/mkdata.cpp
systems/halide/hello/driver.cpp
systems/halide/hello/gen.cpp
systems/halide/hello/run.sh
systems/halide/hello/build.sh
systems/halide/NOTES.md
systems/goose/IDEAS.md
data/minio/p2_meth_mtyp.u32
data/minio/p3_rel_start.u32
data/minio/name.off.u64
data/minio/p3_unit.u32
data/minio/rel_pkg.u32
data/minio/p3_rel_kind.u8
data/minio/p2_sig_type.u32
data/minio/p2_sig_name.u32
data/minio/p2_meth_ifn.u32
systems/goose/hello/gen.c
systems/goose/hello/c_ref.c
data/minio/p1_kind.u8
systems/goose/hello/hello8.goose
systems/goose/hello/run.sh
systems/goose/hello/build.sh
systems/goose/hello/build_from_source.sh
systems/goose/hello/hello1.goose
systems/goose/NOTES.md
data/minio/p3_data.off.u64
data/minio/p1_builtin.u64x2
data/minio/p3_str.bin
data/minio/p3_bucket.u8
data/minio/p1_hashkey.u64x2
data/minio/p3_rel_tgt.u32
data/minio/p1c_mark.u8
systems/souffle/IDEAS.md
data/minio/p1_namekey.u64x2
data/minio/p2_flags.u8
data/minio/p1_flags.u8
data/minio/rel_off.i32
data/minio/p3_sym.u32
data/minio/p2_meth_name.u32
data/minio/p2_str.off.u64
data/minio/rel_type.u16
data/minio/p3_class.u8
data/minio/name.bin
data/minio/README.md
data/minio/p2_roots.u32
data/minio/p2_meth_tfn.u32
data/minio/p3_data.bin
data/minio/p3_str.off.u64
data/minio/p3_bucket_minalign.u32
data/minio/rel_add.i64
data/minio/p1_rank.u32
data/minio/p2_edge_kind.u8
data/minio/p2_meth_off.u32
data/minio/slot_rel.u32
data/minio/rel_siz.u8
data/minio/p3_tlstr.u32
data/minio/obj_pkg.u32
data/minio/p2_edge_off.u32
data/minio/obj_pkg_off.u32
data/minio/p1_want.u64x2
data/minio/p3_addr_all.u32
data/minio/p3_rel_siz.u8
data/minio/p3_rel_add.i64
data/minio/obj_base.u32
data/minio/slot_obj.u32
data/minio/p3_bucket_start.u32
data/minio/p2_meth_exported.u8
data/minio/meta.txt
data/minio/rel_idx.u32
data/minio/p3_rel_off.u32
data/minio/p2_edge_dst.u32
data/minio/p2_str.bin
data/minio/p3_size.u32
systems/souffle/hello/load.dl
systems/souffle/hello/reach.dl
systems/souffle/hello/reach_emb.dl
systems/souffle/hello/embed.cpp
systems/souffle/NOTES.md
systems/souffle/hello/gen.cpp
systems/souffle/hello/run.sh
systems/souffle/hello/build.sh
systems/souffle/hello/load_c.cpp
systems/souffle/hello/reach_c.cpp
data/minio/oracle/p1_reltarget.u32
data/minio/oracle/p1_builtin.u32
data/minio/oracle/p2_live.u32
data/minio/oracle/p3_addr.u32
data/minio/oracle/live_names.txt
data/minio/oracle/p1c_canon.u32
data/minio/oracle/p3_image.bin
data/minio/oracle/p1_canon.u32

exec
/bin/bash -lc 'nl -ba ref/flat/p3.c; nl -ba ref/flat/p2.c; nl -ba ref/flat/p1.c' in /home/exedev/paradigms-w
 succeeded in 0ms:
     1	// p3.c: problem 3 (lay out, then relocate) from the flat files, plainly, on one core. Checks itself against
     2	// oracle/p3_addr.u32 (3a) and the bytes of every placed symbol in oracle/p3_image.bin (3b).
     3	//   ref/flat/p3 data/minio [out.img]
     4	#include "flat.h"
     5	
     6	enum { bText = 1, bTextFIPS = 2, bTypelink = 6, bBSS = 13, bNoptrBSS = 14, bCov = 15, nBuckets = 16, nClasses = 13 };
     7	static inline u32 rnd(u32 v, u32 a) { return (v + a - 1) & ~(a - 1); }
     8	
     9	static const u64 *soff;
    10	static const u8 *sbin;
    11	static const u32 *tlstr, *psym;
    12	static int cmpTL(const void *a, const void *b) {
    13		u32 x = *(const u32 *)a, y = *(const u32 *)b;
    14		u64 xo = soff[tlstr[x]], xn = soff[tlstr[x] + 1] - xo, yo = soff[tlstr[y]], yn = soff[tlstr[y] + 1] - yo;
    15		u64 n = xn < yn ? xn : yn;
    16		int c = n ? memcmp(sbin + xo, sbin + yo, n) : 0;
    17		if (c) return c;
    18		if (xn != yn) return xn < yn ? -1 : 1;
    19		return psym[x] < psym[y] ? -1 : psym[x] > psym[y];
    20	}
    21	
    22	int main(int argc, char **argv) {
    23		flatDir = argc > 1 ? argv[1] : "data/minio";
    24		u32 L = (u32)metaGet("p3_placed"), nunits = (u32)metaGet("nunits"), minFunc = (u32)metaGet("minFunc");
    25		u32 imageBase = (u32)metaGet("imageBase");
    26		psym = FMAP(u32, "p3_sym.u32", NULL);
    27		const u8 *pb = FMAP(u8, "p3_bucket.u8", NULL), *pcl = FMAP(u8, "p3_class.u8", NULL);
    28		const u32 *psize = FMAP(u32, "p3_size.u32", NULL), *punit = FMAP(u32, "p3_unit.u32", NULL);
    29		tlstr = FMAP(u32, "p3_tlstr.u32", NULL);
    30		soff = FMAP(u64, "p3_str.off.u64", NULL);
    31		sbin = FMAP(u8, "p3_str.bin", NULL);
    32		const u32 *bstart = FMAP(u32, "p3_bucket_start.u32", NULL), *bmin = FMAP(u32, "p3_bucket_minalign.u32", NULL);
    33	
    34		// ---- 3a. addresses
    35		double t0 = now();
    36		u32 *addr = calloc(L, 4);
    37		// bytes per (unit, bucket, class), then each unit's block of each bucket in unit order, classes from the largest
    38		// alignment down; inside a class the symbols in list order
    39		u32 *csize = calloc((size_t)nunits * nBuckets * nClasses, 4);
    40	#define CS(u, b, c) csize[((size_t)(u) * nBuckets + (b)) * nClasses + (c)]
    41		u32 *occ = malloc(4 * (size_t)L);
    42		for (u32 i = 0; i < L; i++) {
    43			u32 s = psize[i], b = pb[i];
    44			if ((b == bText || b == bTextFIPS) && s < minFunc) s = minFunc;
    45			occ[i] = rnd(s, 1u << pcl[i]);
    46			if (b != bTypelink) CS(punit[i], b, pcl[i]) += occ[i];
    47		}
    48		for (u32 b = 1; b < nBuckets; b++) {
    49			if (b == bTypelink) continue;
    50			u32 va = bstart[b];
    51			for (u32 u = 0; u < nunits; u++) {
    52				int top = -1;
    53				for (int c = nClasses - 1; c >= 0; c--)
    54					if (CS(u, b, c)) { top = c; break; }
    55				if (top >= 0) va = rnd(va, bmin[b] > (1u << top) ? bmin[b] : 1u << top);
    56				for (int c = nClasses - 1; c >= 0; c--) {
    57					u32 n = CS(u, b, c);
    58					CS(u, b, c) = va; // from here on: the class's next address
    59					va += n;
    60				}
    61			}
    62		}
    63		u32 ntl = 0;
    64		u32 *tl = malloc(4 * (size_t)L);
    65		for (u32 i = 0; i < L; i++) {
    66			if (pb[i] == bTypelink) { tl[ntl++] = i; continue; }
    67			addr[i] = CS(punit[i], pb[i], pcl[i]);
    68			CS(punit[i], pb[i], pcl[i]) += occ[i];
    69		}
    70		qsort(tl, ntl, 4, cmpTL);
    71		u32 va = bstart[bTypelink];
    72		for (u32 k = 0; k < ntl; k++) {
    73			u32 i = tl[k];
    74			va = rnd(va, 1u << pcl[i]);
    75			addr[i] = va;
    76			va += psize[i];
    77		}
    78		double t1 = now();
    79		printf("p3a (1 core): layout %.1f ms (%u symbols, %u sorted by type string)\n", (t1 - t0) * 1e3, L, ntl);
    80		long bad = check32("addr", addr, L, "oracle/p3_addr.u32");
    81	
    82		// ---- 3b. copy and relocate, with the addresses the oracle gives (so 3b does not depend on 3a)
    83		size_t nimg;
    84		const u8 *want = FMAP(u8, "oracle/p3_image.bin", &nimg);
    85		const u32 *oaddr = FMAP(u32, "oracle/p3_addr.u32", NULL), *all = FMAP(u32, "p3_addr_all.u32", NULL);
    86		const u64 *doff = FMAP(u64, "p3_data.off.u64", NULL);
    87		const u8 *dbin = FMAP(u8, "p3_data.bin", NULL);
    88		const u32 *rstart = FMAP(u32, "p3_rel_start.u32", NULL), *roff = FMAP(u32, "p3_rel_off.u32", NULL), *rtgt = FMAP(u32, "p3_rel_tgt.u32", NULL);
    89		const u8 *rsiz = FMAP(u8, "p3_rel_siz.u8", NULL), *rkind = FMAP(u8, "p3_rel_kind.u8", NULL);
    90		const i64 *radd = FMAP(i64, "p3_rel_add.i64", NULL);
    91		u32 sect[8] = {(u32)metaGet("lay_text"), (u32)metaGet("lay_rodata"), (u32)metaGet("lay_pclntab"), (u32)metaGet("lay_types"),
    92		               (u32)metaGet("lay_noptrdata"), (u32)metaGet("lay_data"), (u32)metaGet("lay_bss"), (u32)metaGet("lay_noptrbss")};
    93		u32 unreach = (u32)metaGet("addr_unreachableMethod"), mapinit = (u32)metaGet("addr_mapinitnoop");
    94		u8 *img = calloc(nimg, 1);
    95		long nerr = 0;
    96		double t2 = now();
    97		for (u32 i = 0; i < L; i++) {
    98			u32 b = pb[i];
    99			if (b == bBSS || b == bNoptrBSS || b == bCov) continue;
   100			u32 a = oaddr[i];
   101			u8 *P = img + (a - imageBase);
   102			memcpy(P, dbin + doff[i], doff[i + 1] - doff[i]);
   103			for (u32 r = rstart[i]; r < rstart[i + 1]; r++) {
   104				u32 tg = rtgt[r], ta = all[tg], off = roff[r], sz = rsiz[r];
   105				i64 v;
   106				switch (rkind[r]) {
   107				case 2: if (ta == 0 && tg != 0) ta = unreach; /* fallthrough */
   108				case 1:
   109					if (ta == 0 && tg != 0) { nerr++; continue; }
   110					v = (i64)ta + radd[r];
   111					break;
   112				case 4: if (ta == 0) { memset(P + off, 0xFF, 4); continue; } /* fallthrough */
   113				case 3: {
   114					if (ta == 0) continue;
   115					int k = 7;
   116					while (k > 0 && ta < sect[k]) k--;
   117					v = (i64)(ta - sect[k]) + radd[r];
   118					break;
   119				}
   120				case 6: if (ta == 0 && tg != 0) ta = mapinit; /* fallthrough */
   121				case 5:
   122				case 7:
   123					if (ta == 0 && tg != 0) { nerr++; continue; }
   124					v = tg == 0 ? radd[r] - (i64)(a + off + sz) : (i64)ta + radd[r] - (i64)(a + off + sz);
   125					break;
   126				case 8: v = radd[r]; break;
   127				default: nerr++; continue;
   128				}
   129				switch (sz) {
   130				case 8: memcpy(P + off, &v, 8); break;
   131				case 4: { u32 x = (u32)v; memcpy(P + off, &x, 4); break; }
   132				case 2: { u16 x = (u16)v; memcpy(P + off, &x, 2); break; }
   133				case 1: P[off] = (u8)v; break;
   134				default: nerr++;
   135				}
   136			}
   137		}
   138		double t3 = now();
   139		long nbad = 0;
   140		u64 nbytes = 0, sum = 0;
   141		for (u32 i = 0; i < L; i++) {
   142			u32 b = pb[i];
   143			if (b == bBSS || b == bNoptrBSS || b == bCov) continue;
   144			size_t o = oaddr[i] - imageBase, n = psize[i];
   145			nbytes += n;
   146			for (size_t k = 0; k < n; k++) sum = sum * 1099511628211ull + img[o + k];
   147			if (memcmp(img + o, want + o, n) != 0 && nbad++ < 5) printf("  symbol %u (id %u, bucket %u) differs\n", i, psym[i], b);
   148		}
   149		printf("p3b (1 core): copy + relocate %.1f ms, %llu bytes, %u relocations, %ld errors; checksum %016llx\n", (t3 - t2) * 1e3,
   150		       (unsigned long long)nbytes, rstart[L], nerr, (unsigned long long)sum);
   151		printf("image: %ld placed symbols differ from oracle/p3_image.bin\n", nbad);
   152		if (argc > 2) {
   153			FILE *f = fopen(argv[2], "wb");
   154			fwrite(img, 1, nimg, f);
   155			fclose(f);
   156		}
   157		bad += nbad + (nerr != 0);
   158		printf(bad ? "P3 FAIL\n" : "P3 OK\n");
   159		return bad != 0;
   160	}
     1	// p2.c: problem 2 (reach) from the flat files, plainly, on one core, with a worklist. Checks itself against
     2	// oracle/p2_live.u32. The rules are data/README.md's (they are cmd/link's deadcode.go):
     3	//   live(r)  for every root r
     4	//   live(t)  <- live(s), plain(s,t)
     5	//   iface(t) <- live(s), iface(s), gotype(s), plain(s,t), gotype(t), def(t)
     6	//   iface(t) <- live(s), useiface(s,t)
     7	//   sig(x)   <- live(s), ifacemethod(s,x);   named(n) <- live(s), namedmethod(s,n);   reflect <- live(s), reflect(s)
     8	//   iface(m.mtyp) and markable(m) <- live(s), iface(s), gotype(s), method(s,m)
     9	//   live(m.mtyp), live(m.ifn), live(m.tfn) <- markable(m), (reflect & exported(m) | sig(m.name, m.mtyp) | named(m.name))
    10	// (only defined symbols have edges; edges are given only for them).
    11	//   ref/flat/p2 data/minio
    12	#include "flat.h"
    13	
    14	enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16 };
    15	
    16	static u8 *live, *iface, *ifaceDone;
    17	static u32 *stack;
    18	static size_t sp;
    19	static inline void mark(u32 c) {
    20		if (c && !live[c]) {
    21			live[c] = 1;
    22			stack[sp++] = c;
    23		}
    24	}
    25	static inline void setIface(u32 c) {
    26		if (c && !iface[c]) {
    27			iface[c] = 1;
    28			if (live[c]) stack[sp++] = c;
    29		}
    30	}
    31	
    32	int main(int argc, char **argv) {
    33		flatDir = argc > 1 ? argv[1] : "data/minio";
    34		u32 N = (u32)metaGet("nids"), nsl = (u32)metaGet("nslots");
    35		size_t nroots, nsig, nstr, nmeth;
    36		const u8 *fl = FMAP(u8, "p2_flags.u8", NULL), *ek = FMAP(u8, "p2_edge_kind.u8", NULL);
    37		const u32 *eoff = FMAP(u32, "p2_edge_off.u32", NULL), *ed = FMAP(u32, "p2_edge_dst.u32", NULL);
    38		const u32 *sigName = FMAP(u32, "p2_sig_name.u32", &nsig), *sigType = FMAP(u32, "p2_sig_type.u32", NULL);
    39		const u32 *moff = FMAP(u32, "p2_meth_off.u32", NULL), *mName = FMAP(u32, "p2_meth_name.u32", &nmeth);
    40		const u32 *mTyp = FMAP(u32, "p2_meth_mtyp.u32", NULL), *mIfn = FMAP(u32, "p2_meth_ifn.u32", NULL), *mTfn = FMAP(u32, "p2_meth_tfn.u32", NULL);
    41		const u8 *mExp = FMAP(u8, "p2_meth_exported.u8", NULL);
    42		const u32 *roots = FMAP(u32, "p2_roots.u32", &nroots);
    43		FMAP(u64, "p2_str.off.u64", &nstr);
    44		nroots /= 4, nsig /= 4, nmeth /= 4, nstr = nstr / 8 - 1;
    45	
    46		double t0 = now();
    47		live = calloc(N, 1), iface = calloc(N, 1), ifaceDone = calloc(N, 1);
    48		stack = malloc(4 * (size_t)N * 3);
    49		for (u32 g = 1; g <= nsl; g++) iface[g] = (fl[g] & fIface) != 0;
    50		u8 *sigSeen = calloc(nsig, 1), *named = calloc(nstr + 1, 1);
    51		// sig ids are (name, type) pairs; a method's (name, mtyp) is looked up in a hash of the live sigs
    52		u64 hs = 1;
    53		while (hs < 4 * nsig + 64) hs <<= 1;
    54		u64 *sigTab = calloc(hs, 8);
    55		u32 *markable = malloc(4 * (nmeth + 1)), nm = 0;
    56		int reflectSeen = 0, rounds = 0;
    57		for (size_t i = 0; i < nroots; i++) mark(roots[i]);
    58		for (;;) {
    59			rounds++;
    60			while (sp) {
    61				u32 c = stack[--sp];
    62				if (c > nsl || !(fl[c] & fDef)) continue;
    63				if (fl[c] & fReflect) reflectSeen = 1;
    64				int uii = (fl[c] & fGoType) && iface[c];
    65				if (uii) {
    66					if (ifaceDone[c]) continue;
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
     1	// p1.c: problem 1 (resolve) from the flat files, plainly, on one core: the reference that shows the flat files
     2	// say everything resolve needs. Checks itself against oracle/p1_canon.u32, oracle/p1_builtin.u32,
     3	// oracle/p1_reltarget.u32 and (the content-hash join, 1c) oracle/p1c_canon.u32.
     4	//   ref/flat/p1 data/minio
     5	#include "flat.h"
     6	
     7	#define rankDef (1u << 29)
     8	#define PkgIdxSelf 2147483643u
     9	
    10	typedef struct {
    11		u64 k1, k2, val; // val = rank << 32 | ~id: the maximum is the winner
    12	} ent;
    13	static ent *tab;
    14	static u64 mask;
    15	static ent *find(u64 k1, u64 k2, int add) {
    16		for (u64 h = mix64(k1) & mask;; h = (h + 1) & mask) {
    17			if (tab[h].k1 == 0) {
    18				if (!add) return NULL;
    19				tab[h].k1 = k1, tab[h].k2 = k2, tab[h].val = 0;
    20				return &tab[h];
    21			}
    22			if (tab[h].k1 == k1 && tab[h].k2 == k2) return &tab[h];
    23		}
    24	}
    25	
    26	int main(int argc, char **argv) {
    27		flatDir = argc > 1 ? argv[1] : "data/minio";
    28		u32 N = (u32)metaGet("nslots"), nobjs = (u32)metaGet("nobjs"), nb = (u32)metaGet("nbuiltin");
    29		size_t nw, nrel;
    30		const u8 *kind = FMAP(u8, "p1_kind.u8", NULL), *flags = FMAP(u8, "p1_flags.u8", NULL);
    31		const u64 *key = FMAP(u64, "p1_namekey.u64x2", NULL), *want = FMAP(u64, "p1_want.u64x2", &nw);
    32		const u64 *bkey = FMAP(u64, "p1_builtin.u64x2", NULL);
    33		const u32 *rank = FMAP(u32, "p1_rank.u32", NULL), *slotRel = FMAP(u32, "slot_rel.u32", NULL);
    34		const u32 *slotObj = FMAP(u32, "slot_obj.u32", NULL), *pkgOff = FMAP(u32, "obj_pkg_off.u32", NULL);
    35		const u32 *pkg = FMAP(u32, "obj_pkg.u32", NULL);
    36		const u32 *rPkg = FMAP(u32, "rel_pkg.u32", &nrel), *rIdx = FMAP(u32, "rel_idx.u32", NULL);
    37		nrel /= 4;
    38		nw /= 16;
    39		const u64 *hkey = FMAP(u64, "p1_hashkey.u64x2", NULL);
    40		const u8 *hmark = FMAP(u8, "p1c_mark.u8", NULL);
    41	
    42		double t0 = now();
    43		// 1a. the join by name: the linker's wanted names, then every definition and reference by name
    44		u64 sz = 1;
    45		while (sz < 4 * (u64)(N / 4 + nw)) sz <<= 1;
    46		tab = calloc(sz, sizeof(ent));
    47		mask = sz - 1;
    48		for (size_t i = 0; i < nw; i++) find(want[2 * i], want[2 * i + 1], 1);
    49		u32 *canon = malloc(4 * ((size_t)N + 1));
    50		canon[0] = 0;
    51		for (u32 g = 1; g <= N; g++) {
    52			canon[g] = g;
    53			if ((flags[g] & 2)) canon[g] = 0; // a reference that only debug information makes
    54			if ((kind[g] != 4 && kind[g] != 5) || key[2 * (size_t)g] == 0) continue;
    55			ent *e = find(key[2 * (size_t)g], key[2 * (size_t)g + 1], 1);
    56			u64 v = (u64)rank[g] << 32 | (u32)~g;
    57			if (v > e->val) e->val = v;
    58		}
    59		// definitions by package index join only a name that is already there
    60		for (u32 g = 1; g <= N; g++) {
    61			if (kind[g] != 1 || key[2 * (size_t)g] == 0) continue;
    62			ent *e = find(key[2 * (size_t)g], key[2 * (size_t)g + 1], 0);
    63			if (!e) continue;
    64			u64 v = (u64)rank[g] << 32 | (u32)~g;
    65			if (v > e->val) e->val = v;
    66		}
    67		for (u32 g = 1; g <= N; g++) {
    68			if (kind[g] == 2 || kind[g] == 3 || key[2 * (size_t)g] == 0) continue;
    69			ent *e = find(key[2 * (size_t)g], key[2 * (size_t)g + 1], 0);
    70			if (e) canon[g] = ~(u32)e->val;
    71		}
    72		u32 *builtin = calloc(nb, 4);
    73		for (u32 i = 0; i < nb; i++) {
    74			ent *e = find(bkey[2 * i], bkey[2 * i + 1], 0);
    75			if (e && e->val && ((e->val >> 32) & rankDef)) builtin[i] = ~(u32)e->val;
    76		}
    77		double t1 = now();
    78		// 1b. every relocation's target: a package-indexed reference is a read of the object's package table
    79		u32 *tgt = malloc(4 * nrel);
    80		for (u32 g = 1; g <= N; g++) {
    81			u32 o = slotObj[g];
    82			const u32 *pb = pkg + pkgOff[o];
    83			u32 npk = pkgOff[o + 1] - pkgOff[o] - 5;
    84			for (u32 r = slotRel[g]; r < slotRel[g + 1]; r++) {
    85				u32 p = rPkg[r], idx = rIdx[r], sp = p - PkgIdxSelf, t = sp < 5 ? npk + sp : p, s = 0;
    86				if (t < npk + 5 && pb[t] != 0xFFFFFFFFu) s = sp == 1 ? (idx < nb ? builtin[idx] : 0) : pb[t] + idx;
    87				tgt[r] = s ? canon[s] : 0;
    88			}
    89		}
    90		double t2 = now();
    91		printf("p1 (1 core): join by name %.1f ms, relocation targets %.1f ms\n", (t1 - t0) * 1e3, (t2 - t1) * 1e3);
    92		long bad = check32("canon", canon, (size_t)N + 1, "oracle/p1_canon.u32");
    93		bad += check32("builtin", builtin, nb, "oracle/p1_builtin.u32");
    94		bad += check32("reltarget", tgt, nrel, "oracle/p1_reltarget.u32");
    95	
    96		// 1c. the content-hash join over the copies the flood reached (mark 1) or names without following (2)
    97		double t3 = now();
    98		typedef struct {
    99			u64 k1, k2;
   100			u32 win;
   101		} hent;
   102		u64 hs = 1;
   103		while (hs < 4 * (u64)(N / 2)) hs <<= 1;
   104		hent *ht = calloc(hs, sizeof(hent));
   105		u64 hm = hs - 1;
   106		// a group: the same kind (short or long hash) and key; short: the largest copy wins, then the lowest id;
   107		// long: the lowest id; a copy marked 2 never beats one marked 1
   108	#define HFIND(g, e)                                                                                         \
   109		do {                                                                                                    \
   110			u64 k1 = hkey[2 * (size_t)(g)], k2 = kind[g] == 2 ? 0 : hkey[2 * (size_t)(g) + 1] | (1ull << 63);  \
   111			for (u64 h = mix64(k1 ^ k2) & hm;; h = (h + 1) & hm) {                                             \
   112				if (ht[h].win == 0) {                                                                           \
   113					ht[h].k1 = k1, ht[h].k2 = k2;                                                               \
   114					e = &ht[h];                                                                                 \
   115					break;                                                                                      \
   116				}                                                                                               \
   117				if (ht[h].k1 == k1 && ht[h].k2 == k2) {                                                         \
   118					e = &ht[h];                                                                                 \
   119					break;                                                                                      \
   120				}                                                                                               \
   121			}                                                                                                   \
   122		} while (0)
   123		for (u32 g = 1; g <= N; g++) {
   124			if ((kind[g] != 2 && kind[g] != 3) || hmark[g] == 0) continue;
   125			hent *e;
   126			HFIND(g, e);
   127			if (e->win == 0) { e->win = g; continue; }
   128			u32 w = e->win;
   129			int wr = hmark[w] == 2, gr = hmark[g] == 2;
   130			int better;
   131			if (wr != gr) better = wr; // a reached copy beats one only named
   132			else if (kind[g] == 2) better = hkey[2 * (size_t)g + 1] > hkey[2 * (size_t)w + 1]; // larger; ties: lower id (g > w)
   133			else better = 0;
   134			if (better) e->win = g;
   135		}
   136		u32 *canon2 = malloc(4 * ((size_t)N + 1));
   137		memcpy(canon2, canon, 4 * ((size_t)N + 1));
   138		for (u32 g = 1; g <= N; g++) {
   139			if ((kind[g] != 2 && kind[g] != 3) || hmark[g] == 0) continue;
   140			hent *e;
   141			HFIND(g, e);
   142			u32 w = e->win;
   143			if (hmark[w] == 2 || w == g) continue; // nothing reached the class: not in the output
   144			if (kind[g] == 2 && hmark[g] == 2 && hkey[2 * (size_t)g + 1] > hkey[2 * (size_t)w + 1]) continue;
   145			canon2[g] = w;
   146		}
   147		double t4 = now();
   148		printf("p1c (1 core): content-hash join %.1f ms\n", (t4 - t3) * 1e3);
   149		bad += check32("canon after 1c", canon2, (size_t)N + 1, "oracle/p1c_canon.u32");
   150		printf(bad ? "P1 FAIL\n" : "P1 OK\n");
   151		return bad != 0;
   152	}

codex
This checkout has no filled paradigm cells, no “falls down” verdicts, and no ATTACKS entries. The concrete concern is C problem 3: it checks bytes, but relocation uses oracle addresses instead of the layout it just computed. I’m also checking for data-contract errors and precomputed inputs.
exec
/bin/bash -lc "nl -ba ref/hlink/dumpflat.c | head -n 195; cat ref/flat/flat.h; cat ref/flat/extract.sh; cat ref/hlink/bench.sh; cat data/minio/meta.txt; rg -n 'falls down|session|time left|usage|budget' systems/*/NOTES.md JOURNAL.md; nl -ba ref/flat/README.md | tail -n 100" in /home/exedev/paradigms-w
 succeeded in 0ms:
     1	// dumpflat.c (paradigms repo; not part of bottom-up's hlink): -dumpflat DIR writes the flat files that every
     2	// system of the paradigm grid reads, and the oracles, from hlink's own state at the phase boundaries.
     3	// data/README.md (written by ref/flat/readme.sh) says what each file is. Snapshots: after resolve (canon and
     4	// flags, before the content-hash join), at the end of the flood (the marks, before the content-hash join);
     5	// everything else is read at the end of the link.
     6	
     7	static u32 *flatCanon1; // canon after resolve (content-addressed copies are still themselves)
     8	static u8 *flatFL1;     // FL after resolve
     9	static u8 *flatFL2;     // FL at the end of the flood
    10	
    11	static void flatSnapResolve(void) {
    12		flatCanon1 = malloc((size_t)nids * 4);
    13		memcpy(flatCanon1, canon, (size_t)nids * 4);
    14		flatFL1 = malloc(nids);
    15		for (u32 g = 0; g < nids; g++) flatFL1[g] = FL(g);
    16	}
    17	static void flatSnapFlood(void) {
    18		flatFL2 = malloc(nids);
    19		for (u32 g = 0; g < nids; g++) flatFL2[g] = FL(g);
    20	}
    21	
    22	static const char *flatDir;
    23	static void wfile(const char *name, const void *p, size_t n) {
    24		char path[4096];
    25		snprintf(path, sizeof path, "%s/%s", flatDir, name);
    26		FILE *f = fopen(path, "wb");
    27		if (!f) fatalf("dumpflat: %s: %s", path, strerror(errno));
    28		if (n && fwrite(p, 1, n, f) != n) fatalf("dumpflat: write %s", path);
    29		fclose(f);
    30	}
    31	#define WVEC(name, v) wfile(name, (v).a, (v).n * sizeof *(v).a)
    32	static FILE *metaF;
    33	static void meta(const char *k, unsigned long long v) { fprintf(metaF, "%s %llu\n", k, v); }
    34	
    35	// a quiet oref: 0 for anything hlink would report as an error
    36	static u32 orefQ(const object *o, u32 pkg, u32 idx) {
    37		u32 sp = pkg - PkgIdxSelf;
    38		u32 t = sp < 5 ? o->npkg + sp : pkg;
    39		if (t >= o->npkg + 5) return 0;
    40		u32 b = o->pkgBase[t];
    41		if (b == badBase) return 0;
    42		if (sp == 1) return idx < NBUILTIN ? builtins[idx] : 0;
    43		return b + idx;
    44	}
    45	static inline u32 c1(u32 g) { return g < nids ? flatCanon1[g] : 0; }
    46	
    47	// interned strings
    48	typedef VEC(u64) vecu64;
    49	typedef struct {
    50		strmap m;
    51		vecu8 bin;
    52		vecu64 off;
    53	} strtab;
    54	static void stInit(strtab *t) {
    55		smInit(&t->m, 1 << 16);
    56		u64 z = 0;
    57		VPUSH(t->off, z); // string 0 is ""
    58		u64 e = 0;
    59		VPUSH(t->off, e);
    60		bool added;
    61		smPut(&t->m, (str){"", 0}, 0, &added);
    62	}
    63	static u32 stId(strtab *t, str s) {
    64		bool added;
    65		long *v = smPut(&t->m, s, (long)(t->off.n - 1), &added);
    66		if (added) {
    67			for (u32 i = 0; i < s.n; i++) VPUSH(t->bin, (u8)s.p[i]);
    68			u64 e = t->bin.n;
    69			VPUSH(t->off, e);
    70		}
    71		return (u32)*v;
    72	}
    73	static void stWrite(strtab *t, const char *prefix) {
    74		char a[256];
    75		snprintf(a, sizeof a, "%s.off.u64", prefix);
    76		WVEC(a, t->off);
    77		snprintf(a, sizeof a, "%s.bin", prefix);
    78		WVEC(a, t->bin);
    79	}
    80	
    81	static void dumpFlat(void) {
    82		flatDir = flagDumpFlat;
    83		mkdir(flatDir, 0777);
    84		char mp[4096];
    85		snprintf(mp, sizeof mp, "%s/meta.txt", flatDir);
    86		metaF = fopen(mp, "w");
    87		u32 N = nslots;
    88		meta("nslots", N);
    89		meta("nids", nids);
    90		meta("nobjs", (u64)nobjs);
    91		meta("nunits", (u64)nunits);
    92		meta("imageBase", imageBase);
    93	
    94		// ---------------------------------------------------------------- common: slots
    95		u32 *slotObj = calloc(N + 1, 4);
    96		vecu64 nameOff = {0};
    97		vecu8 nameBin = {0};
    98		u64 z = 0;
    99		VPUSH(nameOff, z); // slot 0
   100		for (int i = 0; i < nobjs; i++) {
   101			object *o = objs[i];
   102			for (u32 j = 0; j < o->total; j++) {
   103				u32 g = o->base + j;
   104				slotObj[g] = (u32)i;
   105				str n = symNameOf(o, osym(o, j));
   106				for (u32 k = 0; k < n.n; k++) VPUSH(nameBin, (u8)n.p[k]);
   107				u64 e = nameBin.n;
   108				VPUSH(nameOff, e);
   109			}
   110		}
   111		u64 e0 = nameBin.n;
   112		VPUSH(nameOff, e0);
   113		wfile("slot_obj.u32", slotObj, (size_t)(N + 1) * 4);
   114		WVEC("name.off.u64", nameOff);
   115		WVEC("name.bin", nameBin);
   116	
   117		// ---------------------------------------------------------------- P1: resolve
   118		u8 *kind = calloc(N + 1, 1), *p1f = calloc(N + 1, 1), *hmark = calloc(N + 1, 1);
   119		u64 *nkey = calloc(2 * (size_t)(N + 1), 8), *hkey = calloc(2 * (size_t)(N + 1), 8);
   120		u32 *rank = calloc(N + 1, 4);
   121		u32 *slotRel = calloc(N + 2, 4);
   122		VEC(i32) relOff = {0};
   123		vecu8 relSiz = {0};
   124		VEC(u16) relType = {0};
   125		VEC(i64) relAdd = {0};
   126		vecu32 relPkg = {0}, relIdx = {0}, relTgt = {0};
   127		u32 *objBase = calloc((size_t)nobjs, 4), *objPkgOff = calloc((size_t)nobjs + 1, 4);
   128		vecu32 objPkg = {0};
   129		long nNamed = 0, nPkgCand = 0;
   130		for (int i = 0; i < nobjs; i++) {
   131			object *o = objs[i];
   132			objBase[i] = o->base;
   133			objPkgOff[i] = (u32)objPkg.n;
   134			for (u32 t = 0; t < o->npkg + 5; t++) VPUSH(objPkg, o->pkgBase[t]);
   135			u32 h64Lo = o->ndef, hLo = o->ndef + o->nh64, npLo = hLo + o->nh;
   136			const u8 *h64 = o->b + o->off[BlkHash64], *hh = o->b + o->off[BlkHash];
   137			for (u32 j = 0; j < o->total; j++) {
   138				u32 g = o->base + j;
   139				const u8 *s = osym(o, j);
   140				slotRel[g] = (u32)relOff.n;
   141				u8 k = j < h64Lo ? 1 : j < hLo ? 2 : j < npLo ? 3 : j < o->nalldef ? 4 : 5;
   142				kind[g] = k;
   143				bool dwarf = isDwarfKind(symType(s));
   144				if (symUsedInIface(s)) p1f[g] |= 1;
   145				if (k == 5 && dwarf) p1f[g] |= 2;
   146				bool named = false;
   147				if (k == 1 || k == 4) named = !dwarf && symABI(s) != SymABIstatic && symNameLen(s) != 0;
   148				if (k == 5) named = !dwarf;
   149				if (named) {
   150					key128 kk = nameKey128(o->b + ld32(s + 4), symNameLen(s), symABI(s));
   151					nkey[2 * (size_t)g] = kk.k1;
   152					nkey[2 * (size_t)g + 1] = kk.k2;
   153					if (k == 1) nPkgCand++;
   154					else nNamed++;
   155				}
   156				if (k == 1 || k == 4) rank[g] = rankOf(o, j, s) & ~rankIface;
   157				if (k == 2) {
   158					hkey[2 * (size_t)g] = ld64(h64 + (size_t)(j - h64Lo) * 8);
   159					hkey[2 * (size_t)g + 1] = symSiz(s);
   160				} else if (k == 3) {
   161					const u8 *h = hh + (size_t)(j - hLo) * HashSize;
   162					hkey[2 * (size_t)g] = ld64(h);
   163					hkey[2 * (size_t)g + 1] = ld32(h + 8);
   164				}
   165				if (k == 2 || k == 3) {
   166					u8 f = flatFL2[g];
   167					hmark[g] = (f & hashedFlags) ? 1 : (f & flRef) ? 2 : 0;
   168				}
   169				if (j < o->nalldef) {
   170					u32 nr;
   171					relocp rs = oRelocs(o, j, &nr);
   172					for (u32 r = 0; r < nr; r++) {
   173						relocp rl = RELOC(rs, r);
   174						VPUSH(relOff, rOff(rl));
   175						VPUSH(relSiz, rSiz(rl));
   176						VPUSH(relType, rType(rl));
   177						VPUSH(relAdd, rAdd(rl));
   178						VPUSH(relPkg, rPkg(rl));
   179						VPUSH(relIdx, rIdx(rl));
   180						u32 tg = orefQ(o, rPkg(rl), rIdx(rl));
   181						VPUSH(relTgt, c1(tg));
   182					}
   183				}
   184			}
   185		}
   186		objPkgOff[nobjs] = (u32)objPkg.n;
   187		slotRel[N + 1] = (u32)relOff.n;
   188		slotRel[0] = 0;
   189		meta("nrel", relOff.n);
   190		meta("p1_named", (u64)nNamed);
   191		meta("p1_pkgdef_candidates", (u64)nPkgCand);
   192		wfile("p1_kind.u8", kind, N + 1);
   193		wfile("p1_flags.u8", p1f, N + 1);
   194		wfile("p1_namekey.u64x2", nkey, 16 * (size_t)(N + 1));
   195		wfile("p1_rank.u32", rank, 4 * (size_t)(N + 1));
nl: write error: Broken pipe
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
#!/usr/bin/env bash
# Extract a frozen workload's flat files and oracles:  ref/flat/extract.sh minio  ->  data/minio/
# (frozen by bottom-up's link/freeze.sh into /home/exedev/targets/frozen/<name>; hlink copy in ref/hlink)
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd); w=$1
d=${FROZEN:-/home/exedev/targets/frozen}/$w
[ -x "$here/build/hlink" ] || "$here/ref/hlink/build.sh"
mapfile -t F < "$d/link.flags"
mkdir -p "$here/data"; rm -rf "$here/data/$w"
out=$(mktemp -d)
"$here/build/hlink" -nodetach -fresh -o "$out/a.out" -importcfg "$d/importcfg.link" "${F[@]}" -s -w \
  -dumplive "$here/data/$w.live.txt" -dumpflat "$here/data/$w" "$d/main.a"
"$out/a.out" --version > /dev/null 2>&1 || "$out/a.out" --help > /dev/null 2>&1 || true
mv "$here/data/$w.live.txt" "$here/data/$w/oracle/live_names.txt"
rm -rf "$out"
cp "$here/ref/flat/README.md" "$here/data/$w/README.md"
du -sh "$here/data/$w"
#!/usr/bin/env bash
# Time a linker on a frozen workload (link/freeze.sh) on 1, 2, 4 and 8 cores, warm and cold.
#   link/bench.sh <workload> [linker=build/hlink] [rounds=7]     -> link/results/<workload>.<linker name>.tsv
# Rows: "<cores> <phase> <ms>" from the linker's -stats table of the run whose whole process was fastest on that
# many cores (taskset and -j both set), plus phase "process" (that run's whole process by the caller's clock,
# start to exit) and, for 8 cores, "cold" (process time after dropping the page cache; the output directory
# stays). Core counts are interleaved within a round so that drift on this noisy host hits all of them alike.
# Knobs: OUT (the output path; default $HLINK_OUT/bench/<workload>.out; removed afterwards if it is on hugetlbfs, where
# it holds huge pages), FLAGS (more hlink flags, e.g. -fresh or -teardown=exit), COLD=0 (skip the cold runs: they
# drop the whole page cache).
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd)
w=$1; linker=${2:-$here/build/hlink}; rounds=${3:-7}
d=${FROZEN:-/home/exedev/targets/frozen}/$w; out=${HLINK_OUT:-/tmp/hlink-try}/bench; mkdir -p "$out" "$here/results"
mapfile -t F < "$d/link.flags"
o=${OUT:-$out/$w.out}
res=$here/results/$w.$(basename "$linker").tsv
run() { # run <cores> -> prints process ms; stats in $out/stats
  local j=$1 t0 t1
  t0=$(date +%s%N)
  taskset -c 0-$((j-1)) "$linker" -stats ${FLAGS:-} -j "$j" -o "$o" -importcfg "$d/importcfg.link" "${F[@]}" -s -w "$d/main.a" > "$out/stats"
  t1=$(date +%s%N)
  echo $(( (t1 - t0) / 1000 ))
}
declare -A best
for r in $(seq 1 "$rounds"); do
  for j in 8 1 4 2; do
    us=$(run $j)
    tot=$(awk '$1=="total"{print int($2*1000)}' "$out/stats")
    # the fastest whole process on this many cores, and the phase table of that same run
    if [ -z "${best[$j]:-}" ] || [ "$us" -lt "${best[$j]}" ]; then best[$j]=$us; cp "$out/stats" "$out/stats.$j"; fi
  done
done
cold=""
for r in $([ "${COLD:-1}" = 0 ] || echo 1 2 3); do
  sync; echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
  us=$(run 8); if [ -z "$cold" ] || [ "$us" -lt "$cold" ]; then cold=$us; fi
done
{
  for j in 1 2 4 8; do
    awk -v j=$j '$1!="phase" && $2 ~ /^[0-9.]+$/ {print j "\t" $1 "\t" $2}' "$out/stats.$j"
    echo -e "$j\tprocess\t$(awk -v u=${best[$j]} 'BEGIN{printf "%.1f", u/1000}')"
  done
  [ -n "$cold" ] && echo -e "8\tcold\t$(awk -v u=$cold 'BEGIN{printf "%.1f", u/1000}')"
  # what the link was made of, from the fastest 8-core run (the notes of the -stats table)
  sed 's/^/# /' "$out/stats.8"
  echo "# output_bytes $(stat -c %s "$o")"
  echo "# measured $(date -u +%FT%TZ), $rounds rounds, $(basename "$linker")"
} > "$res"
[ "$(stat -f -c %T "$o")" = hugetlbfs ] && rm -f "$o"
cat "$res"
nslots 5205427
nids 5205435
nobjs 1421
nunits 1763
imageBase 4194304
nrel 9403582
p1_named 627018
p1_pkgdef_candidates 162385
nbuiltin 299
p2_edges 8320873
p2_methods 106079
p2_sigs 3940
p2_strings 18937
p2_method_errors 0
p2_live 663630
p3_placed 360315
p3_rel 2053801
p3_rel_unsupported 0
p3_data_bytes 76086345
p3_typelinks 33261
p3_unitSlots 8192
lay_text 4198400
lay_rodata 46125056
lay_pclntab 68468960
lay_types 101410368
lay_noptrdata 111620096
lay_data 112981824
lay_bss 113679808
lay_noptrbss 114092896
lay_end 147849424
addr_unreachableMethod 10823232
addr_mapinitnoop 11309248
minFunc 16
image_bytes 109486380
systems/futhark/NOTES.md:74:- `futhark_new_u32_1d` copies (no zero-copy wrap of a host pointer on c/multicore); budget the copy
    53	**1a. The join by name.** Group the slots with a key (kinds 4 and 5) and the wanted keys by key. A kind-1 candidate
    54	joins a group only if its key is already there (it never creates a group). The winner of a group is the maximum
    55	of `rank << 32 | ~id` (highest rank, then lowest id); every member's canonical id is the winner. Every other slot
    56	is its own canonical id, except a kind-5 slot with `p1_flags` bit 1, whose canonical id is 0. A group whose
    57	winner is a reference stands for an undefined symbol. Builtin `i` resolves to its key's winner if that is a
    58	definition (rank bit 29), else 0. Oracle: `oracle/p1_canon.u32` (per slot), `oracle/p1_builtin.u32`.
    59	
    60	**1b. Relocation targets.** For each relocation of slot `g` in object `o`: with `sp = rel_pkg - 2147483643`,
    61	`t = sp < 5 ? npkg + sp : rel_pkg`; if `t < npkg+5` and `base = obj_pkg[o][t] != 0xFFFFFFFF`, the target slot is
    62	`builtin[rel_idx]` when `sp == 1`, else `base + rel_idx`; then its canonical id (1a). Otherwise 0.
    63	Oracle: `oracle/p1_reltarget.u32` (per relocation, in `slot_rel` order).
    64	
    65	**1c. The content-hash join** (it runs after the flood in hlink, over what the flood reached: `p1c_mark`).
    66	Among the slots with mark 1 or 2, group kind 2 by k1, kind 3 by (k1, k2). Winner: a mark-1 copy beats a mark-2
    67	one; then for kind 2 the largest size (k2), then the lowest id; for kind 3 the lowest id. If the winner has mark
    68	2 the group is not in the output and nothing changes. Otherwise every member's canonical id becomes the winner,
    69	except a mark-2 kind-2 copy larger than the winner (it keeps itself). Oracle: `oracle/p1c_canon.u32` (the
    70	final canonical id of every slot).
    71	
    72	## Problem 2: reach
    73	
    74	The graph after 1a (content-addressed copies are still themselves: the flood treats each copy as its own symbol).
    75	Ids are canonical slot ids.
    76	
    77	| file | what |
    78	|---|---|
    79	| `p2_flags.u8` | per slot: 1 def (a definition, not an undefined symbol), 2 text, 4 reflect (calls reflect's Method), 8 gotype, 16 iface (converted to an interface, known from resolve), 32 has something to follow |
    80	| `p2_edge_off.u32`, `p2_edge_dst.u32`, `p2_edge_kind.u8` | CSR per source (only canonical definitions with flag 32 have rows). Kind 0 plain: a relocation or funcdata the flood follows, dst = target id. Kind 1 useiface: dst is a type converted to an interface (an itab already replaced by its type). Kind 2 ifacemethod: dst = a signature id (an interface method some live code may call). Kind 3 namedmethod: dst = a string id (a method name called by reflection-like generic code) |
    81	| `p2_sig_name.u32`, `p2_sig_type.u32` | per signature id: (method name string id, function type id) |
    82	| `p2_meth_off.u32` + `p2_meth_name.u32 p2_meth_mtyp.u32 p2_meth_ifn.u32 p2_meth_tfn.u32 p2_meth_exported.u8` | CSR per type: its methods: name string id, the method's function type, the interface-call entry, the direct entry (ids, 0 if none), and whether the name is exported |
    83	| `p2_str.off.u64`, `p2_str.bin` | the method name strings |
    84	| `p2_roots.u32` | the entry points (entry symbol, init tasks in order, a few runtime symbols) |
    85	
    86	Rules (cmd/link's deadcode.go, as hlink does them; `s` ranges over live defined symbols):
    87	
    88	    live(r)              for every root r
    89	    live(t)              <- live(s), plain(s,t)
    90	    iface(t)             <- flag 16 on t
    91	    iface(t)             <- live(s), useiface(s,t)
    92	    iface(t)             <- live(s), iface(s), gotype(s), plain(s,t), gotype(t), def(t)
    93	    sig(x)               <- live(s), ifacemethod(s,x)
    94	    named(n)             <- live(s), namedmethod(s,n)
    95	    reflect              <- live(s), reflect(s)
    96	    iface(m.mtyp), cand(m) <- live(s), iface(s), gotype(s), method(s,m)
    97	    live(m.mtyp), live(m.ifn), live(m.tfn)
    98	                         <- cand(m), (reflect and m.exported) or sig(m.name, m.mtyp) or named(m.name)
    99	
   100	`sig(m.name, m.mtyp)` means some live signature id has that (name, type) pair. Oracle: `oracle/p2_live.u32`, the
   101	sorted ids of every live symbol (663,630 for MinIO: this counts each content-addressed copy and undefined symbol;
   102	after 1c, 360,376 canonical symbols are live: `oracle/live_names.txt`, which equals fclink's and the stock linker's
   103	live list).
   104	
   105	## Problem 3: lay out and relocate
   106	
   107	The placed symbols: live canonical definitions that go into a bucket (a part of a section), in hlink's unit
   108	order (a unit is an object, or a run of 8192 slots of a big one) and then id order.
   109	
   110	| file | what |
   111	|---|---|
   112	| `p3_sym.u32` | per placed symbol: its id |
   113	| `p3_bucket.u8` | 1 text, 2 text FIPS, 3 rodata, 4 rodata FIPS, 5 go:func, 6 typelink, 7 type, 8 itab, 9 noptrdata, 10 noptrdata FIPS, 11 data, 12 data FIPS, 13 bss, 14 noptrbss, 15 coverage counters |
   114	| `p3_class.u8` | log2 of its alignment |
   115	| `p3_size.u32`, `p3_unit.u32` | its size; its unit |
   116	| `p3_tlstr.u32` + `p3_str.*` | typelink bucket only: its type string (string id) |
   117	| `p3_bucket_start.u32`, `p3_bucket_minalign.u32` | per bucket (16 entries): its start address and minimum alignment |
   118	| `p3_data.off.u64`, `p3_data.bin` | per placed symbol: its bytes (may be shorter than its size: the rest is zero; bss buckets: none) |
   119	| `p3_rel_start.u32` + `p3_rel_off.u32 p3_rel_siz.u8 p3_rel_kind.u8 p3_rel_add.i64 p3_rel_tgt.u32` | CSR per placed symbol: its relocations that write bytes: offset, width (1, 2, 4, 8), kind (below), addend, target slot id |
   120	| `p3_addr_all.u32` | per id (`nids`): the final address of what it resolves to (0: not in the output) |
   121	
   122	**3a. Layout.** occupied(i) = size rounded up to 2^class, where a text symbol (buckets 1, 2) is at least `minFunc`
   123	(16) bytes. For each bucket other than typelink: `va = start`; for each unit in order: if the unit has bytes in
   124	this bucket, `va = round_up(va, max(minalign, 2^top))` with `top` its largest class with bytes; then for each class
   125	from 12 down to 0: the class's symbols in list order get `va, va + occupied, ...`. Typelinks: sort by type string
   126	(bytewise, then shorter first), then id; `va = start`; each gets `va = round_up(va, 2^class)` and then
   127	`va += size` (not occupied). Oracle: `oracle/p3_addr.u32` (per placed symbol).
   128	
   129	**3b. Relocate.** Copy each placed symbol's bytes to `image[addr - imageBase]` (not buckets 13-15) and apply its
   130	relocations, `P = addr + off`, `T = p3_addr_all[tgt]`:
   131	
   132	| kind | value written (`siz` bytes, little-endian) |
   133	|---|---|
   134	| 1 addr | `T + add`; T = 0 with tgt != 0 is an error |
   135	| 2 addr, weak | as 1, but T = 0 with tgt != 0 means T = `addr_unreachableMethod` (meta) |
   136	| 3 addroff | `T - sectbase(T) + add`; nothing if T = 0. sectbase: the largest of `lay_text, lay_rodata, lay_pclntab, lay_types, lay_noptrdata, lay_data, lay_bss, lay_noptrbss` (meta) not above T |
   137	| 4 methodoff | as 3, but T = 0 writes 0xFFFFFFFF |
   138	| 5 call, 7 pcrel | `T + add - (P + siz)`; tgt = 0: `add - (P + siz)`; T = 0 with tgt != 0 is an error |
   139	| 6 call, weak | as 5, but T = 0 with tgt != 0 means T = `addr_mapinitnoop` (meta) |
   140	| 8 const | `add` (TLS and size relocations folded in) |
   141	
   142	Oracle: `oracle/p3_image.bin` is hlink's whole output file (file offset = address - imageBase); every placed
   143	symbol's `[addr, addr+size)` must match it. hlink also writes the pclntab, GC bitmaps, type links, moduledata
   144	and ELF headers; those are not part of problem 3.
   145	
   146	## What the extraction did for the systems (so a timing is read fairly)
   147	
   148	The flat files are not raw object files: they have names hashed into 128-bit keys (P1), references decoded
   149	into canonical ids and itabs resolved to their types (P2), method tables decoded from type descriptors (P2), and
   150	relocation types normalised and size relocations folded into constants (P3). hlink does that work inside its
   151	phases; a system here does not. Compare a system's time with the plain C references on the same files first,
   152	and with hlink's phase times only with that caveat.

exec
/bin/bash -lc 'build/p3 data/minio' in /home/exedev/paradigms-w
 exited 127 in 0ms:
/bin/bash: line 1: build/p3: No such file or directory

exec
/bin/bash -lc "rg -n 'bText|bTypelink|rankOf|rankStrong|rankDef|hashedFlags|stPC' ref/hlink/hlink.h ref/hlink/*.c | head -n 65; nl -ba GRID.md; nl -ba STATUS.md; nl -ba JOURNAL.md; rg -n 'enum|bText|bRodata|bTypes|bItab' ref/hlink/*.h" in /home/exedev/paradigms-w
 succeeded in 0ms:
rg: ref/hlink/hlink.h: No such file or directory (os error 2)
ref/hlink/layout.c:96:	case K_STEXT: return bText;
ref/hlink/layout.c:97:	case K_STEXTFIPS: return bTextFIPS;
ref/hlink/layout.c:101:		if (hasPrefix(symNameOf(o, s), "type:")) return symTypelink(s) ? bTypelink : bType;
ref/hlink/layout.c:116:static inline bool isTextBucket(int b) { return b == bText || b == bTextFIPS; }
ref/hlink/layout.c:163:		if ((st & (stLive | stPC | stNameNeeded)) == 0 || canon[gid] != gid) continue;
ref/hlink/layout.c:165:		if (st & stPC) u->pcSize += oDataSize(o, j);
ref/hlink/layout.c:169:		if ((st & stLive) && !(st & (stOverride | stPC))) b = bucketOf(o, s, st);
ref/hlink/layout.c:173:			u->nfunc[b - bText]++;
ref/hlink/layout.c:181:		if (b == bTypelink) {
ref/hlink/layout.c:183:			state[gid] = st | (u32)bTypelink << stBucketShift;
ref/hlink/layout.c:196:		if (isTextBucket(b)) u->nfuncCl[b - bText][cl]++;
ref/hlink/layout.c:307:		if ((st & (stLive | stPC | stNameNeeded)) == 0 || canon[gid] != gid) continue;
ref/hlink/layout.c:309:		if (st & stPC) {
ref/hlink/layout.c:321:		if (b == bNone || b == bTypelink) continue;
ref/hlink/layout.c:379:	bucket(bText, funcAlign, &lay.text, NULL);
ref/hlink/layout.c:380:	bucket(bTextFIPS, funcAlign, &lay.textFIPS, &lay.etextFIPS);
ref/hlink/dumpflat.c:156:			if (k == 1 || k == 4) rank[g] = rankOf(o, j, s) & ~rankIface;
ref/hlink/dumpflat.c:167:				hmark[g] = (f & hashedFlags) ? 1 : (f & flRef) ? 2 : 0;
ref/hlink/dumpflat.c:468:			if ((st & (stLive | stPC | stNameNeeded)) == 0 || canon[g] != g) continue;
ref/hlink/dumpflat.c:472:			u32 cl = b == bTypelink ? (u32)__builtin_ctz(MAX(8, symAlign(s))) : (st >> stClassShft & 15);
ref/hlink/dumpflat.c:483:			u32 tls_ = b == bTypelink ? stId(&T, typeString(g)) : 0;
ref/hlink/dumpflat.c:557:	flatBucketStart[bTypelink] = lay.types + 8;
ref/hlink/dumpflat.c:558:	flatBucketMinAlign[bTypelink] = 8;
ref/hlink/write.c:341:		if ((st & (stLive | stPC | stNameNeeded)) == 0 || canon[gid] != gid) continue;
ref/hlink/write.c:343:		if (st & stPC) {
ref/hlink/write.c:486:		u32 fi0 = fidx[b - bText][st >> stClassShft & 15]++;
ref/hlink/dead.c:404:				if (atg[i] != 0 && !(atomicLoad8(&FL(atg[i])) & stPC)) atomicOr8(&FL(atg[i]), stPC);
ref/hlink/resolve.c:29:#define rankDef (1u << 29)
ref/hlink/resolve.c:182:static u32 rankOf(const object *o, u32 j, const u8 *s) {
ref/hlink/resolve.c:183:	u32 r = MIN(symSiz(s), (1u << 28) - 1) | rankDef;
ref/hlink/resolve.c:232:				rk = rankOf(o, j, s);
ref/hlink/resolve.c:331:#define hashedFlags (stLive | stPC | stFuncdata | stNameNeeded)
ref/hlink/resolve.c:377:		if ((f & (hashedFlags | flRef)) == 0) continue;
ref/hlink/resolve.c:378:		u32 id = gid | ((f & hashedFlags) ? 0 : refOnly);
ref/hlink/resolve.c:447:				FL(win) |= FL(gid) & hashedFlags; // (a class is in one partition: no other thread writes these)
ref/hlink/resolve.c:480:				u64 val = (u64)rankOf(o, j, s) << 32 | (u32)~gid, old = __atomic_load_n(&e->val, __ATOMIC_RELAXED);
ref/hlink/resolve.c:512:		u32 f = (te[h].val >> 32 & rankDef ? 0 : stUndef) | (te[h].val & valIface ? stUsedInIface : 0);
     1	# GRID
     2	
     3	One cell per (system, problem): correct? (oracle command) · time 1 core / 8 cores, warm / end to end ·
     4	lines · easy / forced / impossible. Dataset: MinIO unless marked. Data contract and rules: `ref/flat/README.md`.
     5	
     6	All times are wall ms on this VM (AMD EPYC 9554P under KVM, 8 cores), best of >= 3 runs, load < 1 unless marked.
     7	
     8	## Reference: C
     9	
    10	Two references, measured 2026-11-16T15:33Z (load 0.4):
    11	
    12	- **hlink** (bottom-up's tuned C linker, `ref/hlink` = bottom-up `774e37c`), from its own `-stats` and `HLINK_TSC=1`
    13	  stage clocks, `ref/hlink/bench.sh minio` -> `results/minio.hlink.tsv`. It reads raw Go objects, so its phases
    14	  include work the flat files have done in advance (name hashing, decoding references): it is the speed target,
    15	  not a like-for-like row. Whole link: 537 ms on 1 core, **149 ms on 8 cores** (process, warm).
    16	- **plain C on the flat files** (`ref/flat/p1.c p2.c p3.c`, single-threaded, untuned, one hash table or one loop
    17	  per rule): the like-for-like baseline every system is compared with first. `ref/flat/check.sh` runs them and
    18	  checks every oracle.
    19	
    20	| problem | floor (8 cores, FLOOR.md model) | hlink 1 core / 8 cores | plain C, flat, 1 core warm | check |
    21	|---|---|---|---|---|
    22	| 1a+1b. resolve: join by name, relocation targets | 1.36 (resolve) | 126-136 / 29-33 (resolve phase) | 186-206 (join) + 43-58 (9.4 M targets) | `build/p1 data/minio` -> P1 OK |
    23	| 1c. content-hash join | (in resolve) | 37-39 / 8-9 (inside deadcode) | 310-380 (untuned: a 400 MB table) | same |
    24	| 2. reach | 2.78 | 98-103 / 22-23 floods + 7 / 1.5 methods | 41-46 | `build/p2 data/minio` -> P2 OK |
    25	| 3a. lay out | 0.60 | 85 / 23-25 (layout phase, includes pclntab sizing and the typelink sort) | 8-19 | `build/p3 data/minio` -> P3 OK |
    26	| 3b. relocate | 4.92 (write) | 102-104 / 15 (write phase, includes pclntab and GC bitmaps) | 59-67 (76 MB, 2.05 M relocations) | same |
    27	| 4. codegen by shape | | | | |
    28	
    29	Floors are bottom-up's FLOOR.md model (same CPU model, the 2026-11-13 VM); they count the raw object bytes hlink
    30	reads, so for the flat files they are an upper bound on the floor.
    31	
    32	## The paradigms
    33	
    34	| problem | Goose | Futhark | Halide | Soufflé |
    35	|---|---|---|---|---|
    36	| 1. resolve | | | | |
    37	| 2. reach | | | | |
    38	| 3. lay out + relocate | | | | |
    39	| 4. codegen by shape (stretch) | | | | |
     1	# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)
     2	
     3	2026-11-16T15:40Z. Campaign 1, **M0 reached**: MinIO's flat files and oracles for problems 1-3, checked by plain C
     4	references that reproduce every oracle exactly; hlink here: 149 ms whole link on 8 cores. Oracle number: 0/20 cells, 0 attacks.
     5	In flight (M1/M2): Opus builders for Soufflé, Futhark, Halide, Goose, each doing problems 1-3.
     6	Early surprise to chase: a plain 1-core worklist flood over the flat (pre-decoded) edges takes 41-46 ms; hlink's tuned flood
     7	over raw objects takes 98-103 ms on 1 core. The decode, not the graph walk, is most of reach.
     8	
     9	## Decisions for Dan
    10	- None yet.
     1	# JOURNAL (append-only: one line per commit, a paragraph at each milestone)
     2	- 2026-11-16T15:31:42Z M0 data: MinIO frozen (1,311 archives + main.a), hlink copied to ref/hlink with -dumpflat, flat files + oracles for P1-P3 (ref/flat/README.md); plain 1-core C references reproduce every oracle exactly (ref/flat/check.sh). Installs merged: Futhark 0.29.1, Halide 21, Soufflé 2.5, Goose v2026.4 (systems/*/NOTES.md).
     3	- 2026-11-16T15:34:24Z M0: GRID.md C column (hlink phases on this VM, plain flat C references), IDEAS.md index, four Opus cell builders launched (ideas/BUILDER.md).
ref/hlink/common.h:170:enum {
ref/hlink/common.h:172:	bText,
ref/hlink/common.h:173:	bTextFIPS,
ref/hlink/common.h:174:	bRodata,
ref/hlink/common.h:175:	bRodataFIPS,
ref/hlink/common.h:179:	bItab,
ref/hlink/common.h:363:enum {
ref/hlink/common.h:388:enum {

exec
/bin/bash -lc "python3 - <<'PY'
import struct,pathlib
p=pathlib.Path('data/minio')
def arr(n,fmt='I'):
 b=(p/n).read_bytes();return struct.unpack('<'+str(len(b)//struct.calcsize(fmt))+fmt,b)
s=arr('p3_sym.u32'); a=arr('p3_addr_all.u32'); o=arr('oracle/p3_addr.u32')
print('p3_addr_all[p3_sym] equals oracle:',sum(a[g]==v for g,v in zip(s,o)),'/',len(s))
f=arr('name.off.u64','Q'); b=(p/'name.bin').read_bytes()
print('name slices 0,1:',repr(b[f[0]:f[1]]),repr(b[f[1]:f[2]]))
mt=arr('p2_meth_mtyp.u32'); mi=arr('p2_meth_ifn.u32'); mf=arr('p2_meth_tfn.u32')
print('zero method targets:',*[x.count(0) for x in (mt,mi,mf)])
PY" in /home/exedev/paradigms-w
 succeeded in 449ms:
p3_addr_all[p3_sym] equals oracle: 360315 / 360315
name slices 0,1: b'main.main' b'main..inittask'
zero method targets: 0 0 0

exec
/bin/bash -lc "python3 - <<'PY'
from pathlib import Path
import array
p=Path('data/minio'); m=(p/'p1c_mark.u8').read_bytes(); a=array.array('I'); a.frombytes((p/'oracle/p2_live.u32').read_bytes()); live=set(a)
print('mark=1 but absent from flood live oracle:',sum(v==1 and i not in live for i,v in enumerate(m)))
PY
sed -n '170,190p' ref/hlink/common.h
sed -n '180,192p' ref/hlink/resolve.c" in /home/exedev/paradigms-w
 succeeded in 454ms:
mark=1 but absent from flood live oracle: 807866
enum {
	bNone,
	bText,
	bTextFIPS,
	bRodata,
	bRodataFIPS,
	bGofunc,
	bTypelink, // laid out one by one in sorted order, not by unit
	bType,
	bItab,
	bNoptrdata,
	bNoptrdataFIPS,
	bData,
	bDataFIPS,
	bBSS,
	bNoptrBSS,
	bCov,
	nBuckets,
};
static const char *const bucketName[nBuckets] = {"none", "text", "textfips", "rodata", "rodatafips", "gofunc", "typelink",
	"type", "itab", "noptrdata", "noptrdatafips", "data", "datafips", "bss", "noptrbss", "covctrs"};
}

static u32 rankOf(const object *o, u32 j, const u8 *s) {
	u32 r = MIN(symSiz(s), (1u << 28) - 1) | rankDef;
	if (symUsedInIface(s)) r |= rankIface;
	if (symDupok(s)) return r;
	r |= rankNoDup;
	if (kindIsText(symType(s)) || oDataSize(o, j) != 0) r |= rankContent;
	return r;
}

// The names the linker itself looks up; definitions by package index of these names must join by name.
static const char *const linkerNames[] = {"runtime.firstmoduledata", "runtime.lastmoduledatap", "runtime.runtime_inittasks",

codex
1. **No cell literally omits output checking.** C P1/P2 compare arrays; P3 compares addresses and symbol bytes. But **P3 does not test the combined layout→relocation pipeline**: relocation reads `oracle/p3_addr.u32` instead of its computed addresses ([p3.c:80](/home/exedev/paradigms-w/ref/flat/p3.c:80)). I tried `build/p3 data/minio`; it failed with exit **127**, binary absent. `check.sh` requires writing build artifacts, so I did not run it.

2. **No premature “falls down” verdict exists in this checkout.** All **16 paradigm cells are empty** ([GRID.md:34](/home/exedev/paradigms-w/GRID.md:34)).

3. **The flat C/hlink timings compare different work.** Reach is **41–46 ms versus 98–103 ms**, with decoding already performed for flat C. Relocation is **59–67 ms versus 102–104 ms**, with pclntab and GC bitmap generation excluded from flat C ([GRID.md:24](/home/exedev/paradigms-w/GRID.md:24)). GRID acknowledges this. STATUS nevertheless concludes “The decode, not the graph walk, is most of reach” without an isolated decoding measurement.

4. **The reach surprise is recorded, but its explanation is untested** ([STATUS.md:6](/home/exedev/paradigms-w/STATUS.md:6)). Measure raw-reference versus predecoded traversal in the same implementation, interleaved, including extraction cost separately. Another unexplored result: flat C’s content-hash join takes **310–380 ms versus hlink’s 37–39 ms**—roughly **8–10× slower** ([GRID.md:23](/home/exedev/paradigms-w/GRID.md:23)).

5. **None: ATTACKS has zero entries**, only its heading and instructions ([ATTACKS.md:1](/home/exedev/paradigms-w/ATTACKS.md:1)). The decoding hypothesis above could become an entry after that controlled measurement.

6. **No qualifying JOURNAL passage exists.** Its entire history is **two entries**, at **15:31:42Z** and **15:34:24Z**, reporting extraction and builder launches; neither declines work ([JOURNAL.md:2](/home/exedev/paradigms-w/JOURNAL.md:2)).

The flat-file contract has concrete defects:

- **Layout answers leak into an input.** My read-only check found `p3_addr_all[p3_sym[i]] == oracle/p3_addr[i]` for **360,315 / 360,315 symbols**. The dump writes final `addr` directly ([dumpflat.c:576](/home/exedev/paradigms-w/ref/hlink/dumpflat.c:576)). P3a actually computes layout, so its success is not merely copying; nevertheless, another implementation could gather the complete answer from this input. P3b explicitly depends on precomputed source and target addresses.

- **The name table is shifted by one.** README says index equals symbol ID, but the dump omits an empty slot-zero offset before appending symbol names ([dumpflat.c:99](/home/exedev/paradigms-w/ref/hlink/dumpflat.c:99)). Measured slices: **index 0 = `main.main`; index 1 = `main..inittask`**. P1 hashes bypass this broken table.

- **`p1c_mark=1` does not mean simply “the flood reached it.”** It includes `stLive | stPC | stFuncdata | stNameNeeded` ([resolve.c:331](/home/exedev/paradigms-w/ref/hlink/resolve.c:331)). I counted **807,866 mark-1 slots absent from `oracle/p2_live.u32`**. The README’s description would produce an incorrect pipeline if marks were reconstructed from the live set alone.

- **Rank bit 30 is described incorrectly.** It is set only for **non-DUPOK** definitions, and text qualifies even without bytes; DUPOK returns before setting it ([resolve.c:182](/home/exedev/paradigms-w/ref/hlink/resolve.c:182)).

- **Reach rules omit the zero-ID guard.** README permits method targets of zero, but its rules would derive `live(0)`; C explicitly ignores zero in both `mark` and `setIface` ([p2.c:19](/home/exedev/paradigms-w/ref/flat/p2.c:19)). This dataset has **zero null method targets**, so that omission is unexercised.

No files were edited.
tokens used
55,356
1. **No cell literally omits output checking.** C P1/P2 compare arrays; P3 compares addresses and symbol bytes. But **P3 does not test the combined layout→relocation pipeline**: relocation reads `oracle/p3_addr.u32` instead of its computed addresses ([p3.c:80](/home/exedev/paradigms-w/ref/flat/p3.c:80)). I tried `build/p3 data/minio`; it failed with exit **127**, binary absent. `check.sh` requires writing build artifacts, so I did not run it.

2. **No premature “falls down” verdict exists in this checkout.** All **16 paradigm cells are empty** ([GRID.md:34](/home/exedev/paradigms-w/GRID.md:34)).

3. **The flat C/hlink timings compare different work.** Reach is **41–46 ms versus 98–103 ms**, with decoding already performed for flat C. Relocation is **59–67 ms versus 102–104 ms**, with pclntab and GC bitmap generation excluded from flat C ([GRID.md:24](/home/exedev/paradigms-w/GRID.md:24)). GRID acknowledges this. STATUS nevertheless concludes “The decode, not the graph walk, is most of reach” without an isolated decoding measurement.

4. **The reach surprise is recorded, but its explanation is untested** ([STATUS.md:6](/home/exedev/paradigms-w/STATUS.md:6)). Measure raw-reference versus predecoded traversal in the same implementation, interleaved, including extraction cost separately. Another unexplored result: flat C’s content-hash join takes **310–380 ms versus hlink’s 37–39 ms**—roughly **8–10× slower** ([GRID.md:23](/home/exedev/paradigms-w/GRID.md:23)).

5. **None: ATTACKS has zero entries**, only its heading and instructions ([ATTACKS.md:1](/home/exedev/paradigms-w/ATTACKS.md:1)). The decoding hypothesis above could become an entry after that controlled measurement.

6. **No qualifying JOURNAL passage exists.** Its entire history is **two entries**, at **15:31:42Z** and **15:34:24Z**, reporting extraction and builder launches; neither declines work ([JOURNAL.md:2](/home/exedev/paradigms-w/JOURNAL.md:2)).

The flat-file contract has concrete defects:

- **Layout answers leak into an input.** My read-only check found `p3_addr_all[p3_sym[i]] == oracle/p3_addr[i]` for **360,315 / 360,315 symbols**. The dump writes final `addr` directly ([dumpflat.c:576](/home/exedev/paradigms-w/ref/hlink/dumpflat.c:576)). P3a actually computes layout, so its success is not merely copying; nevertheless, another implementation could gather the complete answer from this input. P3b explicitly depends on precomputed source and target addresses.

- **The name table is shifted by one.** README says index equals symbol ID, but the dump omits an empty slot-zero offset before appending symbol names ([dumpflat.c:99](/home/exedev/paradigms-w/ref/hlink/dumpflat.c:99)). Measured slices: **index 0 = `main.main`; index 1 = `main..inittask`**. P1 hashes bypass this broken table.

- **`p1c_mark=1` does not mean simply “the flood reached it.”** It includes `stLive | stPC | stFuncdata | stNameNeeded` ([resolve.c:331](/home/exedev/paradigms-w/ref/hlink/resolve.c:331)). I counted **807,866 mark-1 slots absent from `oracle/p2_live.u32`**. The README’s description would produce an incorrect pipeline if marks were reconstructed from the live set alone.

- **Rank bit 30 is described incorrectly.** It is set only for **non-DUPOK** definitions, and text qualifies even without bytes; DUPOK returns before setting it ([resolve.c:182](/home/exedev/paradigms-w/ref/hlink/resolve.c:182)).

- **Reach rules omit the zero-ID guard.** README permits method targets of zero, but its rules would derive `live(0)`; C explicitly ignores zero in both `mark` and `setIface` ([p2.c:19](/home/exedev/paradigms-w/ref/flat/p2.c:19)). This dataset has **zero null method targets**, so that omission is unexercised.

No files were edited.
