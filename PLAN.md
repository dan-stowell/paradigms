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

Now (2026-11-16T16:02Z): M1/M2 in flight. Builders (Soufflé, Futhark, Halide, Goose, P4 data) run in .claude/worktrees/agent-* on branches worktree-agent-*, owned by an earlier session; a DuckDB builder (new column, JOURNAL 16:00Z) runs from this session in its own worktree. Merge a branch when its systems/<name>/CELLS.md exists: rerun its oracle check, paste cells into GRID.md. If a worktree has no commit for >1 h and no process (`pgrep -af worktrees`), restart a builder from its branch with ideas/BUILDER.md. Codex jobs: ~/paradigms-c (branch codex-c: p3.c --pipeline from its own 3a addresses + parallel 1c in p1par; log /tmp/claude-1000/w/codex-c.log; it found 81 relocation errors when rebuilding p3_addr_all from canon: find out what p3_addr_all encodes beyond canon) and ~/paradigms-d (branch codex-d: ref/flat/p2pipe.c, eager decode of every relocation into a canonical CSR + flood, 1 and 8 threads, both datasets; log codex-d.log). When codex-d reports: write ATTACKS.md entry 1 (reach over an eagerly decoded canonical CSR vs hlink's lazy decode: so far flood 27-30 / 13-17 ms vs hlink 98-103 / 22-23; dumpflat's untuned decode 133-139 ms 1 core; p1par's 1b decode 31 / 5-6 ms) with p2pipe's totals. kube-apiserver is extracted and passes every C ref (GRID.md).
