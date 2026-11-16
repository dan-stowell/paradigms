# Brief for a cell builder (paradigms repo)

Read, in order: /home/exedev/paradigms/CHARTER.md (the goal and rules: legibility counts, "falls down" needs two
serious attempts with the system's own idioms and tuning), /home/exedev/paradigms/ref/flat/README.md (THE data
contract: every file and every rule of problems 1-3), and the plain C references ref/flat/p1.c, p2.c, p3.c (the
executable form of the rules; each checks itself against the oracles). Your system's install notes are in
systems/<name>/NOTES.md (from an earlier installer; versions, paths, gotchas, hello-world timings).

Data: /home/exedev/paradigms/data/minio/ (1.1 GB, read-only, NOT in git: always use this absolute path, also from
your worktree). Oracles are in data/minio/oracle/. The plain C references, 1 core, warm: P1a join 195 ms + 1b
targets 44 ms, P1c hash join 300 ms, P2 flood 55 ms, P3a layout 20 ms, P3b relocate 63 ms. hlink (tuned C
reading raw objects, 8 cores) phase times are in /home/exedev/targets/bottom-up/FLOOR.md.

Your job: cells of the grid for your system, problems 1, 2 and 3 (sub-parts 1a/1b/1c, 3a/3b count separately
in your notes). For each cell:
1. A correct implementation in your system's own idiom (not C in disguise; if you must drop to C for I/O, keep it
   to loading the arrays and say so). Its oracle check must pass exactly: compare your output arrays with the
   oracle files (write a tiny checker or dump your output and `cmp` it), and put the check command in NOTES.md.
2. Time: 1 core and 8 cores; warm (inputs already in memory, the compute only) and end to end (process start,
   reading the flat files, compute, output). Report absolute ms with a `date -u` timestamp, best of >= 5 runs.
   Other builders share this 8-core machine, so your numbers are provisional: write systems/<name>/bench.sh that
   reruns every cell's timings and prints one line per (cell, cores, warm/e2e) so the lead can rerun them on a
   quiet machine. Check `uptime` and note the load next to your numbers.
3. Tune with the system's own knobs (schedules, indexes, backends, parallel modes) before calling anything slow.
4. Write systems/<name>/NOTES.md (long form): per cell what was easy, what had to be forced, what was impossible
   (one line each), lines of code, the timings, and an excerpt of the core code so a reader can judge how it reads.
   Also write systems/<name>/CELLS.md: one short block per cell in GRID.md's format (correct? + check command,
   1-core/8-core warm/e2e ms, lines, easy/forced/impossible), for the lead to paste into GRID.md.
5. A cell "falls down" only after two serious, idiomatic, tuned attempts, with the reason written down. A toy is
   not an attempt. If something surprises you (your system beats the C reference, or fails for a reason nobody
   predicted), write it at the top of NOTES.md under "Surprises" with the measurement.
6. Ideas that a measurement suggests (a new way to shape the problem): add to systems/<name>/IDEAS.md with
   the evidence.

Rules: work only in your worktree; commit often with real messages on your branch (do not push to main; the lead
merges). Never modify /home/exedev/targets/* or data/. Cap memory: run anything big under
`prlimit --as=12G` or `systemd-run --user --scope -p MemoryMax=12G` (31 GiB machine shared by 4 builders, no swap).
Don't stop at the first correct cell: do problems 1, 2 and 3 all the way (or to a written "falls down"). When
done, reply in five lines or fewer: cells correct, best 8-core warm times, falls-downs, branch name.
