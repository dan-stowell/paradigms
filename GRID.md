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
| 1. resolve | **P1 OK** (1a/1b/1c). 1a 79-115 / 37-57; 1b 39-57 / 42-67; 1c 97-118 / 39-47 (1 / 8 workers, load 8-13). Forced: shuffle join, one table per worker; impossible: a shared table. | **P1 OK** (1a/1b/1c). 2 MB pages: 1a 79 / **24**, 1b 47 / 18, 1c 111 / 38 (1 / 8, load 5-13; 4 KB 1.2-2.5x slower). Easy: winner as reduce_by_index max; forced: the hash table as rounds of probing. |  | **P1 OK** (1a/1b/1c x2). 1a 1928-2034 / 636; 1b 5220-5426 / 1080; 1c 1625-1865 / 394-587 (1 / 8 threads, load 3-6): 30-200x C. Easy: join + max aggregate are the rules; forced: B-tree loading of 5.2 M rows. | **P1 OK** (1a/1b/1c). 8 threads: 1a 103-113, 1b 272-375, 1c 128-190 (10-60x tuned C). Easy: the joins are SQL joins; forced: every lookup by dense id is a hash join (SQL has no a[i]). |
| 2. reach | **P2 OK**. CSR flood 41-49 / 22-32; lazy decode 74-79 / 37-49 (C p2lazy same window 61-71 / 21-31). Easy: decode in the drain (10 lines); forced: owner-partitioned supersteps; 8 workers scale 1.7x. | **P2 OK** (eager + lazy). 2 MB: flood 97 / 58; lazy 165 / 90. Falls down on ATTACKS 1 (two attempts): lazy is slower than its own eager flood; 111 levels x ~25 passes each. |  | **P2 OK** (eager + lazy). 1227-1467 / 431; lazy 960 on 8 (decodes 1.73 M of 9.4 M relocations, 3 rules), 2x its eager, ~30x eager C. Easy: rules are the README; forced: methods as stratified rounds. | **P2 OK** (eager, lazy, recursive). 8 threads: 103 prep + 1965-2354 flood: **falls down** on speed (2 attempts; 45 rounds x ~25 statements at 1-10 ms each). Lazy ties eager. A single recursive query: 425 ms for plain edges. |
| 3. lay out + relocate | **P3 OK + pipeline OK**. 3a 15-26 / 17-31; 3b 87-99 / 33-40 on 4 KB, **41-42 / 17-24 on 2 MB** (C sidecar remaps pages). Falls down: one image array on 8 workers (gather +142-171). | **P3a/P3b OK**. 3a loop 14 / 7.3; scan 22 / 10. 3b 107 / 41 (2 MB via glibc tunable; 4 KB first rep 126-137 on 8). Forced: copy as a scatter of 64-byte chunks; impossible in-language: page size. |  | **P3 OK**. 3a 7222-7589 on 1 thread, 1339 on 8: **falls down** (no sort in Datalog; typelink order by pair counting, 2 attempts). 3b 320 on 8 (patch values by rules; bytes copied by a C writer: Datalog has no bytes). | **P3 OK + pipeline OK** (from its own canon and addresses). 3a 217-263 (list_reduce fold; no running sum for round-up-then-add). 3b 339-368 on 4 KB, 309-332 on 2 MB; the C host does the byte stores (SQL cannot store at an offset). |
| 4. codegen by shape (stretch) | not attempted | not attempted |  | not attempted | not attempted |

DuckDB column merged 17:13Z (lead reran every check in a copy, pipeline included; `systems/duckdb/CELLS.md`).
Soufflé column merged 16:58Z (lead rebuilt all 8 programs from source; every check OK; `systems/souffle/CELLS.md`).
Futhark column merged 16:42Z (lead rebuilt from source; every cell OK; `systems/futhark/CELLS.md`). Surprise: `GLIBC_TUNABLES=glibc.malloc.hugetlb=2`, no code change, makes most Futhark cells 1.2-1.7x faster.
Goose column merged 16:35Z; lead reran every check (P1/P2/P3/pipeline OK, a source rebuild of p2par passes). Long form and lines: `systems/goose/CELLS.md`, `NOTES.md`.
