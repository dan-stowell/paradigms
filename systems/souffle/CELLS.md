# Soufflé cells (MinIO; for GRID.md)

Times: ms, min / median / max of 5 interleaved runs, 2026-11-16T16:37Z, load 2.9-6.2 (`systems/souffle/bench.sh`).
warm = rules only (inputs bulk-loaded into B-trees); e2e = process. 8 threads with `OMP_WAIT_POLICY=passive`.
Checks: NOTES.md "Checks" (cmp against the oracles / check_p3 -> P3 OK). Lines: rules + decls without comments;
flatio.h (385 lines C++, I/O only) shared.

**1a join by name** — correct (`cmp out/p1_canon.u32 oracle/p1_canon.u32`, builtin too) · 1 thr warm 1928-2034,
e2e 3136-3295 · 8 thr warm 636-754, e2e 1722-1888 · 37 lines · easy: groups + max/min aggregates; forced: nothing.

**1b relocation targets** — correct (`cmp p1_reltarget.u32`) · 1 thr warm 5220-5426, e2e 7371-7657 · 8 thr warm
1080-1301, e2e 2669-2972 · 25 lines · easy: package-table gather as a join; forced: count aggregate for npkg,
negation for "no target -> 0"; slow (9.4 M x 4 B-tree joins), ~120x plain C.

**1c content-hash join** — correct (`cmp p1c_canon.u32`) · copy relations: 1 thr warm 1625-1865, e2e 2805-3127 ·
8 thr warm 587-718, e2e 1566-1761; rules over the slot columns: warm 1013-1069 / 394-437, e2e 3086-3256 / 1936-2107 ·
26 / 23 lines · easy: winner order as aggregates; forced: each group its own relation before an aggregate.

**2 reach** — correct (`cmp p2_live.u32`) · 1 thr warm 1227-1467, e2e 2571-2921 · 8 thr warm 431-579, e2e 1242-1429
· 47 lines · easy: every deadcode.go rule is one Datalog rule, incl. the conditional method rule; forced: static
joins hoisted out of the flood. ~30x C.

**2' reach with its own relocation decode (ATTACKS 1)** — correct · lazy (decode in the flood rule): 1 thr warm
2905-3214, e2e 4637-4982 · 8 thr warm 960-1610, e2e 2312-3081 · 65 lines · eager in Soufflé = 1b + 2: warm
~6.5 s / ~1.6 s. Lazy is expressible (semi-naive evaluation decodes only live sources: 1.73 M of 9.4 M) and beats
Soufflé's eager shape ~2x, but is ~30x / ~35-50x the eager C pipeline (108.7 / 30.1); and the relocation columns are
still bulk-loaded into a B-tree (the engine joins only over indexes).

**3a layout** — correct (`check_p3 $D oracle/p3_image.bin out/p3_addr.u32` -> 0 differ) · 1 thr warm 7222-7589,
e2e 7367-7717 · 8 thr warm 1339-1784, e2e 1474-1933 · 45 lines · forced: running sums as range aggregates over
earlier group members, unit walk as 1,763-iteration recursion, typelink sort as O(n²) "sum of sizes before me"
(numeric 8-byte prefix + string compare: 28 s -> 7.2 s); **falls down on speed** (~500x C; no sort/scan in Datalog).

**3b relocate** — correct (`check_p3 $D out/p3_image.out` -> P3 OK) · rules: 1 thr warm 1054-1115, e2e 2086-2139 ·
8 thr warm 320-451, e2e 1205-1374 · image assembly (C writer): 54-56 / 18-29 ms on 4 KB pages; on 2 MB pages
(ATTACKS 2) 20.0-34.7 / 7.8-16.2 ms, 39 / 150-172 minor faults vs 19,108 · 31 lines · easy: one rule per kind;
forced: bytes (copy + patch) in C; `site` inlined (5.2 s -> 2.5 s at load 11).
