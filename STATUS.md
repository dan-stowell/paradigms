# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T21:00Z. **Grid full: 20/20 cells** (+ DuckDB 3, GraphBLAS 2, Differential Dataflow 2, GraphIt 1), **5 ATTACKS backed**.
**Surprise, up: a relink after a real edit can be maintained in every phase measured so far, deletions included.** In C, 1 core,
deleting edit (12 K live ids die): identity 51 ms (was 0.7-1.1 s in DD), reach 6.5-10 ms from a fresh state (witnesses + BFS
stamps; recompute 17-20 ms on 8 threads), relayout 4.9 ms vs 87 ms (no compaction: holes + 1.8 MB slack, 0 addresses
move). The 5-line addition: µs everywhere. Corrected on the way: fresh-state reach was 2x *slower* than recompute until
witnesses (skeptic-inc). Caveats: each phase's diff is prepared outside its timer; the image is checked, not run.
Next: maintained resolve (builder running), then one end-to-end relink number per edit (Codex), quiet 8-thread reruns.

## Decisions for Dan
- Layout without compaction changes the runtime's typelinks format (a sorted offset array beside stable descriptors).
