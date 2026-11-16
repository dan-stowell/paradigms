# Goose cells for GRID.md

MinIO. ms, min-max over 5 interleaved rounds of `systems/goose/bench.sh` at 2026-11-16T16:22-16:25Z, **load 8-13**
(provisional). Warm = min rep of in-process repeats; e2e = one process, one rep. 8 = 8 shared-nothing workers (queues
block on a condition variable; no spin-wait policy exists). Check: `systems/goose/bin/<cell> /home/exedev/paradigms/data/minio [8]`.

**1a join by name** — correct: P1 OK (`bin/p1`, `bin/p1par D 8`; canon + builtin vs oracle) · 1 thread 79-115 warm
(first rep with table first-touch 213) / 8 workers 37-57 · e2e 1a+1b+1c 852-1066 / 518-687 · 195 / 411 lines ·
easy: open addressing over inline structs, winner = max(rank<<32 | ~id) · forced: shuffle join, a queue type per
worker (`Mail<u8[j+1],M>`) · impossible: a shared table.

**1b relocation targets** — correct: P1 OK (reltarget vs oracle) · 39-57 / 42-67 (8 includes sending 9.4 M targets to
main) · lines in p1/p1par · easy: one gather per relocation · forced: canon allgather to every worker, results shipped
to main · impossible: -.

**1c content-hash join** — correct: P1 OK (p1c_canon vs oracle) · 97-118 / 39-47 · lines in p1/p1par · easy: same as 1a ·
forced: same shuffle · impossible: -.

**2 reach (CSR flood)** — correct: P2 OK (`bin/p2`, `bin/p2par D 8`) · 41-49 / 22-32 · e2e 205-269 / 225-316 · 187 /
241+341 lines · easy: `checkfree` drain over index-typed columns validated once by `from_bytes` (zero bounds checks) ·
forced: owner-partitioned BSP, per-owner inbox types, main merges global facts · impossible: a shared visited set;
scales only ~1.7x on 8.

**2' reach incl. decode (lazy, ATTACKS 1)** — correct: P2 OK (`bin/p2lazy`, `bin/p2par D 8 lazy`) · 74-79 / 37-49
(C p2lazy same window: 61-71 / 21-31; eager C pipeline 108.7 / 30.1) · e2e 731-859 / 411-807 · 286 lines + lazy mode
in p2rules · easy: decode inside the drain, ten lines · forced: cutting relocation edges out of the p2 rows at load;
a canon copy per worker · impossible: -. Beats the eager C pipeline on 1 thread, not on 8.

**2 reach, no supersteps (async, ATTACKS 3)** — correct: P2 OK (`bin/p2async D 8`, `bin/p2async D 8 lazy`) · 8 workers
best flood 33.4 / 38.9 / 44.2 vs BSP p2par 48.1 / 52.9 / 62.5 (min/med/max, interleaved, 17:45Z, load 21-23; second
window 28.2 / 37.4 / 46.6 vs 33.8 / 48.0 / 80.0); flush every 4 K ids 30.1 / 33.8 / 35.7; lazy 60.4 / 64.6 / 76.6 vs
66.7 / 95.4 / 103.0; 1 worker = BSP within noise · 233 lines + p2rules · easy: `qpoll` between local fixpoints,
termination by per-pair batch counts reported when idle · forced: facts broadcast to every worker in the data
batches · impossible: a shared visited set. 10 supersteps -> 0; scaling still ~1.3-1.9x (the partition, not the
barriers; results/levelfree.md).

**3a layout** — correct: P3 OK (`bin/p3`, `bin/p3par D 8`; addr vs oracle/p3_addr) · 15-26 / 17-31 · e2e 3a+3b 338-392 /
595-730 · 251 / 386 lines · easy: count, scan, assign; block-comparator sort for typelinks · forced: counts to main and
bases back · impossible: -. `bin/p3 D --pipeline`: 3b from its own 3a addresses -> P3 pipeline OK.

**3b relocate (ATTACKS 2)** — correct: P3 OK (image vs oracle/p3_image.bin per symbol) · 4 KB pages: 87-99 / 33-40;
**2 MB pages (P3_THP=1): 41-42 / 17-24**; minor faults/rep 26,731 -> 258-457 (1 thread) · 8 = image left distributed
by address range; gathered into one array in main +142-171 · easy: `move_from` + match on kind · forced: byte-wise
stores; huge pages only via C remapping the pages under the builder's stack top (runtime maps 4 KB, MADV_NOHUGEPAGE) ·
impossible: one image written in place by 8 workers (falls down as one array; distributed is the cell).
