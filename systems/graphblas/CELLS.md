# SuiteSparse:GraphBLAS cells (for GRID.md)

MinIO. SuiteSparse:GraphBLAS 10.5.2, JIT build. bench.sh 2026-11-16T18:03Z-18:10Z, load 9.9 -> 7.1, five
interleaved rounds; min-median-max ms; tuned C (p2par, p1par) in the same window. Check commands: NOTES.md
"Oracle checks" (`cmp out/*.u32` against `oracle/`, plus the in-process check).

**1a. join by name** — correct (`cmp out/p1_canon.u32`, `out/p1_builtin.u32`) · warm 1 thr 841-918-938 / 8 thr
**268-306-339** (no collision guard: 599-612-699 / 175-206-411); p1par 49-51-54 / 15-17-18 · e2e (all of P1)
2902-3097-3444 / 1055-1535-1925 · 60 lines · easy: the winner rule is a vector build with dup = MAX; "candidates
join only existing groups" is a structural mask; the join back is `w any.first G` (key x slot incidence) · forced:
the 128-bit key hashed into GraphBLAS's 60-bit index space, plus a guard (MIN = MAX of k1 and k2 per group: +40%)
· impossible: nothing; 18x p1par at 8 threads (sorts in build, a hashed product, whole-vector passes).

**1b. relocation targets** — correct (`cmp out/p1_reltarget.u32`) · warm 503-537-697 / **162-194-203**; p1par
31-33-35 / 8-9-12 · 40 lines · easy: gathers are `extract` with a vector index list · forced: conditionals as
clamp-then-mask over 9.4 M entries (~25 whole-vector ops); the loader expands slot_rel to each relocation's slot
(3 lines of C) · impossible: nothing; 21x p1par (no fusion: every op is a pass over 9.4 M).

**1c. content-hash join** — correct (`cmp out/p1c_canon.u32`) · warm 1146-1198-1294 / **279-349-392** (no guard
682-705-820 / 176-204-325); p1par 42-44-51 / 12-15-15 · 50 lines · easy: the winner order packed into a u64
(mark1 << 63 | size << 32 | ~id) and reduced with MAX; dropping unreached winners is one select · forced: the same
60-bit key + guard · impossible: nothing; 24x p1par.

**2. reach** — correct (`cmp out/p2_live.u32`; push, pull and col directions all correct) · warm 191-196-203 /
**179-186-213** (JIT off 202-210-213 / 189-193-229); p2par 30-34-40 / 15-20-26, p2lazy 68-75-80 / 17-25-33 · e2e
857-935-1205 / 563-658-696 (the four matrix builds are ~0.5 s) · 48 lines rules + 20 matrices + 30 loader · easy:
the flood is `fr<!live> = fr any.pair A0`; every rule is one masked product; the signature-method match is a join
written as `(SN MN') .* (ST MT0')` · forced: the iface rule (live AND iface AND gotype) as its own closure loop
per round; per-round `delta` by set difference; CSR rows to tuples per edge kind in C · impossible: parallelism
inside a level (379 of 382 vxm/mxv calls run on 1 thread: frontiers ~7k nodes, below the 64k grain; 8 threads =
1 thread), a level-free flood. 97 BFS + 81 iface levels in 7 rounds, 382 vxm/mxv (1,166 GraphBLAS calls) vs
ATTACKS 3's 111 levels. Pull direction 40x slower (11.5 s). 9x p2par at 8 threads, 6x at 1.

**3a. lay out** — not attempted: no scan primitive; the unit chain (round up, then add) is a non-monoid fold over
1,763 units x 15 buckets (~26k sequential calls).

**3b. relocate** — not attempted: gather + arithmetic + byte scatter is array programming with no semiring in it.
