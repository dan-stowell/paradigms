# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T17:22Z. **M1 + M2 done**: Goose, Futhark, Halide, Soufflé and DuckDB are each correct on problems 1-3 (lead-verified).
Oracle number: **16/20 cells + DuckDB 3; 3 ATTACKS backed by measurement.** No paradigm beats tuned C on any cell (Halide is
closest, 1.2-2.5x; Soufflé furthest, 30-200x); the distances and their causes are in GRID.md and MORNING.md.
ATTACKS 1: decode in the flood, 2x the eager graph build (C). 2: huge pages: relocate at the VM's bandwidth bound
(5.4-6.8 ms on 8), and `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` gets it with no code. 3: drop level sync: 111 levels cost
every paradigm; a worklist flood is 1.7x passive OpenMP under overload, 25x default spinning.
Next (M3/M4): problem 4 in two paradigms, kube-apiserver for paradigm cells, second attempts on falls-downs, GraphBLAS.

## Decisions for Dan
- None. (vm.nr_hugepages is 1536 now; not persistent across a reboot.)
