# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T19:35Z. **Grid full: 20/20 cells** (+ DuckDB 3, GraphBLAS 2), **4 ATTACKS backed** (3 and 4 extended today).
Soufflé P4 correct but ~70x / ~130x C; its finding: set semantics silently merged 37,703 duplicate relocations.
Every P4 cell is also correct on kube-apiserver. Halide batched levels: 1.0-1.2x slower (per-level passes are the cost).
P4 hole cost is record traversal, not stores: baking integers into prepared blobs 1.5x; merging stores is noise.
Running: Differential Dataflow (new column: incremental relink after a real edit), Halide on the flattened P4 stream.
Skeptic fired on 5e179fe.

## Decisions for Dan
- None. (vm.nr_hugepages is 1536 now; not persistent across a reboot.)
