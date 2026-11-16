# Independent review of C diagnostics

Reviewed `ref/flat/p1compact.c` and `results/review5-c-run.py` independently of
their author. No kernel computation or oracle comparison was removed/changed.
The optional excluded warmup goes through the existing join and target gather;
its timing is omitted by `if (rep < 0) continue`. The name-join sum and divisor
remain the measured `reps`. Single-thread profiling is explicitly enforced.

`perf stat -D -1` starts disabled. FIFO enable/ack completes before the name
join; disable/ack completes before the relocation-target loop. Thus the counters
exclude mapping, target gathers, content joins and checking. They include a
small amount of FIFO/getrusage/timer overhead around each join. The join timer
itself excludes FIFO acknowledgements, but the existing *total* and *target*
time include disable/ack time; use the new mean **join** and first-join fields
for this experiment, not target/total timings.

The runner performs five rotating interleaved rounds of four combinations, pins
CPU 6, sets one OpenMP thread and FLAT_HUGE=0, and explicitly sets/clears the glibc
tunable. Input mappings use MAP_POPULATE in flat.h: "cold" here refers to fresh
malloc-backed kernel outputs/workspace, not cold disk reads. A cold process has
**one cold join and 63 reused joins**; its counter average must not be called
64 cold joins. The first-join timer/fault count separates that first-touch cost.
The warm process performs one full excluded iteration: it both prefaults and
warms caches, not an isolated prefault-only treatment.

Smaps sums AnonHugePages, Private_Hugetlb and Shared_Hugetlb within the actual
process before its buffers are freed. That verifies huge-page backing; the
before/after process meminfo snapshots alone cannot, since allocation is freed
at exit. Round 1 independently inspected: baseline reports all three as zero;
hugetlb reports Private_Hugetlb=192512 kB. Counters are not multiplexed (100%).
The generic dTLB-load-misses and AMD all_l2_miss values are exactly identical
in these records (7,420,338 baseline / 2,726,248 hugetlb); treat them as aliases,
not independent corroborating mechanisms. Page faults agree exactly with
getrusage for these records: 28,408 / 72. No claim should attribute all remaining
warm speed differences solely to TLB misses without the five-round results.

Robustness limitations outside the controlled runner: specifying P1_PERF_CTL
without P1_PERF_ACK was not validated before fopen at review time (now fixed); FIFO reads assume a complete
ack in one read. The runner always supplies both paths and the collected logs
show successful acknowledgements. These do not invalidate this measurement.

Independent execution after the main counter measurements: ran the built
instrumented executable for one measured repetition, with diagnostics enabled
but no perf FIFO, in all four tunable/warmup combinations. All four pass canon,
builtin, relocation targets and content-canon comparisons and print P1 OK.
Raw output: `results/review5-c-smoke.log`. Fresh baseline/hugetlb measured
28,407/72 minor faults; both warmed variants measured zero. Smaps independently
reproduced 0 versus 192,512 kB Private_Hugetlb. These are smoke checks, not an
additional timing comparison (single repetitions cannot establish speed).
