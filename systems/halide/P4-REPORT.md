# Halide on problem 4 (code generation by shape): report

Branch `worktree-agent-a3315ac9f2aa9aba3`. Long form: `systems/halide/NOTES.md` ("Problem 4"); grid block:
`systems/halide/CELLS.md` ("4. codegen by shape"); timings: `P4=only systems/halide/bench.sh`.

## Correct?

Yes. `HL_NUM_THREADS=8 systems/halide/cells/build/p4 /home/exedev/paradigms/data/minio-p4 e2e lay_t_lf emit_t_i2_v32_p1`
prints `P4 OK`: the 60,733,909 code bytes equal oracle/p4_code.bin (all 136,634 functions, offsets equal
p4_fn_off.u64), and the 1,780,281 relocations, sorted, equal oracle/p4_rel_*. Every schedule variant (4 layouts,
38 emit variants; the 4 `diag*` ones are diagnostics, left unchecked) passes at 1 and 8 threads, and so does the top-K
variant for K = 0, 256, 1024, 4096, 16384 and all. The check can fail: a top-K bug (items over 2^17 bytes) printed
"6 functions differ ... P4 FAIL" before it was fixed.

## Times (ms; min / median / max of 5; 1 thread = process CPU time, 8 threads = wall)

bench.sh at 2026-11-16T18:31:45Z (load 4.1-4.9); C references at 18:32:54Z (load 3.7-4.3).

| | Halide 1 | Halide 8 | p4.c 1 core | p4par 1 | p4par 8 |
|---|---|---|---|---|---|
| templates (template-only index) | 9.4 / 9.5 / 9.6 | 6.8 / 7.3 / 7.7 | - | - | - |
| layout | 17.9 / 18.4 / 18.7 | 4.9 / 5.4 / 5.6 | 6.5 / 6.9 / 7.0 | 7.3 / 7.4 / 7.4 | 1.3 / 1.7 / 2.0 |
| emit (best: scatter copy) | **157 / 160 / 162** | **23.0 / 23.4 / 25.1** | 103 / 107 / 112 | 97 / 98 / 99 | 16.4 / 18.5 / 20.1 |
| emit (gather form) | 237 / 243 / 251 | 55.6 / 60.3 / 72.4 | | | |
| e2e process, s | 0.21 / 0.23 / 0.24 | 0.07 / 0.08 / 0.10 | | | |

So emit is 1.5x the plain C on one core and 1.3x the tuned C on 8. Layout plus the template index is 3.9x on 1 and
7x on 8. e2e uses the output on 2 MB pages (`P4_HUGE=1`) and `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` for Halide's
own allocations. With fresh output and faults inside the run, the emit is 230-255 -> 184-189 ms on 1 thread and
93-121 -> 42-53 ms on 8.

## Lines

gen_p4.cpp 345 (Templates 49, Item helper 24, Layout 43, Emit 203 counting all variant forms; the best variant's
path is about 90), p4.cpp 288 (the top-K rewrite is 97, the checks about 50). Non-blank, non-comment.

## The shape, and what Halide made of it

- Layout: one Tuple scan gives both `at` (where each item goes) and `rat` (where its relocations go).
  **No C host is needed for the variable-length relocation list.** Because `rat` comes out of the layout, each
  relocation is a parallel, deterministic scatter to `rat[i] + k`.
- Emit is three per-item loops: copy, patch and relocations. The rel32 of a jump is a gather of `at[v]`.
- **Attempt 1, the scatter copy** (each item writes its bytes). Vectorizing it the obvious way (split + vectorize the
  byte RVar) took 26-28 s: a split RVar's `where` stops being a loop bound. It works as a 32-byte read-modify-write
  with two parity passes over the chunks, so that no two tasks write back neighbouring bytes. The copy alone is then
  31-34 ms for 60.7 MB in 2.38 M pieces.
- **Attempt 2, the gather copy** (each output byte fetches its own: a pure Func, parallel over tiles, race-free).
  Per tile it does a binary search, a scatter of the deltas and a running fill. It was correct first time and
  legible, but 2x slower (it costs a scan step and a byte gather per output byte).
- Patch: an 8-lane read-modify-write beats 8 scalar stores into scratch bytes, by 2.5x on 8 threads, because the
  scratch cache line is shared by every thread. Storing exactly w bytes (a `where` on the byte) is correct but takes
  10-16 s, because the hole loop loses its bound.
- **Main finding: Halide's select evaluates both sides, and that was the largest cost.** A glue item chased node ->
  template -> segment from its glue id, and an immediate read `at[its value]`. Indexing the untaken side with 0 took
  emit from 200-213 to 157-172 ms and layout from 26-32 to 18-19 ms.
- Where to compute the per-item facts is a schedule: inline, a vectorized table, or at each loop's item level. Once
  the select was fixed, all three were within noise of each other.
- Fusing the relocation loop with the patch loop via `compute_with` (the RVar had to be renamed to match) worked but
  gave nothing. Fusing copy with patch is refused, because they are two updates of one Func.
- Chunk size was within noise.
- Top K templates (`P4_TOPK`): the emit cost is per piece (about 42 ns per item) and per hole (about 5.8 ns), not
  per byte. Copying all 60.7 MB as 136 K glue runs takes 12.8 ms on 1 thread and 4.8 ms on 8; K = 4,096 is 25%
  cheaper than all templates. C agrees: swapping memcpy for 32-byte vector copies moves its emit by at most ~5%
  (`cells/p4_copy32.sh`; one window 5% apart, a second within noise). So C's cost is also the walk, not the bytes.
  This contradicts the earlier p4par note that "the copy is the cost". IDEAS.md C3 turns this into fewer, larger
  pieces, with a kill test.

## Gotcha worth passing on

`f.parallel(j, n).vectorize(j, m)` vectorizes the outer loop: parallel keeps the name `j` for the outer variable.
On a pure+update Func it crashed every warm-loop run, with indices from never-written values. Splitting explicitly
fixed it. gen_p3.cpp:181 has the same pattern, on a pure-only Func; it was not changed.

## Files

- systems/halide/cells/gen_p4.cpp, p4.cpp, build.sh (p4 case), bench.h (BENCH_CPU option), p4_copy32.sh
- systems/halide/NOTES.md (P4 section + 2 surprises), CELLS.md (P4 block), IDEAS.md (C3, C4), bench.sh (P4 part;
  its p4par reference line needs main's ref/p4/p4par.c, which this branch's older ref/ lacks; it is skipped if absent)
