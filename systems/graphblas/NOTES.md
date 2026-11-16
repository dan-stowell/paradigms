# SuiteSparse:GraphBLAS (sparse linear algebra over semirings)

SuiteSparse:GraphBLAS **10.5.2** (GitHub release tarball, 2026-10-25), built from source with cmake 3.28 (apt) and
gcc 13.3, OpenMP on, two builds:

- `/home/exedev/tools/graphblas` (JIT **off**, `-DGRAPHBLAS_USE_JIT=OFF`): only the precompiled "factory" kernels;
  anything else (e.g. `apply` with a bound bit-shift, most `extract` on uint32) runs a generic, function-pointer kernel.
  Build 11.5 min with `make -j6` at load ~15.
- `/home/exedev/tools/graphblas-jit` (JIT **on**): kernels for the exact types/ops are compiled with `cc` on first use
  and cached in `~/.SuiteSparse/GrB10.5.2` (first run of a cell compiles ~30 kernels, a few seconds; later runs load
  them). This is the tuned build: `p1gb-jit`, `p2gb-jit`.

Sources: `/home/exedev/tools/graphblas-src/GraphBLAS-10.5.2` (`build/`, `build-jit/`). No LAGraph: its BFS is a pure
BFS, and the method rules need the loop below anyway.

Threads: `GB_THREADS=n` (sets `GxB_NTHREADS`); `OMP_WAIT_POLICY=passive` for all timings (GraphBLAS runs on OpenMP,
and the active policy has the same barrier problem as ATTACKS 3). `GB_BURBLE=1` prints SuiteSparse's per-call
diagnostics (method chosen, threads, time): every profile below comes from it.

## Surprises

- **The flood does not parallelise, because GraphBLAS parallelises per call and each call is small.** p2gb
  makes 382 vxm/mxv calls; the average BFS frontier is ~7k nodes (663k live / 97 levels), below SuiteSparse's work
  grain (`GxB_CHUNK`, 64k), so almost every call runs on one thread (burble, 8 threads: 379 of 382 vxm/mxv calls on 1 thread). The
  wall time is flat in the thread count: best of 7 at load ~7 (18:10Z), 1/2/4/8 threads 192 / 188 / 187 / 173 ms, while
  process CPU time grows 192 -> 256 ms (spinning up teams that have nothing to do). bench.sh: 1 thread 192-203 ms,
  8 threads 179-213 ms (ranges overlap). A finer grain (`GB_CHUNK=8192`) doubles CPU time (400 ms) for 161-223 ms
  wall, within noise; at load ~18 the same setting was 2-3x slower (1.1-1.5 s). The C level-synchronous flood
  (p2par) gets 30-40 -> 15-26 ms from the same 8 cores, because a level there is one parallel loop over the frontier,
  not ~6 library calls each deciding its own thread count. One solve is 1,166 GraphBLAS calls (382 of them
  vxm/mxv), ~190 us each on average.
- **Push/pull direction optimisation is a 40x loss here.** Pull (`mxv` over A0', unvisited rows) 11.5 s, push 0.26-0.89 s
  (1 thread, same window); storing A0 by column so SuiteSparse picks itself ("col": dot products) 14.3 s. Only 13% of
  the nodes are ever live, so the unvisited set is never small. SuiteSparse does not switch direction by itself for
  `vxm`; it takes the format you give it.
- **Every cell matched its oracle on the first run** (P2, 1a, builtins, 1b, 1c), and the code reads as the rules.
- **A 128-bit key does not fit a GraphBLAS index (60 bits).** Resolve hashes (k1, k2) to 60 bits and checks every
  group for a single full key (MIN and MAX of k1 and of k2 per group agree): 0 collisions on MinIO, but the guard
  costs 4 extra builds per grouping, ~+40% on 1a and 1c. Without the guard, a collision would be silent.
- **JIT on vs off is worth little** here: P2 flood 203-233 vs 219-261 ms, P1 (no guard) 2.46-3.04 vs 2.94-3.32 s (1
  thread, interleaved, load 13). The cost is in sorts (build), hashed lookups and whole-vector passes, not in the
  generic kernels.

## Layout

| file | what |
|---|---|
| `gb.h` | the flat-file reader (`ref/flat/flat.h`), an error check, iso-bool matrix/vector builders |
| `p2gb.c` | P2 reach: loader (CSR rows -> tuples per edge kind), the rule matrices, the round loop |
| `p1gb.c` | P1 resolve: 1a, builtins, 1b, 1c, as whole-vector operations; flat files loaded zero-copy (`GxB_Vector_load`, read-only) |
| `build.sh` | builds `p1gb p2gb` (JIT off) and `p1gb-jit p2gb-jit` (JIT on) |
| `bench.sh` | every timing, interleaved with the tuned C references `p2par`, `p2lazy`, `p1par` |
| `IDEAS.md` | what the measurements suggest |

Knobs: `GB_THREADS`, `GB_DIR=push|pull|col` (P2), `GB_CHUNK` (P2), `GB_JOIN=mxv|extract` (P1's join back),
`GB_NOGUARD=1` (P1 without the key-collision guard), `GB_BURBLE=1`.

## Oracle checks

    cd systems/graphblas && ./build.sh && D=/home/exedev/paradigms/data/minio
    GB_THREADS=8 ./p2gb-jit $D 1 && cmp out/p2_live.u32 $D/oracle/p2_live.u32 && echo P2 ok
    GB_THREADS=8 ./p1gb-jit $D 1 && for f in p1_canon p1_builtin p1_reltarget p1c_canon; do cmp out/$f.u32 $D/oracle/$f.u32 && echo ok $f; done

Each program also compares in-process (`check32`) and prints `P2 OK` / `P1 OK` (P1 also requires 0 key collisions).
All pass (2026-11-16T17:30Z first, every later run since): P2 with push, pull and col; P1 with both joins, with and
without the guard, JIT on and off, 1 and 8 threads.

## P2: reach

**Shape.** State is boolean vectors over the 5.2M ids: `live`, `iface` (bitmaps), the frontier `fr`, and small ones
(`sig` over 3,940 signatures, `named` over 18,938 strings, `cand` over 106,079 methods). Every rule is a matrix,
built once from the flat files: A0 plain (N x N), A1 useiface, A2 ifacemethod (N x nsig), A3 namedmethod (N x
nstr), M type -> its methods, Mtyp method -> mtyp, Mout method -> {mtyp, ifn, tfn}, MN method -> name, and S
(nsig x nmeth) "this signature names this method", which is itself a join written as two products:
`S = (SN any.pair MN') .* (ST any.pair MT0')` (same name and same type), computed inside the timing (~2 ms).

One round:

1. **BFS** over plain edges to a fixpoint: `fr<!live> = fr any.pair A0; live<fr> = true`.
2. **delta** = what the round made live (`live` minus the round's start). From it: `t1<!iface> = delta any.pair A1`
   (newly iface), `sig |= delta A2`, `named |= delta A3`, `reflect |= any(delta .* reflectflag)`.
3. **iface closure**: `src` = (delta .* iface  +  t1 .* live), masked to defined gotypes. Until empty:
   `cand |= src M`, `t1<!iface> = (src A0) .* gotypedef  +  (src M) Mtyp`, `iface |= t1`, `src = t1 .* live .* gotypedef`.
4. **methods**: `call = cand .* (sig S + MN named + reflect ? exported)`, `cand -= call`, `fr<!live> = call Mout`.
   Stop when `fr` is empty.

```c
while (nv(fr)) {                                                  // 1. the flood: a masked BFS
    OK(GrB_vxm(fr, live, NULL, sr, fr, A0, GrB_DESC_RSC));        //    fr<!live> = fr any.pair A0
    OK(GrB_Vector_assign_BOOL(live, fr, NULL, 1, GrB_ALL, N, GrB_DESC_S));
}
...
while (nv(src)) {                                                 // 3. the iface closure
    OK(GrB_vxm(cm, NULL, NULL, sr, src, M, NULL));                //    its methods are candidates
    OK(GrB_Vector_assign_BOOL(cand, cm, NULL, 1, GrB_ALL, nmeth, GrB_DESC_S));
    OK(GrB_vxm(t1, iface, NULL, sr, src, A0, GrB_DESC_RSC));      //    iface along plain edges ...
    OK(GrB_eWiseMult(t1, NULL, NULL, GrB_FIRST_BOOL, t1, gtdef, NULL)); // ... to defined gotypes
    OK(GrB_vxm(t2, iface, NULL, sr, cm, Mtyp, GrB_DESC_RSC));     //    iface(m.mtyp)
    OK(GrB_eWiseAdd(t1, NULL, NULL, GrB_LOR, t1, t2, NULL));
    OK(GrB_Vector_assign_BOOL(iface, t1, NULL, 1, GrB_ALL, N, GrB_DESC_S));
    OK(GrB_eWiseMult(src, gtdef, NULL, GrB_LAND, t1, live, GrB_DESC_RS));
}
OK(GrB_vxm(csig, NULL, NULL, sr, sig, S, NULL));                  // 4. methods some live code can call
OK(GrB_mxv(cnam, NULL, NULL, sr, MN, named, NULL));
OK(GrB_eWiseAdd(csig, NULL, NULL, GrB_LOR, csig, cnam, NULL));
if (reflect) OK(GrB_eWiseAdd(csig, NULL, NULL, GrB_LOR, csig, E, NULL));
OK(GrB_eWiseMult(call, NULL, NULL, GrB_LAND, cand, csig, NULL));
OK(GrB_vxm(fr, live, NULL, sr, call, Mout, GrB_DESC_RSC));        //    their entries become live
```

**Counts (MinIO).** 97 BFS levels + 81 iface-closure levels in 7 rounds, **382 vxm/mxv calls**, 3.34 M
multiply-adds (edges and method links visited, summed from the burble's `axbwork`). ATTACKS 3's level-synchronous C
flood has 111 levels in 7 rounds: the round count is the same (the method rule is a round in both), but GraphBLAS
pays 178 synchronous levels instead of 111, because the iface rule cannot be tested inline while a node is visited:
it is a conjunction (live AND iface AND gotype) of two sets that grow separately, so it becomes its own closure loop
after the BFS. 3.34 M edge visits in ~210 ms is ~63 ns per visit on 1 core; the plain C flood does all of P2 in
55 ms.

**Easy:** the BFS is two lines; every rule is one product with a mask; S (signature matches method) is a join
written as two products; the masks (`<!live>`, `<!iface>`) are exactly "only what is new". **Forced:** the iface
rule's conjunction as a separate closure loop per round; `delta` (what a round made live) by a set difference, since
the rules on A1/A2/A3 are applied once per round, not per level; the loader turning CSR rows into tuples per edge
kind (30 lines of C: GraphBLAS can pack a CSR, but not one whose rows hold four kinds of edges with different
column spaces). **Impossible:** parallelism within a level at this frontier size; a level-free (asynchronous)
flood: every operation is bulk-synchronous. **Lines:** 48 (the round loop) + 20 (rule matrices) + 30 (loader).

## P1: resolve

**Shape.** Every per-slot column is a GraphBLAS vector over 5.2 M slots, loaded zero-copy from the mmap. The key is
hashed to a 60-bit index `h` (four elementwise ops: times, xor, times, shift).

- **1a.** The winner rule is a vector **build**: `w = build(h45, rank<<32 | ~id, dup = MAX)` over kinds 4, 5 (and
  the wanted names with value 0): GraphBLAS sorts the tuples and reduces each key with the MAX monoid, giving a
  hypersparse vector over the 2^60 key space. Candidates join only existing groups: `w<struct(w)> max= build(hc, ...)`.
  The join back to members is `c = w any.first G`, with G the key x slot incidence matrix (`G(h(i), i) = true`):
  a semiring product that reads "each slot takes the value of its key". (`GB_JOIN=extract`: the gather `w(h)` and
  a scatter; within noise.) Builtins: `w(hb)` (an extract), kept where rank bit 29 (value bit 61) is set.
- **1b.** A chain of gathers (`extract` with a GrB_Vector index list) and elementwise ops over the 9.4 M
  relocations: object, its package-table base and length, the special package indices, `obj_pkg[o][t]`, the builtin
  table, then `canon(x)`. Indices are clamped so every gather is in range and invalid ones masked to 0 afterwards.
- **1c.** The same build-with-MAX per kind over the marked copies, value `mark1<<63 | size<<32 | ~id` (kind 2) or
  `mark1<<63 | ~id` (kind 3); groups whose winner has mark 2 are dropped (`select >= 2^63`); a mark-2 kind-2 member
  larger than its winner keeps itself (a compare of the sizes in the two values).

```c
GrB_Vector w = groupMax(h45, x, k1, k2, guard);                   // build(h, val, MAX): the winner per name
OK(GrB_eWiseAdd(w, NULL, NULL, GrB_MAX_UINT64, w, ww, NULL));      // + the linker's wanted names
GrB_Vector wc = groupMax(hc, x, k1, k2, guard);                   // kind-1 candidates ...
OK(GrB_eWiseAdd(w, w, NULL, GrB_MAX_UINT64, w, wc, GrB_DESC_S));   // ... join only groups already there
GrB_Vector cw = joinBack(w, hall, N);                              // c = w any.first G(key, slot)
lowId(cid, NULL, cw, NULL);                                        // ~(u32)value
OK(GrB_eWiseAdd(canon, NULL, NULL, GrB_SECOND_UINT32, canon, cid, NULL));
```

**Where the time goes** (1 thread, burble, JIT): the builds (sorts of 0.8-2.6 M tuples, ~50-120 ms each), the
join-back products (190 and 180 ms: SuiteSparse builds a hash of G's hypersparse rows first), and whole-vector
passes (at least 44 of the 141 calls of a P1 solve read a full 5.2 M- or 9.4 M-entry vector: apply, select,
eWiseMult, extract, 15-30 ms each). Holding the per-key vectors sparse
(`GxB_SPARSE`) instead of letting selects stay bitmaps changed nothing measurable. The C reference does one hash
table and three passes. **Easy:** the winner rule (dup = MAX), "candidates join only existing groups" (a structural
mask), the join back as a product, 1c's rules as compares on packed values. **Forced:** the 128-bit key into 60 bits
plus a guard; values packed into a u64 so that MAX is the winner order (as in the C); 1b's conditionals as
clamp-then-mask; the loader expanding `slot_rel` into each relocation's slot (3 lines of C). **Impossible:** nothing
in the logic; the speed is 5-15x the plain C (below). **Lines:** p1gb.c 300 (1a 60, 1b 40, 1c 50, helpers 60).

## P3: lay out and relocate (not attempted)

- **3a:** GraphBLAS has no scan. The in-class offsets could be a log-step (Hillis-Steele) scan, 19 shifted eWiseAdds
  over 360k symbols, but the unit chain (round up to the unit's alignment, then add) is a fold over 1,763 units x 15
  buckets that no monoid expresses: it would be ~26k sequential scalar calls, a toy.
- **3b:** relocation is gather + arithmetic + a byte scatter into the image; expressible as extract/assign over byte
  vectors, but that is array programming with no semiring in it (DuckDB and Futhark already cover the shape), so not
  a test of this column's axis.

## Timings

`bench.sh` 2026-11-16T18:03:19Z-18:09:54Z, load 9.9 -> 7.1 (other builders), five interleaved rounds of 1 and 8
threads, each warm number the best of 10 (P2) or 5 (P1) in-process solves, then min / median / max over the five
processes; e2e = one whole process (start, mmap, building the GraphBLAS objects, one solve, writing out/, the
oracle check). JIT build. Raw output: `results-bench-20261116T1809Z.txt`. C references in the same window: `p2par`,
`p2lazy` and `p1par` (tuned C, OpenMP, passive wait) at the same thread counts. ms.

| cell | GraphBLAS 1 thr warm | GraphBLAS 8 thr warm | tuned C 1 thr | tuned C 8 thr | GB / C (8 thr, medians) |
|---|---|---|---|---|---|
| P2 flood (+ S join) | 191.5 / 195.5 / 202.8 (193.5 / 197.4 / 204.7) | 178.6 / 186.3 / 212.6 (180.8 / 188.2 / 214.7) | p2par 29.6 / 33.8 / 39.5; p2lazy 67.9 / 74.9 / 80.1 | p2par 14.5 / 20.2 / 26.4; p2lazy 17.2 / 25.1 / 33.0 | 9.2x p2par |
| P2 flood, JIT off | 201.5 / 209.5 / 212.7 | 189.1 / 192.7 / 228.8 | | | |
| 1a (guard) | 841 / 918 / 938 | 268 / 306 / 339 | p1par 48.9 / 50.6 / 53.9 | 14.6 / 16.9 / 18.3 | 18x |
| 1a (no guard) | 599 / 612 / 699 | 175 / 206 / 411 | | | 12x |
| 1b | 503 / 537 / 697 | 162 / 194 / 203 | p1par 31.3 / 32.5 / 35.3 | 8.2 / 9.4 / 11.7 | 21x |
| 1c (guard) | 1146 / 1198 / 1294 | 279 / 349 / 392 | p1par 41.9 / 44.0 / 50.9 | 12.4 / 14.7 / 15.4 | 24x |
| 1c (no guard) | 682 / 705 / 820 | 176 / 204 / 325 | | | 14x |
| P1 all (guard) | 2535 / 2686 / 2979 | 723 / 848 / 962 | | | |
| P1 all (no guard, JIT off) | 2162 / 2198 / 3011 | 619 / 832 / 1002 | | | |

| e2e | 1 thread | 8 threads |
|---|---|---|
| P2 (GraphBLAS) | 857 / 935 / 1205 | 563 / 658 / 696 |
| P2 (C p2par, 1 rep) | 88 / 113 / 134 | 85 / 107 / 143 |
| P1 all (GraphBLAS) | 2902 / 3097 / 3444 | 1055 / 1535 / 1925 |

P2's e2e is mostly the matrix builds (GraphBLAS sorts 8.3 M edge tuples into four matrices: ~0.5 s); P1 loads the
flat files zero-copy, so its e2e is the solve plus the 1-rep noise. Plain C references from the brief (1 core):
P1a 195, 1b 44, P2 55 ms; on those, GraphBLAS 1 thread is 4.7x (1a), 12x (1b), 3.5x (P2).
