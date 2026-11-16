# MORNING, 2026-11-16T17:22Z (M1 + M2)

## The grid (MinIO, ms, 1 / 8 threads, warm; load 2-13 during the builders' runs, so 8-thread numbers are upper bounds)

| problem | tuned C (flat) | Goose | Futhark | Halide | Soufflé | DuckDB |
|---|---|---|---|---|---|---|
| 1a join by name | p1compact 28 / 7-10 | 79-115 / 37-57 | 79 / 24 (2 MB) | 81-93 / 20-27 | 1928-2034 / 636 | - / 103-113 |
| 1b targets | 43 / 5-6 | 39-57 / 42-67 | 47 / 18 | 43-46 / 10-11 | 5220-5426 / 1080 | - / 272-375 |
| 1c content join | 37 / 7-9 | 97-118 / 39-47 | 111 / 38 | 122-133 / 33-37 | 1625-1865 / 394-587 | - / 128-190 |
| 2 reach (edges given) | 27-30 / 13-17 | 41-49 / 22-32 | 97 / 58 | 312-332 / 70-127 | 1227-1467 / 431 | 103 + ~2000 (falls down) |
| 2' reach, own decode | lazy 53 / 14-15 (eager 109 / 30) | 74-79 / 37-49 | 165 / 90 (falls down) | 417-465 / 79-90 | - / 960 | ~3500 (lazy ties eager) |
| 3a lay out | p3a_par 5.0 / 3.9 | 15-26 / 17-31 | 14 / 7.3 | - / 20-23 | 7222-7589 / 1339 (falls down) | - / 217-263 |
| 3b relocate (2 MB pages) | 24 / 5.4-6.8 | 41-42 / 17-24 | 107 / 41 | - / 27-31 | - / 320 | - / 309-332 |

Every cell is correct against its oracle; the lead rebuilt each from source and reran every check. Long form:
GRID.md and systems/*/CELLS.md, NOTES.md (with code excerpts).

## Surprises
- **Up:** Halide's 1a beats plain C 2.5-3x on one core (it compacts the 15% keyed slots first). The same idea, plus
  partitioning the probe side, made the C reference 1.5x faster (p1compact; codex-h showed the probe side was the cost).
- **Up:** one environment variable (`GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, found by the Futhark builder) gives 2x on
  relocate and 1.5x on the hash join, in C and in Futhark, with no code change.
- **Down:** the decode-in-the-flood win (ATTACKS 1) does not carry over to the data-parallel paradigms: in Futhark and
  Halide the lazy flood is slower than their eager one, because each of the 111 levels costs whole-array passes.
- **Down:** on this shared VM, OpenMP's default spinning barrier makes an 8-thread flood 60x slower (900-1300 ms vs 14).
- **Down:** this VM streams 25 / 36 GB/s (1 / 8 threads); FLOOR.md's floors assume ~87 GB/s, so they are ~2.4x low here.

## ATTACKS.md (all three new this milestone)
1. **Reach: don't build the graph; decode each reference when the flood reaches it.** 2x the eager pipeline in C
   (52.6 / 13.6 vs 108.7 / 30.1 ms; Codex reproduced 2.1-2.6x), still 1.6x with its own aux decode. The hlink ratio was
   withdrawn after review (not equivalent work).
2. **Relocate: the output's page faults are half the work.** 2 MB pages: 24 vs 49 ms (1 thread), 5.4-6.8 vs 14 (8),
   at the VM's bandwidth bound; free via the glibc tunable.
3. **Reach: the level count is the cost; drop level synchronization.** A worklist flood with 7 joins instead of 111
   barriers: within noise when the machine is lightly loaded, 1.7x passive OpenMP and 25x spinning under overload.

## Ideas tried and killed
Per-source edge dedup (8%); flood inputs on huge pages (9-12%); "the join is bandwidth-bound" (it was random probes);
hlink gaining from huge pages (noise).

## Review disagreements worth your attention
- Codex on ATTACKS 1: the hlink comparison does different work in both directions -> accepted; claim narrowed.
- M2 skeptic review is running (reviews/inbox); triage in REVIEWS.md.

## Decisions for you
None. vm.nr_hugepages was raised to 1536 (3 GB) for the builders and the tunable; it resets on reboot.
