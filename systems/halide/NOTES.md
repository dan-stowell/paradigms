# Halide: the grid cells (problems 1, 2, 3)

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
