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

Now: Start M0. Clone the reference repos into /home/exedev/targets, build MinIO with the stock toolchain and freeze its objects (bottom-up's scripts), design and write the flat-file extractor and the oracles. In parallel: builders in worktrees install Goose, Futhark, Halide and Soufflé and run each one's hello-world on this VM; Sonnet prior-art readers (one per paradigm, one per problem, one wildcard) write to IDEAS.md.
