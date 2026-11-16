# GraphBLAS: ideas a measurement suggests

1. **The method rules are conjunctions, and a conjunction is what forces rounds.** Plain reach is one masked BFS
   (`frontier<!live> = frontier any.pair A0`). Every other rule fires on an AND of two facts that grow
   independently: `live(s) & iface(s) & gotype(s)` (the iface closure), `cand(m) & (sig | named | reflect&exported)`
   (the method check). A semiring product propagates one fact along edges; an AND of two growing sets has to be
   re-evaluated when either grows, so it becomes an outer round. Evidence: p2gb needs 97 BFS levels + 81 iface
   levels in 7 rounds, 382 vxm/mxv calls (MinIO), against p2par's 111 levels in 7 rounds, where both facts are
   tested inline per visited node. Kill test for a fix: encode (node, iface) as a second plane of a 2N-node graph
   so the iface closure is reachability too; only the `live & iface` join of the two planes stays a round. If the
   round count does not drop below 7 or the call count below ~200, the conjunctions are irreducible in this form.

2. **Per-level work is below GraphBLAS's parallel grain.** The average frontier is ~7k nodes (663k live / 97
   levels); SuiteSparse picks threads per call from the work (`GxB_CHUNK`, default 64k), so almost every vxm runs
   on 1 thread (burble, 8 threads: 379 of 382 vxm/mxv calls). Wall time 192 / 188 / 187 / 173 ms at 1 / 2 / 4 / 8
   threads (load ~7), while CPU time grows 192 -> 256 ms; lowering the chunk to 8192 doubles CPU time (400 ms) for
   no wall-time gain (161-223 ms; at load ~18 it was 2-3x slower).
   The flood is not data-parallel at the grain GraphBLAS can use; the parallelism that exists is across levels
   (ATTACKS 3's asynchronous worklist), which a bulk-synchronous library cannot express.

3. **Direction optimisation does not apply to this graph.** Pull (`mxv` on A0', mask = unvisited) is 40x slower
   than push (11.5 s vs 0.26-0.89 s, 1 thread): only 663k of 5.2M nodes are ever live, so the unvisited set the
   pull scans is always ~8x larger than the frontier. Beamer's switch (pull when frontier edges > unvisited/14)
   never triggers. The linker's flood is a sparse reach in a mostly dead graph, the opposite of the social graphs
   push/pull was made for.

4. **Resolve's join as a merge of sorted lists.** 1a's group-by is a vector *build* with dup=MAX (a sort), and the
   join back is either `extract` (a hash lookup per member into a hypersparse 2^60 vector) or `w any.first G` with G
   the key x slot incidence matrix (a merge of two sorted lists). They tie (1a 1.0-1.3 s vs 1.1-1.3 s, 1 thread,
   load ~13). Profile (1 thread, JIT): builds ~250 ms per 1a/1c, the join-back products ~180-190 ms each (SuiteSparse
   hashes G's hypersparse rows first), and whole-vector passes (at least 44 of the 141 calls of a P1 solve read a full 5.2 M- or 9.4 M-entry vector, 15-30 ms each). The C does one
   hash table and three passes (p1par 1a: 49-54 ms on 1 thread). Kill test for a better GraphBLAS 1a: densify the
   key first (one build to get the sorted unique keys, their positions as dense group ids), so the join back is a
   gather from a dense vector, not a product with a 2^60-row matrix; kill if 1a stays above 10x p1par.
