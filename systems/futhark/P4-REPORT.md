# Futhark on P4 (code generation by shape): report

Branch `worktree-agent-a0bb39babb91798e8`. Code: `systems/futhark/p4.fut` (all compute), `p4.c` (loading, timing,
checking only). Long form: the "P4" section of `systems/futhark/NOTES.md`; the cell block is at the end of `CELLS.md`;
ideas I18-I19 are in `IDEAS.md`; the raw bench output is in `p4-bench-2026-11-16T1800Z.txt`.

## Correct

All five shapes pass on the c backend and on multicore (8 threads): 60,733,909 bytes equal `oracle/p4_code.bin`
(memcmp), the function offsets equal `oracle/p4_fn_off.u64`, and the 1,780,281 relocations equal `oracle/p4_rel_*`
as a sorted list. 0 bad holes.

    cd systems/futhark && ./build.sh p4
    for c in 4 4g 4f 4b 4bf; do build/p4_c.exe /home/exedev/paradigms/data/minio-p4 1 1 $c; done   # P4* OK

## Times (min / median / max ms, 2 MB pages, 2026-11-16T18:00-18:03Z, load 10-12, interleaved with C)

| | 1 core warm | 8 threads warm | e2e 1 / 8 |
|---|---|---|---|
| Futhark `4bf` (blocked copy + patch) | 326 / 374 / 439 (multicore at 1 thread); 357 / 424 / 466 (c backend) | 161 / 187 / 216 (yield runtime); 185 / 204 / 251 (spin) | 383-460 / 254-298 |
| Futhark `4b` (blocked copy, patch scatter) | 348 / 376 / 417 | 189 / 210 / 249 | 440-475 / 310-373 |
| Futhark `4` (flat: row scatter copy) | 411 / 458 / 578 | 220 / 245 / 337 | 508-545 / 383-440 |
| Futhark `4g` (gather copy) | 535 / 569 / 593 | 254 / 304 / 346 | 576-675 / 398-530 |
| C ref/p4/p4.c, 1 core | 102-110 min, 106-115 median | | |
| C ref/p4/p4par.c, 8 threads | | 28-43 min, 30-46 median | |

Futhark is 3.2x the plain C at 1 core and 5.3x the OpenMP C at 8 threads (8-thread numbers on a loaded machine are
upper bounds: cpu/rep 525-750 ms means the threads got 3-4 cores). 4 KB pages cost 1.4-1.6x at 1 core (515 vs 357).
Rerun: `HUGE=1 CFGS="c:1:1 multicore:1:1mc multicore:8:8spin mcy:8:8yield" CELLS="4 4b 4bf 4g" systems/futhark/bench.sh`.

## Main finding

The problem's shape maps directly onto Futhark (lengths by map, places by exclusive scan, holes as one flat map over
(node, hole) pairs, rel32 by gathering `at[v]`, relocations as expands), and it is correct first time. What costs is
the copy: a `map` cannot memcpy a run into a slice of a shared array, so the idiomatic copy is a scatter of one
(index, byte) pair per byte (~110 ms CPU for 60.7 MB). The fix is a map over 64 equal output blocks with a
*sequential* loop of slice updates inside (`row with [a:a+m] = xs[f:f+m]` compiles to a memcpy): ~20-35 ms. That
corrects P3b's note ("no bulk copy into a slice"): there is one, but only in a loop, so the fast shape is C's loop
wrapped in a map. The remaining ~3x is passes: four expands (scan + scatter + max-scan + map each, tuples
materialized) and the hole tables. Second attempts that lost: copy as a gather (owner per byte by max-scan over
60.7 M: slower), copy + patch fused in 32-byte rows (2x slower than separate). Tuning that did not matter: chunk
8-64, `--unsafe`, c vs multicore at 1 thread. The "commonest K templates, rest as glue" variant does not change the
program (glue and node segments already go through the same copy), so it was not built.

## Lines

p4.fut 159 non-blank non-comment with all five shapes (~95 for the `4bf` path); p4.c 122.
