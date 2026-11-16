# Halide column: prior art outside images

Problems: 1 resolve=join, 2 reach=flood, 3 layout+relocate=scan+scatter, 4 codegen by shape.
Papers marked (read) were downloaded and read in part (full text, via pdf extraction) by the author; others from abstracts/search only.
Data refs: MinIO 5.2 M symbol slots, 9.4 M relocs, ~2.7 M by-name refs, 360,376 live symbols, 109 MB image. Floors/hlink numbers from FLOOR.md in bottom-up (hlink ~4-5% of floor).

## Headline: GraphIt is a better column for problem 2 than Halide

**Problem 2 (reach) should be tried in GraphIt, not Halide.** GraphIt (read) is exactly "Halide's idea for graphs": an algorithm
language (`edges.from(frontier).to(filter).applyModified(f)`, vertexsets) plus a separate schedule language with
direction (SparsePush, DensePull, DensePush, hybrid DensePull-SparsePush switched on frontier out-degree sum), frontier as sparse
list or bitvector, edge-aware vertex-parallel, cache segmenting (SSGs), NUMA partitioning, kernel fusion. Halide has none of that: no
graph type, no frontier, no worklist, no direction choice; a flood in Halide would have to be a fixed number of dense RDom rounds
over all 9.4 M relocations (a "bulk-synchronous label propagation"), which does O(diameter) full passes. GraphIt emits C++/OpenMP/Cilk and
matches or beats Ligra/GAPBS/Galois (up to 4.8x vs next fastest on 24 of 32 experiments; BFS up to 28% faster than the next fastest
framework on 4 of 5 graphs). Source: https://arxiv.org/abs/1805.00923 (OOPSLA 2018), code https://github.com/GraphIt-DSL/graphit.
Caveat: the method rule is two coupled sets (live types converted to interface, live method name+sig), each feeding the other:
GraphIt's `to(filter)` is a per-vertex predicate that sees state, so it can express a monotone two-phase iteration, but Datalog
(Soufflé column) is the more natural fit; run both. Recommend adding GraphIt as a sixth column or replacing Halide's cell for problem 2.

## Ideas

### I1. GraphIt direction + frontier schedules for the live-symbol flood
- Idea: express reach as `edges.from(frontier).to(notVisited).applyModified(mark)`; sweep schedules {SparsePush, DensePull, hybrid, bitvector frontier, cache segmenting}.
- Source: GraphIt https://arxiv.org/abs/1805.00923 (read).
- Problem: 2 (also 1 later: by-name join is a bipartite edge map).
- Kill test: build the reloc graph (5.2 M nodes, 9.4 M edges, CSR from the flat column files) from MinIO; run the flood from the entry roots with best GraphIt schedule at 8 threads. KILL if it is slower than 2x the hlink reach phase time or the result differs from the oracle live set (360,376). Also report ms vs the floor (9.4 M edges x 4-8 B at 92 GB/s is ~1 ms; random vertex touches at 0.74 G/s is ~7 ms for 5.2 M).
  Expect the diameter to be shallow-but-long-tailed; if frontier sizes are tiny for most rounds, round overhead dominates (kill signal for bulk-synchronous schemes).

### I2. Bucketed priority queues / bucket fusion for ordered passes
- Idea: GraphIt's ordered extension (lazy vs eager bucketing, bucket fusion that processes several rounds of the same bucket without a global barrier; up to 3x on road graphs, 16.9x vs Julienne). Use for any ordered phase: layout orders by (section, alignment, package order), "earliest wins" resolution.
- Source: https://arxiv.org/abs/1911.07260 (read).
- Problem: 2 (deep, thin floods such as a linear chain of package inits), 3.
- Kill test: measure the number of BFS levels and frontier-size histogram of the MinIO flood. KILL if the median frontier < 1,000 vertices on fewer than 20% of levels (no sync-overhead problem to fuse away); otherwise run the flood with and without fusion and require a >=1.3x gain.

### I3. GraphIt GPU/CPU compilation: dedup flag and load-balance schedules
- Idea: `configDeduplication`, load-balance strategies chosen per frontier size, plus one algorithm language to two backends. CPU use: the dedup-vs-no-dedup choice for appending to the next frontier (the same symbol reached via many relocs).
- Source: https://arxiv.org/abs/2012.07990 (read, only the CPU-relevant parts).
- Problem: 2.
- Kill test: count duplicate-insert rate in the flood (inserts / distinct). KILL if < 1.2x (dedup is free: skip it); else measure atomic-CAS visited bitmap vs sort-based dedup on 9.4 M edges: kill if CAS bitmap costs > 2x a single pass over the edges.

### I4. TACO position space: split by nonzeros (relocation) not by coordinate (symbol)
- Idea: TACO's unified iteration-space transformation (read) schedules sparse loops in position space (cut the pos array equally), which load-balances skewed rows. In Halide terms this is the "edge-aware / split by relocation" parallelization for gather/scatter over reloc lists grouped by symbol (symbols with hundreds of relocs vs thousands with one).
- Source: https://arxiv.org/abs/2001.00532 (read) (Senanayake et al., OOPSLA 2020).
- Problem: 3 (patch relocations, grouped per symbol), 1.
- Kill test: from the MinIO reloc-per-symbol CSR, parallelize patching by symbol-chunks vs by equal reloc chunks at 8 threads. KILL if the max/mean per-thread load ratio under symbol chunking is < 1.15 (skew absent: no gain available) or if equal-reloc chunking gains < 10% wall time.

### I5. Halide `rfactor` / privatization as the schedule for reduce-by-key (dedup counts, per-section sizes)
- Idea: private per-thread partial results then merge, which Halide expresses with rfactor. My hello measurement (NOTES.md): 2^20 bins, 50M updates: serial 65 ms, atomic 698-1584 ms, rfactor 150-184 ms. So the naive schedule loses to serial; the open question is a radix-partition-then-count schedule that Halide cannot write.
- Source: Halide docs on `rfactor`/`atomic` (https://halide-lang.org/docs/class_halide_1_1_func.html), measured in hello/.
- Problem: 1 (content-hash dedup counting; winner rule needs a keyed reduction), 3 (section sizes).
- Kill test: MinIO's content hashes (5.2 M slots): histogram into 2^23 bins in Halide best schedule at 8 threads. KILL if it does not reach 2x a single-thread C++ loop (hello's hist: 0.99x at 8 threads, already near-kill) or is > 5x off the random-access floor (5.2 M / 0.74 G/s = 7 ms).

### I6. Hand-blocked scan, expressed as a Halide pipeline, vs memory-bandwidth floor
- Idea: Halide has no scan; the 3-phase blocked scan is the way. Hello measured 60 ms at 8 threads vs a 20 ms scalar C++ loop for 50M u32. For layout the scan is over 360,376 live symbols (sizes), tiny: ~0.5 ms scalar, so the scan is not the bottleneck at MinIO's size.
- Source: measured in hello/ (NOTES.md); parallel-scan background Blelloch, "Prefix sums and their applications" https://www.cs.cmu.edu/~guyb/papers/Ble93.pdf.
- Problem: 3.
- Kill test: run the 3-phase Halide scan on the real live-symbol size column (360 K elements, by section with segment resets). KILL the "parallel scan in Halide" idea if it is slower than the scalar loop at that size (very likely: thread-pool overhead ~tens of microseconds per Func) and note that segmented scan (by section) is not a clean RDom (needs a (flag,value) tuple reduction with a non-trivial operator; test whether Halide's tuple RDom update expresses it).

### I7. Sparse tensor compilers (TACO, SparseTIR, Finch) for join as intersection
- Idea: a join of two sorted key columns is sparse-vector intersection (co-iteration over compressed levels). TACO generates merge loops (two-pointer, galloping not offered); SparseTIR (abstract read; full paper skimmed for formats: ELL/BSR decomposition, composable transformations) targets GPU GNN/attention, and Finch (https://arxiv.org/abs/2404.16730) handles looplets with run-length, hash, and sparse-list levels. Name+ABI keys are strings (hash them to u64 first).
- Source: TACO https://arxiv.org/abs/1802.10574; SparseTIR https://arxiv.org/abs/2207.04606; Finch https://arxiv.org/abs/2404.16730 (last two from abstracts).
- Problem: 1.
- Kill test: sort 2.7 M by-name refs' hashes and 5.2 M def hashes; run the TACO/Finch intersection-with-lookup (coiterate sorted lists) vs hlink's hash join. KILL if the sort+merge total at 8 threads exceeds hlink's hash-join phase (hash join is one random access per ref: 2.7 M / 0.74 G/s ~ 4 ms; sorting alone will not beat that), which would settle that merge-based sparse compilers lose to hashing on a lookup-shaped join.

### I8. Taichi spatially sparse structures: hash/bitmasked hierarchies as the symbol table
- Idea: Taichi (SIGGRAPH Asia 2019, Hu et al.) separates data structure layout (dense/pointer/bitmasked/hash trees) from the computation, with automatic struct-for iteration over active cells and AsyncTaichi optimizing whole programs of kernels. A symbol table as a bitmasked hash hierarchy, with the active-set iteration giving the frontier for free.
- Source: https://yuanming.taichi.graphics/publication/2019-taichi/ ; AsyncTaichi https://arxiv.org/abs/2012.08141 (neither read in full).
- Problem: 1, 2.
- Kill test: Taichi CPU backend (pip install taichi, x64) build `ti.root.pointer`/`hash` table keyed by 64-bit name hash with 5.2 M entries; time 2.7 M lookups at 8 threads. KILL if above 20 ms (5x the random-load floor) or if insertion of 5.2 M entries takes > 10x a flat open-addressing table in C.

### I9. Exo / user-schedulable languages: scheduling of irregular code via rewrites
- Idea: Exo (PLDI 2022) and Exo 2 (ASPLOS 2025) make scheduling a library of verified rewrites on an imperative language, so one can write a reduce-by-key or scan kernel in Exo with explicit loops and schedule it with user-defined transformations (loop split, privatize, fission) without Halide's bounds-inference limits. Unlike Halide, indirect accesses `a[b[i]]` are allowed in the object code.
- Source: https://arxiv.org/abs/2411.07211 (Exo 2, from search listing only); Exo https://github.com/exo-lang/exo.
- Problem: 3, 4 (codegen by shape: Exo's templates and replace-by-instruction; "exocompilation" lets a user provide instruction/memory definitions, which is a template emitter).
- Kill test: write the relocation patch (gather + scatter, 9.4 M relocs) in Exo, schedule with split+parallel; KILL if Exo cannot express the data-dependent store (`out[off[r]] = ...`) without an escape hatch, or if the generated C is slower than the straightforward C loop by > 1.2x at 8 threads.

### I10. UNIT (tensorized instructions): not relevant
- Source: https://arxiv.org/abs/2101.08458 (grepped: no sparse or irregular content; matches dense ops to VNNI/Tensor Core/DOT).
- Problem: none. Possible weak link to 4 (matching instruction patterns by shape), but it matches loop nests, not byte templates.
- Kill test: none worth running; recorded so nobody re-checks it. Mark as killed on reading.

### I11. Halide-style "shape grouping" for codegen: a schedule language over templates (problem 4)
- Idea: Halide AOT compiles pipelines once per target by specialization (`specialize`, GeneratorParams) into compile-time-fixed code. Use Halide's generator machinery itself as the codegen-by-shape template engine: 4,096 node shapes as 4,096 generator specializations is absurd (compile minutes each), so use it at the other end: one generator parametrized by the shape's operand pattern to emit patch kernels ("apply immediate to field") rather than bytes.
- Source: Halide docs on Generators/specialize (https://halide-lang.org/tutorials/tutorial_lesson_21_auto_scheduler_generate.html and lesson 8 on scheduling); a measurement, not a paper.
- Problem: 4.
- Kill test: time Halide's AOT compile for one shape-kernel (one hello pipeline takes 0.75 s wall on this VM, measured; a real shape kernel will be larger). KILL if per-shape compile > 100 ms (4,096 shapes x 0.75 s = 51 min versus byte-template emission in microseconds). Expected to die; keep as the documented reason Halide's problem-4 cell falls down.

### I12. Dense bulk-synchronous reach in Halide as a negative control
- Idea: fixed-round label propagation over reloc arrays: `live(dst[r]) |= live(src[r])` as an RDom scatter, repeated K rounds (K = measured depth).
- Source: GraphIt DensePush discussion (read); Halide scatter measured in hello/.
- Problem: 2.
- Kill test: depth K from I2; rounds x 9.4 M relocs x (~2 random accesses) / 0.74 G/s = K x 25 ms. KILL if K x 25 ms exceeds 5x the GraphIt result from I1, which would put Halide's cell as "falls down on problem 2" with the reason that it has no frontier or worklist.

### I13. Halide Mullapudi/Adams autoschedulers for irregular loops
- Idea: auto-schedulers (shipped in the tarball: libautoschedule_adams2019.so, anderson2021, li2018, mullapudi2016) would pick tilings for the gather/scatter. Mullapudi2016 handles reductions heuristically; Adams2019 uses a learned cost model trained on dense stencils.
- Source: https://halide-lang.org (autoschedulers), Adams et al. SIGGRAPH 2019 https://halide-lang.org/papers/halide_autoscheduler_2019.pdf (not read).
- Problem: 1, 3.
- Kill test: run `autoscheduler=Mullapudi2016` / `Adams2019` on hello's gather/hist pipeline (flags `autoscheduler=...` in the generator invocation; needs the plugin `-p libautoschedule_*.so`). KILL if the best autoscheduled time is not within 1.2x of my hand schedule (gather 68-91 ms at 8 threads), which is expected for data-dependent access, where the cost model has no footprint to reason about.

## Not found
- No work found on parallel scans in Halide (searched web; hello shows a scan update cannot be parallelized at schedule time).
- No Halide-for-sparse work in the Halide project itself: TACO's schedule API is explicitly modeled on Halide's but Halide is dense-only.

## From the cells (2026-11-16, measured)

### C1. A join as rounds of scatter-min + gather, after compacting the participants
- Evidence: Halide 1a 81-93 ms on 1 core vs the plain C hash join 231-249 (same window): compact the 789 K keyed
  slots of 5.2 M first, then 8 rounds over tables of 2^20 -> 2^13 buckets (`cells/join.h`). Each round is two
  data-parallel passes (an associative scatter, a gather), so it is the shape every data-parallel system can run.
- Kill test: the same rounds in C (or Futhark) at 1 and 8 threads vs p1par; kill if not within 1.5x of p1par.

### C2. A flood whose every write is a constant store
- Evidence: one byte per fact (LIVE, IFACE, EXP, DONE, block-in-next-list) makes racing writers harmless without
  atomics; the frontier is implicit ("live and not expanded"). Correct 30/30 parallel runs; Halide's atomic()
  version was a mutex array per call (3.5 s). In Halide it costs 3 iterations per edge (79-90 ms at 8 threads).
- Kill test: the same layout in C/OpenMP (plain byte stores, no CAS) vs p2par's test-and-set; kill if slower.
