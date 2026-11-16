## Tuned C comparison

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
