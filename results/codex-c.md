# P1c partitioned join

`p1c.h` is shared by `p1.c` (ordinary serial loops) and `p1par.c` (OpenMP).
Count and scatter marked kind-2/3 records into 256 radix partitions using the
high eight bits of a mixed content key. Each partition uses a separate small
linear-probing table storing record indices. Kind is part of equality; kind 2
ignores size for grouping but uses it for winner selection. The reduction
explicitly prefers mark 1, then largest kind-2 size, then lowest id, independent
of execution order. Mark-2-only groups and oversized mark-2 short copies retain
the incoming name canon. Only disjoint slot ids are written by each partition.

This replaces plain P1c's 16,777,216-entry / 402,653,184-byte hash table with a
compact record array plus small per-partition tables. Both retain all oracle
checks. `p1c_test.c` independently enumerates every pair for 100 generated cases
of 1000 slots; it passes with OMP_NUM_THREADS=1 and 8, including zero keys,
cross-kind collisions, mark precedence, size exceptions, and id tie-breaking.

## Measurements

All builds: `gcc -O2 -march=native`, plus `-fopenmp` for p1par/test. All runs:
`prlimit --as=8589934592` (8 GiB; explicit bytes avoid prlimit suffix ambiguity).
Input: `/home/exedev/paradigms/data/minio`. Every measured process returned
`P1 OK`, with zero mismatches for 5,205,428 final canonical ids and all 1a/1b
oracles. Baseline source is `f0b912fbc677d13e04e70a3c513009db7019f3b7:ref/flat/p1.c`, compiled
with `-I ref/flat` before editing; executable `/tmp/codex-c-p1-before`.

Fresh-process measurements interleaved before, after, par1, par8, five times;
parallel command uses repetition argument `1`. Timed region includes count,
scatter, allocation/free, table joins and output writes; files are mapped before
the region. OS page cache is warm but each process pays its own allocation and
first-touch costs. Date stamps are from `date -u +%Y-%m-%dT%H:%M:%SZ`.

| Repetition | before UTC / ms | after UTC / ms | par1 UTC / ms | par8 UTC / ms |
|---|---|---|---|---|
| 1 | 15:57:01 / 328.7 | 15:57:01 / 67.2 | 15:57:02 / 57.4 | 15:57:02 / 25.0 |
| 2 | 15:57:02 / 345.3 | 15:57:03 / 57.0 | 15:57:03 / 57.9 | 15:57:03 / 23.2 |
| 3 | 15:57:04 / 315.7 | 15:57:04 / 60.9 | 15:57:04 / 57.0 | 15:57:05 / 26.8 |
| 4 | 15:57:05 / 394.7 | 15:57:06 / 62.6 | 15:57:06 / 62.0 | 15:57:06 / 20.1 |
| 5 | 15:57:06 / 302.9 | 15:57:07 / 61.0 | 15:57:07 / 61.0 | 15:57:08 / 21.3 |
| Best | **302.9** | **57.0** | **57.0** | **20.1** |

All dates 2026-11-16. Plain before/after improves 5.31x in this window.

Warm in-process best-of-five (the program's normal benchmark convention):

- 2026-11-16T15:57:20Z: `OMP_NUM_THREADS=1 prlimit --as=8589934592 build/p1par /home/exedev/paradigms/data/minio 5`: **36.9 ms** P1c.
- 2026-11-16T15:57:21Z: same with `OMP_NUM_THREADS=8`: **7.2 ms** P1c.

These warm numbers are comparable in magnitude to the supplied hlink 37–39 ms
and 8–9 ms, but are not a same-window hlink remeasurement. The flat inputs have
already decoded/prehashed work hlink performs itself, so this is not a claim of
an end-to-end hlink speedup. Other jobs were running throughout.

Reproduce independent test:

```sh
gcc -O2 -march=native -fopenmp -o build/p1c_test ref/flat/p1c_test.c
OMP_NUM_THREADS=1 prlimit --as=8589934592 build/p1c_test
OMP_NUM_THREADS=8 prlimit --as=8589934592 build/p1c_test
```

# P3 pipeline: blocked by extraction contract

2026-11-16T15:59:09Z (`date -u`): implemented `build/p3 DATA --pipeline`.
It scatters its own 3a addresses by placed id and gathers through
`oracle/p1c_canon.u32`. This is the final resolve mapping, including 1a and 1c;
1b computes targets without changing canon. Generated ids outside nslots and
all unplaced canonical ids initialize to zero. No address oracle contributes
to the computed result. At the very end both address arrays are compared.

The requested passing pipeline is **not achieved** with these read-only inputs.
The README's claim that addr_all is merely resolved image addresses is false:
`ref/hlink/layout.c:addrUnit` stores PC-table offsets for stPC symbols in the same
array; `otherSlots` propagates final canon. `special.c:makeSpecials` and
`layout.c:place` assign overridden and synthesized symbols separately, and
`defineSyms` supplies section-boundary addresses. `dumpflat.c` exports this raw
array but omits those symbols from the p3 placed list. Their layout inputs
(string values, init task lists, special sizes/order and PC metadata) are not
fully represented in the flat contract. Reading p1_canon as well cannot supply
these missing addresses.

Measured evidence:

- 360,315 computed placed addresses: all match oracle/p3_addr.u32.
- All 5,205,435 ids initialized/computed; 661,018 differ from raw p3_addr_all.
- Exactly seven distinct relocation targets have missing addresses:
  runtime.buildVersion, runtime.modinfo, runtime.godebugDefault,
  runtime.firstmoduledata, runtime.lastmoduledatap, runtime.runtime_inittasks,
  go:fipsinfo. These cause 81 relocation errors and 40 differing symbol ranges.
- Isolated original P3 still passes (0 errors, 0 differing ranges).
- `OMP_NUM_THREADS=8 prlimit --as=8589934592 bash ref/flat/check.sh
  /home/exedev/paradigms/data/minio` passes all existing references, including
  new P1c. This script does not claim the new pipeline passes.

Reproduce the failed pipeline and verify address-oracle access occurs only at
final comparison:

```sh
gcc -O2 -march=native -o build/p3 ref/flat/p3.c
prlimit --as=8589934592 strace -o /tmp/p3.trace -e trace=openat,write \
  stdbuf -oL build/p3 /home/exedev/paradigms/data/minio --pipeline
rg -n 'p3_addr|p3b|image:' /tmp/p3.trace
```

The trace has writes for completed 3b and image comparison at lines 76-77,
then opens oracle/p3_addr.u32 at 78 and p3_addr_all.u32 at 81. Exit status is 1;
it prints `P3 pipeline FAIL`, not a false success. Image comparison covers the
placed symbol ranges, as the contract requires; the ELF headers and other
linker-generated image bytes remain outside this problem.

Needed next step: extend extraction with special-symbol layout records
(id/size/alignment/order/section start), and non-image PC-table layout inputs
if every raw addr entry must match. This requires extending/re-extracting the
read-only dataset. No oracle-derived constants or image-derived addresses were
used. A clarification to permit that extension is pending.
