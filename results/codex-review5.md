# Review 5: poisoned checks, file assembly, and page-size mechanisms

All edits and generated outputs are inside `/home/exedev/paradigms-c`; datasets
and target repositories are read-only. Branch `codex-review5`; no push.
The source review is `reviews/done/20261116T1930-codex-skeptic-full.md` in the
sibling clone (already moved from its inbox when this work started).

## A. Halide P3a: output-independent address validation

**Before.** The driver copied `oracle/p3_addr.u32` into its output before layout.
This made a skipped pipeline call capable of passing the address check. There is
an important qualification: the original generator also had `addr(i) = 0`, so
an omitted update after that initialization would not necessarily retain the
oracle. The review's blanket omitted-write claim is stronger than the source
alone establishes. Oracle-prefilling a computed output is still unnecessary.

**Observation.** `check32` already compares every entry, checks the oracle length,
and counts all mismatches (only the diagnostic printing stops after five).
MinIO has **360,315** placed addresses. The address type is `u32`, so the poison
is **0xFFFFFFFF**, not a 64-bit value. With one actual layout unit omitted,
addresses 0 and 1 retain poison instead of 4,198,400 and 96,599,872.

**Fix.** `runLayout` poisons the full buffer before **every** invocation, including
warm repetitions, and checks Halide's return code. Each schedule additionally
gets a separate poisoned checked run. The generator's redundant zero-init is
now `undef`: the updates must supply every address. Runtime `P3_SKIP_UNIT=N`
skips that unit's ordinary and typelink address writes; default -1 omits none.
This is an omitted write inside the generator, not corruption after execution.
The e2e driver checks all addresses before relocation, so a failed address cannot
cause out-of-bounds image writes. Only isolated P3b runs load oracle addresses.
The e2e total includes the early address check; its separate 3b timer excludes it.

**Number.** All eight schedules pass all **360,315** address comparisons at both
1 and 8 threads; e2e also reports **P3 OK**, zero image-symbol differences and
zero relocation errors at both thread counts. With `P3_SKIP_UNIT=0`, both e2e
runs report exactly **2** address differences, **P3 FAIL**, and exit **1**.
The 8-thread warm benchmark with the same omitted unit also fails with exactly
2 differences and exit 1 after all repetitions, proving reuse cannot hide it.
For `layout_c3_w1`, five calibrated timing batches give min/median/max
**57.13 / 61.88 / 64.19 ms** at 1 thread and
**21.54 / 40.35 / 44.13 ms** at 8 threads. Poisoning is included in these times;
these establish continued operation, not a speedup against historical numbers.
Window 2026-11-16 19:39–19:41 UTC; preceding uptime samples show
4h24m–4h25m uptime, 1-minute load **4.11–9.66**. Other review work shared the VM.

Reproduce (all writes stay in the clone; input is read-only):

```sh
systemd-run --user --scope -p MemoryMax=12G systems/halide/cells/build.sh p3
bash results/review5-a-check.sh
```

Build output: `results/review5-a-build.log`; all oracle outcomes, fault exits,
timestamps, uptime and every schedule's timing: `results/review5-a-check.log`.

Independent lead rerun: `results/review5-a-independent.log` reproduces P3 OK
and the omitted-unit exit-1 failure.

## B. Goose P3b: a shared file instead of a gathered array

**Before.** The “falls down” claim was specifically about collecting 8 workers'
109 MB into one Goose array. The language cannot share that array, and queue
transport copies each segment twice. The 17–24 ms distributed result omitted
assembly into a usable single output.

**Fix.** `P3_FILE=/absolute/output/path` in `p3par.goose` selects a file join.
Main creates/truncates the output inside every timed repetition, then releases
all workers. Worker k builds and `pwrite`s its disjoint `[floor(n*k/W),
floor(n*(k+1)/W))` range using P4's existing `flat_write_at` C sidecar. Main
stops the clock only after all write completions. No image segment traverses
Goose's queue. The legacy queue mode remains available by leaving P3_FILE unset.
Worker relocation errors now contribute to the final failure status.
`p3.goose` and `ref/flat/p3.c` accept the same optional P3_FILE so the
single-worker comparisons include assembly/write-out too.

The output is a buffered file on this clone's ext4 filesystem (/dev/vda).
Creation, truncation, pwrite/fwrite, and close are included; fsync, storage
persistence, input loading, layout, and oracle checking are excluded. Each
repetition discards/recreates the file and freshly faults the image pages.
C's existing timer starts after allocation; Goose's includes page remapping
and explicit image zeroing. File timings are practical implementations rather
than an instruction-identical allocation experiment.

**Oracle scope.** `oracle/p3_image.bin` is the complete ELF, including headers
and bytes not represented by P3's placed symbols. It has 26,212,864 nonzero
bytes outside the P3-produced regions. Blind whole-file equality would reject
both the existing C reference and Goose. The strengthened Goose check compares
all 109,486,380 produced bytes to a *separate expected buffer*: oracle bytes
in every placed non-BSS symbol (76,359,886 bytes), zero everywhere else.
This is a complete P3 image buffer, not a complete executable ELF. No oracle bytes seed the
produced image. The benchmark independently constructs the same expected
image from the flat address/size/bucket columns in Python and compares the
entire actual output file after every file-output process. Before measuring,
the independent comparator rejects injected errors at placed byte 4096 and
zero-gap byte 0.

**Measurement.** Five interleaved rounds, rotating variant order; 64 fresh-image
repetitions per process. Report min/median/max of the five process minima,
matching the previous cell protocol; per-process min/median/max also retained
in the raw log and JSON. Only the final output of each 64-repetition process is checked, matching the
existing timing harness; earlier repetitions are not individually oracle-checked.
Every process must pass P3 and every final file must pass the
independent full-byte check; the script asserts minimum kernel time × 64 ≥ 1 s.
Goose release v2026.4, GCC 13.3.0, C `-O2 -march=native`; 8-worker affinity 0–7,
1-worker affinity CPU 6; all processes capped via systemd user scopes at 8 GiB.
`P3_THP=1` asks for explicit 2 MiB hugetlb output pages, despite its historical
name. Input data `/home/exedev/paradigms/data/minio` is read-only.

Reproduce from the clone root:

```sh
(cd systems/goose && /home/exedev/tools/goose/rel/goose-v2026.4-linux-x64/goose build p3.goose -o bin/p3)
(cd systems/goose && /home/exedev/tools/goose/rel/goose-v2026.4-linux-x64/goose build p3par.goose -o bin/p3par)
cc -O2 -march=native ref/flat/p3.c -o build/review5-p3
python3 results/review5-b-bench.py > results/review5-b-runs.log 2>&1
```

**Number.** Runs began 2026-11-16T19:45:26Z; completion observed at
19:51:21Z. The 50 per-process `uptime` samples span one-minute load
2.65–5.13, with uptime 4h30m–4h35m. All **50 P3 checks and 30 independent full-file checks pass**.
Shortest measured kernel lower bound (minimum × 64) is **1.8176 s**.
Raw logs: `results/review5-b-runs.log`; all process triplets:
`results/review5-b-runs.json`.

| Implementation | Pages | Relocate + file min / median / max ms | Kernel-only min / median / max ms |
|---|---|---:|---:|
| Goose, 8 workers | 4 KiB | 77.0 / 79.5 / 86.1 | — |
| Goose, 8 workers | 2 MiB | 72.3 / 77.3 / 78.8 | — |
| Goose p3, 1 worker | 4 KiB | 148.4 / 151.5 / 155.9 | 83.2 / 86.8 / 88.5 |
| Goose p3, 1 worker | 2 MiB | 99.1 / 101.2 / 103.3 | 38.1 / 38.3 / 39.5 |
| C p3, 1 worker | 4 KiB | 138.5 / 141.9 / 145.2 | 58.6 / 59.3 / 59.6 |
| C p3, 1 worker | 2 MiB | 105.3 / 107.8 / 108.2 | 28.4 / 28.6 / 28.8 |

**Observation.** The file join is real and faster than the matching single-worker
file outputs: at 2 MiB, median **77.3 ms** versus Goose's **101.2 ms**
(**1.31×**) and C's **107.8 ms** (**1.39×**), with disjoint ranges in both
comparisons. At 4 KiB, **79.5 ms** versus **151.5 / 141.9 ms**.
Eight-worker 4 KiB versus 2 MiB file results overlap: **within noise**.
The file assembly is substantial: this does not preserve the historical
17–24 ms distributed-only timing, and comparing it directly with C's
**28.6 ms** kernel-only median would compare different deliverables.
“Falls down” remains justified for one shared Goose array; it is **not**
a failure to assemble one checked P3 output file. The language needs the
C positioned-write sidecar for that escape, exactly as in P4.

Independent lead rerun and byte-range/gap check also passed: `results/review5-b-independent.log`;
static review: `results/review5-b-review.md`.

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

Final rebuilt C diagnostics and the ordinary no-profile path both pass P1 OK;
FIFO-gated perf still counts successfully (`results/review5-c-final-check.log`).
