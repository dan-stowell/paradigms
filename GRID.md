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
