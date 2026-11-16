# Datalog prior art -> ideas for the grid

Problems: (1) resolve = join, (2) reach = flood with conditional method rules, (3) layout+relocate = scan+scatter,
(4) codegen by shape. Sources read: GDlog paper (arXiv HTML, numbers below), Brie abstract, Ascent/FlowLog/Free Join/
index-selection abstracts and search summaries; the PPoPP/CC/VLDB PDFs themselves were NOT read in full (marked
"abstract only"). Anything not from a source is my inference and labelled so.

## Up front: is another Datalog engine a better column?

No engine on this CPU-only 8-core box is likely to beat hlink; Datalog's honest role is "what does the declarative
formulation cost". Our own hello (NOTES.md) already shows Soufflé at 2.5 s for a 10 M-edge BFS (~4 M edges/s)
and 9.4 s to read TSV, i.e. ~100-1000x off the hardware floor before MinIO's rules even start.
- **Best second column: Ascent (Rust)** with BYODS (bring-your-own data structures: can plug a CSR/union-find/bitmap
  for a relation) and rayon parallel mode, since it lets a relation be a flat array and rules be Rust closures
  (conditional method rule = a filter closure). Also lattice columns (`lattice` keyword) express "winner rule" for
  resolve as a lattice join (max over (is_def, !dupok, has_content, size, -index)) without a join+aggregate.
  Source: https://s-arash.github.io/ascent/cc22main-p95-seamless-deductive-inference-via-macros.pdf , OOPSLA'23 BYODS https://dl.acm.org/doi/pdf/10.1145/3622840
- **FlowLog** (Differential Dataflow based, VLDB 2026, per-rule relational IR, boolean specialization for recursion):
  https://arxiv.org/abs/2511.00865 . Abstract only: no numbers seen. Worth a smoke run if Rust+DD builds; likely heavier
  memory than Soufflé (DDlog is reported to have significant memory overhead per the FlowLog paper).
- GDlog is the fastest reported (6-20x over 32-thread Soufflé on REACH, 37-45x on CSPA) but **needs a GPU; none here**.
  Do not add a column; use its published numbers only as a ceiling for the "semi-naive loop" cost model.

## Ideas (each with kill test)

1. **Bulk-load relations from columns, never text.** Idea: embed Soufflé, mmap column files, parallel sort, `btree_set::load`
   (done in hello: 446 ms vs 9,400 ms text for 10 M rows). Source: Soufflé BTree.h (`load`), PPoPP'19 B-tree paper
   https://souffle-lang.github.io/ (hints + bulk). Problems: all. Kill test: on MinIO's 9.4 M relocations + 5.2 M
   slots, load into Soufflé relations must be <= 1 s at 8 threads (<= 10x the 92 GB/s read of the columns,
   ~0.1 s); otherwise the column is dead before evaluation starts.
2. **Resolve as a Datalog join with lattice/aggregate winner** (`best(n,max(rank)) :- def(n,r,rank)` then join back).
   Source: Soufflé aggregates (docs https://souffle-lang.github.io/docs.html), Ascent lattices (above). Problem 1.
   Kill test: 2.7 M by-name refs x 5.2 M slots hash join with the 6-key winner rule must run <= 20x hlink's resolve
   time (hlink is at 4-5 % of floor, so 20x hlink ~ the floor's 1 %). Soufflé joins go through btree
   lower_bound per tuple (O(log n) cache misses); if one probe costs > 300 ns at 5.2 M rows it fails.
3. **Index selection chooses the join order for free.** Idea: auto minimum-chain-cover index selection builds one
   btree per lexicographic chain. Source: Subotic et al., "Automatic Index Selection for Large-Scale Datalog
   Computation", PVLDB 12(2) 2018 https://dcmpx.remotevs.com/org/vldb/www/PL/pvldb/vol12/p141-subotic.pdf ; precursor
   https://arxiv.org/abs/1709.03685 (abstract only). Problem 1. Kill test: for resolve (lookup by name hash, by
   (pkg,idx), by content hash) count indexes Soufflé selects with `--show=...`/profile; if it builds > 3 btrees
   over the 5.2 M slots and peak RSS > 8 GB (3x hlink's footprint) the representation is too fat.
4. **Brie (trie) for dense id relations.** Idea: relations over small dense ints (symbol index, object index) compress
   up to 3.6x and insert up to 15x faster than state-of-the-art alternatives; 4x on a points-to benchmark.
   Source: Brie https://souffle-lang.github.io/pmam19-paper (abstract only; `.decl r(...) brie`). Problems 1, 2.
   Kill test: declare `reloc(src,dst)` as `brie` and `btree`; if brie does not beat btree by >= 2x on the
   9.4 M-row bulk insert + BFS over MinIO's live flood, drop it (the data is sparse in dst, dense in src).
5. **Reach as semi-naive REACH: cost model.** Soufflé: 77.8 % of REACH time is tuple insertion; 16->32 threads = 7 %
   (GDlog paper https://arxiv.org/html/2311.02206v2 sec. on Soufflé scaling). Our hello: eval 2.5 s / 10 M edges both
   at -j1 and -j8. Problem 2. Kill test: run the MinIO reach (entry symbols -> relocations, 360,376 live of 5.2 M)
   in Soufflé -j8; kill if eval > 100 ms (a flat bitmap flood does ~9.4 M edges in ~15 ms at 0.74 G random loads/s;
   100 ms = 6x off floor is the most we allow a *declarative* column before calling it the wrong paradigm).
6. **Conditional method rule as stratified negation/aggregation.** Idea: `live_method(t,m) :- iface_conv(t), live_call(sig(m)), method(t,m)`
   is a non-linear recursive rule (live_call depends on live_method code). Source: Doop-style on-the-fly call graph
   construction, Bravenboer & Smaragdakis OOPSLA'09 https://courses.cs.washington.edu/courses/cse503/10wi/readings/p243-bravenboer.pdf ;
   Soufflé/Doop port. Problem 2. Kill test: express Go's method rule (+ reflection flag) in Soufflé; the live set
   must equal hlink's 360,376 exactly (correctness kill), and the number of semi-naive iterations on MinIO must
   be < 200 (each iteration costs a full delta/new merge; GDlog's usroads needs 606 iterations and 52 s).
7. **Eager buffer management / avoid per-iteration allocation (GDlog).** Idea: pre-size full+delta*K buffers to avoid
   allocator cost, which is > 65 % of time on long-tail graphs (K=5, 1.35x memory, up to 3x faster, Table 1).
   Source https://arxiv.org/html/2311.02206v2 . Problem 2. Kill test (CPU analogue): count iterations of MinIO's
   flood; if the tail (frontier < 1000 nodes) is > 50 iterations, per-iteration fixed overhead in the engine
   (measure empty-iteration ms) x iterations must stay < 10 ms total, else the engine's loop is the bottleneck.
8. **Hash-indexed sorted array (HISA) layout for joins.** Idea: tuples in one sorted compact array, a hash table holding
   only join columns -> offset of first match (load factor 0.8, MurmurHash3, linear probing); dedup by sort+adjacent
   scan; two-pass (count then write) join. This is exactly hlink-style columns + open-addressing. Source: GDlog.
   Problem 1, 3. Kill test: CPU HISA microbenchmark on the 2.7 M refs x 5.2 M slots join: if single-thread
   probes/s < 30 M (=33 ns, a cache-miss-bound join) it cannot pass the resolve floor; compare to Soufflé btree probes.
9. **Count-then-materialize joins => prefix sum + scatter.** Same two-pass idea is how a linker does layout+relocate
   (problem 3): first pass counts output per row, scan, second pass scatters. Source: GDlog; Shovon et al.
   "Towards Iterative Relational Algebra on the GPU" USENIX ATC'23 https://www.usenix.org/conference/atc23/presentation/shovon
   (open-addressing hash join, fused operators, two dedup variants; up to 3.9x over Soufflé on TC). Problem 3.
   Kill test: Datalog can't express addresses (prefix sum over aligned sizes) without recursion over a successor
   chain: implement `addr(s,a) :- prev(s,p), addr(p,pa), size(p,z), a=align(pa+z)` on 360 k live symbols; if rounds =
   360 k deep the engine takes > 1 s (one iteration per symbol) -> dead; layout needs a host-side scan (functor
   or embedded), which is the point to document.
10. **Worst-case optimal / Free Join for multi-way resolve.** Idea: resolve with 3-way joins (ref -> name -> def -> ABI/dupok)
    avoids intermediate blowup; Free Join unifies hash tries and binary plans with lazy trie building.
    Source: Wang, Willsey, Suciu https://arxiv.org/abs/2301.10841 ; FlowLog structural optimizer. Problem 1.
    Kill test: intermediate result size of the binary-join plan on MinIO (ref x def-candidates before the winner rule):
    if max intermediate < 3x the final 2.7 M rows, WCOJ buys nothing (the key is nearly unique), drop it.
11. **Specialise the engine per program (Futamura-style synthesis).** Idea: Soufflé compiles a rule set to C++ with
    specialised btrees per relation/index arity, templates on arity; so rules are known at compile time.
    Source: Scholz, Jordan, Subotic, Westmann, "On fast large-scale program analysis in Datalog", CC 2016
    https://dl.acm.org/doi/10.1145/2892208.2892226 (title from memory; abstract not re-read) and "Soufflé: On Synthesis of
    Program Analyzers", CAV 2016. Problem 4 (shapes as templates, compiled into the program). Kill test: compile cost
    ~10 s per program here (hello); a codegen-by-shape column that needs a g++ run per link is dead if wall time of
    the whole link must be < 2 s. Measure `souffle -o` for a 4,096-shape program: kill if > 60 s.
12. **Codegen by shape as relation grouping.** `shape(e, s)` is a group-by over node signatures; emission = join
    (shape -> template bytes) then scatter with patched operands. Datalog gives grouping by `.functor` hash +
    aggregate `min`. Source: Soufflé functors/aggregates; Ddisasm (Datalog over binaries on Soufflé, symbolization of
    addresses, https://arxiv.org/abs/1906.03969). Problem 4. Kill test: assign 1.4 M expressions to 4,096 shapes via a
    Soufflé aggregate and hash functor; kill if it takes > 1 s (hlink-style radix grouping is ~10 ms).
13. **Incremental linking via differential dataflow.** DDlog/Differential Dataflow maintain results under input
    deltas: a one-package change should recompute only affected resolves/reach. Source: DDlog
    https://github.com/vmware-archive/differential-datalog (project; no paper read), FlowLog above. Problems 1-3.
    Kill test: change one of MinIO's 1,312 packages; kill if incremental update of the resolve+reach takes > 25 % of
    the from-scratch time or memory exceeds 5x the batch run (DDlog's known memory overhead).
14. **Datalog for binary analysis already scales on Soufflé** (Ddisasm: disassembly + symbolization as Datalog; Doop
    points-to on Soufflé; CSPA on httpd/Linux/PostgreSQL 13-58 s in Soufflé 32 threads per GDlog's Table 4, with only
    449-683 % CPU use of 32 cores). Supports "Datalog can be the right tool for analysis, wrong for the linker's
    hot loop". No linker/build-system written in Datalog was found in web searches (honest negative result).
    Kill test (for our claim): Soufflé's CPU utilisation on MinIO reach at -j8 must exceed 400 %; if it sits
    at ~100 % as in hello's BFS, the parallel-Datalog story is not worth pursuing for flood.
15. **Flix fixpoint engine (lattices in a functional language).** Source: Madsen, Yee, Lhotak "From Datalog to Flix",
    PLDI 2016 https://dl.acm.org/doi/10.1145/2908080.2908096 (not read). Problem 1 (lattice winner rule), 2.
    Kill test: JVM startup + warmup alone > 0.5 s on MinIO-size input => skip; only a measurement of the
    lattice-based winner on 2.7 M refs under 1 s would resurrect it.

## Takeaway for the grid
Soufflé's strengths: expressing the rules (esp. problem 2's conditional method rule) concisely and getting
parallel btrees for free. Its weaknesses, measured and cited: text input (9.4 s/10 M rows), per-iteration btree
merge cost, no parallel speedup on flood, no way to express a prefix-sum layout. Recommended outcome: Soufflé column
= problems 1 and 2 only via embedded bulk load (idea 1), with layout/relocate done in a host functor.
