# GraphIt: problem 2 (reach) on MinIO and kube-apiserver

GraphIt (https://github.com/GraphIt-DSL/graphit, OOPSLA 2018, https://arxiv.org/abs/1805.00923) is Halide's split
for graphs: an algorithm language (edgesets, vertexsets as frontiers, `edges.from(f).to(filter).applyModified(...)`)
and a schedule language (direction, frontier layout, parallelization, cache segments, NUMA). systems/halide/IDEAS.md
named it the better column for the flood; this is that column, for problem 2 only.

Files: `p2.gt` (the algorithm, 50 lines), `sched/*.gt` (13 schedules, one line each), `towsg.c` (the flat files ->
one GraphIt graph; 145 lines: this is where the rules went, see below), `host.cpp` (the one extern: compare with the
oracle), `build.sh`, `check.sh`, `bench.sh`, `graphit.patch` (one codegen fix). Logs: `check-*.txt`, `bench-*.txt`.

## Result

Correct on both datasets under all 13 schedules at 1 and 8 threads: `systems/graphit/check.sh` -> every line
`live: 663630 entries (oracle 663630), 0 differ  P2 OK` (kube-apiserver: 926,486), "all P2 OK" (20:37Z). The
comparison is exact and element-wise against `oracle/p2_live.u32`.

Times, ms, `bench.sh`, 5 interleaved rounds (warm: min of process mins / median of process medians / max of process
medians; e2e: whole process, min / med / max), MinIO 20:40-20:41Z load 2.4-3.4; kube-apiserver 20:41-20:43Z load 2.5-2.9:

| | MinIO warm | MinIO e2e | kube warm | kube e2e |
|---|---|---|---|---|
| GraphIt `serial`, 1 thread | **39.8 / 45.0 / 46.2** | 290 / 295 / 322 | **53.6 / 58.0 / 59.7** | 571 / 606 / 649 |
| GraphIt `push`, 1 thread | 52.8 / 57.3 / 61.1 | 299 / 315 / 335 | 58.0 / 79.8 / 83.3 | |
| GraphIt `hybrid_bv`, 1 thread | 48.8 / 56.3 / 63.5 | 279 / 285 / 321 | 58.0 / 80.4 / 86.1 | |
| C `p2` (plain worklist), 1 | 37.1 / 42.3 / 49.4 | 61 / 68 / 84 | 61.9 / 66.3 / 79.5 | 96 / 120 / 137 |
| C `p2par`, 1 thread | 27.6 / 33.9 / 34.8 | 77 / 80 / 93 | 39.0 / 44.6 / 45.5 | 109 / 130 / 134 |
| GraphIt `push`, 8 | 17.5 / 29.4 / 30.8 | 251 / 285 / 314 | 23.3 / 32.8 / 44.4 | 486 / 499 / 507 |
| GraphIt `push_sliding`, 8 | 18.2 / 25.9 / 27.3 | 259 / 267 / 341 | 23.8 / 31.0 / 36.2 | 457 / 501 / 508 |
| GraphIt `hybrid_bv`, 8 | **16.8 / 24.2 / 28.5** | 254 / 278 / 325 | 23.5 / 33.9 / 36.7 | 505 / 509 / 524 |
| GraphIt `densepush`, 8 | 17.7 / 24.3 / 29.7 | 260 / 274 / 278 | **23.4 / 31.2 / 37.1** | 464 / 506 / 570 |
| C `p2par`, 8 threads | **10.6 / 15.9 / 19.7** | 45 / 48 / 53 | **15.1 / 19.5 / 20.4** | 91 / 95 / 112 |

- **1 core: GraphIt's serial push is the plain C worklist's speed** (MinIO within noise of p2; kube 54-60 vs 62-80,
  ranges apart, but p2 times its calloc) and 1.3x the tuned p2par at 1 thread.
- **8 threads: 1.5-1.7x p2par** (medians 24 vs 16 MinIO, 31 vs 19.5 kube; the ranges barely overlap on MinIO and are
  apart on kube). The four good schedules are within noise of each other.
- **End to end it is 5x worse**, and none of that is the flood: GraphIt reads only its own graph files, so a
  process reads the 269 MB `.wsg` (0.14-0.17 s, its own "Read Time"; 0.30 s and 530 MB for kube) before it starts.
  Making the `.wsg` from the flat files is a separate step (`towsg`, 1 core C: MinIO 118 ms build + 490 ms write;
  kube 231 + 926 ms) not in the e2e column; counted, GraphIt's e2e is ~0.9 s (MinIO) vs C's 0.05-0.08 s.
- Mechanism (counts, not time): GraphIt's flood is **59 rounds** (kube 58) and **916,944 frontier pops** (kube
  1,357,043) over a 6.24 M vertex / 10.56 M edge graph (kube 15.4 M / 19.9 M); p2par does 111 levels in 7 method
  rounds and 678,032 pops over 5.2 M slots / 8.3 M edges. GraphIt has fewer barriers (the method rule fires inside
  the flood, not in a serial phase between floods) but 35% more pops (the I, U, C, F vertices below) and a 4-byte
  state per vertex where C has 1. `perf` is not installed on the VM, so no cache-miss counts.

## How the rules fit (the main finding)

GraphIt has one way to change state: from a frontier, along edges, apply a function to (src, dst, edge weight),
and the next frontier is the dsts whose *one* tracked vector changed. There are no joins, no hash tables, no bit
operations, no `%` (it starts a comment), and one tracked vector per apply. P2 has two coupled sets (live, iface),
two conjunctions (`live and iface and gotype`; `candidate method and called method`) and a join (a method's
(name, type) against the live signatures). None of that is in the language, so **the rules were compiled into the
graph** (`towsg.c`), and the flood became one guarded BFS:

- every fact is a vertex: L(s) "s is live" (the slot id itself), I(s) "s converted to an interface" and U(s) "live
  and iface" (only for defined gotypes: iface elsewhere has no consequence), Sig(x), Name(n), R "reflect's Method
  is live", C(m) "something can call m", F(m) "m is a candidate and called". Vertex 0 (slot 0 is never live) is the
  super-root, with edges to the roots and to the flag-16 types.
- **an AND is an edge whose weight is a guard vertex**: dst is reached through the edge only if the guard is
  reached too. Each input of an AND carries the other input as its guard (L(s)->U(s) guarded by I(s), I(s)->U(s)
  guarded by L(s)), so whichever input is reached last sees the first: sound and complete under any schedule.
  Plain edges are guarded by vertex 0, which is reached first. This is the trick that makes the whole rule set one
  monotone `min=` on one int vector.
- **the method join is done by the converter**, as edges Sig(x)->C(m) for each method with that (name, type) (a
  sort and a binary search), Name(n)->C(m), R->C(m) for exported m. It is data-dependent work, so it is counted in
  the converter's time, not the flood's.

The algorithm, whole (`p2.gt`):

```
const edges : edgeset{Edge}(Vertex, Vertex, int) = load (argv[1]);
const st : vector{Vertex}(int) = 1;  % 1 not reached, 0 reached

func notReached(v : Vertex) -> output : bool
    output = 0 < st[v];
end

func reach(src : Vertex, dst : Vertex, guard : int)
    st[dst] min= st[guard];
end
...
        while (frontier.getVertexSetSize() != 0)
            #s1# var output : vertexset{Vertex} = edges.from(frontier).to(notReached).applyModified(reach, st, true);
            delete frontier;
            frontier = output;
        end
```

and a schedule (`sched/hybrid_bv.gt`):

```
schedule:
    program->configApplyDirection("s1", "SparsePush-DensePull")->configApplyParallelization("s1", "dynamic-vertex-parallel")->configApplyDenseVertexSet("s1", "bitvector", "src-vertexset", "DensePull");
```

**No C++ host loop was forced.** The method rule could have been a C++ extern between GraphIt flood calls (p2.c's
shape); compiling it into the graph instead kept the whole fixpoint inside one `while` over `applyModified`, and
that is why it needs 59 rounds where p2par needs 111 levels. The price is the converter and a 27% bigger graph.

## Schedules: which help

One-shot sweep, 10 reps per process, best 3 (20:31Z, load 3.5), MinIO, all P2 OK:

| schedule | 1 thread | 8 threads |
|---|---|---|
| `serial` (SparsePush, serial) | **43-45** | 53-55 (one thread) |
| `push` (SparsePush, dynamic-vertex-parallel) | 56-58 | 26-29 |
| `push_static` (static-vertex-parallel) | 63 | 25-27 |
| `push_g64` (grain 64 instead of 1024) | 64 | 27-29 |
| `push_sliding` (sliding queue frontier) | 58-60 | **22-25** |
| `pull` (DensePull) | 1,208-1,224 | 383-390 |
| `pull_ea` (DensePull, edge-aware vertex-parallel) | 1,312-1,326 | 1,250-1,303 |
| `hybrid` (SparsePush-DensePull, bool-map frontier) | 55-56 | 25-27 |
| `hybrid_bv` (+ bitvector frontier for pull) | 57 | **17-18** |
| `hybrid_bv_ssg4` (+ 4 cache segments for pull) | 60-62 | 30-31 |
| `densepush` (DensePush-SparsePush) | 64-66 | 23-25 |
| `push_densepush` (SparsePush-DensePush) | 60-61 | 22-26 |
| `hybrid_bv_numa` (+ NUMA, 4 segments; needs graphit.patch) | | 24-35 |

- **Direction is the only knob that matters, and only negatively**: pure pull is 25-50x worse. The flood is 59
  rounds of mostly tiny frontiers; a pull round scans all 6.24 M vertices (and their in-edges), so pull pays
  59 x 6.24 M. Hybrid only switches to pull for the few rounds whose frontier out-degree passes E/20, and there
  the bitvector frontier is a small gain (17-18 vs 25-27 in this sweep; within noise of `push` in the interleaved
  bench, medians 24 vs 29).
- **Parallel code costs 1.3x at 1 thread**: the parallel push is Ligra's edgeMapSparse (degrees, a prefix sum, an
  out-edge array, a filter: three passes over each frontier's edges and allocations per round), while `serial` is
  a plain loop. So the best 1-core schedule is `serial` and the best 8-core one any of the push/hybrid family.
- Frontier as a sliding queue, static vs dynamic scheduling, grain size, push-side dense frontier: within noise.
- **Cache segmenting and NUMA do not help**: the graph is 6 M vertices (25 MB of state, the L3 is 32 MB) and the
  VM has one NUMA node, so segments only add work. NUMA's generated code did not compile (below).
- Spinning OpenMP (the default wait policy) on the shared machine: one run of `hybrid_bv` at 8 threads went 2,465 ms
  on its first rep and 30-310 ms after; everything here runs with `OMP_WAIT_POLICY=passive` (as ref/flat does).

## Easy / forced / impossible

- **Easy**: the flood is one line and the schedules are one line each; all 13 schedules produce the exact live set
  on both datasets the first time they compiled; the AND-as-guard-weight makes the rule set a single monotone `min=`.
- **Forced**: (1) the rules compiled into a graph outside GraphIt (split vertices, ANDs as guard weights, the
  (name, type) join as edges): no joins, maps, bit ops or second tracked vector in the language; (2) the to-filter
  written `0 < st[v]` to dodge a miscompile (below); (3) the state as `int` (no byte type: 4x C's bytes); (4) a
  tensor index must be a variable (`st[guard - 1]` asserts in the midend); (5) GraphIt reads graphs only from
  files: a 269 MB `.wsg` per dataset, read in 0.15 s, made in 0.6 s.
- **Impossible**: nothing for correctness. Cilk (the paper's runtime) is gone from g++ 13, so OpenMP only; NUMA
  schedule codegen broken without a patch (and meaningless on one node).

## GraphIt bugs met (with the generated code as proof)

1. **A guarded assignment is tracked as "always changed".** `if cond  st[dst] = 0  end` gives
   `output = true` regardless of `cond` (change_tracking_lower.cpp sets the tracking variable to the literal `true`
   for any assignment, ignoring control flow), so a dst whose guard failed would enter the next frontier. Seen in
   the generated code of my first version (bool vector, guard in an `if`; no CAS either), not run; avoided by the `min=` form, whose tracking is the `writeMin` result.
2. **`min=` plus a `to` filter of the form `st[v] == c` (or `st[v] > c`) is rewritten into
   `compare_and_swap(st[dst], c, st[guard])`** (atomics_op_lower.cpp: a ReduceStmt passes its "one assignment"
   check, and any comparison passes its `==` check, operand 1 taken as the expected value). The CAS "succeeds"
   when `st[guard]` is 1 too: with `== 1` the live set had 806,876 entries (P2 FAIL); with `> 0` it compiled to
   `compare_and_swap(st[dst], 0, ...)` and found 0 (P2 FAIL). Writing `0 < st[v]` puts a literal on the left, the
   pattern does not match, and the `min=` stays a `writeMin`. (Generated files kept in `build/gen/push_cas_bug*.cpp`,
   not committed; rerun by changing the filter back.)
3. **NUMA codegen**: `configApplyNUMA` emits `#pragma omp for` followed by a `ligra::parallel_for_lambda(...)` call
   (not a `for`), which does not compile. `graphit.patch` (2 lines in gen_edge_apply_func_decl.cpp) emits a plain
   `for` in the NUMA path. Without `configApplyNumSSG`, the NUMA schedule segfaults the compiler (exit 139).

## Install

```
git clone https://github.com/GraphIt-DSL/graphit /home/exedev/tools/graphit   # 2e01497
cd /home/exedev/tools/graphit && git apply .../systems/graphit/graphit.patch   # only for the NUMA schedule
mkdir build && cd build && cmake .. && make -j8      # builds clean with g++ 13.3, cmake 3.28; graphit_test: 364/364 pass
sudo apt-get install libnuma-dev                     # only for the NUMA schedule
```

The stock build needed no patch. `graphitc.py -a ALGO.gt -f SCHED.gt -o OUT.cpp` generates C++; it compiles with
`g++ -std=c++14 -O3 -march=native -DOPENMP -fopenmp -I src/runtime_lib`. Rerun: `systems/graphit/check.sh`
(build + convert + every schedule), `systems/graphit/bench.sh [DATADIR]`.
