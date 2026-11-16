## C. p1compact: TLB reach and first-touch faults

**Before.** ATTACKS entry 2 attributed 20.5–21.2 versus 30.1–32.3 ms to
"TLB reach, not faults" without counters or confirmation of allocation page size.

**Observation.** Both effects exist, at different times. On the first join,
ordinary malloc incurs **28,407** median minor faults versus **72** with the
hugetlb tunable. After one excluded full iteration, both incur **zero** faults
through all 64 measured joins, while huge pages still reduce L1 dTLB misses
and page-walk requests substantially. The hardware-counter ranges do not overlap.
The generic `dTLB-load-misses` event exactly equals AMD's
`ls_l1_d_tlb_miss.all_l2_miss` in every run: these are the **same page-walk
request count**, not independent evidence. `ls_l1_d_tlb_miss.all` measures L1
misses, including those resolved by L2. These counters count requests, not walk
latency or completed walks.

**Fix.** Added optional `P1_PROFILE=1` diagnostics in `ref/flat/p1compact.c`:
acknowledged perf FIFO control enables counters immediately before the name
join and disables them before relocation-target gathering. `getrusage` brackets
the same join; first-iteration time/faults are printed separately. `P1_WARMUP=1`
performs one complete excluded iteration, prefaulting exactly the accessed pages
and warming caches too. No changes to the join algorithm or normal oracle checks.
All 20 processes pass canon, builtins, relocation targets and content-join checks.
An independent reviewer reran all four configurations with one measured iteration
(`review5-c-review.md`, `review5-c-smoke.log`). ATTACKS now qualifies the claim.

**Number.** Five interleaved rounds rotating ordinary/hugetlb × no warmup/warmup,
64 measured joins per process, GCC `-O2 -march=native -fopenmp`, one thread pinned
to CPU 6, `FLAT_HUGE=0`. `perf stat -D -1 --control=fifo:...` records user-mode
events without multiplexing (100% running); input mapping, warmup, relocation
targets, content join and oracle checks are excluded from counters. Small control
and `getrusage` overhead is included in counters, excluded from join wall time.
The printed target/total wall time includes disable/ack overhead and is **not**
used here. Minimum accumulated measured name-join time is **1.597 s** per process.
"Cold" below means **one cold first join followed by 63 reused joins**, not 64
cold starts. Every cell is min / median / max across five processes; mean-join
columns summarize each process's arithmetic mean, not its fastest iteration.

| Allocation / warmup | Mean join ms | First join ms | First-join minor faults |
|---|---|---|---|
| ordinary / none | 30.228 / 38.352 / 51.477 | 76.515 / 90.401 / 198.646 | 28,407 / 28,407 / 28,408 |
| hugetlb / none | 25.589 / 29.287 / 36.965 | 33.796 / 35.694 / 38.330 | 72 / 72 / 73 |
| ordinary / one iteration | 30.043 / 31.582 / 51.205 | 28.508 / 32.086 / 41.587 | 0 / 0 / 0 |
| hugetlb / one iteration | 24.954 / 26.495 / 45.617 | 25.124 / 25.772 / 29.254 | 0 / 0 / 0 |

Counters below are totals divided by 64 (nearest integer), except faults, which
are totals over the whole measured 64-join region. Perf page-fault counts equal
`getrusage` minor faults exactly in every process; no measured major faults.

| Allocation / warmup | L1 dTLB misses per join | Page-walk requests per join | Faults across 64 joins |
|---|---|---|---|
| ordinary / none | 1,091,150 / 1,114,066 / 1,127,557 | 110,280 / 115,943 / 127,953 | 28,407 / 28,407 / 28,408 |
| hugetlb / none | 199,040 / 374,321 / 499,741 | 42,099 / 46,641 / 52,210 | 72 / 72 / 73 |
| ordinary / one iteration | 1,088,473 / 1,095,355 / 1,116,918 | 105,508 / 113,467 / 129,089 | 0 / 0 / 0 |
| hugetlb / one iteration | 268,593 / 389,057 / 569,068 | 42,251 / 46,073 / 48,282 | 0 / 0 / 0 |

Page-size verification: `/proc/self/smaps` reports **192,512 kB (188 MiB)
Private_Hugetlb**, zero Shared_Hugetlb, in every tunable run, versus zero in every
ordinary run. AnonHugePages is zero throughout. `/proc/meminfo` reports a
**2,048 kB** huge-page size and **1,536** reserved pages. Before/after process
snapshots show 1,500–1,536 free pages amid other VM activity; smaps sampled while allocations are live
provides the actual allocation evidence. This is explicit hugetlb, not transparent
huge pages. Input file mappings remain ordinary pages.

Window: **2026-11-16 19:41:39–19:44:25 UTC**; neighboring uptime samples **4h26m–4h28m**,
1-minute load **6.28–9.21**. No Goose benchmark overlapped; the last Halide checks
finished before this window. Other VM workloads remain uncontrolled.

**Verdict: both on first execution; TLB pressure remains after faults disappear.**
Warm medians show **2.81× fewer L1 dTLB misses** and **2.46× fewer walk requests**,
with zero faults on both sides. The warm time ranges overlap, so the timing
speedup is **within noise** in this window, not a replication of the historical
20.5–21.2 ms result. First-run time ranges are apart, but warmup also changes
cache residency, so these data do not assign an exact millisecond cost to faults
versus TLB walks. The old categorical "not faults" statement is withdrawn.

Reproduce: `python3 results/review5-c-run.py`, then
`python3 results/review5-c-summary.py`. Raw perf CSVs, program output, uptime,
meminfo snapshots and the complete AMD `perf list` are in `results/review5-c-raw/`.
