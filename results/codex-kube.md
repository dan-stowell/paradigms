# kube-apiserver, unchanged paradigm cells

Completed 2026-11-16, 17:21–18:16 UTC, on branch `codex-kube`. Dataset: `/home/exedev/paradigms/data/kube-apiserver` (read-only); checkout: `/home/exedev/paradigms-w`. All five paradigms were rebuilt and checked, then their bench.sh ran on kube. Tuned C was sampled in five rounds interleaved across the same window. All cell oracle checks passed; no algorithms changed and nothing was pushed.

All builds and measured processes used `systemd-run --user --scope -p MemoryMax=10G` and `OMP_WAIT_POLICY=passive`. Tools were read from `/home/exedev/tools`. The only source changes are benchmark memory caps and Soufflé's data-directory argument. Each active job had its own cap; some jobs overlapped as documented below. Other builders shared the host. We did not wait for quiet; sampled one-minute load ranged from 2.43 to 25.72 in the long SQL window.

Times are warm milliseconds, min / median / max over five rounds. Each system retains its bench.sh definition of a round, specified below. Historical MinIO values are from GRID.md, not same-window reruns. Range ratios use the midpoint unless the endpoints identify distinct variants; missing comparable GRID values are marked N/A or —. Futhark's historical values often use minima, while kube ratios use medians. Ratios therefore mix dataset, load, and sometimes runtime/page/statistic differences: they are descriptive, not measured speedups. Overlapping ranges are within noise.

## Failures and limits

- **Cell oracle failures: none.** No first differing values exist for the successful checks. Exact commands and outputs are retained in the linked logs and reproducer scripts.
- **Setup only:** one stray Soufflé `prep_lazy` invocation printed usage; the correct kube invocation immediately succeeded before cells ran. The reproducer was corrected and the original log retained.
- **Memory-limit kills: none observed.** Huge-page checks pass for Goose and DuckDB. Halide checks the reused-output image; fresh-mapping timings use the same verified pipeline but do not independently compare each fresh image.
- `p3par`'s C layout remains serial even with eight OpenMP threads. Goose's distributed relocation excludes the separately listed gather. No algorithm or schedule was changed for kube.

Independent table reviews: [Goose](codex-kube-goose-review.md), [Futhark](codex-kube-futhark-review.md), [Halide](codex-kube-halide-review.md), [Soufflé](codex-kube-souffle-review.md), [DuckDB](codex-kube-duckdb-review.md). They check raw timing triples, sample counts, oracle status and GRID mappings. Corrections were confined to report parsing/baseline attribution.

## Tuned C comparison

Built here with `gcc -O2 -march=native -fopenmp ref/flat/NAME.c -o build/NAME` for `NAME=p1par,p2par,p2lazy`, and `gcc -O2 -march=native -fopenmp ref/flat/p3.c -o build/p3par`, inside the same 10G scope/passive environment. These are the build flags in `ref/flat/check.sh`.

Commands: `bash results/codex-kube-c-round.sh LABEL`; each process runs under a 10 GiB systemd scope with `OMP_WAIT_POLICY=passive`. P1/P2 values are each process’s best of 10/30/20 in-process repetitions respectively; 3a is the mean of iterations until >=1 s; 3b is the median of 30 repetitions. Table entries are min / median / max across independent process rounds, in ms. 3a in `p3par` is serial at both thread settings. Ratios use the midpoint of the exact MinIO range in GRID.md. The 1a denominator is GRID’s later p1par-versus-p1compact snapshot (16:34Z, load 3–5), while the 1a+1b total and 1b use its earlier snapshot. GRID has no matching separated 1-thread p1par 1b or p3par 3b number, and its tuned 3a row is a different program.

| Cell | Oracle | Kube 1 thread | Kube 8 threads | MinIO GRID 1 / 8 | Kube/MinIO 1 / 8 | Rounds 1 / 8 |
|---|---|---:|---:|---|---|---|
| 1a+1b | OK | 158.00 / 159.20 / 171.80 | 36.10 / 89.70 / 94.40 | 74–77 / 13.9–16.8 | 2.11x / 5.84x | 5 / 5 |
| 1a | OK | 95.50 / 97.60 / 102.10 | 22.50 / 60.10 / 61.60 | 41.7–48.7 / 9.5–12.9 | 2.16x / 5.37x | 5 / 5 |
| 1b | OK | 61.20 / 63.60 / 69.70 | 13.70 / 28.30 / 33.60 | — / 5.3–6.3 | — / 4.88x | 5 / 5 |
| 1c | OK | 84.10 / 95.40 / 118.10 | 24.00 / 53.20 / 60.00 | 36.9 / 7.2–8.9 | 2.59x / 6.61x | 5 / 5 |
| 2 CSR | OK | 39.70 / 46.90 / 51.20 | 17.70 / 55.20 / 116.90 | 27–30 / 13–17 | 1.65x / 3.68x | 5 / 5 |
| 2 lazy | OK | 82.00 / 95.80 / 121.40 | 23.70 / 110.70 / 135.80 | 52.6 / 13.6–15.4 | 1.82x / 7.63x | 5 / 5 |
| 3a serial layout | OK | 12.83 / 17.57 / 20.41 | 12.78 / 14.21 / 24.70 | — / — | — / — | 5 / 5 |
| 3b 4 KB | OK | 64.30 / 76.20 / 82.50 | 11.30 / 28.80 / 59.40 | — / 13.4–29 | — / 1.36x | 5 / 5 |
| 3b 2 MB | OK | 32.20 / 34.00 / 46.40 | 9.80 / 24.00 / 28.10 | — / 5.4–12 | — / 2.76x | 5 / 5 |

Raw data: [codex-kube-c-raw.log](codex-kube-c-raw.log). Every run performs built-in oracle checks; `--layout` checks all addresses, P3 compares all placed image symbols, P1 checks canon/builtins/targets/content canon, and P2 checks the full live list.

Recorded window and uptime/load:

```text
ROUND 1 2026-11-16T17:21:55Z
 17:21:55 up  2:06,  0 user,  load average: 13.66, 6.39, 4.41
 17:22:22 up  2:06,  0 user,  load average: 14.05, 7.07, 4.69
ENDROUND 1 2026-11-16T17:22:22Z
ROUND 2 2026-11-16T17:27:13Z
 17:27:13 up  2:11,  0 user,  load average: 20.14, 15.90, 9.18
 17:27:49 up  2:12,  0 user,  load average: 18.82, 16.06, 9.49
ENDROUND 2 2026-11-16T17:27:49Z
ROUND between-halide-futhark 2026-11-16T17:34:28Z
 17:34:28 up  2:18,  0 user,  load average: 18.23, 15.66, 11.31
 17:35:02 up  2:19,  0 user,  load average: 19.88, 16.30, 11.68
ENDROUND between-halide-futhark 2026-11-16T17:35:02Z
ROUND 4-after-fh 2026-11-16T17:55:04Z
 17:55:04 up  2:39,  0 user,  load average: 14.90, 17.93, 16.91
 17:55:36 up  2:40,  0 user,  load average: 13.94, 17.34, 16.75
ENDROUND 4-after-fh 2026-11-16T17:55:36Z
ROUND 5-after-duckdb 2026-11-16T18:15:34Z
 18:15:34 up  3:00,  0 user,  load average: 2.56, 5.14, 9.59
 18:15:52 up  3:00,  0 user,  load average: 2.62, 4.99, 9.45
ENDROUND 5-after-duckdb 2026-11-16T18:15:52Z
```

Failures: none.


## Goose

Oracle: **OK** for all cells, including the serial P1→P3 pipeline. Checks compare every u32 output and placed symbol bytes; raw commands and results: [check log](codex-kube-goose-check.log). Serial p1/p2/p3 ignore a numeric trailing argument (p3 writes an optional image there); only `*par` commands select workers. The lazy serial numeric argument selects repetitions.

Build: `goose build p.goose -o bin/p` for p1, p1par, p2, p2lazy, p2par, p3, p3par. Benchmark: `OMP_WAIT_POLICY=passive systemd-run --user --scope -p MemoryMax=10G bash systems/goose/bench.sh 5 /home/exedev/paradigms/data/kube-apiserver`. Five interleaved rounds; each round uses the bench’s best in-process repetition, except layout and gather (one sample per round). 8-worker relocate excludes gathering the worker segments; gather is separate.

| Cell | Oracle | kube 1 thread min/med/max | kube 8 workers min/med/max | GRID MinIO 1 / 8 | kube/MinIO median 1 / 8 |
|---|---|---|---|---|---|
| p1a | OK | 181.2 / 206.9 / 256.3 | 144.4 / 152.5 / 191.0 | 79–115 / 37–57 | 2.13× / 3.24× |
| p1b | OK | 79.2 / 84.6 / 103.4 | 160.0 / 212.4 / 269.9 | 39–57 / 42–67 | 1.76× / 3.90× |
| p1c | OK | 176.8 / 198.8 / 250.8 | 76.7 / 81.5 / 103.4 | 97–118 / 39–47 | 1.85× / 1.90× |
| p2-csr | OK | 64.5 / 75.4 / 204.9 | 53.7 / 58.5 / 75.2 | 41–49 / 22–32 | 1.68× / 2.17× |
| p2-lazy | OK | 97.0 / 113.3 / 158.0 | 82.0 / 98.3 / 123.2 | 74–79 / 37–49 | 1.48× / 2.29× |
| p3a | OK | 33.3 / 46.1 / 61.6 | 43.4 / 103.9 / 156.7 | 15–26 / 17–31 | 2.25× / 4.33× |
| p3b-4k | OK | 107.3 / 112.5 / 135.1 | 55.5 / 77.1 / 92.8 | 87–99 / 33–40 | 1.21× / 2.11× |
| p3b-2m | OK | 49.9 / 58.9 / 97.6 | 29.5 / 37.5 / 52.0 | 41–42 / 17–24 | 1.42× / 1.83× |
| p3b-gather | OK | N/A | 246.9 / 309.7 / 460.3 | N/A / 142–171 | N/A / 1.98× |

Load and UTC from [bench log](codex-kube-goose-bench.log):
```
goose bench: 5 rounds, 2026-11-16T17:22:52Z, load 17.07 8.43 5.21
round 1 17:22:52 load 17.07
round 2 17:23:31 load 19.82
round 3 17:24:09 load 20.69
round 4 17:25:00 load 20.93
round 5 17:25:56 load 23.13
done 2026-11-16T17:26:45Z, load 22.45 15.92 9.01
 17:27:27 up  2:11,  0 user,  load average: 20.05, 16.08, 9.35
```

Goose overlapped DuckDB checks/bench and external builders. Historical ratios therefore include load differences as well as dataset differences. No oracle failures. Explicit huge-page checks also pass at 1/8 workers: [log](codex-kube-goose-huge-check.log), with nonzero huge-page byte counts.


## Futhark and Halide measurement notes

Builds completed successfully under `systemd-run --user --scope -p MemoryMax=10G`, with `OMP_WAIT_POLICY=passive`:

- `env YIELD=1 PATH=/home/exedev/tools/futhark/bin:/usr/local/bin:/usr/bin:/bin systems/futhark/build.sh`
- `systems/halide/cells/build.sh`

Build logs: `results/codex-kube-futhark-build.log`, `results/codex-kube-halide-build.log`.
Reproducer: `bash results/codex-kube-fh-run.sh` (build first).
The input directory is read-only `/home/exedev/paradigms/data/kube-apiserver`.

Futhark uses `HUGE=1`, matching GRID.md's 2 MB-page baseline, and the sequential C, stock multicore,
and yielding multicore backends. The environment's `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` requests
huge pages but glibc can fall back to ordinary pages when the reserved pool is exhausted.
Each warm statistic is min/median/max of the per-process medians across five full bench.sh rounds,
with the script's unchanged inner repetition counts. Oracle checks omit `nocheck`.
Halide's unchanged bench.h times five rounds per schedule; each round repeats the kernel for at least
approximately one second and reports per-iteration time. Its bench.sh covers all schedules and fresh
4 KB/2 MB as well as reused output mappings.

The ratios use the kube median divided by the midpoint of GRID.md's MinIO range (or its lone stated
number), and are descriptive cross-window comparisons; a missing GRID entry is shown as —.

Historical-baseline caveat: GRID.md generally quotes best/minimum Futhark times, whereas the kube
columns below summarize medians of repeated runs. GRID's Futhark layout-loop 8-thread value (7.3 ms)
is from the yielding runtime without the huge-page configuration (see NOTES.md's detailed table),
while this run requests huge pages for both stock and yielding runtimes. These kube/MinIO ratios
therefore mix historical load, statistic, and in some rows runtime/page conditions; they are workload
comparisons requested against GRID, not measured scaling factors or evidence of a regression.
Halide fresh-mapping timing rows execute the same relocation pipeline that passed its image oracle;
the stock benchmark driver checks the reused-output image, not a separate image from each fresh mapping.

Scheduling: checks began 2026-11-16T17:26:50Z, Halide benchmarks 17:27:55–17:34:28Z
(`uptime` load-average triples 17.77/15.93/9.52 and 18.23/15.66/11.31 at those endpoints).
A tuned-C round ran between Halide and Futhark; Futhark round 1 started 17:35:02Z.
These measurements overlapped DuckDB, external builders, and (from 17:43:55Z) Soufflé;
we did not wait for low load. Each individual executable had its own 10 GiB memory scope.

Futhark completed at 2026-11-16T17:54:56Z (exit 0). Round-boundary `uptime` load averages:

```text
 17:35:02 up  2:19,  0 user,  load average: 19.88, 16.30, 11.68
 17:38:16 up  2:22,  0 user,  load average: 18.41, 16.96, 12.83
 17:38:16 up  2:22,  0 user,  load average: 18.41, 16.96, 12.83
 17:41:24 up  2:25,  0 user,  load average: 17.62, 17.52, 13.83
 17:41:24 up  2:25,  0 user,  load average: 17.62, 17.52, 13.83
 17:45:33 up  2:30,  0 user,  load average: 22.62, 19.38, 15.38
 17:45:33 up  2:30,  0 user,  load average: 22.62, 19.38, 15.38
 17:50:38 up  2:35,  0 user,  load average: 17.87, 20.01, 16.93
 17:50:38 up  2:35,  0 user,  load average: 17.87, 20.01, 16.93
 17:54:56 up  2:39,  0 user,  load average: 15.99, 18.23, 17.00
```

Oracle checks: all 27 Futhark commands and all 14 initial Halide commands returned 0 and printed OK.
Checks and exact commands are retained in `codex-kube-futhark-check.log` and `codex-kube-halide-check.log`;
all 24 additional Halide P3 schedule checks passed (eight layout and four relocation schedules at each
thread count), recorded in `codex-kube-halide-p3-check.log`.

Failures: none detected. No algorithms were changed.

## Futhark

Warm ms; min / median / max of five process medians. All configurations request huge pages.

| cell | config | oracle | kube warm | rounds | GRID MinIO | kube / MinIO |
|---|---|---|---|---|---|---|
| 1a | 1+huge | OK | 285.75 / 302.78 / 373.24 | 5 | 79 | 3.83x |
| 1a | 8spin+huge | OK | 163.04 / 253.40 / 419.91 | 5 | 24 | 10.56x |
| 1a | 8yield+huge | OK | 127.99 / 204.00 / 344.68 | 5 | 24 | 8.50x |
| 1b | 1+huge | OK | 1066.73 / 1275.39 / 2100.93 | 5 | — | — |
| 1b | 8spin+huge | OK | 326.43 / 332.00 / 535.80 | 5 | — | — |
| 1b | 8yield+huge | OK | 297.82 / 336.01 / 527.97 | 5 | — | — |
| 1b_scan | 1+huge | OK | 99.56 / 124.99 / 357.99 | 5 | 47 | 2.66x |
| 1b_scan | 8spin+huge | OK | 56.92 / 88.02 / 157.21 | 5 | 18 | 4.89x |
| 1b_scan | 8yield+huge | OK | 44.34 / 71.98 / 137.80 | 5 | 18 | 4.00x |
| 1c | 1+huge | OK | 154.16 / 207.44 / 278.01 | 5 | 111 | 1.87x |
| 1c | 8spin+huge | OK | 123.59 / 140.93 / 262.21 | 5 | 38 | 3.71x |
| 1c | 8yield+huge | OK | 92.34 / 112.47 / 217.22 | 5 | 38 | 2.96x |
| 2 | 1+huge | OK | 159.19 / 191.28 / 309.29 | 5 | 97 | 1.97x |
| 2 | 8spin+huge | OK | 220.04 / 270.74 / 420.03 | 5 | 58 | 4.67x |
| 2 | 8yield+huge | OK | 174.70 / 300.04 / 671.97 | 5 | 58 | 5.17x |
| 2lazy | 1+huge | OK | 248.99 / 276.91 / 320.05 | 5 | 165 | 1.68x |
| 2lazy | 8spin+huge | OK | 355.31 / 375.88 / 589.76 | 5 | 90 | 4.18x |
| 2lazy | 8yield+huge | OK | 342.47 / 397.32 / 917.94 | 5 | 90 | 4.41x |
| 3a | 1+huge | OK | 33.68 / 47.75 / 60.16 | 5 | 22 | 2.17x |
| 3a | 8spin+huge | OK | 28.89 / 54.28 / 80.15 | 5 | 10 | 5.43x |
| 3a | 8yield+huge | OK | 44.42 / 60.04 / 117.58 | 5 | 10 | 6.00x |
| 3a_loop | 1+huge | OK | 21.61 / 34.00 / 42.97 | 5 | 14 | 2.43x |
| 3a_loop | 8spin+huge | OK | 32.64 / 41.44 / 51.15 | 5 | 7.3 | 5.68x |
| 3a_loop | 8yield+huge | OK | 28.69 / 32.72 / 72.06 | 5 | 7.3 | 4.48x |
| 3b | 1+huge | OK | 128.06 / 146.77 / 567.02 | 5 | 107 | 1.37x |
| 3b | 8spin+huge | OK | 60.78 / 79.88 / 140.98 | 5 | 41 | 1.95x |
| 3b | 8yield+huge | OK | 62.47 / 81.90 / 131.05 | 5 | 41 | 2.00x |

## Halide

| cell / schedule / mapping | threads | oracle | kube warm min / median / max ms | GRID MinIO | kube / MinIO |
|---|---|---|---|---|---|
| counts | 1 | OK | 27.26 / 35.68 / 61.16 | — | — |
| 1a resolve_f | 1 | OK | 502.87 / 579.73 / 657.91 | 81–93 | 6.66x |
| 1b reltarget_f | 1 | OK | 85.31 / 105.18 / 110.49 | 42.7–46 | 2.37x |
| 1c contentjoin_f | 1 | OK | 295.76 / 302.37 / 341.33 | 122–133 | 2.37x |
| 2 flood_t_f | 1 | OK | 815.06 / 899.27 / 938.03 | — | — |
| 2 flood_f_f | 1 | OK | 589.35 / 615.15 / 733.41 | — | — |
| 3a layout_c0_w0 | 1 | OK | 90.89 / 98.70 / 107.51 | — | — |
| 3a layout_c0_w1 | 1 | OK | 95.59 / 100.15 / 105.42 | — | — |
| 3a layout_c1_w0 | 1 | OK | 102.40 / 114.00 / 125.00 | — | — |
| 3a layout_c1_w1 | 1 | OK | 100.04 / 115.26 / 121.02 | — | — |
| 3a layout_c2_w0 | 1 | OK | 100.74 / 103.92 / 108.52 | — | — |
| 3a layout_c2_w1 | 1 | OK | 100.27 / 104.08 / 111.71 | — | — |
| 3a layout_c3_w0 | 1 | OK | 89.61 / 93.16 / 111.78 | — | — |
| 3a layout_c3_w1 | 1 | OK | 87.60 / 90.88 / 103.83 | — | — |
| 3b reloc_f0_pf_v0 warm | 1 | OK | 113.91 / 116.41 / 123.22 | — | — |
| 3b reloc_f0_pf_v0 fresh 4KB | 1 | OK | 146.25 / 172.28 / 193.23 | — | — |
| 3b reloc_f0_pf_v0 fresh 2MB | 1 | OK | 105.06 / 105.48 / 113.17 | — | — |
| 3b reloc_f0_pf_v32 warm | 1 | OK | 105.18 / 106.07 / 108.03 | — | — |
| 3b reloc_f0_pf_v32 fresh 4KB | 1 | OK | 144.42 / 145.88 / 148.27 | — | — |
| 3b reloc_f0_pf_v32 fresh 2MB | 1 | OK | 109.71 / 110.37 / 122.76 | — | — |
| 3b reloc_f0_pt_v0 warm | 1 | OK | 105.20 / 106.86 / 113.41 | — | — |
| 3b reloc_f0_pt_v0 fresh 4KB | 1 | OK | 142.97 / 144.58 / 154.63 | — | — |
| 3b reloc_f0_pt_v0 fresh 2MB | 1 | OK | 112.42 / 113.21 / 123.47 | — | — |
| 3b reloc_f0_pt_v32 warm | 1 | OK | 110.64 / 111.98 / 129.01 | — | — |
| 3b reloc_f0_pt_v32 fresh 4KB | 1 | OK | 141.97 / 144.36 / 152.10 | — | — |
| 3b reloc_f0_pt_v32 fresh 2MB | 1 | OK | 110.20 / 111.86 / 112.94 | — | — |
| counts | 8 | OK | 5.11 / 6.71 / 7.22 | — | — |
| 1a resolve_t | 8 | OK | 79.41 / 84.46 / 94.32 | 19.8–27 | 3.61x |
| 1b reltarget_t | 8 | OK | 19.80 / 22.67 / 38.96 | 9.9–10.9 | 2.18x |
| 1c contentjoin_t | 8 | OK | 125.65 / 147.42 / 163.86 | 33–36.5 | 4.24x |
| 2 flood_t_t | 8 | OK | 350.00 / 399.29 / 410.53 | 79–90 | 4.73x |
| 2 flood_f_t | 8 | OK | 294.99 / 328.94 / 344.29 | 70–127 | 3.34x |
| 3a layout_c0_w0 | 8 | OK | 46.93 / 48.04 / 59.51 | — | — |
| 3a layout_c0_w1 | 8 | OK | 46.76 / 47.80 / 48.37 | — | — |
| 3a layout_c1_w0 | 8 | OK | 54.26 / 57.02 / 58.58 | — | — |
| 3a layout_c1_w1 | 8 | OK | 53.86 / 56.89 / 59.50 | — | — |
| 3a layout_c2_w0 | 8 | OK | 48.00 / 53.95 / 58.48 | — | — |
| 3a layout_c2_w1 | 8 | OK | 43.80 / 50.53 / 58.37 | — | — |
| 3a layout_c3_w0 | 8 | OK | 47.38 / 49.14 / 101.58 | — | — |
| 3a layout_c3_w1 | 8 | OK | 71.56 / 84.44 / 137.00 | 20–23 | 3.93x |
| 3b reloc_f0_pf_v0 warm | 8 | OK | 123.04 / 187.72 / 201.33 | — | — |
| 3b reloc_f0_pf_v0 fresh 4KB | 8 | OK | 263.76 / 285.11 / 352.86 | — | — |
| 3b reloc_f0_pf_v0 fresh 2MB | 8 | OK | 170.93 / 186.20 / 189.83 | — | — |
| 3b reloc_f0_pf_v32 warm | 8 | OK | 141.76 / 163.06 / 172.87 | — | — |
| 3b reloc_f0_pf_v32 fresh 4KB | 8 | OK | 189.92 / 230.12 / 246.29 | — | — |
| 3b reloc_f0_pf_v32 fresh 2MB | 8 | OK | 136.13 / 139.89 / 169.52 | — | — |
| 3b reloc_f0_pt_v0 warm | 8 | OK | 62.16 / 66.76 / 72.31 | — | — |
| 3b reloc_f0_pt_v0 fresh 4KB | 8 | OK | 111.90 / 136.34 / 154.57 | — | — |
| 3b reloc_f0_pt_v0 fresh 2MB | 8 | OK | 80.40 / 97.18 / 112.40 | — | — |
| 3b reloc_f0_pt_v32 warm | 8 | OK | 49.25 / 86.54 / 96.07 | 24.9–27 | 3.33x |
| 3b reloc_f0_pt_v32 fresh 4KB | 8 | OK | 83.71 / 113.51 / 151.62 | — | — |
| 3b reloc_f0_pt_v32 fresh 2MB | 8 | OK | 71.49 / 78.57 / 81.21 | 27–30.8 | 2.72x |


## SQL-family runs: method and oracle checks

Dataset `/home/exedev/paradigms/data/kube-apiserver` was read-only. Builds, checks and runs used `systemd-run --user --scope -p MemoryMax=10G` with `OMP_WAIT_POLICY=passive`. No cell algorithms changed. See `codex-kube-{duckdb,souffle}-run.sh` for commands and `*-build.log`, `*-raw.txt`, `*-bench.txt` for raw evidence. DuckDB's bench cap was changed from 8G to the requested 10G; Soufflé's bench accepts its second argument as the data directory and uses a cgroup rather than a virtual-address limit.

DuckDB rebuilt with `systems/duckdb/build.sh`. Oracle commands are the NOTES commands with kube substituted: `./dk "$D" T ...` followed by binary `cmp` for P1's canonical/builtin/relocation-target/content-canonical arrays, P2's sorted live IDs, and P3a's addresses; `python3 -I check3b.py "$D" out/p3_image.bin` for isolated and pipeline P3b. All passed at both T=1 and T=8. The complete script lists every SQL file and exact command. Warm runs use five process rounds with five in-process repetitions; the table reports min/median/max of each process's median. E2e additionally runs once per round.

MinIO comparisons below are specifically from GRID.md, not substituted from the fuller NOTES tables. A range denominator is its midpoint; ratios are kube median / midpoint, approximate cross-window dataset scaling rather than controlled speedups. `—` means GRID.md does not give that thread-count/variant's number. Warm timings are milliseconds. The 3b SQL computation includes the C host's stores in DuckDB; Soufflé rules exclude its separately timed C image assembly.

Soufflé rebuilt all eight generated programs with `./build.sh "$p.dl"`, plus the C oracle checker and lazy-input preparer. `build/prep_lazy "$D" build/lazyfacts` generated kube-specific lazy facts (1,592,404 retained auxiliary/kind1–3 edges). All eight programs passed at 1 and 8 threads; `bench.sh 5 "$D"` repeated each oracle check on every timing round. The initial setup log includes an accidental `build/prep_lazy prep_lazy.c` invocation, which printed usage and exited; the immediately following correct invocation succeeded. The reproducible runner now omits that stray invocation. This was a harness setup error, not a cell failure.

For Soufflé 1c at eight threads, GRID's `394–587` combines two implementations: NOTES identifies 587 with `resolve_c` and 394 with `resolve_c2`. The table uses those matching GRID endpoints, not a midpoint across different variants.

Reproduce the run scopes from the repository root (the scripts' bare child invocations inherit this cap):

```sh
systemd-run --user --scope -q -p MemoryMax=10G env OMP_WAIT_POLICY=passive bash results/codex-kube-duckdb-run.sh
systemd-run --user --scope -q -p MemoryMax=10G env OMP_WAIT_POLICY=passive bash results/codex-kube-souffle-run.sh
```

Build first, also scoped: `systemd-run --user --scope -q -p MemoryMax=10G env OMP_WAIT_POLICY=passive systems/duckdb/build.sh`; for Soufflé use the same scope prefix around `bash -c 'cd systems/souffle; for p in resolve_a resolve_b resolve_c resolve_c2 reach layout relocate reach_lazy; do ./build.sh "$p.dl" || exit; done; gcc -O2 -o build/check_p3 check_p3.c; gcc -O2 -o build/prep_lazy prep_lazy.c'`.

Soufflé checks ran **2026-11-16 17:43:55–17:49:10 UTC**, benchmark **17:49:10–18:03:25 UTC**. Sampled one-minute uptime load over its full check/benchmark window ranged **8.56–25.72**; benchmark start `21.43, 21.10, 16.93`, end `9.61, 12.26, 14.59` (1/5/15-minute loads). [Sampled uptime log](codex-kube-souffle-load.log), [checks/raw summary](codex-kube-souffle-raw.txt), [five-round benchmark](codex-kube-souffle-bench.txt). It overlapped DuckDB, Futhark, short C reference runs, and unrelated builders. Measurements did not wait for a quiet host.

DuckDB checks ran **2026-11-16 17:21:30–17:25:56 UTC**, benchmark **17:25:56–18:15:01 UTC**. Sampled one-minute uptime load over the check/benchmark window ranged **2.43–25.72**; benchmark start `23.13, 14.90, 8.32`, end `3.76, 5.66, 9.93` (1/5/15-minute loads). [Sampled uptime log](codex-kube-duckdb-load.log), [checks/raw summary](codex-kube-duckdb-raw.txt), [five-round benchmark](codex-kube-duckdb-bench.txt). The long benchmark overlapped Goose, Halide/Futhark, Soufflé, short C references, and other builders during portions of its window. Every one of its 22 warm rows and ten e2e rows has five rounds.

After benchmarking, the huge-page P3b configuration was additionally checked at both thread counts: `DK_THP=1 ./dk "$D" T sql/load3b.sql sql/load3b_iso.sql sql/p3b.sql` then `python3 -I check3b.py "$D" out/p3_image.bin`, inside the same 10G scope/passive environment. Both checked 72,579,169 bytes with zero differing symbols; [raw huge-page oracle checks](codex-kube-duckdb-huge-check.txt).

Tables: [DuckDB](codex-kube-duckdb-table.md), [Soufflé](codex-kube-souffle-table.md). All tested cell oracle checks passed. No mismatching values, failed cells, or memory-limit kills were observed. The only setup-command failure was the disclosed stray Soufflé preparer invocation, corrected before any checked or timed cell. Wide timing ranges reflect the measured shared-host load; overlapping ranges are within noise, including DuckDB's 4KB versus huge-page relocation results.


## Soufflé

| Cell | Oracle 1 / 8 | kube 1 thread min / median / max ms | kube 8 threads min / median / max ms | GRID MinIO 1 / 8 ms | kube/MinIO 1 / 8 |
|---|---|---|---|---|---|
| 1a | OK / OK | 7068.2 / 10490.7 / 13011.5 | 2860.3 / 3438.0 / 4699.4 | 1928–2034 / 636 | 5.30× / 5.41× |
| 1b | OK / OK | 13002.9 / 17545.4 / 23584.9 | 4252.4 / 6378.8 / 8177.3 | 5220–5426 / 1080 | 3.30× / 5.91× |
| 1c | OK / OK | 3598.2 / 5166.5 / 6141.9 | 1884.8 / 2240.5 / 2606.2 | 1625–1865 / 587 | 2.96× / 3.82× |
| 1c-slot | OK / OK | 2133.7 / 3769.0 / 4245.5 | 1056.8 / 1921.0 / 2702.5 | — / 394 | — / 4.88× |
| 2 | OK / OK | 2177.1 / 2433.9 / 4220.9 | 841.8 / 1661.0 / 3325.5 | 1227–1467 / 431 | 1.81× / 3.85× |
| 2-lazy | OK / OK | 4711.4 / 5463.7 / 7039.3 | 2450.1 / 5219.7 / 18146.9 | 2900–3200 / 960 | 1.79× / 5.44× |
| 3a | OK / OK | 15344.3 / 17146.8 / 38350.5 | 4558.8 / 6677.2 / 8814.9 | 7222–7589 / 1339 | 2.32× / 4.99× |
| 3b | OK / OK | 1715.4 / 1837.1 / 2370.2 | 732.9 / 900.8 / 1126.3 | — / 320 | — / 2.81× |

E2e and separate assembly (same five rounds):

| Cell / metric | 1 thread min / median / max ms | 8 threads min / median / max ms |
|---|---|---|
| 1a / e2e | 11196.1 / 15063.0 / 17857.4 | 7431.4 / 7500.2 / 10343.1 |
| 1b / e2e | 19144.2 / 25209.7 / 32713.9 | 8758.7 / 13327.3 / 17399.4 |
| 1c / e2e | 7583.6 / 11324.5 / 12140.5 | 5013.5 / 6691.9 / 7344.9 |
| 1c-slot / e2e | 10295.6 / 11316.5 / 12037.0 | 5861.9 / 9681.5 / 12802.0 |
| 2 / e2e | 5281.0 / 6853.8 / 11089.6 | 2930.1 / 4046.5 / 6238.5 |
| 2-lazy / e2e | 9107.0 / 10761.5 / 13481.1 | 6181.5 / 9951.2 / 25657.4 |
| 3a / e2e | 15616.8 / 17512.7 / 38792.5 | 4809.3 / 7050.4 / 9064.6 |
| 3b / e2e | 3839.5 / 4307.6 / 5640.2 | 2881.5 / 3168.2 / 3651.8 |
| 3b / assemble | 59.2 / 71.9 / 75.1 | 23.7 / 34.3 / 39.8 |


## DuckDB

| Cell | Oracle 1 / 8 | kube 1 thread min / median / max ms | kube 8 threads min / median / max ms | GRID MinIO 1 / 8 ms | kube/MinIO 1 / 8 |
|---|---|---|---|---|---|
| 1a | OK / OK | 877.5 / 991.5 / 1499.9 | 225.9 / 411.2 / 590.1 | — / 103–113 | — / 3.81× |
| 1b | OK / OK | 3485.4 / 4189.7 / 6193.3 | 693.7 / 1252.9 / 1473.1 | — / 272–375 | — / 3.87× |
| 1c | OK / OK | 1144.2 / 1472.2 / 2378.1 | 238.4 / 438.7 / 586.0 | — / 128–190 | — / 2.76× |
| 2eager | OK / OK | 7317.6 / 8539.1 / 11485.2 | 1599.0 / 2856.1 / 3470.0 | — / — | — / — |
| 2loop | OK / OK | 5004.1 / 6767.4 / 9075.5 | 2767.2 / 3687.5 / 5904.1 | — / 1965–2354 | — / 1.71× |
| 2loop-eager | OK / OK | 5474.2 / 6842.6 / 9514.3 | 2934.7 / 4395.7 / 6786.3 | — / — | — / — |
| 2loop-lazy | OK / OK | 12388.4 / 13640.9 / 19550.4 | 4489.3 / 9663.5 / 15583.7 | — / — | — / — |
| 2prep | OK / OK | 449.7 / 610.0 / 758.3 | 158.6 / 272.4 / 374.0 | — / 103 | — / 2.64× |
| 3a | OK / OK | 438.6 / 448.2 / 511.4 | 326.0 / 594.0 / 1159.0 | — / 217–263 | — / 2.48× |
| 3b | OK / OK | 1372.3 / 1612.8 / 2060.6 | 405.4 / 968.5 / 1707.3 | — / 339–368 | — / 2.74× |
| 3b-hugepages | OK / OK | 1269.0 / 1614.6 / 2877.2 | 379.3 / 897.9 / 1347.5 | — / 309–332 | — / 2.80× |

E2e and separate assembly (same five rounds):

| Cell / metric | 1 thread min / median / max ms | 8 threads min / median / max ms |
|---|---|---|
| P1 / e2e | 10031.6 / 11407.1 / 14792.3 | 2347.5 / 4136.2 / 7483.0 |
| P1-P3 / e2e | 12946.5 / 14437.6 / 18607.5 | 3535.2 / 5739.1 / 9433.7 |
| P2 / e2e | 6464.0 / 8362.1 / 8991.0 | 2919.8 / 4446.3 / 7401.3 |
| P3a / e2e | 544.2 / 720.6 / 816.5 | 438.5 / 529.0 / 1071.6 |
| P3b / e2e | 2666.2 / 3303.2 / 4265.4 | 871.7 / 1690.1 / 2484.3 |
