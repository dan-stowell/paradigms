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
