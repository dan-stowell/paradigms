# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T18:55Z. **M4 done** (problem 4 correct in Futhark, Halide, Goose + tuned C). Oracle number: **19/20 cells**
(+ DuckDB 3, GraphBLAS 2), **4 ATTACKS backed**. Only Soufflé P4 is missing.
Problem 4 is the paradigms' best row: Goose ties OpenMP C at 8 workers, Halide 1.3x, Futhark 1.9x on a flattened stream.
ATTACKS 4 (corrected same hour): emit costs lookups per piece, not bytes or pieces; flattening = 2x C, 4.5x Futhark;
after that the 8 M holes are the cost. Width-relaxed templates: 108,833 -> 66,046.
Next: Soufflé P4 (20/20), a Halide batched-level flood (M3 review), hole-cost attack (fewer holes per byte), quiet reruns.

## Decisions for Dan
- None. (vm.nr_hugepages is 1536 now; not persistent across a reboot.)
