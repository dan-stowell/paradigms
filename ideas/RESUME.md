# Resuming a cell builder (paradigms repo)

You are resuming a builder whose session ended mid-work. Its worktree and branch keep everything it committed
(plus a "WIP (saved by lead)" commit of whatever was uncommitted, and possibly new uncommitted edits: inspect them
with `git status`/`git diff`, keep what is good, and commit it).

1. Read /home/exedev/paradigms/ideas/BUILDER.md (your brief, unchanged) and /home/exedev/paradigms/CHARTER.md,
   especially "Measuring on a noisy machine": make each timed kernel run >= 1 s by repeating it in-process,
   report min/median/max over >= 5 runs, note `uptime`. Five builders + Codex share 8 cores; numbers are ranges.
2. In your worktree: `git merge main` (main has new data contract fixes, p3 --pipeline special-symbol inputs
   p3_fixed_*, P4 data, ATTACKS.md). Then read your systems/<name>/NOTES.md, CELLS.md (if any) and `git log` to
   see which cells are done.
3. Read /home/exedev/paradigms/ATTACKS.md. Two questions your cells must answer, with a measurement:
   - ATTACKS 1 (reach): does your system's P2, *counting its own decode of the relocations from rel_* columns*,
     beat the eager C pipeline (108.7 ms 1 thread / 30.1 ms 8) and approach the lazy one (52.6 / 13.6-15.4)?
     Can your paradigm express "decode a reference when the flood reaches it", or does it force the graph to be
     materialized first? That is a finding either way.
   - ATTACKS 2 (relocate): can your runtime put the 3b output on 2 MB huge pages (C: 24 vs 49 ms 1 thread,
     5.4-6.8 vs 14 ms 8 threads)? Report the minor page faults (`/usr/bin/time -v` or getrusage) of your 3b.
4. Finish problems 1, 2, 3 per the brief (correct vs oracle, timings, NOTES.md, CELLS.md, bench.sh). Problem 4
   (ref/p4/README.md, data/minio-p4) is optional after 1-3.
5. Commit often on your branch; never push to main. Cap memory at 6G per process (`prlimit --as=6G`).
   When done, reply in five lines or fewer: cells correct, best 8-core warm times, falls-downs, branch name.

**Load warning (16:20Z).** With all builders running, OpenMP-style spinning barriers lose ~100x: p2par's 8-thread flood
(111 levels) went 14 ms -> 900-1300 ms with the default wait policy, 14-55 ms with `OMP_WAIT_POLICY=passive`. Any runtime
with spin-wait barriers or a thread pool (OpenMP, Futhark's multicore backend, Halide's thread pool, DuckDB's scheduler) can
show this. Report multi-thread numbers with the passive / non-spinning setting where one exists, say which, and treat an
8-thread time far above the 1-thread time as this effect, not as the paradigm's cost.
