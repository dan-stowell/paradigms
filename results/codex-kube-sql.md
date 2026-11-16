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
