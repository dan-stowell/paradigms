# onlyB implementation and validation

`ref/flat/p1onlyB.c` retains p1par's two dense passes: count initializes canon and
counts members and candidates; scatter rereads dense keys/kinds and emits separate
24-byte record and 4-byte slot arrays. The two histograms share one per-chunk
allocation, with independent prefix sums and destinations. Wanted names remain
members only. Candidates never create table entries.

Each partition owns table zeroing/build, candidate maximum, candidate readback,
and member readback. Therefore candidates hit their own partition table while it
is hot, no atomic maximum is needed, and only a final partition-loop barrier is
needed before builtins/relocations. Member table capacities and insertion order
are unchanged; baseline member-readback code is retained. No compact staging pass
or scratch records are added. Partitioning necessarily replaces the two global
dense candidate passes with compact partition lists; this is intrinsic to B.

Validation (all commands limited to 8 GiB virtual memory):

```sh
prlimit --as=8589934592 gcc -O2 -march=native -fopenmp -o build-h/p1onlyB ref/flat/p1onlyB.c
prlimit --as=8589934592 env OMP_NUM_THREADS=1 OMP_WAIT_POLICY=passive build-h/p1onlyB data/minio 1
prlimit --as=8589934592 env OMP_NUM_THREADS=1 OMP_WAIT_POLICY=passive build-h/p1onlyB data/kube-apiserver 1
```

Both final-version runs exited successfully and printed `P1 OK`. Canon, builtins,
relocation targets and canon after 1c each had zero oracle differences on each
dataset. The one-repetition runs check correctness, not performance.

## Independent static review of onlyA and phase instrumentation

Reviewed final `ref/flat/p1onlyA.c` and `results/codex-h-phases.py` while the main
benchmark window ran; no additional benchmark or build was started.

onlyA correctly emits each keyed kind-1 candidate once into its chunk prefix,
retains original order within each chunk, and never scatters candidates by hash
partition. Both candidate traversals use global FIND; the max traversal retains
baseline atomic load/CAS, and intervening OpenMP barriers make readback safe.
Members retain compact's staging/scatter and all-partition build precedes global
candidate probes. Scratch intervals cannot overlap: chunk 0 alone gets the wanted
prefix, later member chunks start at their dense offset plus nw; each candidate
chunk starts at its dense offset and emits at most its dense length. Partition
member offsets/counts are independent and fit allocated N+nw capacity. No
correctness or A/B isolation defect found.

Experimental interpretation: A includes compact records (32 bytes), eager rank
loads for candidates, and candidate chunk scheduling; B includes ownership,
fused member readback, eliminated CAS/barriers, and candidate partitioning.
These intermediates isolate the two implementation packages, not individual
cache effects or every instruction. onlyB necessarily gathers candidates during
count/scatter and thus avoids the old global dense candidate scans as part of B.
A and B need not have additive speedups.

Phase instrumentation correctly charges (0) dense count/emission and initial
histogram clear, (1) prefix+scatter, (2) table allocation/clear/build, (3) candidate
maximum, (4) candidate readback, (5) member readback+builtins. Baseline clocks
span the OpenMP loops; compact uses per-partition clocks without splitting the
fused loop, so it preserves table locality. The script enforces one thread;
shared pt/phase would otherwise race. Markers select the intended occurrences;
content-join timers are untouched. No timer-placement defect found.

Small accounting limitation: each marker calls now twice, excluding the gap
between calls; compact also excludes dispatch/gaps before each partition's
fresh pt and after the last partition. Therefore phase sums may fall slightly
below full join time, and instrumentation adds many clock calls to compact.
Report phase timings as diagnostic measurements and compare their sum to the
instrumented join, rather than asserting exact equality or counter-derived
cache-miss/DRAM attribution.
