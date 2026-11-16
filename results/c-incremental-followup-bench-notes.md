# Benchmark follow-up details

`ref/flat/bench-p2inc.sh` now times recomputing each direction's destination:
edit B for A->B, original MinIO A for B->A. Its cap is 8 GiB and temporary
files are inside the clone's build directory. It retains DD comparisons.

The focused reproduction command is:

```
python3 ref/flat/bench-p2inc-followup.py 5
```

This builds C executables locally and runs five interleaved rounds. Each edit
and treatment starts a new process: fresh A (one round trip), fresh B
(`FRESH_B=1`, one round trip), five toggles, five cache-evicted toggles
(`EVICT=1`). Only rep 0 in the appropriate direction counts as a fresh update.
The repeated sample excludes rep 0. Each direction's p2 and p2par destination
recompute follows the maintained treatments within each round. p2par reports
its existing best-of-five kernel timing (not a single cold flood). The actual
commands and uptime/date observations are in the raw log; all child benchmark
processes have address-space limit 8589934592 bytes.

Summary columns include update times and work counters (retracted live IDs and
reverse entries read), each as min/median/max. Fresh treatments have n=5 per
edit/direction; repeat treatments have n=20. Cache displacement occurs outside
the timer. Raw stdout includes every oracle result, setup, initial flood, and
update count.

Measured 2026-11-16T20:30:29Z–20:31:20Z; one-minute load observations
3.14–4.18. All 280 timed samples and all reported full-live-set oracle checks
passed. These results replace the preliminary run after dynamic overlay
storage growth and direct fresh-B fact installation were implemented.

| Treatment (ms min/median/max) | edit1 A->B | edit1 B->A | edit2 A->B | edit2 B->A |
|---|---|---|---|---|
| Fresh derivation state, new process | 0.035/0.038/0.049 | 0.026/0.031/0.041 | 20.979/23.581/33.811 | 99.367/105.433/116.229 |
| Repeat toggles | 0.002/0.009/0.020 | 0.004/0.012/0.022 | 10.582/13.550/47.584 | 5.737/8.101/23.076 |
| Repeat after touching 1 GiB | 0.011/0.013/0.015 | 0.016/0.019/0.046 | 12.327/14.119/20.225 | 7.321/8.118/11.150 |
| p2 destination recompute | 40.300/49.300/63.000 | 40.600/44.100/57.900 | 40.400/45.200/56.400 | 40.100/45.700/55.600 |
| p2par 8 threads, best-of-five | 17.300/18.000/20.600 | 18.200/20.800/22.700 | 16.700/21.500/26.200 | 15.800/17.000/23.600 |

Fresh edit2 A->B retracts 32,238 live IDs and reads 3,049,918 reverse entries;
repeats retract 20,412 and read 2,257,483 on rep1, then 2,253,023. Eviction
changes no work counters. Repeat median is 13.550 ms versus 14.119 ms after
cache displacement; the ranges overlap. Cache timing differences are within
noise by the charter's rule in every edit/direction in this loaded window;
history's reduction in work is unambiguous. A claim that first-vs-repeat
improvement is solely cache warmth is false regardless of noisy timing.

Fresh edit2 B->A retracts 35,378 IDs and reads 49,171,996 reverse entries;
repeats retract 8,102 and read 1,086,145 on rep1 then 1,086,132. The fresh-B
update (99.367/105.433/116.229 ms) is slower than serial recompute A
(40.100/45.700/55.600 ms), with nonoverlapping ranges. A blanket claim that
maintained reach beats recompute in both directions is false for this
initialization. Fresh B is freshly flooded from A's CSR with B's overlay;
its scratch flood seeds roots in ascending ID order, whereas A's initial flood
uses file root order. Stamps depend on flood traversal order as well as history.
It is not a measurement of freshly indexing compact B CSR. B facts are installed
directly before scratch: no untimed update primes retraction buffers or support
history. A's initial flood and indexing still precede B fact installation, so
this is freshly derived B state, not a process that reads B alone.

Fresh edit1 A->B counts are 1 retraction / 16 reverse reads; repeats 1 / 14.
Fresh B->A counts are 6 / 31, repeats 2 / 7. The summary TSV includes every
treatment's min/median/max counters, including first toggles and single-thread
p2par. Maintenance excludes initialization, stored diff construction, oracle
checks, and the eviction pass; it remains a reach-only kernel measurement.
