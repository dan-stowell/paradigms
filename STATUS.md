# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T20:20Z. **Grid full: 20/20 cells** (+ DuckDB 3, GraphBLAS 2, Differential Dataflow 2), **5 ATTACKS backed**.
**Surprise, up: relinking after a real edit.** A 5-line MinIO edit changes 31 symbols of 9.7 M facts; a C maintained
flood (ref/flat/p2inc.c) updates reach in **3 us** (vs 43-45 ms recompute); a deleting edit (12 K symbols die) in
5.5-9.5 ms. Textbook delete-and-rederive is 3x slower than recompute; derivation-order stamps fix it. ATTACKS 5.
Prerequisite: stable symbol identity (positional ids shift on any edit; recovering identity costs 0.7-1.1 s today).
Also today: Soufflé P4 (set semantics dropped 37,703 duplicate relocations), Halide flat-stream P4 3.8x, hole cost = traversal.
Next: layout + relocate after an edit (do addresses have to move?), quiet reruns.

## Decisions for Dan
- None. (vm.nr_hugepages is 1536 now; not persistent across a reboot.)
