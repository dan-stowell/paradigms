# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T18:40Z. **M3 done; M4 half done** (Futhark P4 correct; Halide P4 building). Oracle number: **17/20 cells**
(+ DuckDB 3, GraphBLAS 2), **4 ATTACKS backed by measurement**. All P1-P3 paradigm cells also correct on kube-apiserver.
New: ATTACKS 4 (emit costs pieces, not bytes: 0.42 GB/s, patching 7.6%; width-relaxed templates 108,833 -> 66,046).
ATTACKS 3 corrected in the paradigms: level count is not Futhark's cost (per-level passes are); async helps Goose 1.3-1.5x.
Soufflé 3a no longer falls down (ranked chain, 1.9 s); DuckDB P2 falls down after three attempts; GraphBLAS: 9-24x C, no scaling.
Next: Halide P4 merge (M4), then a third P4 paradigm, supernodes (CP7), quiet-machine reruns of 8-thread numbers.

## Decisions for Dan
- None. (vm.nr_hugepages is 1536 now; not persistent across a reboot.)
