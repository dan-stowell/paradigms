# Independent review of final Futhark table

- Reparsed all 135 raw warm lines independently: exactly 27 (cell, configuration) keys, exactly five process medians each. All min/median/max triples match the rendered table; no missing bench cell/configuration.
- All 27 initial oracle commands have EXIT 0 and no FAIL. Driver checks omit `nocheck`, while timing commands use it as documented.
- GRID baselines and ratios match: 1b_scan is GRID's scan-based 1b; binary-search 1b has no GRID entry. Layout-loop 14/7.3 and scan 22/10 reproduce GRID exactly, despite different page/runtime conditions in historical detailed NOTES (already disclosed).
- Ratios correctly compare kube medians with GRID's historical single/minimum numbers, not with like-statistic fresh MinIO measurements. The final report explicitly discloses this limitation and huge-page fallback.
- No further correction required. This verifies the nine bench.sh cells; NOTES also mentions non-bench variants `2lazy_look` and `3b_bytes`, which are outside this table.
