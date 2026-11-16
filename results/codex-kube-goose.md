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
