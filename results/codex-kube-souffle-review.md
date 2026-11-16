# Independent review of Soufflé report

- All 16 warm rows report five runs; all 34 warm/e2e/assembly triples match the generated table. Every expected bench cell appears at both 1 and 8 threads.
- All 16 independent program-run statuses and all 18 oracle-check statuses are zero. Raw output has no CHECK FAILED marker. P3 checks all 430,349 placed symbols.
- Historical baselines and ratios match GRID. Review caught that GRID's 1c 394–587 is a range across two variants, not one variant's noise range. SQL agent corrected 1c to endpoint 587 and 1c-slot to endpoint 394, confirmed against NOTES; final ratios 3.82x and 4.88x.
- Raw run starts with a stray `build/prep_lazy prep_lazy.c` usage error; the next correct preparation succeeds with 1,592,404 edges. SQL agent removed the typo from the reproducer and retained the original evidence. This is a setup invocation error, not a cell oracle failure.
- `bench.sh` discards per-round raw compute samples after producing its five-run summaries, so independent review verifies summary counts and table transcription rather than recomputing summary medians from those discarded samples. Independent pre-bench oracle commands and statuses are retained.
