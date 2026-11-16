# Independent review of DuckDB report

- All 22 warm and 10 e2e summary rows report exactly five rounds, and all 32 triples match the generated table. Every benchmark-emitted cell appears at both thread counts; min/median/max ordering passes.
- The independent oracle log contains 14 successful run statuses and 20 zero-status oracle checks, covering P1 arrays, three P2 paths, layout, isolated relocation, and the P1→P3 pipeline at both thread counts. SQL agent additionally runs huge-page image checks.
- GRID denominators match exactly: 103–113, 272–375, 128–190, 103 prep, 1965–2354 loop, 217–263 layout, 339–368 normal relocation, 309–332 huge-page relocation. Ratios correctly divide kube median by historical range midpoint. Missing 1-thread/variant baselines remain explicitly absent.
- No missing samples or table errors found. `bench.sh` deletes the per-round temporary timing files, so this review verifies its reported five-round counts and final transcription; it cannot independently reconstruct medians from discarded individual samples.
- Baselines are historical cross-window comparisons under different machine load, not controlled dataset-only scaling.
