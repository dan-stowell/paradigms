# kube-apiserver, unchanged paradigm cells

Dataset: `/home/exedev/paradigms/data/kube-apiserver` (read-only). Checkout: `/home/exedev/paradigms-w`, branch `codex-kube`. All builds and measured processes use `systemd-run --user --scope -p MemoryMax=10G`; `OMP_WAIT_POLICY=passive`. No cell algorithms changed. Tools come from `/home/exedev/tools` without modifying them.

Goose, Futhark, Halide and Soufflé measurements are complete; DuckDB and the final C comparison round are in progress.

Times are warm milliseconds, min / median / max over at least five rounds. Each system retains its bench.sh definition of a round (specified below); these definitions differ. The historical MinIO values are from GRID.md, not same-window reruns. Where GRID gives a range, ratios use its midpoint, so ratios are descriptive, not speedup claims. A missing comparable GRID value is N/A. Same-window tuned C samples are interleaved across the measurement window. The machine is shared; uptime/load is recorded, without waiting for quiet.

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
Each warm statistic will be min/median/max of the per-process medians across five full bench.sh rounds,
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

## Souffle

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
