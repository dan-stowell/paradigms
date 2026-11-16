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

Now (2026-11-16T16:20Z, lead session paradigms-46): M1/M2 in flight. **A /clear or session end kills the session's
subagents**: the builders below run inside paradigms-46; if it is gone, relaunch them with ideas/RESUME.md (one Opus agent
per worktree, prompt = "You are the <sys> cell builder ... worktree <path> ... follow ideas/RESUME.md").
Builders (worktrees .claude/worktrees/): souffle agent-a04daa.., futhark agent-a86aa6.., halide agent-a45ed3.., goose
agent-a8177a.., duckdb agent-a66651... Each replies when done; then rerun its oracle check, merge its branch, paste CELLS.md
into GRID.md. Codex: codex-e (paradigms-e) = lazy funcdata aux in p2lazy (ATTACKS 1 follow-up a); codex-f (paradigms-c) =
p1compact, testing "p1par's join is bound by 4 passes over the dense 83 MB key array; only 15% of slots are keyed"
(results/codex-f.md). When codex-e lands: add the timed-decoder hash check and relaxed atomics (REVIEWS.md, attack1).
Done this session: ATTACKS 2 numbers (huge pages 24 vs 49 ms 1 thread, 5.4-6.8 vs 14 on 8, floor 4.92); hlink huge vs
none within noise; ATTACKS 1 narrowed to lazy vs eager. Measured bandwidth here: 25 GB/s read 1 thread, 36 on 8.
