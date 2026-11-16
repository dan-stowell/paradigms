# ATTACKS 3 in Halide: are the ~100 pipeline calls the cost of the flood?

Builder, 2026-11-16T18:50-19:24Z, owed to the reviewer (REVIEWS.md, M3 item 5; M4 item 5). The Futhark and Goose
tests are in [levelfree.md](levelfree.md). Every variant below reproduces `oracle/p2_live.u32` exactly (663,630 ids,
`0 differ`, P2 OK), in every counted and timed run (14/14 count runs, 70/70 timed processes, plus the attempt 1 runs).

**Rerun:** `systems/halide/cells/build.sh p2`, then `systems/halide/levelfree.sh counts` (calls, per-call and
empty-call costs) and `systems/halide/levelfree.sh 5` (interleaved timing with ref/flat/p2par). Raw output:
[halide-levelfree-counts.txt](halide-levelfree-counts.txt), [halide-levelfree-runs.txt](halide-levelfree-runs.txt).
**Load:** 6-10 on the 8 cores during the counts and timings (Soufflé and other builders), so the numbers are
provisional. 8-thread numbers vary the most.

## Verdict

**No. Batching levels does not help Halide**, because the calls cost nothing: one pipeline call is about
**0.02 ms**, so 98 calls are about 2 ms of a 300 ms (1 thread) or 75 ms (8 threads) flood. What each level does cost is
its fixed passes over the 325,340-entry block summary: the clear, the scan and the scatter that build the next
block list. An empty level costs 0.63 ms on 1 thread and 0.23 ms on 8, which comes to about 62 ms (21%) and 23 ms (30%)
of the flood. Those passes are per level, not per call, so a batched call still runs them once for every level
inside it. Halide cannot size a loop from data computed in the same call, so the later levels of a batched call
add work: either a dense loop over all blocks (attempt 1, 5-7x slower) or a fixed cap on the list (attempt 2,
equal or up to 1.2x slower). The cost is the same as in Futhark: passes per level, not the number of levels or calls.
The remaining 75-80% of the flood is per-element work (each op is 3 loop iterations, `info` per symbol) plus a
40 ms state init on 1 thread.

## (a) What the original costs (1 call = 1 level)

`p2 DATA count V` (5 floods, best; per-call timings from the best one). Eager is `flood_f_*` (over p2_edge), lazy is
`flood_t_*` (decodes relocations itself).

| | eager 1 thr | eager 8 thr | lazy 1 thr | lazy 8 thr |
|---|---|---|---|---|
| flood (best of 5), ms | 303.8 | 75.4 | 437.7 | 184.8 (load spike; 79-90 in GRID) |
| flood pipeline calls (levels) | 98 | 99 | 98 | 102 |
| method pipeline calls (rounds) | 7 | 7 | 7 | 7 |
| sum of the flood calls, ms | 248.4 | 61.8 | 382.3 | 162.3 |
| 44-47 calls with < 64 blocks, ms | 30.0 | 12.2 | 31.6 | 27.4 |
| **empty level** (full-size state, nact = 0), ms | **0.63** | **0.23** | 0.69 | 0.23 |
| **the call alone** (nid = 16: one-block state, nact = 0), ms | **0.02** | **0.02** | 0.02 | 0.02 |
| outside the flood calls: prep / init / methods, ms | 5.8 / 40.1 / 9.4 | 1.7 / 9.4 / 2.6 | 4.6 / 41.1 / 9.6 | 2.4 / 16.8 / 3.1 |

Passes per level in the original: **3 passes over the nblk = 325,340-entry block summary** (clear the next
parity: `st` update 0; prefix scan `pos`: serial, or chunked scan + serial chunk totals on 8; scatter of the
list: `list` update), plus the expansion. The expansion visits the listed blocks: 16 symbols x (`info`: 9
loads), and 3 iterations per op. Blocks per level range from 1 to 23,038 (181,706 in total). Tail calls with fewer
than 64 blocks cost 0.68-0.75 ms each, which is the empty-level cost. So 98 calls x 0.02 ms = 2 ms is the
synchronization cost, and 98 x 0.63 ms = 62 ms is the fixed cost of the per-level list passes.

## (b) The batched variants

`Flood` in gen_p2.cpp has `levels = k`: k levels per pipeline call, unrolled in the generator as consecutive
update definitions of `st`.

- **The cycle.** A Func cannot call a Func that calls it. So the block list of level i (built from `st` after level
  i - 1) cannot be a Func of `st`. It is built from a new input `stIn`, which the caller binds to the same buffer
  as `st`. The list is computed inside level i's own update stage, at an outermost RVar of extent 1, so it runs
  after level i - 1 has finished. Halide does not know about the aliasing. It is correct here (oracle-checked every
  run) because the list stage runs between the two update loops, but it is outside Halide's model.
- **The bound.** Level i's block loop must run `count_i` times. But "the bounds of an RDom may not depend on a call
  to a Func", and that includes an input buffer: reading the count back through `stIn` is refused too. `where
  w < count` is not hoisted. Halide places every predicate in the innermost loop, so the z loop and its `info`
  still run for every w.
- **Attempt 1 (`flood_f_f_dK`, original clear pass, later levels' block loop to nblk with `where w < count`):**
  correct, 1.38-1.49 s (k = 2) and 2.05-2.15 s (k = 4) on 1 thread at 19:23Z, against 0.30 s for the original. In an
  earlier window at higher load it was 1.9-4.5 s for k = 2, 4, 8. Every later level costs nblk x 17 iterations.
- **Attempt 2 (`flood_*_eK`, `endclear=true`):** no clear pass. A 17th "symbol" in each block clears that block's
  own summary byte after its 16 symbols, which is safe because a level pushes only to the other parity. Levels
  2..k visit at most `cap` blocks of their list: `cap` is a param, and a param may bound an RDom. Unvisited blocks
  keep their summary byte and are picked up two levels later. A new output `nleft` counts them so the driver does
  not stop early. `P2_HYB=1` sends lists of at least `cap` blocks to the `_e1` sibling, so only the narrow levels
  are batched. `_e1` (k = 1, endclear) is the control for "no clear pass".

Counts and best-of-5 floods (count mode, the same window as the timings):

| variant | threads | calls | first-list blocks | nblk passes per call | call alone, ms | empty call, ms | best flood, ms |
|---|---|---|---|---|---|---|---|
| original `f_f` | 1 | 98 | 181,706 | 3 | 0.02 | 0.63 | 303.8 |
| `f_f_e1` (no clear pass) | 1 | 98 | 181,706 | 2 | 0.02 | 0.59 | 302.6 |
| `f_f_e2`, cap 4096 | 1 | 61 | 82,445 | 2k + 1 = 5 | 0.37 | 1.51 | 328.5 |
| `f_f_e4`, cap 4096 | 1 | 32 | 48,222 | 9 | 1.10 | 3.31 | 331.0 |
| `f_f_e8`, cap 4096 | 1 | 17 | 28,771 | 17 | 2.63 | 6.96 | 354.9 |
| `f_f_e4`, hyb, cap 256 | 1 | 58 | 180,227 | 9 (narrow) / 2 | 0.08 | 2.30 | 319.0 |
| `f_f_e8`, hyb, cap 256 | 1 | 52 | 180,127 | 17 / 2 | 0.18 | 4.43 | 314.2 |
| original `f_t` | 8 | 99 | 180,918 | 3 | 0.02 | 0.23 | 75.4 |
| `f_t_e1` | 8 | 101 | 181,139 | 2 | 0.02 | 0.30 | 63.3 |
| `f_t_e4`, cap 4096 | 8 | 33 | 48,618 | 9 | 0.34 | 1.08 | 74.8 |
| `f_t_e8`, cap 4096 | 8 | 18 | 28,673 | 17 | 0.64 | 2.09 | 79.4 |
| `f_t_e4`, hyb, cap 256 | 8 | 58 | 179,607 | 9 / 2 | 0.09 | 0.98 | 64.1 |

"Call alone" for a k-level variant is the cost of the capped levels (cap x 17 iterations each, even with nothing
to do). The empty call is k levels of list passes. A batched call that empties partway still runs its remaining
levels. Levels per flood are not exact for k > 1: calls x k is an upper bound (106-136), because capped leftovers are
taken up later. Cap 1024 or smaller without the hybrid driver made the wide levels spill for many calls (k = 2,
cap 256: 399 calls, 728 ms), so the table uses 4096 unless the hybrid is on.

## (c) Timing, interleaved

`levelfree.sh 5`, 2026-11-16T19:16-19:23Z, load 6.1-9.8. Five rounds, each round one process per configuration
(bench.h: min over 5 runs of >= 1 s in-process), and p2par (OpenMP, `OMP_WAIT_POLICY=passive`, best of 20) in each
round. ms, min / median / max over the 5 rounds:

| configuration | 1 thread | 8 threads |
|---|---|---|
| original, eager (`f_f` / `f_t`) | **294 / 298 / 313** | **66 / 83 / 144** |
| `e1`: no clear pass, k = 1 | 300 / 312 / 365 | 81 / 91 / 107 |
| `e2`, cap 4096 | 323 / 343 / 405 | 83 / 108 / 140 |
| `e4`, hybrid, cap 256 | 299 / 310 / 402 | 73 / 95 / 125 |
| `e8`, cap 4096 | 344 / 349 / 351 | 75 / 97 / 149 |
| original, lazy (`t_f` / `t_t`) | 399 / 414 / 479 | 70 / 104 / 191 |
| lazy `e4`, hybrid, cap 256 | 420 / 433 / 446 | 84 / 106 / 188 |
| **ref/flat/p2par (tuned C)** | 26.7 / 28.4 / 34.2 | **10.3 / 11.6 / 16.3** |

What the numbers show:
- 1 thread, where the ranges are tight: no batched variant beats the original. Hybrid e4 ties it (299 vs 294
  min). e2 and e8 are 1.1-1.2x slower, and e8's range (344-351) does not overlap the original's (294-313). Cutting the calls
  from 98 to 17 costs time, because every batched level still runs its list passes, and the capped levels add
  cap x 17 iterations each.
- 8 threads: all ranges overlap (load). The original has the best min and median. The best count-mode floods
  of e1 and hybrid e4 (63-64 vs 75 ms) were not reproduced in the interleaved rounds (min 73-81 vs 66).
- Removing the clear pass (e1) saves nothing measurable (0.63 -> 0.59 ms per empty level on 1 thread). The clear is a
  vectorized memset. The scan and the scatter are the per-level cost.
- Halide stays 10-11x slower than p2par on 1 thread and 6-7x on 8 in the same window. The gap is the per-element
  expansion (75-80% of the flood) and the 40 ms single-threaded `init` of the 26 MB state, not the levels.

## What would cut the per-level cost (not built)

Each level's fixed cost is O(nblk) list building: Halide has no worklist and no append, so the next frontier is a
dense summary turned into a list by scan + scatter. Ways to shrink it are not about batching: a coarser block
(BLK 64: nblk / 4, but 4x the per-block symbol tests in each listed block), or a two-level summary (a scan over
nblk / 64 super-blocks, then only their blocks, at the price of a fourth phase per op to set the super-block byte,
on the expansion that is already 75% of the time). At most they would save the 62 / 23 ms of list passes, which still
leaves Halide 5-8x slower than p2par.

## What this adds to ATTACKS 3

Halide joins Futhark: **the level count is not the cost**. The calls are 0.02 ms each (2 ms per flood). The
per-level work that does exist (3 passes over the block summary, 21-30% of the flood) is per level wherever the
level runs, and Halide's rule that loop bounds come only from params (or the innermost `where`) makes the levels
batched into one call more expensive than separate calls. Batching cut the calls 98 -> 17-58 and was 1.0-1.2x
slower. Only C (spinning barriers under load) and Goose (supersteps waiting on a descheduled worker) pay for
synchronization itself.
