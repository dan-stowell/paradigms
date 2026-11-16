# P2 asynchronous flood — codex-g

2026-11-16, measured 16:25:41–16:28:30 UTC on this VM. **The asynchronous
worklist avoids the active-wait collapse. Against passive waits it is within
noise under ambient load, and faster in the deliberately overloaded block.**
The claim “no slower when quiet” is unproven: this was not a quiet-machine test.

## Implementation and correctness

`ref/flat/p2async.c` uses pthreads, private LIFO stacks, and per-worker mutexed
mailboxes. A worker publishes half its private stack when it exceeds 64 items,
capped at 256 items; an empty worker takes a whole published chunk, starting
with its own mailbox and then checking the other workers. Published chunks
are the stealable part of each worker's stack. `steals` counts successful
cross-worker transfers, not empty probes or reclaiming one's own chunk.
Idle workers sleep 50 microseconds; mutexes and joins block. There is no
OpenMP runtime in this executable. `OMP_NUM_THREADS` selects its thread count.
Thread creation/join costs are included in the timed kernel.

Termination uses outstanding **credits**, an equivalent scheme to pending
items: each active private stack (including its currently executing item) and
each published chunk owns one credit. Publication adds a credit before exposing
the chunk; stealing transfers the credit; a worker retires its credit only
after all its private work finishes. Thus zero means neither queued nor
executing work can generate children. Main joins all workers before inspecting
method candidates or resetting state. There is one completion phase per round,
seven per solve, and no per-level barriers.

The README rules are positive/monotone, but monotonicity alone is not a queue
correctness proof. A newly live item is queued; a later iface transition queues
an already-live type again. `ifaceDone` admits exactly one interface-aware
expansion, including its method candidates; any redundant pop may skip because
the owner must finish before termination. The plain-edge/interface propagation,
reflection, signatures, names, and serial method phase are copied from p2par.
`sigSeen`'s 1→2 is bookkeeping for an already-seen signature, not removal of a
fact. Method targets of zero retain p2par's explicit guard.

Flag LD/ST1/tas remain relaxed. The live/iface two-flag handshake also needs
store-to-load ordering: on this x86 machine the exchange compiles to locked
`xchg`; non-x86 builds add a sequentially consistent fence after the exchange.
This addresses a portability caveat that the baseline's relaxed accesses alone
do not address. Non-x86 execution has not been tested.

**120/120 separate executions printed P2 OK**, with one repetition each:
both datasets × 1/2/8 threads × 20 runs. Every sorted live ID matched the oracle:
663,630 MinIO; 926,486 kube-apiserver. See `codex-g-validation.log`.
An independent review compiled and checked all six dataset/thread combinations;
see `codex-g-review.md`. The complete check.sh suite passed on both datasets (including P3 pipeline),
recorded separately in `codex-g-check.log`.

## Measurement method

GCC 13.3.0, `-O2 -march=native`; baseline `-fopenmp`, async `-pthread`.
Linux 6.12.115, x86_64 AMD EPYC 9554P, eight VM CPUs, affinity 0–7, no pinning.
All builds/tests/benchmarks and load generators ran under
`prlimit --as=8589934592` (8 GiB). Data stayed read-only through the existing
`data` symlink. Executables are in `build-g/`.

Five interleaved process runs per variant in each block, rotating variant order.
Each process repeats the kernel enough to accumulate at least one second of
**timed kernel work**; short attempts are retained as `short-retry` records and
excluded from the table. The table uses each process's total kernel time divided
by repetitions, then min/median/max across its five runs, not its best iteration.
Mapping, state clearing, and oracle checking are outside the timed region,
matching p2par's existing boundary; whole-process wall times are also in the raw
log. Only the last repetition is oracle-checked in these timing runs; the 120
one-repetition checks above verify every execution. Counts likewise describe
the final repetition, not a sum over benchmark repetitions.

Background jobs continued throughout. The first block also overlapped the end
of the correctness matrix. The extra-load block added eight independent busy
Python loops, each capped by `timeout 180s`, for the entire 39-second block and
terminated them afterward. The async binary ignores OMP_WAIT_POLICY; the harness
sets passive for it anyway. Baseline active/passive are explicitly set.

## Times (milliseconds per complete flood + method solve)

| Dataset/load | Threads | p2par active min/median/max | p2par passive min/median/max | async min/median/max | Reps/process active/passive/async |
|---|---:|---:|---:|---:|---:|
| minio ambient | 1 | 34.47 / 36.86 / 38.19 | 34.03 / 36.91 / 53.17 | 37.37 / 38.87 / 43.80 | 43 / 42 / 38 |
| minio ambient | 8 | 592.71 / 953.49 / 1070.92 | 15.20 / 21.40 / 26.61 | 16.65 / 19.93 / 30.26 | 2 / 91 / 81 |
| kube-apiserver ambient | 1 | 48.19 / 52.05 / 83.35 | 45.52 / 52.17 / 62.51 | 50.96 / 55.09 / 79.81 | 24 / 31 / 27 |
| kube-apiserver ambient | 8 | 856.11 / 940.07 / 1064.27 | 27.95 / 33.49 / 102.83 | 30.34 / 34.38 / 81.70 | 2 / 49 / 39 |
| minio + 8 busy workers | 8 | 2079.27 / 2289.64 / 3509.66 | 105.90 / 152.71 / 156.13 | 81.30 / 89.40 / 93.40 | 1 / 17 / 19 |

All 1-thread comparisons overlap: within noise. At 8 threads under ambient
load, active vs async ranges do not overlap (median ratios 47.8× MinIO and
27.3× kube); passive vs async overlap: within noise. With eight extra busy
workers, active vs async is 25.6× at the medians, and passive vs async is 1.71×;
async's maximum 93.40 ms is below passive's minimum 105.90 ms. This is evidence
for removing level synchronization under oversubscription, not a claim that
async eliminates resource contention or always beats passive OpenMP.

## Mechanism counts

One barrier means one team completion phase. Async has zero literal barrier
calls, but seven pthread join phases; p2par has 111 MinIO / 103 kube OpenMP
parallel-region exit barriers across seven rounds. These counts are per full
solve, not per benchmark process. At one thread these are logical phase counts,
not waits on peers. Baseline has no explicit work-stealing counter (its OpenMP
runtime dynamically schedules frontier slices).

| Dataset/load | Threads | p2par active pops | p2par passive pops | async pops | async steals | barriers p2par / async |
|---|---:|---:|---:|---:|---:|---:|
| minio ambient | 1 | 678450 | 678450 | 672505 | 0 | 111 / 7 |
| minio ambient | 8 | 677652–678174 | 677673–678258 | 672044–672607 | 233–307 | 111 / 7 |
| kube-apiserver ambient | 1 | 945245 | 945245 | 945046 | 0 | 103 / 7 |
| kube-apiserver ambient | 8 | 938869–939307 | 938962–939143 | 936991–944348 | 294–446 | 103 / 7 |
| minio + 8 busy workers | 8 | 678102–678306 | 678078–678208 | 671646–672711 | 160–222 | 111 / 7 |

**Identical live sets do not imply identical pop counts.** A type may be popped
as live and later again as iface, or both facts may arrive before its first pop.
A second queued item can also become a redundant ifaceDone pop. LIFO vs level
order changes this even on one thread; parallel scheduling adds variability.
The table counts every pop, including undefined symbols and redundant pops.
The invariant checked is the complete live set, not accidental queue order.

## Load record (`uptime` before/after every block)

```
minio T=1 extra=False before 2026-11-16T16:25:41Z
  16:25:41 up  1:10,  0 user,  load average: 6.16, 7.88, 5.56
minio T=1 extra=False after 2026-11-16T16:26:09Z
  16:26:09 up  1:10,  0 user,  load average: 5.61, 7.59, 5.54
minio T=8 extra=False before 2026-11-16T16:26:09Z
  16:26:09 up  1:10,  0 user,  load average: 5.61, 7.59, 5.54
minio T=8 extra=False after 2026-11-16T16:26:41Z
  16:26:41 up  1:11,  0 user,  load average: 6.23, 7.54, 5.58
kube-apiserver T=1 extra=False before 2026-11-16T16:26:41Z
  16:26:41 up  1:11,  0 user,  load average: 6.23, 7.54, 5.58
kube-apiserver T=1 extra=False after 2026-11-16T16:27:10Z
  16:27:10 up  1:11,  0 user,  load average: 7.18, 7.63, 5.68
kube-apiserver T=8 extra=False before 2026-11-16T16:27:10Z
  16:27:10 up  1:11,  0 user,  load average: 7.18, 7.63, 5.68
kube-apiserver T=8 extra=False after 2026-11-16T16:27:51Z
  16:27:51 up  1:12,  0 user,  load average: 10.00, 8.30, 5.99
minio T=8 extra=True before 2026-11-16T16:27:51Z
  16:27:51 up  1:12,  0 user,  load average: 10.00, 8.30, 5.99
minio T=8 extra=True after 2026-11-16T16:28:30Z
  16:28:30 up  1:12,  0 user,  load average: 16.84, 10.27, 6.75
```

## Reproduce

```sh
prlimit --as=8589934592 -- bash -c '
mkdir -p build-g
gcc -O2 -march=native -fopenmp -o build-g/p2par ref/flat/p2par.c
gcc -Wall -Wextra -O2 -march=native -pthread -o build-g/p2async ref/flat/p2async.c
for d in minio kube-apiserver; do
  for t in 1 2 8; do
    for r in $(seq 1 20); do
      OMP_NUM_THREADS=$t build-g/p2async data/$d 1 || exit
    done
  done
done'
prlimit --as=8589934592 -- python3 results/codex-g-bench.py
prlimit --as=8589934592 -- env OMP_NUM_THREADS=8 ref/flat/check.sh data/minio
prlimit --as=8589934592 -- env OMP_NUM_THREADS=8 ref/flat/check.sh data/kube-apiserver
```

Raw samples including pilots, retries, wall times, counts, and load:
`codex-g-bench.jsonl`. Re-running the script overwrites that log. The baseline
source change only adds total/mean timing and counts; its scheduling/rules are
unchanged. `check.sh` now includes p2async and defaults to build-g.
