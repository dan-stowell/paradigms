# Independent review of Halide report

- All 52 warm triples in `codex-kube-fh-tables.md` match the raw Halide bench log exactly. Each is the five-run statistic from `cells/bench.h` (calibrated repetition count per run), not five process runs. Min/median/max ordering passes throughout.
- P1 historical ranges, lazy/eager P2 mappings, and layout c3_w1 mapping match GRID.md; the precise layout schedule is confirmed by NOTES.md.
- Found one report bug: double spaces in `3b reloc_f0_pt_v32  warm` and `...  fresh 2MB` prevent single-space `startswith` checks from matching. Normalize whitespace before matching. Those two 8-thread rows should show 24.9–27 and 27–30.8 ms, ratios 3.33x and 2.72x respectively. Notified FH agent directly.
- Other schedules intentionally lack GRID-specific baselines; no fabricated comparison needed. Counts is setup and correctly has no baseline.
- Independent check log has zero failing exits; benchmark P2/P3 output also confirms zero differing live ids/addresses. The P3 check command covers canonical pipeline variant e2e 0 2; bench performs further layout checks. Raw `uptime` samples and command are retained.
