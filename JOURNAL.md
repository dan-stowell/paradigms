# JOURNAL (append-only: one line per commit, a paragraph at each milestone)
- 2026-11-16T15:31:42Z M0 data: MinIO frozen (1,311 archives + main.a), hlink copied to ref/hlink with -dumpflat, flat files + oracles for P1-P3 (ref/flat/README.md); plain 1-core C references reproduce every oracle exactly (ref/flat/check.sh). Installs merged: Futhark 0.29.1, Halide 21, Soufflé 2.5, Goose v2026.4 (systems/*/NOTES.md).
- 2026-11-16T15:34:24Z M0: GRID.md C column (hlink phases on this VM, plain flat C references), IDEAS.md index, four Opus cell builders launched (ideas/BUILDER.md).
- 2026-11-16T15:36:04Z ref/flat/p2par.c: level-synchronous OpenMP flood on the flat CSR, full rules, P2 OK: 33.4 ms 1 thread, 12.1 ms 8 threads (load 14: provisional), 111 levels in 7 rounds; hlink's flood is 98-103 / 22-23 ms on raw objects. Problem 4 data/oracle handed to an Opus agent.
- 2026-11-16T15:37:00Z ref/flat/p1par.c: partitioned join (256 partitions, per-partition tables) + parallel gather, P1 OK: 72.7 ms 1 thread, 19.2 ms 8 threads (load 7, provisional) vs plain 230 ms; hlink resolve 29-33 ms on raw objects.
- 2026-11-16T15:54:54Z triage M0 skeptic: flat README contract fixes (name offsets, p1c_mark meaning, rank bit 30, id 0, p3_addr_all leak); stats.c note

- 2026-11-16T15:59Z codex-c: partitioned content-hash join shared by plain C/OpenMP; all MinIO oracles and independent all-pairs cases pass; interleaved plain 302.9 -> 57.0 ms, warm par 1/8 threads 36.9/7.2 ms (results/codex-c-p1.md).
