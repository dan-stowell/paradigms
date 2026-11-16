# STATUS (at most ten lines; updated at every milestone and whenever Dan is needed)

2026-11-16T16:04:04Z. Campaign 1, M0 done, M1/M2 in flight: builders for Soufflé, Futhark, Halide, Goose, DuckDB (new column) and P4 data.
Oracle number: 0/20 paradigm cells merged yet (several correct on branches), **1 ATTACKS entry backed by a measurement**.
Surprise (ATTACKS 1): the pre-decoded graph was the wrong shape for reach. The flood reads 21-29% of its edges; eager
decode+CSR+flood is 108.7 / 30.1 ms (1/8 threads), decoding relocation columns inside the flood is 52.6 / 13.6 ms,
vs hlink's raw-object flood 98-103 / 22-23 (caveats in ATTACKS.md). kube-apiserver is the second dataset (all C refs OK).
C refs now complete: 1c partitioned join 7.2-8.9 ms on 8 threads (hlink 8-9); P3 runs as a real 3a -> 3b pipeline.

## Decisions for Dan
- None yet. (DuckDB added as a 6th column before M2: JOURNAL 15:58Z says why.)
