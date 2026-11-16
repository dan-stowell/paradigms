# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T15:40Z. Campaign 1, **M0 reached**: MinIO's flat files and oracles for problems 1-3, checked by plain C
references that reproduce every oracle exactly; hlink here: 149 ms whole link on 8 cores. Oracle number: 0/20 cells, 0 attacks.
In flight (M1/M2): Opus builders for Soufflé, Futhark, Halide, Goose, each doing problems 1-3.
Early surprise to chase: a plain 1-core worklist flood over the flat (pre-decoded) edges takes 41-46 ms; hlink's tuned flood
over raw objects takes 98-103 ms on 1 core. The decode, not the graph walk, is most of reach.

## Decisions for Dan
- None yet.
