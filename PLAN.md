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

Now: M1/M2 in flight (fresh context taken 2026-11-16T15:40Z with helpers running). Helpers launched before the clear, each in a worktree under .claude/worktrees/agent-* on branch worktree-agent-* (see `git worktree list`): Opus cell builders for Soufflé, Futhark, Halide, Goose (brief: ideas/BUILDER.md; each writes systems/<name>/NOTES.md, CELLS.md, bench.sh), and an Opus problem-4 data builder (ref/p4/, data/minio-p4/, oracle = fc's per-node emitted bytes). If their completion notifications don't arrive, check progress with `git -C .claude/worktrees/agent-<id> log --oneline -5` and their files; merge a branch when its CELLS.md/NOTES.md say a problem is done, rerun its oracle check, and paste cells into GRID.md; if a worktree has had no commit for >1 hour and no process is running in it (`pgrep -af worktrees`), restart a builder from its branch with the same brief. Also running: Codex writing ref/flat/stats.c -> results/minio.stats.txt (BFS level histogram etc.; check `ls results/`, log /tmp/claude-1000/w/codex-stats.log), the M0 Codex skeptic -> reviews/inbox/*-codex-skeptic.md (triage into REVIEWS.md), and kube-apiserver freeze (log ~/targets/freeze-kube.log -> /home/exedev/targets/frozen/kube-apiserver; then `ref/flat/extract.sh kube-apiserver` and `ref/flat/check.sh data/kube-apiserver`). When the machine is quiet (uptime load < 1): re-measure `ref/flat/check.sh` (plain + p1par/p2par: provisional 19.2 ms resolve, 12.1 ms flood on 8 threads under load) and put a "tuned C on flat, 8 threads" column in GRID.md next to hlink; then draft ATTACKS.md entry 1 (reach over pre-decoded canonical CSR: plain 1-core flood 41-46 ms vs hlink's 98-103 ms flood on raw objects; parallel 12 ms vs 22 ms; the price is decoding all 9.4 M references, 1b = 7.5 ms on 8 threads) with a measurement of the full decode + flood pipeline against hlink resolve+deadcode.
