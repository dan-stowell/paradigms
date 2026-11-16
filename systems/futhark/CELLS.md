# Futhark cells (for GRID.md)

MinIO; times ms, min / median; 1 core = `c` backend, 8 = `multicore` (stock spinning runtime), measured
2026-11-16T16:26-16:33Z at load 5-13 on the shared 8 cores (8-thread numbers are upper bounds). "2 MB" =
`GLIBC_TUNABLES=glibc.malloc.hugetlb=2` (no code change, Surprise 1 in NOTES.md). Rerun: `systems/futhark/bench.sh`,
`HUGE=1 systems/futhark/bench.sh`.

**1a join by name** - correct (`build/p1_c.exe $D 1 1 1a` -> P1a OK) - warm 152 / 178 (1), 60 / 84 (8);
2 MB: 79 / 88 (1), **24 / 28 (8)**; e2e 275 (1), 208 (8); 2 MB 68 (8) - 49 lines - easy: winner as
`reduce_by_index max` of `rank<<32|~id`; forced: the hash table (rounds of linear probing claimed by reduce_by_index).

**1b relocation targets** - correct (`... 1b_scan` -> P1b_scan OK) - warm 76 / 103 (1), 59 / 71 (8); 2 MB:
47 / 49 (1), **18 / 24 (8)**; e2e 215 (1), 211 (8); 2 MB 64 (8) - 18 lines - easy: decode is a map; forced: owner
row by scatter + max-scan (binary search per relocation: 459 ms, 1 core).

**1c content-hash join** - correct (`... 1c` -> P1c OK) - warm 202 / 210 (1), 127 / 165 (8); 2 MB: 111 / 115 (1),
**38 / 46 (8)**; e2e 332 (1), 267 (8); 2 MB 83 (8) - 14 lines - easy (1a's table, another packed key).

**2 reach** - correct (`build/p2_c.exe $D 1 1 2` -> P2 OK; `2lazy` too) - flood over p2_edge: warm 140 / 188 (1),
155 / 259 (8); 2 MB 97 / 105 (1), **58 / 102 (8)**; e2e 303 (1), 347 (8). Lazy decode inside the flood (ATTACKS 1):
2 MB 165 / 173 (1), 90 / 137 (8) - 74 lines eager + 63 lazy - easy: rules as scatters of true; forced: level-
synchronous flood with expand per level (111 levels x ~25 passes), skip rule as a segmented scan of state
functions. ATTACKS 1: expressible, but does not beat the eager C pipeline (108.7 / 30.1): falls down on speed
(two attempts: segmented-scan and look-back skip rule).

**3a layout** - correct (`build/p3_c.exe $D 1 1 3a` -> P3a OK; `3a_loop` too) - scan: warm 22 / 25 (1), 10 / 31 (8);
loop: 14 / 21 (1), **7.3-8.5 / 10.5-12 (8)**; e2e 40 (1, loop), 36 (8) - 86 lines + 26 (merge sort) - easy: the walk
as a scan of composable "align then add" maps; forced: a CPU-shaped merge sort for typelinks (the library's is
GPU-shaped, 6x slower).

**3b relocate** - correct (`... 3b` -> P3b OK, 0 errors) - warm 108 / 119 (1), 40 / 48 (8); 2 MB 107 / 109 (1),
**41 / 50 (8)**; e2e 342 (1), 286 (8); 2 MB 114 (8) - 60 lines - forced: the copy as a scatter of (index, byte)
pairs in 64-byte chunks (no bulk copy into a slice); impossible from the language: output page size (glibc tunable
does it: faults 104 K -> 2.5 K per process; first rep 126-137 -> 52-64 ms at 8 threads).
