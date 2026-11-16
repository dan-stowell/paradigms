# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T16:23Z. Campaign 1, M0 done, M1/M2 in flight: five builders resumed (Soufflé, Futhark, Halide, Goose, DuckDB); 0/20 cells merged yet.
**2 ATTACKS entries backed by measurement.** 1: reach, decoding relocation columns inside the flood is 2x the eager decode+CSR+flood
(52.6 / 13.6-15.4 vs 108.7 / 30.1 ms; Codex reproduced 2.1-2.6x); the hlink ratio is withdrawn (not equivalent work).
2: relocate on 2 MB output pages, 24 vs 49 ms on 1 thread and **5.4-6.8 ms on 8 = 1.1-1.4x the 4.92 ms floor** (14 on 4 KB pages).
Next lead hypothesis (Codex testing): resolve's join is bound by 4 passes over the dense 83 MB key array, of which 15% is keyed.
VM bandwidth measured: 25 GB/s read on 1 thread, 36 on 8 (FLOOR.md assumes more; floors here are optimistic by ~2x).

## Decisions for Dan
- None. (PLAN.md's campaign text had been truncated by an agent's Now-line edit; restored from git at 16:22Z.)
