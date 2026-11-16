# Resolve and Reach: prior art and kill tests

Problems: R = resolve (2.7 M by-name refs join definitions, winner rule, content-hash dedup, package-index gather).
E = reach (flood over ~9.4 M relocation edges, 5.2 M nodes, Go method rule). Sizes from BRIEF.md.
"Kill test" = a measurement on MinIO's columns with a pass threshold; fail means drop the idea.
Reading status: sources marked [read] I fetched this session; [known] are from memory of the paper, link found by search, not re-read.

## Resolve

### 1. Radix-partitioned join (PRO) vs no-partition join (NPO)  [known]
Idea: partition refs and defs by hash bits (2 passes, ~7-8 bits each, software write-combining buffers), then join each partition in L2. Balkesen found partitioning wins when the build side exceeds cache/TLB reach. Here the build side (definitions, ~5.2 M slots, ~100+ MB with keys) is far above L2, so it applies; the 2.7 M probe side is small.
Source: Balkesen et al., ICDE 2013, https://www.comp.nus.edu.sg/~hebs/course/cs6284/papers/Balkesen-icde2013.pdf (code: https://mars-research.github.io/vldb13-eth-hashjoin). Also Schuh/Chen/Dittrich SIGMOD 2016 (no single best join; NPO often wins at small probe sizes): https://faculty.cc.gatech.edu/~jarulraj/courses/4420-s19/papers/19-hashjoins/schuh-sigmod2016.pdf
Problem: R. Columns: C first (reference for the others), then Futhark (histogram + scatter is its native shape), Halide (no: no scatter-heavy hash), Soufflé (its join is NPO-like, B-tree/brie indexes).
Kill test: on 8 cores, time (probe 2.7 M against 5.2 M-slot table) as NPO (one 512 MB-class open-addressing table) vs PRO (2-pass radix, 12-14 bits total). Pass: PRO warm join phase is >= 1.3x faster than NPO. Fail: NPO within 1.3x, since 2.7 M random probes at 0.74 G/s is ~3.6 ms and there is little to win. Compute that floor first: if NPO is already within 2x of 3.6 ms, kill.

### 2. Do not build a hash table of definitions: sort-free perfect/dense id via name interning  [known]
Idea: intern each distinct name once (one pass), then every ref/def carries a dense u32 name id; the "join" becomes a scatter into an array indexed by name id (reduce-by-key into a dense array), with no probing. mold does this (interns names, then resolves) and wild uses dense Vec<SymbolId> u32.
Source: mold design notes, https://github.com/rui314/mold/blob/main/docs/design.md (404 on raw fetch; secondary summary only); wild, https://github.com/davidlattimore/wild
Problem: R. Columns: all (it changes the representation); Futhark gets a pure reduce_by_index. 
Kill test: count distinct (name,ABI) among 5.2 M defs + 2.7 M refs. Time one interning pass (hash 7.9 M names, 1 table) plus a dense reduce-by-key vs the direct probe-join. Pass: intern+dense total <= 0.8x of direct join total; interning amortised if the same id is reused by reach (see 9). Fail if distinct names are > 70% of 7.9 M (interning does not shrink the work).

### 3. Pre-hash names at "compile time" (store the 64-bit name hash in the column file)  [Dan's ask; no single source]
Idea: the extractor writes hash64(name+abi) as a column, so the grid never touches string bytes. All systems read an 8 B key instead of a ptr+len+bytes. Related: Go's own objects already carry a content hash for dedup (ContentAddressable symbols, 16-byte hash); use those as keys for problem 1c for free.
Source: Go object file format, https://github.com/golang/go/blob/master/src/cmd/internal/goobj/objfile.go (HashType, Hash64Type). [known]
Problem: R. Columns: all; makes the comparison fair.
Kill test: bytes touched in resolve with string compares vs with the u64 column. Measure resolve time with strings (hash computed in-loop) vs prehashed. Pass: prehashed >= 2x faster warm. Also count 64-bit hash collisions among the 7.9 M names (pass: 0, else add a 128-bit check or a verify-on-match step costing < 5%).

### 4. Deterministic reduce-by-key by priority-write (winner rule as a monotone key)  [known]
Idea: encode the winner rule (def>ref, non-DUPOK>DUPOK, content>BSS, larger size, earliest object) as one packed u64 priority, then the "winner" is a parallel atomic-max (writeMax) per key. Order-independent, so deterministic without sorting. This is Blelloch/Shun "priority write".
Source: Blelloch, Fineman, Gibbons, Shun, "Internally deterministic parallel algorithms can be fast", PPoPP 2012, https://jshun.csail.mit.edu/determ.pdf ; Shun et al., "Reducing contention through priority updates" (SPAA 2013). 
Problem: R (and dedup 1c). Columns: Futhark (reduce_by_index with max op is deterministic), C, Goose (needs threads with no shared memory: one partition per thread, so atomics are not needed).
Kill test: winner-rule as packed key; compare (a) CAS-max into dense winner array, (b) sort-by-key then pick first, (c) per-partition serial. Pass: result equal to the oracle's resolution on all 2.7 M refs (byte-identical winner index) and (a) within 1.5x of (c). Fail on any mismatch: the rule is not a total order (check the packing handles "larger wins then earliest" and ties).

### 5. Sort-based join is dead; skip it  [known]
Idea: Kim et al. (SIMD merge sort) and Balkesen's later "Multi-core, main-memory joins: sort vs hash revisited" show hash with radix wins unless inputs are pre-sorted. Our data is not sorted by name; but the extractor could emit names in sorted order for free if objects are written sorted. 
Source: Balkesen, VLDB 2013, https://faculty.cc.gatech.edu/~jarulraj/courses/4420-s19/papers/20-sortmergejoins/p85-balkesen.pdf ; Kim et al., VLDB 2009.
Problem: R. Columns: Futhark (radix sort is in its library; merge join = scan). 
Kill test: Futhark radix_sort_by_key on 7.9 M u64 keys + segmented pick-first vs hash join. Pass: sort+scan <= 1.5x of the hash join time (the sort pays for itself only if layout/reach reuse the order). Expected fail; run it once to close the question.

### 6. Package-index gather as a software-prefetched, sorted-by-source gather  [known]
Idea: per-object index tables (ref -> (pkg,idx) -> global slot) are a gather chain of two dependent loads. Batch-prefetch (group prefetching / AMAC) or reorder refs by object (they already come grouped by object, so the per-object table is cache-resident). Source: Chen et al. "Improving hash join performance through prefetching" (ICDE 2004); Kocberber et al. AMAC (VLDB 2015).
Problem: R. Columns: C; check whether Goose's narrow typed links keep the table resident; Halide could express the gather with a compute_at tile.
Kill test: time the package-index gather of 9.4 M relocs (or 2.7 M refs) unsorted vs with a 16-deep prefetch window. Pass: prefetch version >= 1.5x faster, else the table is already cache-resident (per-object tables small) and the gather is at the 0.74 G/s floor, kill.

### 7. Content-hash dedup as hash-partitioned group-by, not a global table  [known]
Idea: dedup of identical (DUPOK/content-addressable) symbols is a group-by on the 128-bit content hash; with radix partitioning on the first byte, each partition is a small private table (no atomics, deterministic if the earliest-wins rule is used inside a partition in input order).
Source: same as 1 (partitioning) and Leis et al. morsel-driven parallelism, SIGMOD 2014, https://db.in.tum.de/~leis/papers/morsels.pdf [known]
Problem: R (dedup). Columns: Goose (no shared memory: partition per thread is its native model), Soufflé (a functional dependency/ choice-domain could express "first wins"; check it can).
Kill test: count dedup candidates (symbols with a content hash) and distinct hashes. Time group-by with 256 partitions vs one shared table. Pass: partitioned <= 0.7x of shared at 8 cores AND winners identical to the oracle. If candidates < 300 k, the whole step is < 1 ms and the idea is moot: report and kill.

## Reach

### 8. Direction-optimizing BFS (push/pull switching)  [known]
Idea: when the frontier is large, switch from push (follow out-edges of frontier) to pull (each unvisited node checks if any in-neighbour is visited, stop at first). Cuts edges examined 3-8x on low-diameter graphs. Needs the reverse CSR.
Source: Beamer, Asanovic, Patterson, SC12, https://people.eecs.berkeley.edu/~krste/papers/beamer-sc2012.pdf ; GAP suite, https://arxiv.org/pdf/1508.03619 (reference code github.com/sbeamer/gapbs).
Problem: E. Columns: C first; Futhark (both directions are map + reduce_by_index); GraphIt-style schedule = Halide-like column (see 11).
Kill test: load the 9.4 M-edge CSR in gapbs bfs.cc (or 40 lines of C), measure edges examined in push-only vs hybrid with alpha=15, beta=18 from the 4 entry-root set. Pass: hybrid examines <= 0.5x edges and is faster end to end including the reverse-CSR build (a 9.4 M-edge counting sort, ~tens of ms: must be paid). Likely fail: a program-dependence graph flood is probably many levels with a thin frontier (a deep call graph), where push wins; measure the level histogram (frontier sizes per level) first: if max frontier < 2% of edges, kill outright.

### 9. Frontier-level synchronization cost: count BFS levels before choosing a paradigm  [known]
Idea: data-parallel floods (Futhark, Soufflé's semi-naive iteration) pay one barrier plus a pass per level; Go call graphs from main may be 100s of levels deep. Ligra's sparse-frontier representation and GAP's queue-per-thread handle this; Futhark's dense map-per-level does not.
Source: Shun & Blelloch, Ligra, PPoPP 2013, https://jshun.csail.mit.edu/ligra.pdf [known; fetch failed to confirm link]; GAP above.
Problem: E. Columns: Futhark, Soufflé (semi-naive: one iteration per level).
Kill test: compute the BFS level count L and frontier-size histogram from the entry points on the 9.4 M-edge graph (a 20-line C program). Pass for Futhark/Soufflé being viable: L <= 200 and time/level fixed overhead x L < 20% of the 9.4 M-edge scan time (9.4 M x 8 B at 92 GB/s ~ 1 ms is the floor, so per-level overhead must be < ~1 ms/L). Else dense per-level kernels are dead and only a worklist (C/Goose) is viable.

### 10. GraphBLAS as a column: BFS as masked vxm over a boolean semiring  [known]
Idea: reach = repeated q = q (ANY.PAIR) A, masked by the complement of visited; SuiteSparse:GraphBLAS has push/pull switching in LAGraph (LG_BreadthFirstSearch_SSGrB), JIT-compiled kernels, and a bitmap/hypersparse format chooser. A column with the most mature graph kernels; ~10 lines. The method rule is a second matrix product (type x method) iterated outside.
Source: Yang, Buluc, Owens, "Implementing push-pull efficiently in GraphBLAS", ICPP 2018, https://arxiv.org/pdf/1804.03327 ; LAGraph source https://graphblas.org/LAGraph/src/algorithm/LG_BreadthFirstSearch_SSGrB.c.gcov.html ; Davis, SuiteSparse:GraphBLAS (ACM TOMS 2019; link not verified).
Problem: E (also a deterministic "resolve" via GrB_mxm with a MIN/MAX monoid, less natural). Column: new (GraphBLAS), it is not in the grid; compare with C/hlink.
Kill test: build A (5.2 M x 5.2 M, 9.4 M entries) from the reloc columns; run LAGraph_BreadthFirstSearch with the entry roots, 8 threads. Pass: reach set equals the oracle's (bit-identical for the no-method-rule subgraph) and time <= 5x of hlink's flood phase. If it is >5x, GraphBLAS is not worth a column (its overhead is per-level and per-call). Check that SuiteSparse builds on the VM (no GPU needed).

### 11. Go's method-liveness fixpoint is a monotone two-relation Datalog program: evaluate it as one rule set  [read]
Idea: deadcode.go: ifaceMethod (name+sig set, from R_USEIFACEMETHOD at reached call sites), markableMethods (methods of types that are "usedInIface"), genericIfaceMethod (name only, R_USENAMEDMETHOD), reflectSeen (global flag => all exported methods of reached types). Loop: flood; for each pending method m, mark if sig(m) in ifaceMethod or name in genericIfaceMethod or (reflectSeen and exported); repeat until no change. It is exactly semi-naive Datalog: live(S) :- live(T), reloc(T,S). live(M) :- usedInIface(Ty), methodof(Ty,M), sig(M)=Sg, ifaceUse(Sg), liveIfaceUse. Soufflé's strength.
Source: https://github.com/golang/go/blob/master/src/cmd/link/internal/ld/deadcode.go (read); Soufflé, https://souffle-lang.github.io/
Problem: E. Column: Soufflé (natural), Futhark (outer fixpoint loop around the flood).
Kill test: count the outer fixpoint rounds in the oracle (instrument: how many times flood() restarts after a method sweep on MinIO). Pass for "each round = full pass" paradigms: rounds <= ~10 and the pending-method set shrinks geometrically; then Soufflé/Futhark can recompute the flood naively each round. If rounds > 30, only incremental (worklist-driven) evaluation is viable. Also count: markableMethods size and fraction ever marked, to size the join (type,method)x(sig).

### 12. Method rule as a sig-id bitmap lookup, with interning (name+type-descriptor -> dense id)  [read]
Idea: methodsig = (name, type descriptor sym). With ids from idea 2, ifaceMethod is a bitmap over sig ids; "is this method pending-live" is one bit test; and the per-round work is only the newly set bits (index methods by sig id: CSR sig -> methods). Replaces Go's map[methodsig]bool by a dense bitset plus an inverted index, so the sweep is only the newly-live sigs' bucket, not all pending methods.
Source: deadcode.go (above); same idea as inverted-index semi-naive evaluation in Soufflé.
Problem: E. Column: C, Goose (links as dense ids), Futhark (scatter into bitset).
Kill test: instrument sweep work in the oracle: total (method, round) checks = markable x rounds vs total newly-live-sig bucket sizes. Pass: inverted-index work <= 0.1x of rescan work. And end-to-end check: live method set equals oracle's, exactly (diff of live lists = empty; 360,376 symbols).

### 13. Min-heap worklist is unnecessary: the flood is order-independent, but the output is deterministic anyway  [read]
Idea: Go's wq is a min-heap (for deterministic layout order?). Reach as a set is order-independent (monotone), so any traversal order (parallel, unordered, bitmap) gives the same set; ordering for layout can be recovered later by sorting live ids (a scan/compact), not by visiting in order. Frees the flood to be a lock-free parallel visited-bitmap flood.
Source: deadcode.go (wq heap); GAP (parallel BFS with atomic visited bitmap).
Problem: E -> feeds 3. Columns: all.
Kill test: run the oracle flood with a FIFO vs a min-heap; compare live sets (pass: identical, else the method/weak-reloc rules are order-dependent and there is a bug in our model). Then time parallel bitmap flood vs the heap one: pass if >= 4x faster at 8 cores; fail if live set differs.

### 14. SCC-condense / reachability index before flooding is not worth it for one-shot reach  [known]
Idea: BFS on a condensed DAG, or 2-hop labels, pay off for many queries; we have one query from fixed roots. Document and skip. A related real option: precompute reach per package (per-object "static" edges) in the extractor, a form of incremental linking (wild's goal: link dependencies once, add the user's code).
Source: wild incremental design, https://davidlattimore.github.io/posts/2024/11/19/designing-wilds-incremental-linking.html
Problem: E. Column: new (incremental).
Kill test: partition live set by package; measure what fraction of edges from stdlib/vendored packages stay inside stdlib closure (entry-independent). Pass: >= 60% of live symbols are in the closure reachable from runtime roots alone, so a precomputed closure for them can be reused across builds. Fail below 30%.

### 15. Parallel section-GC as mark over relocations: mold/lld pass structure  [known, mold secondary]
Idea: mold runs passes serially with a parallel-for inside each (resolve, then gc mark, then layout), parses once and interns names, uses a concurrent hash map pre-sized (HyperLogLog estimate) so it never resizes, and lld has the same structure with less parallelism. The grid's phases already have this shape; the only transferable trick is presizing tables from a cheap cardinality estimate.
Source: mold design, https://github.com/rui314/mold/blob/main/docs/design.md (could not fetch; summaries only: https://maskray.me/blog/2021-12-19-why-isnt-ld.lld-faster read as search snippet) 
Problem: R and E. Columns: C, Goose.
Kill test: presized vs growing open-addressing table in the C join (with load factor 0.5): time to build the 5.2 M table with exact-size vs doubling from 1 M. Pass: presize >= 1.3x faster build; also the distinct-name count from a HyperLogLog sketch within 5% of the true count so that sizing is safe.

### 16. Futhark-shaped flood: expand frontier by segmented scan + scatter with a flag array, using CSR-with-offsets  [known]
Idea: the standard Futhark BFS (Rodinia/Futhark benchmarks; see the futhark-benchmarks repo, bfs) uses replicate/iota/scan over frontier degrees to build the next frontier; cost is O(sum of frontier degrees) per level plus a scan. Nested-irregular parallelism is flattened by the compiler.
Source: Futhark benchmarks, https://github.com/diku-dk/futhark-benchmarks (rodinia/bfs); Henriksen et al., PLDI 2017, https://futhark-lang.org/publications/pldi17.pdf [known]
Problem: E. Column: Futhark (multicore backend).
Kill test: the Futhark bfs on the 9.4 M-edge graph, multicore backend 8 threads. Pass: <= 10x hlink flood time; fail if the level count (idea 9) makes the per-level fixed cost dominate. Combine with 9 first.

### 17. Edge-weight-free graph compression of the edge column (delta/varint) to cut bandwidth  [known]
Idea: the flood reads 9.4 M edges; Ligra+ compresses adjacency lists with delta + byte codes and runs faster on memory-bound traversal. Our edges are (src slot, dst slot) with src sorted, dst local to the object: deltas fit in 1-2 bytes. Floor is bandwidth, so this is bytes-per-edge reduction.
Source: Shun, Dhulipala, Blelloch, "Smaller and faster: parallel processing of compressed graphs with Ligra+", DCC 2015, https://people.csail.mit.edu/jshun/ligra+.pdf [known]
Problem: E. Column: C.
Kill test: encode dst as delta from src within each adjacency list; measure average bytes per edge. Pass: <= 3 B/edge (vs 4-8 B) AND flood time with decode <= 0.8x of uncompressed. Else kill: decode cost on a latency-bound flood outweighs bandwidth saved. Note the flood is random-access latency bound, not streaming; if the visited-bitmap access dominates (0.74 G/s limit), compression cannot help: check by counting edge bytes (9.4 M x 8 B = 75 MB = ~0.8 ms at 92 GB/s) versus the 9.4 M random visited probes (12.7 ms at 0.74 G/s): the probes dominate by 15x, so kill unless the visited bitmap (5.2 M bits = 650 KB) is shown to fit L2 (it does: then probes are L2 hits and compression matters). Run that arithmetic first.

### 18. Visited bitmap and bit-parallel multi-source flood  [known]
Idea: 5.2 M nodes -> 650 KB bitmap fits L2; flood becomes edge-streaming-bound, not latency-bound. Plus Beamer's bitmap frontier for pull steps. Distinct from 17: it is why 17 might matter.
Source: GAP/gapbs (bitmap, `Bitmap` class in bitmap.h, https://github.com/sbeamer/gapbs); Beamer SC12 above.
Problem: E. Columns: C, Futhark (bitset as [n/64]u64 with scatter by atomics... not available: Futhark has no atomic OR on a bitset except hist with bitwise-or; note: reduce_by_index with (|) works).
Kill test: flood with byte-array visited (5.2 MB) vs bit-array (650 KB), 1 core. Pass: bit-array >= 1.3x faster; and whether the existing hlink flood already does this (read its reach code first: if yes, nothing to gain, kill).

### 19. Soufflé engine settings: brie/btree indexes and the "no-index specialized" relations  [known]
Idea: the Soufflé column's speed depends on its relation data structures (concurrent B-tree, brie for dense ints, eqrel for equivalence). Reach on dense integer ids is the brie's best case; the 9.4 M-tuple edge relation as a btree with prefix index is a worse choice. Soufflé also supports `.output`-free in-memory use via the compiled (-c) mode and `--jobs=8`.
Source: Jordan et al., "Brie: a fast trie..." and "Soufflé: On synthesis of program analyzers" (CAV 2016, https://link.springer.com/chapter/10.1007/978-3-319-41540-6_23) ; Scholz et al., "On fast large-scale program analysis in Datalog" (CC 2016, https://dl.acm.org/doi/10.1145/2892208.2892226) [known]
Problem: R and E. Column: Soufflé.
Kill test: load 9.4 M edge facts (from a flat file via .input with the compiled binary) and run the reach rule with 8 jobs, comparing `.decl edge(a:number,b:number) brie` vs default btree. Pass for the column: warm reach <= 20x of hlink with the brie; if only the load of facts takes longer than the whole C link, record "end to end" separately (the cell counts both).
