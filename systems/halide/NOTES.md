# Halide: the grid cells (problems 1, 2, 3, 4)

Halide 21.0.0, C++ AOT generators (install notes at the end). Branch `worktree-agent-a45ed3bb4a5a5b2a6`.
Code: `systems/halide/cells/`: `gen_p1.cpp gen_p2.cpp gen_p3.cpp` (the Halide algorithms and schedules),
`join.h` (a hash join in Halide, used by 1a, 1c and P2), `p1.cpp p2.cpp p3.cpp` (drivers: mmap the flat files,
wrap them zero-copy as `Halide::Runtime::Buffer`, call the AOT pipelines, check against the oracles), `bench.h`
(timing), `build.sh` (one `.a` per schedule variant). Timings: `systems/halide/bench.sh` (one line per cell,
threads, warm/e2e).

Build and check (each driver checks every variant it runs against the oracles and ends with `P<n> OK`):

    systems/halide/cells/build.sh                      # ~1 min: generators, every variant, drivers
    B=systems/halide/cells/build; D=/home/exedev/paradigms/data/minio
    HL_NUM_THREADS=8 $B/p1 $D e2e 1                    # P1 OK: canon, builtin, reltarget, canon after 1c
    HL_NUM_THREADS=8 $B/p2 $D e2e 1                    # P2 OK: live set (lazy decode, parallel)
    HL_NUM_THREADS=8 $B/p3 $D e2e 0 2                  # P3 OK: 3a addresses, 3b image
    systems/halide/bench.sh                            # all timings

Threads are Halide's own pool (`HL_NUM_THREADS`). Its idle workers spin for a short while, then sleep on a
condition variable; there is no wait-policy knob, so the 8-thread numbers are with Halide's default (the RESUME.md
load warning: no 100x collapse was seen, see the ranges).


## Surprises

- **P4: both sides of every select are evaluated, and that was the largest single cost of the emit.** Halide's
  select is not lazy, so a glue item chased node -> template -> segment from its glue id and an immediate read
  `at[its value]`: random misses per item that C's `if` never takes. Pointing the untaken side at entry 0 took the
  1-thread emit from 200-213 to 157-172 ms and the layout from 26-32 to 18-19 (P4 section).
- **P4: the gather form (each output byte fetches its own: a pure Func, no races) is correct first time and 2x
  slower than the scatter copy**; the scatter copy itself vectorizes only as a read-modify-write in parity passes
  (26 s when vectorized the obvious way, a split RVar's `where` is no loop bound).

- **1a (join by name) in Halide beats the plain C reference 2.5-3x on one core** (81-93 vs 231-249 ms, same window,
  17:00-17:10Z, load 2-5), with no hash table: the join is rounds of scatter-min + gather (`join.h`). The plain C
  probes a 4-entry-per-slot table (125 MB) for 5.2 M slots; the Halide version first compacts the 789 K keyed slots
  (a scan) and hashes only those, into tables of 2^20 down to 2^13 buckets. It does not beat the tuned C (p1par
  1 thread 74-77 for 1a+1b; Halide 1a+1b 124-139), so the win is the shape (compact, then small tables), not
  Halide.
- **A lazy flood is slower than an eager one in Halide** (1.3-1.5x on 1 thread, 417-465 vs 312-332 ms), the
  opposite of C (p2lazy 52.6 vs p2pipe 108.7). Halide's cost is per op (every op is a full iteration of a select
  chain with three phases), and the lazy rows walk every relocation of a live symbol, followed or not, while the
  eager CSR holds only the followed ones; the decode itself (six dependent loads) is the smaller part.
- **atomic() on a scatter that reads other sites is a mutex per element**: Halide allocates a mutex array the size
  of the Func on every call (`halide_mutex_array_create`), which made the parallel flood 3.4-3.6 s (vs 0.3 s
  serial), and the first byte layout was still WRONG in parallel (2-2,106 live symbols lost, from reading uii
  twice in one iteration). Both are fixed by a layout where every write is a constant store (below).

## Results (MinIO; ms; per-iteration min / median / max over 5 runs of >= 1 s, in-process, inputs mmapped and
warm; bench.sh 2026-11-16T17:00-17:06Z, load 1.8-7.3; 3b at 8 threads re-measured 17:09Z after the chunked
schedule; C references interleaved 17:10Z, load 4.1-4.9, OMP_WAIT_POLICY=passive)

| cell | Halide 1 thread | Halide 8 threads | plain C 1 core (same window) | tuned C 8 threads |
|---|---|---|---|---|
| counts (sizes for 1a/1c) | 9.9-10.4 | 1.6-2.6 | | |
| 1a join by name | 81-93 | **19.8-27.2** | 231-249 | 12.1-20.9 (p1par join) |
| 1b relocation targets (9.4 M) | 42.7-46.3 | **9.9-10.9** | 47-59 | 7.5-8.9 |
| 1c content-hash join | 122-133 | **33.2-36.5** | 63-78 | 11.7-14.8 |
| 2 reach, lazy decode in the flood | 417-465 | **79-90** | p2lazy 1 thr 59-77 | p2lazy 25.7-29.1 |
| 2 reach, eager (p2_edge as given) | 312-332 | **70-127** (med 90) | p2 39-58 | p2par 12.7-14.6 |
| 3a lay out | 53-58 (c3_w0) | **20.0-23.0** (c3_w1) | 8.5-9.0 | (none) |
| 3b relocate, warm output | 78-82 (f0_pf_v0) | **24.9-27.0** (f0_pt_v32) | | |
| 3b, fresh output, 4 KB pages | 113-119; 19,109 faults | 42.7-45.4; 19,187 faults | 55-61 (calloc) | 23.4-25.7 (p3par, calloc) |
| 3b, fresh output, 2 MB pages | 84.6-88.1; 39 faults | **27.0-30.8**; 170 faults | ATTACKS 2: 23.9-30 | ATTACKS 2: 5.4-12 |

End to end (whole process, `/usr/bin/time`, 5 runs, s): P1 (counts+1a+1b+1c) 0.35-0.40 (1 thr) / 0.13-0.17 (8);
P2 lazy 0.55-0.59 / 0.24-0.25 and eager 0.47-0.51 / 0.22-0.26 (both include ~115 ms of untimed C setup that
splits the non-relocation edges off p2_edge, as p2lazy's setup does); P3 (3a+3b, calloc'd output) 0.21-0.25 /
0.10-0.12 (in-process: load 6-20, 3a 22-31, 3b 36-47 ms at 8 threads).

Thread pool: Halide's (`HL_NUM_THREADS`); no wait-policy knob. No 8-thread time came out above its 1-thread time
in these runs, so the spinning-barrier collapse in RESUME.md's load warning was not seen; the 8-thread eager P2
range (70-127) is the widest and is probably load.

## ATTACKS 1 (reach): can Halide decode a reference when the flood reaches it?

Yes, it can express it, and it costs more than it saves. The flood level (`Flood`, `lazy=true`) walks a live
symbol's own `rel_*` range and maps (package, index) -> slot -> canon inside the op loop, exactly p2lazy's shape;
nothing is materialized. Counting its own decode it does **not** beat the eager C pipeline: 79-90 ms on 8 threads
vs p2pipe's 30.1 (and p2lazy's 13.6-29), 417-465 on 1 vs 108.7. What Halide forces instead of the graph:
- the level loop is C++ (a pipeline is a DAG; it cannot run to a fixpoint): 93-111 calls, one per level;
- a worklist is impossible: the frontier is implicit (a byte per fact; "needs expanding" = live and not expanded,
  or an interface type not expanded as one), visited through a list of 16-id blocks that each level writes;
- an update writes one site per iteration, so each op is three iterations (the target's block, its IFACE, its
  LIVE), and the pieces a symbol's ops share must be a Func computed per symbol (`info`), indexed by the loop's
  own variables (indexed by the id, bounds inference sizes it for every block the id could be in);
- a `where` becomes a loop bound only on the innermost RVar that has one; anywhere else it is an `if` re-tested
  every iteration and never hoisted when it reads the state: the first version iterated every symbol's whole row
  every level (2.7 s).
The eager variant over `p2_edge` (no decode) is faster in Halide (312-332 / 70-127) because the lazy rows are
longer (every relocation, followed or not). Halide's eager P2 counting its own 1b decode would add 10 ms (1b) and
a CSR build Halide would do as a histogram + scan + scatter; not built, since the eager flood alone is already
above the eager C pipeline's 30.1.

## ATTACKS 3 (reach): does batching levels help? No (results/halide-levelfree.md)

Measured 2026-11-16T19:15-19:24Z, load 6-10, every run P2 OK. Rerun: `systems/halide/levelfree.sh counts`, then
`systems/halide/levelfree.sh 5`.
- **The calls cost nothing.** `p2 DATA count V`: one flood call on a one-block state is 0.02 ms, so 98 calls are
  about 2 ms. An empty level on the real state costs 0.63 ms on 1 thread and 0.23 ms on 8. That is the clear, the
  scan and the scatter over the 325,340-entry block summary that build the next list: 62 / 23 ms of a 304 / 75 ms
  flood. The rest is the expansion, plus `init` (40 ms on 1 thread, 9 on 8) and prep and methods (15 / 4 ms).
- **k levels per call** (`levels=K`, variants `flood_L_P_eK` for K = 1, 2, 4, 8, and `flood_f_f_dK`). Forced twice:
  - A Func cannot call a Func that calls it, so level i's list is built from `stIn`, an input the caller binds to
    the same buffer as `st`. The list is computed at an outermost extent-1 RVar of level i's own update.
  - An RDom's bounds may not call a Func, and an input buffer counts as one. A `where w < count` stays in the
    innermost loop. So the later levels loop to nblk (attempt 1, `_dK`: 1.4-2.1 s) or to a param `cap` (attempt
    2, `_eK`: unvisited blocks keep their summary byte and come back two levels later, and `nleft` tells the
    driver). Attempt 2 also drops the clear pass: each block clears its own summary byte as a 17th "symbol".
- **Result**, interleaved, 5 rounds, ms min / median / max: 1 thread original 294 / 298 / 313, e2 323 / 343 / 405,
  hybrid e4 (batch only lists < 256 blocks) 299 / 310 / 402, e8 344 / 349 / 351. 8 threads original
  66 / 83 / 144, e2 83 / 108 / 140, hybrid e4 73 / 95 / 125, e8 75 / 97 / 149. p2par 26.7 / 28.4 / 34.2 and
  10.3 / 11.6 / 16.3 in the same window. Calls drop from 98 to 17-58; the time does not.
- Why: the list passes are per level, not per call, and a batched level adds cap x 17 iterations. The same result as
  Futhark (results/levelfree.md): passes per level, not the number of levels, are the cost.
- `prlimit --as=12G` (or even 40G) makes the p2 driver segfault at start (probably the address-space reservations
  of Halide's runtime or the thread pool). Use `systemd-run --user --scope -p MemoryMax=12G`, as bench.sh does.

## ATTACKS 2 (relocate): huge pages

Yes: a Halide output buffer is whatever memory the caller wraps, so `p3.cpp` maps the 3b image MAP_HUGETLB (2 MB)
and hands it to the pipeline. Fresh output per run, faults inside the timed region (`p3 DATA 3b`, getrusage):
- 1 thread: 4 KB pages 113-119 ms with **19,109 minor faults/run**; 2 MB pages 84.6-88.1 ms with **39**.
- 8 threads: 4 KB 42.7-45.4 ms, 19,187 faults; 2 MB **27.0-30.8 ms, 170 faults**; warm (reused, prefaulted) 24.9-27.0.
So huge pages remove the 15-30 ms of faulting, as in C; what remains is Halide's copy + patch (25 ms at 8 threads
vs C's 5.4-6.8 on 2 MB pages): the byte-at-a-time stores (below) are now the cost, 4-5x C.

## Per cell

### 1a join by name (`Resolve` in gen_p1.cpp, `join.h`; 59 + 30 (compaction) + 60 (join) + 23 (counts) lines)
- Easy: the winner per group is an associative update (`win(rep) = max(win(rep), rank << 32 | ~id)`); `atomic()`
  parallelizes it, `rfactor` the counts. Canon and builtins are pure gathers.
- Forced: no hash table (a probe is a data-dependent loop, an update writes one site): the join is rounds of
  scatter-min (claim a bucket) + gather (compare keys), 8 rounds unrolled in the generator, with an output that
  must be 0 (entries that found no group). A scan (compaction of the keyed slots) has no parallel form in Halide:
  the parallel schedule is a different, hand-blocked algorithm (block scans, serial scan of block totals,
  scatter). Sizes computed by a pipeline cannot size a later stage of the same pipeline: a separate `Counts`
  pipeline runs first. Every RVar split for parallelism runs a tail past the end that must be clamped by hand.
- Impossible: nothing.

### 1b relocation targets (`RelTarget`, 33 lines)
- Easy: a ragged RDom (relocation in slot, slot), its `where` is the innermost loop bound; parallel over chunks of
  slots with allow_race_conditions (distinct relocations).
- Forced: `unsafe_promise_clamped` on every data-dependent index (else "accessed in an unbounded way").
- Impossible: nothing.

### 1c content-hash join (`ContentJoin`, 56 lines + join.h)
- Easy: the same join; the winner is one u64 max of (mark 1) << 63 | size << 32 | ~id.
- Forced: the kind folded into the key (kind 2 gets k2 = 2^40); a check output for sizes >= 2^31.
- Slower than C 2x (1 thr) / 2.5x (8): 1.29 M entries through 8 rounds, each round a full pass.

### 2 reach (`Init`, `Flood`, `Methods`, `Prep` in gen_p2.cpp, 241 lines; p2.cpp's level loop)
- Easy: each rule is one select of which byte to set; the sig lookup is join.h again (`Prep`).
- Forced: everything in the ATTACKS 1 list. Attempt 1 (packed bits per symbol, compare-and-swap via `atomic()`):
  correct serially (306 ms lazy / 193 eager after tuning) but Halide refuses atomic() when the written site depends
  on the Func's values and, once the sites came from the inputs only, implemented it with a mutex array per call
  (3.4-3.6 s at 1 or 8 threads) and lost live symbols in parallel. Attempt 2 (this one): one byte per fact,
  constant stores only, `allow_race_conditions`: 30/30 parallel runs correct (17:00Z), 79-90 ms at 8 threads.
- Impossible: a worklist, a loop to a fixpoint, a test-and-set; the frontier must be implicit.

### 3a lay out (`Layout` in gen_p3.cpp, 154 lines; from the earlier session)
- Easy: occupied sizes, per-unit sums, the block alignment walk as RDom updates.
- Forced: "read the cursor, store the address, bump the cursor" is two sites, so a phase RVar over one Func
  whose sites are [cursors, addresses]; the typelink sort is a 16-stage merge sort with merge-path binary search
  (Halide has no sort), with an extern C comparator for ties beyond 24 bytes of type string.
- 2.5x the plain C on 8 threads (20-23 vs 8.5-9 on one C core); 6x on one thread.

### 3b relocate (`Relocate`, 106 lines)
- Easy: the copy is a ragged RDom (byte, symbol); the patch value is one select chain over the 8 kinds.
- Forced: a value is written byte by byte (8 iterations, unrolled) because the image is u8 and an update writes
  one site; bytes a relocation does not write go to 8 scratch bytes past the image, because a `where` on the byte
  loop makes the relocation loop's `where` an `if` inside a loop to 2^24 (the first version ran 2^24 iterations
  per symbol). Parallel over chunks of 512 symbols (`parallel(rc.y)` alone is one task per symbol).
- A second form (relocations in equal chunks, each finding its symbol by a 20-step binary search) was dropped:
  slower than the first even when it ran.

## Code excerpts

The hash join (join.h), one round:

    Func claim(...);                       // bucket -> the smallest entry index that hashes there
    RDom rj(0, M);
    rj.where(creates(rj) != 0 && prev(rj) < 0);   // entries still looking for their group
    claim(h) = M;
    claim(bucket(rj)) = min(claim(bucket(rj)), rj);
    Expr own = claim(bucket(j));
    Expr hit = own < M && k1(own) == k1(j) && k2(own) == k2(j);
    rep(j) = select(prev(j) >= 0, prev(j), hit, own, -1);
    // schedule: claim.update().atomic(), split + parallel over chunks of rj

The flood's rule for a plain edge (gen_p2.cpp), as which bytes to set:

    Expr setIface = uii && bit(flt, fGoType) && bit(flt, fDef) && !ifaceT;
    Expr setLive  = plainOp && !liveT;
    Expr push     = setLive || setIface;                  // its block goes to the next level's list
    Expr site0 = select(push, SUMN + ti / BLK, ..., scr);   // phase 0
    Expr site1 = select(setIface, L.IFACE + ti, scr);        // phase 1
    Expr site2 = select(setLive || setIface, L.LIVE + ti, scr); // phase 2
    st(select(ph == 0, site0, ph == 1, site1, site2)) = u8(1);

The patch (gen_p3.cpp): 

    Expr v = select(kind == 1 || kind == 2, cast<int64_t>(t2) + add,
                    kind == 4 && ta == 0, Expr((int64_t)0xFFFFFFFFll),
                    kind == 3 || kind == 4, cast<int64_t>(t2 - sb) + add,
                    kind >= 5 && kind <= 7, select(tg == 0, add - pc, cast<int64_t>(t2) + add - pc),
                    add);
    image(select(write && rr.x < nbytes, P - imageBase + rr.x, NI + rr.x)) = cast<uint8_t>(v >> (8 * rr.x));

## Lines of code (non-blank, non-comment)
join.h 60, gen_p1.cpp 219, p1.cpp 80, gen_p2.cpp 241, p2.cpp 125, gen_p3.cpp 274, p3.cpp 141, bench.h 22,
build.sh 60. Drivers are loading, wrapping, the level loop (P2), the untimed edge split (P2) and checking.


# Problem 4: code generation by shape (2026-11-16, branch `worktree-agent-a3315ac9f2aa9aba3`)

Code: `cells/gen_p4.cpp` (three generators: `Templates`, `Layout`, `Emit`), `cells/p4.cpp` (driver: mmap, wrap,
run, check; the top-K rewrite), built by `cells/build.sh p4` (one `.a` per schedule variant: 4 layouts, 38 emits + 4 diagnostics).
Data contract: `ref/p4/README.md`. Check (each run checks itself; ends with `P4 OK`):

    systems/halide/cells/build.sh p4
    B=systems/halide/cells/build; D4=/home/exedev/paradigms/data/minio-p4
    HL_NUM_THREADS=8 $B/p4 $D4 e2e lay_t_lf emit_t_i2_v32_p1     # code == oracle/p4_code.bin (136,634 functions,
                                                                  # offsets == p4_fn_off.u64), 1,780,281 relocations
                                                                  # sorted == oracle/p4_rel_*: 0 differ
    HL_NUM_THREADS=1 $B/p4 $D4 all                                # every variant, warm, each checked
    P4_TOPK=4096 HL_NUM_THREADS=8 $B/p4 $D4 e2e lay_t_lf emit_t_i2_v32_p1   # the top-K variant, same oracle
    P4=only systems/halide/bench.sh                               # the timings below

Every variant built is correct (all emit variants except the `diag*` diagnostics, and all 4 layouts, checked
against the oracle at 1 and 8 threads; top-K for K = 0, 256, 1024, 4096, 16384, all).

## The shape in Halide

- **Templates** (template-only: a code generator builds it once with its library; 49 lines): per template
  segment its start `lo`, its bytes in the source buffer, its first hole (a lower_bound over the template's holes,
  16 halving steps as an RDom update) and its first relocation hole; the relocation holes as a list (a scan of
  `enc == 3`, then a scatter to the rank). The segment's template is a ragged scatter (the CSR inverted).
- **Layout** (43 lines): `len(i)` = (bytes, relocations) of item i, then an exclusive scan of both as one Tuple
  update, straight into the output: `at` (where the item goes) and `rat` (where its relocations go). The parallel
  form is the hand-blocked scan of P1/P3 (block scans in parallel, a serial scan of block totals, a sum).
- **Emit** (203 lines with all its variants): three per-item loops, each an RDom (inner, item in chunk, chunk):
  1. copy: `code(at[i] + b) = src(src_i + b)` for `b < len_i` (a ragged copy; glue.bin and tmpl.bin are mapped back
     to back so one source buffer serves both kinds; loading only, nothing copied);
  2. patch: per hole of the item's segment, its value (`v`, or for a jump `at[v] + a - (P + 4)`: the rel32 is a
     gather of `at`) written as bytes;
  3. relocations: `rel(rat[i] + k) = (P, type, sym, add, w)`: markers, glue relocations and relocation holes.
- **The relocation list needs no C host.** The variable-length output became a fixed one: `rat` (from the layout
  scan) gives every item its slots, so each relocation is a scatter to `rat[i] + k`, in parallel, deterministic.

## Results (MinIO P4; ms; min / median / max; 1 thread by process CPU time (`BENCH_CPU=1`), 8 threads wall;
bench.sh 2026-11-16T18:31:45Z, load 4.1-4.9; C references 18:32:54Z, load 3.7-4.3, 5 runs each)

| step | Halide 1 thread | Halide 8 threads | p4.c 1 core | p4par 1 thread | p4par 8 threads |
|---|---|---|---|---|---|
| templates (template-only index) | 9.4 / 9.5 / 9.6 | 6.8 / 7.3 / 7.7 | (in emit) | (in emit) | (in emit) |
| layout (2.38 M items, scan) | 17.9 / 18.4 / 18.7 (`lay_f_lf`) | 4.9 / 5.4 / 5.6 (`lay_t_lf`) | 6.5 / 6.9 / 7.0 | 7.3 / 7.4 / 7.4 | 1.3 / 1.7 / 2.0 |
| emit, best (scatter copy, `i2_v32_p1`) | **157 / 160 / 162** | **23.0 / 23.4 / 25.1** | 103 / 107 / 112 | 97 / 98 / 99 | 16.4 / 18.5 / 20.1 |
| emit, gather form (`g_p0`) | 237 / 243 / 251 | 55.6 / 60.3 / 72.4 | | | |
| e2e process wall, s (5 runs) | 0.21 / 0.23 / 0.24 | 0.07 / 0.08 / 0.10 | | | |

(p4.c's emit includes the page faults of its fresh 60 MB malloc; p4par's and Halide's warm runs reuse the output,
p4par on 2 MB pages. Ratios: emit 1.5x p4.c / 1.6x p4par on 1 thread, 1.3x p4par on 8; layout + templates 3.9x on
1, 7x on 8. e2e is `P4_NOCHECK=1 P4_HUGE=1 GLIBC_TUNABLES=glibc.malloc.hugetlb=2`; in-process its parts are load
9-10 ms, templates 7-11, layout 13-20, emit 42-48 (8 thr) / 167-183 (1 thr).)

Variant names: `emit_{f,t}_i{0,1,2,3}_{v0,v32,g}_p{0,1}`: serial/parallel; per-item facts inline / one table /
computed at each loop's item level / per relocation; copy byte by byte / 32-byte vectors / gather form; patch 8
scalar stores / one 8-byte read-modify-write. Layouts `lay_{f,t}_l{f,t}`: serial/blocked scan; lengths inline or
computed first.

### What the schedules did (1 thread unless said)

- **Vectorized copy: only after an algorithm change.** `split(rc.x, 32).vectorize()` with `TailStrategy::Predicate`
  on the byte RVar made the copy 26-28 s (from 0.5): the `where` (b < len) is a loop bound only on the unsplit
  RVar; split, the outer word loop ran 2^17/32 times per item with an `if`. The working form is an RDom (lane, word,
  item, chunk, parity) with the `where` on the word and the lane folded into a read-modify-write
  `code(q) = select(b < len, src(..), code(q))`: one 32-byte load/blend/store per word. The bytes past an item's end
  are written back unchanged, right in order within a task but a lost update across tasks, so the chunks run in two
  parity passes (even, then odd: a parallel loop inside a serial one). Copy alone: 31-34 ms for 60.7 MB in 2.38 M
  pieces (diagnostic variant `diag1`); with the patch 116-120 (`diag2`), so the patch loop is ~86 ms and the
  relocation loop ~44 ms of the 160.
- **Patch: an 8-byte read-modify-write vector beats 8 scalar stores** (p1 vs p0: 157-172 vs 172-180), and at 8
  threads by 2.5x (23-25 vs 63-64): p0's 3b trick sends the bytes a hole does not own to 8 scratch bytes past the
  image, and 8 threads writing the same scratch line serialize on it. A `where` on the byte (bx < w, to store only
  the hole's bytes, `pform=2`) is correct but takes 10-16 s: the byte loop gets the bound and the hole loop's `where`
  becomes an `if` in a loop to 32,768 per item (3b's lesson, again); not built any more.
- **Both sides of a select are evaluated**, and that was the biggest single cost: a glue item chased node ->
  template -> segment from its glue id (four random misses), a node item read the glue tables at its node id, an
  immediate read `at[its value]`. Making the side an item is not on read entry 0 (a cached line):
  emit 200-213 -> 157-172 ms, layout 26-32 -> 18-19 ms (`select(glue, 0, r)` as the index).
- **Per-item facts: where to compute them is a schedule** (`info`). Inline, each of the three loops re-derives item
  -> node -> template -> segment, and in the relocation loop each Tuple element re-derived it (5x per relocation in
  the stmt); `compute_at` each loop's item RVar (info 2, one Func copy per loop, since a Func has one compute_at)
  hoists it; compute_root (info 1, a vectorized table: 16 items' gathers side by side) streams it; info 3 computes the
  relocation loop's per relocation. All within noise of each other after the select fix (157-234, load 3-4): the
  per-item cost is not the derivation.
- **Loop fusion**: `rel.update(1).compute_with(code.update(1), chunk)` (relocations of a chunk right after its
  patches) works after renaming the chunk RVar to the patch's (compute_with matches loops by name); no gain (within
  noise). Fusing the copy and the patch (two updates of the same Func) is refused ("Cannot schedule code.update(1) to
  be computed with code.s1").
- **Chunk size** (items per task, 1024 / 4096 / 16384): within noise at 8 threads (23.0-27.9).
- **2 MB pages**: the caller maps the output `MAP_HUGETLB` (`P4_HUGE=1`) and Halide's own allocations go to huge pages
  with `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` (Halide allocates with malloc). e2e emit: 1 thread 230-255 -> 184-189
  ms, 8 threads 93-121 -> 42-53 ms; templates at 8 threads 15-24 -> 7.4-7.8 ms with hugetlb=2 (`hugetlb=1`, THP
  madvise, did not help). Mapping the inputs on huge pages (`FLAT_HUGE=1`, a copy at load) costs 40 ms more load
  than it saves.
- **Layout**: computing `len` first (compute_root, vectorized gathers) and scanning it is slower than the inline scan
  (31 vs 18 ms; 8 threads 12.5-16.7 vs 8.3-11.8); the parallel blocked scan is 3.4x on 8 threads.

### The gather form (attempt 2 of the copy)

Each output byte fetches its own: `code(x) = src(x + fill(x))`, a pure definition, parallel over tiles of 4096
output bytes with no race at all. Per tile: the item holding its first byte (a 23-step binary search over `at`,
compute_root over tiles), a scatter of `src_i - at_i` at the first byte of every item starting in the tile, a
running fill (a serial RDom scan over the tile's 4096 bytes), then the vectorized gather. Correct first time,
legible, and 2x slower than the scatter copy where it matters (emit 237-251 vs 157-162; 8 threads 56-72 vs 23-25):
it does a scan step and a byte gather per output byte (60.7 M) where the scatter does one 32-byte vector per word.

### Top K templates only (P4_TOPK=K: the rest as glue, from fc's output)

The driver rewrites the items (untimed, ~0.5 s of C++): every node of a template outside the K commonest becomes
glue (its bytes taken from fc's output, as all glue is), merged with neighbouring glue into one run; its relocation
holes become the run's relocations; the kept nodes' jumps are re-aimed at (new item, offset). Same pipelines, same
oracle, P4 OK for every K. Warm, 18:26Z, load 3.4-4.3 (1 thread CPU time, 8 threads wall, min-max):

| K | items | glue bytes | holes patched | layout 1 thr / 8 thr | emit 1 thread | emit 8 threads |
|---|---|---|---|---|---|---|
| 0 (all glue) | 136,634 | 60.7 M | 0 | 0.8 / 2.8-3.5 | 12.8-13.4 | 4.8-5.1 |
| 256 | 1.21 M | 52.3 M | 1.53 M | 7.8-9.5 / 2.7-2.9 | 74-109 | 13.2-16.3 |
| 1,024 | 1.57 M | 46.4 M | 2.58 M | 9.7-10.5 / 2.2-3.0 | 90-92 | 16.7-18.2 |
| 4,096 | 1.88 M | 39.0 M | 3.89 M | 12.3-12.6 / 3.4-8.8 | 116-127 | 23.8-35.4 |
| 16,384 | 2.13 M | 31.3 M | 5.24 M | 16.4-26.1 / 4.2-5.3 | 140-152 | 26.4-37.7 |
| all (108,833) | 2.38 M | 17.6 M | 7.68 M | 18-19 / 5-6 | 157-162 | 23-25 |

The shape changes the way the counts say: emit time is per piece and per hole, not per byte. A fit to the 1-thread
rows: ~42 ns per item, ~5.8 ns per hole; copying 60.7 MB as 136 K runs is 12.8 ms (4.7 GB/s; 12.6 GB/s on 8
threads). At K = 4,096 (74.6% of nodes) the emit is 25% cheaper than with all templates.

Cross-check of the per-piece claim in C (`cells/p4_copy32.sh REF_P4_C`: main's ref/p4/p4.c with each item's
`memcpy` replaced by whole 32-byte AVX2 load/stores that overrun into the next item, buffers padded; P4_REPS=10, 5
interleaved runs): 18:33:48Z, load 2.2-2.4: emit 92.7-95.9 vs memcpy 98.2-99.6 ms; rerun 18:38:15Z, load 2.0:
93.5-96.9 vs 91.6-100.3 (within noise); all P4 OK. How the bytes are copied moves C's emit by at most ~5%: its cost
is the walk over 2.38 M items and 7.68 M holes, as in Halide.

### Per step: easy / forced / impossible

- Easy: the lengths, the two-column scan, the hole value as one select (rel32 = gather of `at`), the relocation
  list as a scatter to `rat[i] + k`; parallel over chunks of items with allow_race_conditions (disjoint by
  construction); huge pages (the caller owns the output memory).
- Forced: the template index (a ragged scatter to invert the CSR, a binary search as an RDom update); a vector copy
  only as a read-modify-write with parity passes (a `where` on a split RVar is not a bound); the patch's bytes as an
  8-lane read-modify-write (an update writes one site; a `where` on the byte loop loses the hole loop's bound);
  entry-0 indices on every select side that is not taken; one Func copy per loop to hoist per-item facts.
- Impossible: fusing the copy and patch loops (same Func); storing exactly w bytes in a vector loop without a
  read-modify-write.

### Gotchas found here

- `f.parallel(j, 4096).vectorize(j, 8)` vectorizes the OUTER loop (parallel keeps the name `j` for the outer
  variable). On `bs` (pure + update) it crashed in every warm-loop run (4 of 4; never in single runs; a garbage index of ~-1e9
  from bs values that were never written, used under unsafe_promise_clamped; root cause not pinned further). Split
  explicitly: `split(j, jo, ji, 4096).parallel(jo).vectorize(ji, 8)`; 0 crashes since. gen_p3.cpp:181 has the
  same pattern on a pure-only Func (not changed here).
- `compute_at(LoopLevel(f, rvar, stage))`: the stage index refused valid locations; `compute_at(f, rvar)` works
  (RVar names are unique per RDom).
- A `where` added to an RDom after the RDom was used in a definition is an error; add it right after building it.

### Code excerpt (the patch, `Emit` in gen_p4.cpp)

    I pi = I_(op.i, infP);                                   // the item's facts, computed at the item level
    op.r.where(op.r[1] < select(op.real, pi.nh, 0));         // its holes: the inner loop's bound
    Expr h = pi.h0 + op.r[1], o = pi.ob + h;                 // the hole, its operand
    Expr P = pi.pb + i32(holeAt(h)), v = opVal(o);
    Expr tgt = select(enc == encRel32, clamp(i32(v), 0, n), 0);
    Expr val = select(enc == encRel32, i64(at(tgt)) + opAux(o) - i64(P + 4), v);   // a jump: a gather of at[v]
    Expr own = enc != encReloc && bx < w;                    // byte bx belongs to the hole
    code(P + bx) = select(own, cast<uint8_t>(val >> i64(8 * bx)), code(P + bx));     // bx: 8 vector lanes

The layout:

    len(i) = Tuple(u32(it.len), u32(it.nrel));
    lay(i) = Tuple(u32(0), u32(nmark));
    lay(r) = Tuple(lay(r - 1)[0] + len(r - 1)[0], lay(r - 1)[1] + len(r - 1)[1]);   // at, rat

### Lines (non-blank, non-comment)

gen_p4.cpp 345 (Templates 49, Item helper 24, Layout 43, Emit 203 with all variants' forms; the best variant's
path is ~90), p4.cpp 288 (of which the top-K rewrite 97 and the checks ~50), build.sh +8.


# Install and hello-world (from the installer)

### Install (done, 2026-11-16)
- C++ API + AOT generators: official release tarball **Halide 21.0.0**. v21 is the newest tag that ships
  binaries; the v22.x GitHub releases have NO attached tarballs, only PyPI wheels.
  ```
  mkdir -p /home/exedev/tools/halide && cd /home/exedev/tools/halide
  curl -sL -o h21.tar.gz https://github.com/halide/Halide/releases/download/v21.0.0/Halide-21.0.0-x86-64-linux-b629c80de18f1534ec71fddd8b567aa7027a0876.tar.gz
  mkdir v21 && tar xzf h21.tar.gz -C v21 --strip-components=1     # 665 MB; include/ lib/ bin/ share/
  ```
  Host target detected by `v21/bin/get_host_target`: x86-64-linux, avx2, avx512 (skylake/cannonlake), zen4, fma, f16c.
  No cmake on the VM; g++ 13.3 is enough (see build.sh).
- Python: `python3 -m venv /home/exedev/tools/halide/venv && venv/bin/pip install halide==22.1.0 numpy` (JIT sanity-checked).
  Note: C++ AOT is 21.0.0, Python is 22.1.0.
- Data: `/home/exedev/tools/halide/data/{idx.u32,table.u32}` (raw LE u32; idx = 50M values in [0,2^24), table = 2^24 values; made by hello/mkdata.cpp).

### AOT/generator path (hello/)
1. `gen.cpp` defines three `Generator`s (gather, hist, scan) and links `libHalide.so` + `libHalide_GenGen.a` (supplies `main`; the tarball has no GenGen.cpp).
2. `./gen -g NAME -f FUNC -o . target=host variant=K` emits `FUNC.a` + `FUNC.h` (one invocation per variant).
3. `driver.cpp` mmaps the files (MAP_POPULATE), wraps them zero-copy in `Halide::Runtime::Buffer`, calls the AOT functions, checks each result against plain C++, reports best of 5.
`hello/build.sh` then `hello/run.sh` reproduce everything (data generated on first run). Raw output: `hello/run.out.txt`.

### Timings (run 2026-11-16T15:25:36Z, best of 5, N=50,000,000 u32, table 64 MB, 2^20 bins; ms)
| pipeline | 1 thread | 8 threads |
|---|---|---|
| (a) gather out[i]=table[idx[i]] | 488 | 91 (68 in an earlier run) |
| (b) hist, RDom scatter, serial update (default) | 66 | 65 |
| (b) hist, `update().atomic().parallel(r)` | 698 | 1584 |
| (b) hist, `rfactor` private hists + merge, parallel over 8 chunks | 150 | 184 |
| (c) scan, serial RDom `out(r)=out(r-1)+in(r)` | 46 | 44 |
| (c) scan, hand-blocked 3-phase, parallel | 244 | 60 |
| plain C++ gather / hist / scan (1 thread) | 416 / 61 / 20 | n/a |

All Halide outputs verified equal to the C++ reference. Gather: 8 threads give 5-7x (DRAM random-load bound, about 0.55-0.75 G/s).

### What Halide does with these
- **Gather**: pure definition, trivial, parallel over chunks, matches C++ and scales. A data-dependent index REQUIRES `clamp` (else "Buffer may be accessed in an unbounded way"). Halide's model is that it must bound every access footprint.
- **Scatter / histogram**: an RDom update `h(f(in(r))) += 1`. Default is serial. `parallel(r)` is refused with "may introduce a race condition" (tested). Options: `atomic()` (correct but 10-25x slower here; slower with more threads), or `rfactor` (private partial histograms per chunk; did not beat serial for 2^20 bins; I tried one schedule only, untuned, so this is not a verdict). Halide has no sort, radix, or partition primitive.
- **Scan**: no scan primitive. A scan is an RDom update with a loop-carried dependence; `parallel` on it is refused (same race error, tested), and `rfactor` needs an associative reduction, which a scan is not as a whole. The only route is a hand-written blocked scan as separate Funcs (what variant 1 does: in-block scan parallel over blocks, serial scan of block totals, add offsets). That makes about 3 passes over memory: 60 ms at 8 threads vs a 20 ms single-thread C++ loop, so no win at 50M u32 (the serial scan already moves ~9 GB/s).
- Scatter output bounds cannot be inferred; the extent is pinned with `set_bounds`.

### Gotchas
- v22.x has no binary tarball; use v21.0.0 or the pip wheel.
- Arithmetic with `GeneratorParam<int>` and `0u` is ambiguous as Expr; use `cast<uint32_t>(0)`. `RDom(0, buf.dim(0))` does not compile; use `.extent()`.
- `Var`s used in `schedule()` must be the same Vars as in `generate()` (make them generator members), else "could not find split dimension".
- A uint32 index into a Buffer needs an explicit `cast<int>`; no implicit unsigned-to-signed.
- `HL_NUM_THREADS` controls the runtime thread pool (works as expected).
- Compile errors from `.parallel()` are thrown as `Halide::CompileError` at schedule time.
