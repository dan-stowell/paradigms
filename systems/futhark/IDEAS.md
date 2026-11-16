# Futhark prior art -> ideas for the grid

Problems: 1 Resolve (hash join + winner rule), 2 Reach (flood with conditional method rule),
3 Layout/relocate (scan + gather/scatter), 4 Stretch (codegen by shape).
Sources I read as text (PDFs from futhark-lang.org/publications): fhpc18, array19, sc20, ppopp19 (abstract/skim),
pldi17 (skim); code read from diku-dk/futhark-benchmarks (pbbs/, micro/, lib/). "Measured" = run on this VM (NOTES.md).
Honest summary: Futhark's papers target GPUs; the CPU numbers I measured (NOTES.md) are far from the memory floor,
so the value is mostly in the *algorithm shapes* (and as a negative result for the paradigm on CPU), not in the compiler.

Common kill-test conventions: "grid" = MinIO columns (5.2 M slots, 9.4 M relocs, 2.7 M by-name refs, 360,376 live);
8 cores; baseline = hlink's time for the same phase; "dead" = the idea is killed if the measurement misses the threshold.

## I1. Frontier BFS via `expand` + dedup by `hist min` (flattening by expansion)
- Source: Elsman/Henriksen/Serup, "Data-Parallel Flattening by Expansion", ARRAY 2019, https://www.futhark-lang.org/publications/array19.pdf ;
  code https://github.com/diku-dk/futhark-benchmarks/blob/master/pbbs/breadthFirstSearch/breadthFirstSearch.fut and
  https://github.com/diku-dk/segmented (`expand`, `segmented_scan`, `replicated_iota`).
- Problem: 2. Each level: scan of out-degrees -> flat edge array via expand (scan + scatter of segment starts + segmented iota) ->
  filter unvisited -> remove duplicate targets by `hist i64.min` over (vertex -> first queue index) -> scatter parents.
  Reach's relocations are a CSR graph (symbol -> relocs -> target symbol).
- Kill test: build the reloc CSR of the 360 K-live-symbol flood from MinIO; run the pbbs-style BFS with the c and multicore backends
  (8 threads), full reach ignoring the method rule. Dead if wall time > 3x hlink's flood phase, or if the number of levels x
  per-level fixed cost (measure empty-frontier level latency; Futhark multicore launches ~tens of microseconds per parallel op) exceeds the total.
  Also dead if the level count (graph diameter from main) > ~2,000.

## I2. Skew handling: split heavy nodes once, globally (FV4)
- Source: Henriksen/Elsman/Oancea, "Modular Acceleration: Tricky Cases of Functional High-Performance Computing", FHPC 2018 (BFS case study),
  https://www.futhark-lang.org/publications/fhpc18.pdf . Four versions: padding (FV1), full flatten (FV2), iterative split (FV3),
  one-time global split (FV4). On GPUs FV1 wins on regular graphs (up to 5-7x vs Rodinia), FV2 slowest except on skewed graphs, FV4 is a safe default (5.2x on skewed D6).
- Problem: 2 (and 3: relocation counts per symbol are heavily skewed: a few symbols have 10^4-10^5 relocs, most have <10).
- Kill test: histogram of per-symbol reloc counts on MinIO; compute padded work (max_deg x frontier) vs real edges per BFS level. If the
  padded/real ratio is < 2 on every level, padding (FV1) is the winner and flattening is unneeded; if > 20 on some level, FV1 is dead and FV2/FV4 must win
  against hlink's per-symbol loop within 3x, else the idea is dead.

## I3. Reach as a bitmap/visited scatter with write-conflicts resolved by `hist`/min
- Source: same pbbs BFS (parents update uses `scatter` after dedup) ; FHPC'18 observes that benign write-write races (all writers write the same value)
  need no dedup. https://www.futhark-lang.org/publications/fhpc18.pdf
- Problem: 2. Skip the dedup `hist` when only liveness (not parent) is needed: mark `live[target]=true` by a scatter of constants (idempotent),
  then new frontier = filter of newly marked, via a scan-based compaction.
- Kill test: run one level ladder both ways on the MinIO flood. Dead if scatter-of-constant + compaction is not >= 1.5x faster than the dedup variant, or if
  the compaction scan (n=5.2 M slots) costs more than 5 ms per level times the level count budget.

## I4. Conditional method rule as a fixpoint of two coupled floods
- Source: no Futhark paper covers it; closest is the Datalog-like iteration in `containers/set` (unionfind, hashset) and BFS above, https://github.com/diku-dk/futhark-benchmarks/tree/master/lib/github.com/diku-dk/containers .
  Idea: iterate (A) flood over relocations, (B) mark method symbols whose (type converted-to-interface) AND (name+sig called by a live symbol); repeat until no change; each round is a
  map + `reduce_by_index (||)` over a name-id table of "called method names".
- Problem: 2. 
- Kill test: count outer rounds on MinIO (hlink's data). Dead if the number of outer rounds x (cost of a full-array pass at 100 GB/s of the 9.4 M relocs ~1 ms)
  exceeds hlink's whole Reach time, or if rounds > 30 (then a worklist/frontier version is required and the idea reduces to I1).

## I5. Hash join with winner rule as `reduce_by_index` with a packed 64-bit key (rank<<32|index) and `min`
- Source: Henriksen/Hellfritzsch/Sadayappan/Oancea, "Compiling Generalized Histograms for GPU", SC 2020,
  https://www.futhark-lang.org/publications/sc20.pdf (generalized hist: arbitrary operator, atomics for HW ops, CAS for 32/64-bit types, locks otherwise;
  strategy chosen by histogram size H vs cache). `micro/reduce_by_index.fut`, `pbbs/histogram`.
- Problem: 1. Winner rule = total order (definition > ref, non-DUPOK > DUPOK, content > BSS, larger size, earliest) -> encode as one u64 per candidate, then
  `reduce_by_index` with `u64.min` into a table indexed by interned name id (needs name -> dense id first, which is the hard part: itself a hash/sort).
  With dense ids, resolve is one pass of atomic-min (a CAS loop on CPU).
- Kill test: given name ids already dense (precomputed columns), run `hist u64.min` of 5.2 M (key,candidate) into 2.7 M-ish bins on 8 threads. Dead if > 3x the
  random-update floor (5.2 M updates / 0.74 G/s ~ 7 ms -> dead above ~25 ms) -- note the SC'20 CAS path exists only for GPUs; on multicore, measure what the C backend emits
  (check generated .c for atomics vs per-thread subhistograms: subhistograms of size H=2.7 M x 8 threads would be 170 MB, memory-bound).

## I6. Sort-based join: radix sort of (name-hash) with payload, then segmented reduce for winners
- Source: `diku-dk/sorts` radix_sort (https://github.com/diku-dk/sorts), pbbs `integerSort` and `removeDuplicates`
  https://github.com/diku-dk/futhark-benchmarks/tree/master/pbbs ; Futhark's own generalized-histogram fallback is sort-based (SC'20 sec. on non-atomic operators).
  Pipeline: radix sort 2.7 M+5.2 M (hash,payload) pairs -> flag segment heads -> segmented scan/reduce with the winner operator.
- Problem: 1 (and content-hash dedup).
- Kill test: *measured here*: Futhark's `radix_sort_int` sorts 20 M u32 in 0.69 s on 8 threads (0.0345 us/key), i.e. 8 M keys ~ 0.28 s. Hardware floor for an
  LSD radix sort of 8 M 8-byte pairs is ~10-20 ms. Dead if sorting the 8 M MinIO keys with the diku-dk radix sort is not under 100 ms on 8 threads (it will very likely be dead as is;
  re-test with a hand-written 3-pass 11-bit scan/scatter radix using `scatter` on a pre-histogrammed offset array to see if the *shape* can reach 30 ms).

## I7. Hash-set dedup for content-hash (identical-symbol dedup) via `containers` hashset
- Source: https://github.com/diku-dk/futhark-benchmarks/blob/master/pbbs/removeDuplicates/removeDuplicates.fut (`hash_dedup_int` vs `sort_dedup_int`, 10 M / 100 M, strings too)
  using https://github.com/diku-dk/containers (array key, hashmap with two-level hashing, insertion by `hist`/rounds).
- Problem: 1 (content hash) . Output of dedup is first occurrence = "earliest wins".
- Kill test: *measured here*: `array.dedup` of 20 M i32 takes 0.81 s on 8 threads vs 1.79 s on 1 thread. Run it on the real column of 360 K..5.2 M content hashes (u64 key).
  Dead if > 50 ms for 5.2 M keys on 8 threads (hardware: 5.2 M random inserts at 0.74 G/s ~ 7 ms; 7x slack).

## I8. Prefix-sum layout with alignment and per-section grouping as one segmented scan
- Source: segmented_scan via flag-pair operator (Blelloch), ARRAY'19 sec. 2.1 https://www.futhark-lang.org/publications/array19.pdf ; `segmented/segmented.fut`.
  Address = scan over (size rounded up to alignment); alignment padding needs a non-trivial operator: represent offset as a function composition
  (a, b) meaning x -> align(x, a)+b ... which is associative (affine-with-align maps compose), so a plain `scan` computes all addresses in one pass.
- Problem: 3. 360 K live symbols is small (a few ms at any speed), so the real question is the sorting by section/order beforehand.
- Kill test: scan on 360 K elements, 8 threads, c and multicore. Dead if > 1 ms (bottom: the scan's fixed cost is the thing: multicore at 50 M took 160 ms ~ 3 ns/element which would be 1 ms for 360 K;
  anything above 2 ms means Futhark overhead dominates and the grid should drop Futhark for Problem 3's scan). Also verify the alignment-composition operator against hlink's addresses (bit-exact).

## I9. Relocation patching as gather + `scatter` with in-place updates and uniqueness types
- Source: Munksgaard et al., "Memory Optimizations in an Array Language", SC 2022, https://www.futhark-lang.org/publications/sc22-mem.pdf (short-circuiting
  of copies so `scatter`/`with`-updates happen in place, avoiding a 109 MB copy); Henriksen thesis https://www.futhark-lang.org/publications/troels-henriksen-phd-thesis.pdf for `scatter` semantics (out-of-bounds indices are ignored: useful as predication for non-live relocs).
  Pldi26 "Verifying Array Properties in Pure Data-Parallel Programs" https://www.futhark-lang.org/publications/pldi26.pdf (injectivity/bounds of index arrays so scatter has no races).
- Problem: 3. Patch = for each live reloc: value = addr[target] + addend; write bytes at image[off]. Index -1 trick for dead relocs avoids a filter pass.
- Kill test: gather 9.4 M relocs (target id -> address, 32-bit patch) + scatter into a 109 MB u8 image viewed as u32 words when aligned (else byte scatters x4).
  Dead if > 4x hlink's relocation phase, or if the generated code copies the image array (check peak RSS: > 2x 109 MB means the in-place optimization failed).

## I10. Flat byte-template emission by `expand` (shape -> bytes)
- Source: expand (ARRAY'19 sec. 3-4 draw lines, triangles; same shape: variable-length output per element) https://www.futhark-lang.org/publications/array19.pdf ;
  `segmented.fut` `expand`, `expand_reduce`.
- Problem: 4. For each expression: size = template length of its shape; get(e,i) = template[shape(e)][i] or an operand byte patched in. Output = one flat expand, then no per-element branches.
- Kill test: 1.4 M expressions x ~30 B average template; the output is ~40 MB. Dead if expand + scan takes > 60 ms on 8 threads (floor ~ 40 MB at 20 GB/s write-ish ~ 2-3 ms -> 20x slack) or
  if the expand kernel fuses poorly (inspect generated C: a materialized index array of 40 M i64 = 320 MB -> dead for memory).

## I11. Incremental flattening / multi-versioned code and thresholds: not useful on CPU as is
- Source: Henriksen/Thorøe/Elsman/Oancea, "Incremental Flattening for Nested Data Parallelism", PPoPP 2019, https://www.futhark-lang.org/publications/ppopp19.pdf
  and Henriksen et al. PLDI 2017 "Futhark: purely functional GPU-programming with nested parallelism and in-place array updates" https://www.futhark-lang.org/publications/pldi17.pdf .
  Moderate flattening: only outer parallelism is exploited on the CPU backend (the multicore backend sequentialises inner levels once threads are saturated).
- Problem: 2, 1 (segmented work). Idea: for per-symbol nested loops (symbol -> its relocs), expect Futhark multicore to parallelise the outer map only.
- Kill test: skewed segmented sum (max-degree 10^5, 360 K segments): `map (reduce) segs` vs `segmented_reduce` on 8 threads. Dead (as a reason to prefer nested form) if the nested form is
  >2x slower than the flat one on MinIO's degree distribution; if so, always write flat/segmented.

## I12. Union-find / connected components instead of reach for dedup groups (identical-content classes)
- Source: https://github.com/diku-dk/futhark-benchmarks/blob/master/lib/github.com/diku-dk/containers/set/unionfind.fut (bulk parallel find/union) ;
  PBBS minSpanningForest and maximalMatching ports under pbbs/.
- Problem: 1 (content-hash and alias chains: symbol -> forwarded symbol).
- Kill test: resolve alias chains (path lengths in MinIO's resolved-by-name table) by pointer-jumping `map (\i -> p[p[i]])` rounds (log depth). Dead if chains are > 3 long
  on <0.1% of symbols (then a single gather suffices, no iteration needed) or if rounds x 5.2 M-element gather (~7-10 ms each) exceeds the total Resolve budget.

## I13. Bounds checks off + library mode for zero-overhead measurement
- Source: Futhark user guide, C API (https://futhark.readthedocs.io/en/latest/c-api.html) ; NOTES.md of this directory. Not paper-based; included as a kill test of Futhark's fairness in the grid.
- Problem: all.
- Kill test: compile hello.fut-class kernels with and without `--unsafe` and with `-O3 -march=native` (done) vs a hand-written C scan: Dead (Futhark deemed unfit as a CPU paradigm) if the
  best Futhark multicore 8-thread scan of 50 M u32 (currently 159 ms) stays > 5x the hand-C 8-thread chunked scan (expected ~10-15 ms at DRAM speed). This also sets the floor for all above.

## Not found / negative
- No Futhark paper on hash joins as such; the only hash-table code is the `diku-dk/containers` library (two-level static hashing, built with `hist`/retry rounds), whose speed I measured (I7).
- No Futhark paper on reachability with conditional edges, or on linking/symbol resolution.
- Futhark papers report GPU numbers; none of the CPU (`multicore`) backend beyond the compiler docs, and I found no published CPU comparison against PBBS C++. Treat CPU claims as untested until a kill test above runs.

## Measured on the cells (2026-11-16, NOTES.md)

- **I14. Run every array runtime on 2 MB pages.** `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` with no code change:
  1a 143-158 -> 89-111 ms, 1b 74-94 -> 47-48, 1c 197-225 -> 115-133 (1 core, interleaved, 16:33Z); faults per warm
  rep 16-33 K -> 18-67. Any system that allocates per operation (Futhark, Halide's buffers, DuckDB's vectors) should
  be rerun with it before its time is compared. Next: the same tunable on the other paradigms' cells.
- **I15. Owner-of-item by scatter + max-scan, never by binary search.** 1b 459 -> 76 ms, 3b 180 -> 115 ms (1 core).
- **I16. A level-synchronous flood has a per-level floor.** Futhark's flood is ~0.9 ms per level x 111 levels at
  1 core (C: 0.4); on 8 threads each level is ~25 barriers. Kill test for a fix: flood the first ~10 levels
  level-synchronously, then switch to fewer, fatter rounds (e.g. iterate the whole remaining edge set to a fixpoint
  with a dense `live` bitmap pass per round); dead if the round count times a full pass (~9.4 M edges, ~5 ms) is
  not below 111 x 0.9 ms.
  **Measured (results/levelfree.md): dead as stated.** The flood has 45 levels, not 111. A hybrid that drains
  the narrow levels sequentially cuts 45 -> 15 levels (1,211 -> 437 parallel ops) and is within noise of the
  original; a fully sequential worklist in Futhark (`2seq`) is ~2x faster at 1 core (min 73-79 vs 137-141). The
  floor is per element (the flattened level's ~26 passes), not per level.
- **I17. The I1 idea (expand-based BFS) does not carry the lazy decode cheaply.** Lazy (decode inside the flood)
  165-200 ms vs eager flood 97-119 ms at 1 core in Futhark, the reverse of C (52.6 vs 108.7 incl. decode).
- **I18. Copy runs of bytes as a map over output blocks with a sequential loop of slice updates inside.** P4: the
  60.7 MB copy ~20-35 ms CPU (blocks of 1/64 of the image, `row with [a:a+m] = xs[f:f+m]` = memcpy) vs ~110 ms as
  32-byte (index, byte) rows scattered (1 core, 17:56Z); the whole entry 411 -> 348 ms min (1 core), 220 -> 189
  (8). Kill test for 3b: the same shape for its 109 MB image copy; dead if 3b's copy part (`3b_copy`) does not drop
  below half. The block needs its first item (one binary search per block) and clipping at the edges.
- **I19. Expand is the residual cost of irregular scatter in Futhark.** After I18, P4's remaining ~3x over C is
  mostly four expands (6.33 M patched holes, 1.35 M reloc holes, 0.34 M glue relocs, 0.1 M markers), each a scan +
  scatter + max-scan + map with its tuples materialized, plus the hole tables. Kill test: relocations per block too
  (count per block, exscan, each block writes its own range, sequentially); dead if the entry is not below 2x C.
  (Moving only the patch into the block loop, `4bf`, changed nothing measurable: 326-357 vs `4b` 348 ms min, 1 core.)
