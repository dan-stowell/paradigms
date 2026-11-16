# ATTACKS 3 addendum (draft): is the level count the cost, in Futhark and in Goose?

Builder, 2026-11-16T17:20-17:55Z. Scope (ATTACKS.md 3, after M2): build a batched-level or level-free flood in
each system, count its mechanism (levels, rounds, parallel operations, barriers, messages) and time it against the
system's original, interleaved. All variants reproduce `oracle/p2_live.u32` exactly (P2 OK, 663,630 ids).

**Load.** The whole window ran at load **17-26** on the 8 cores (other builders: duckdb, cc1, dk, futhark).
Every time below is min / median / max over 5-7 interleaved rounds (each round one process per configuration,
min of its in-process reps); 8-thread numbers are oversubscribed. Rerun: `systems/futhark/levelfree.sh counts`,
`systems/futhark/levelfree.sh time 7`, `systems/goose/levelfree.sh 7`.

## Two corrections to the premise

- **Futhark's flood has 45 levels, not 111.** 111 is the C level-by-level count (results/levels.md: methods
  checked once per round, 7 rounds); Futhark's flood checks the method rule every level, so method firings join
  the next level instead of opening a new round. Measured with a counting entry (`reach_levels`): 45 levels,
  **1,211 parallel operations per flood** (FUT_PROFILE task count), i.e. 43 of setup and **~26 per level**.
- **Goose's p2par has 10 supersteps, not 111.** Each worker already drains its partition to a local fixpoint
  (method rule included) before it exchanges: a superstep is a cross-partition wave. MinIO, 8 workers:
  10 supersteps, 1,036,250 ids in 560 inbox batches (W x (W-1) per step) + 80 status + 80 go messages.

## Futhark

Attempt 1, **hybrid** (`2hyb`, p2.fut `flood_hybrid`): a data-parallel level while the frontier is >= lo, else a
sequential worklist drain (a `loop` outside any map: Futhark compiles it to plain host C, no parallel operation)
until its stack exceeds hi (or after `budget` pops), which then hands its stack back as the frontier. One
array `ex` (expanded as plain / as live iface gotype) lets the two modes share one frontier. `2seq` is the limit
case: one drain, the whole flood a sequential worklist. Attempt 2, **batched levels** (`2bat`, `flood_batch`): the
"loop inside the map": each round cuts the frontier into C chunks, each chunk runs a sequential worklist over
plain edges for B pops into its own buffers (edges walked, symbols expanded), and the round applies the rules to
all of it with the level's scatters.

Counts (MinIO, deterministic except bat's duplicates):

| variant | data-parallel levels / rounds | drains | parallel ops / flood | pops in drains | symbols expanded |
|---|---|---|---|---|---|
| orig (`2`) | 45 | 0 | 1,211 | 0 | 678,996 |
| hyb lo 0 (control: ex bookkeeping only) | 45 | 0 | 1,213 | 0 | 678,996 |
| hyb lo 1 K, hi 4 K | 16 | 2 | 463 | 271,779 (40%) | |
| hyb lo 4 K, hi 16 K | 15 | 2 | 437 | 273,043 (40%) | |
| hyb lo 4 K, budget 2 K pops | 15 | 134 | 701 | 273,043 | |
| `2seq` (one worklist) | 0 | 1 | 45 (setup) | 670,228 (100%) | |
| bat C 64, B 1 K | 35-42 rounds | | 1,576 | | 700 K distinct, 861 K with chunks' duplicates; 3.3 M edges walked vs 2.5 M |
| bat C 8, B 4 K | 45 rounds | | 1,943 | | 778 K with duplicates |
| bat C 1, B unbounded | 25 rounds | | | | (25 = the floor set by iface/method rules, which a chunk does not follow) |

Times, ms (warm, `c` backend = 1 core, `multicore` stock runtime = 8 threads), two interleaved windows:

| variant | 1 core, 17:40Z (5 rounds) | 8 threads, 17:40Z | 1 core, 17:47Z (7 rounds) | 8 threads, 17:47Z |
|---|---|---|---|---|
| orig | 137 / 171 / 313 | 156 / 201 / 258 | 141 / 265 / 416 | 144 / 245 / 388 |
| hyb lo 0 | 147 / 176 / 260 | 146 / 187 / 242 | | |
| hyb 1 K | 116 / 119 / 168 | 102 / 178 / 339 | 120 / 178 / 312 | 138 / 184 / 288 |
| hyb 4 K | 161 / 197 / 280 | 129 / 171 / 335 | 116 / 161 / 246 | 152 / 231 / 273 |
| hyb 4 K, budget 2 K | 166 / 212 / 382 | 209 / 234 / 240 | | |
| `2seq` | 79 / 156 / 175 | **67 / 102 / 130** | 73 / 131 / 181 | 88 / 112 / 160 |
| bat C 64 | 220 / 340 / 555 | 211 / 370 / 490 | 222 / 356 / 474 | 263 / 355 / 540 |
| bat C 8 | 199 / 313 / 687 | 159 / 266 / 662 | | |

Reading:
- Cutting levels 45 -> 15 (parallel ops 1,211 -> 437) is **within noise** at both widths. If the per-level cost
  were the cost, the hybrid would sit near `2seq`; it sits near the original.
- A **sequential worklist in Futhark beats the level-synchronous flood**: 1 core min 73-79 vs 137-141, medians
  131-156 vs 171-265 (ranges overlap); on 8 threads `2seq` (which uses one) beats the 8-thread flood with ranges
  apart in the first window (67-130 vs 156-258) and overlapping in the second (88-160 vs 144-388).
- So **in Futhark the cost is per element, not per level**: a data-parallel level touches each frontier item and
  each edge in ~26 passes (filters, expands = scan + scatter + segmented iota, scatters, the dedup), each with
  its own allocation; a sequential pop touches them once. At 1 core (no barriers at all) the 15 wide levels that
  remain in the hybrid (60% of the pops) cost about as much as the whole sequential flood. On 8 threads,
  oversubscribed, the wide levels' parallelism does not repay their passes.
- Attempt 2 loses (1.5-2.5x slower than orig): rounds barely drop (35-45; 25 even with one unbounded chunk,
  because iface and method rules still need rounds), chunks re-expand symbols that several of them reach
  (up to +27% expansions, +32% edges), and every chunk's fixed-size buffers are initialised each round (Futhark
  has no uninitialised array: a `replicate` per buffer per round).
- A Futhark-specific trap found on the way: the drain's stack first grew by `++` (an existential-size loop
  variable); the generated C then did several memory-block reference-count updates per push, and the drain ran
  **3x slower** (255 vs 76 ms, 1 core). A fixed-size stack (bounded by `hi + widest row + 3 x methods`) fixed it.

**Verdict, Futhark: no.** The level count is not the cost; the per-element passes of the flattened level are.
The level-free version that wins is the one with no data parallelism at all (`2seq`), which is also the
shortest-reading one (a worklist loop with in-place updates), but it is a sequential program written in Futhark.
Halving the levels changes nothing measurable.

## Goose

**p2async.goose** (233 lines + p2rules.goose shared with p2par): the same owner partition and local rules,
no supersteps. A worker floods its part to a local fixpoint (or until `P2FLUSH` ids for others wait), posts
what it found for others straight into their inboxes (ids with op bits; any new global fact, i.e. signature,
method name, reflect, broadcast to every worker), takes whatever has arrived with `qpoll`, and goes on. Idle, it
reports its per-peer sent/received batch counts to main and blocks on its inbox. Main stops the flood when every
worker's latest report is idle and sent[i][j] == received[j][i] for all pairs (sound because a worker reports
only when idle and only wakes on a message, so an in-flight or unreported message always shows as a mismatch).

Counts (MinIO, 8 workers, one flood; async counts vary by scheduling):

| | barriers (supersteps) | inbox batches | ids sent | control messages |
|---|---|---|---|---|
| p2par (BSP) | 10 | 560 | 1,036,250 | 80 status + 80 go |
| p2async | **0** | 482-687 | 1,036,064-1,036,337 | 102-174 idle reports + 16 start/stop |
| p2async, flush every 4 K ids | 0 | 1,715-2,000 | 1,023,920-1,036,035 | 196-368 idle reports |

Times, best flood ms, min / median / max (interleaved; load 21-23 at 17:45Z, 17-21 at 17:51Z):

| | 17:45Z, 5 rounds | 17:51Z, 7 rounds |
|---|---|---|
| BSP, 1 worker | 34.6 / 86.0 / 106.8 | 44.0 / 47.9 / 59.9 |
| async, 1 worker | 46.5 / 73.6 / 105.2 | 44.6 / 48.4 / 106.7 |
| BSP, 8 | 48.1 / 52.9 / 62.5 | 33.8 / 48.0 / 80.0 |
| **async, 8** | **33.4 / 38.9 / 44.2** | 28.2 / 37.4 / 46.6 |
| async, 8, flush 4 K | 30.1 / 33.8 / 35.7 | 25.0 / 34.5 / 54.9 |
| BSP, 8, lazy decode | 66.7 / 95.4 / 103.0 | 64.0 / 84.7 / 119.8 |
| async, 8, lazy decode | 60.4 / 64.6 / 76.6 | 47.0 / 52.9 / 63.7 |

Where the time goes (P2TRACE, one 8-worker flood at load 18, 17:53Z): BSP flood 54.5 ms, each worker drains
4-23 ms of it and waits the rest; async 28.0 ms, per worker local 4-20 + flush 0.5-10 + absorb 0.1-1.9 ms, wait
7-23 ms. Summed over workers the partitioned work is ~70-80 ms against 43 ms for one worker (cross-partition ids
are applied twice: once as an edge, once as a message; and the 1 M ids are bucketed and copied through queues).

Reading:
- Dropping the 10 barriers helps under this load: async 8 beats BSP 8 by **1.3-1.4x in median** with ranges apart
  in the first window (33-44 vs 48-63), overlapping in the second; lazy decode 1.4-1.6x (ranges touch). Same
  mechanism as codex-g's C result: at every barrier all workers wait for whichever one was descheduled.
- At 1 worker the two are the same program (within noise), so the async machinery costs nothing.
- But scaling stays poor: medians async 8 workers 37-39 vs 1 worker 48-74, BSP 48-53 vs 48-86. The busiest
  partition's own work (17-21 ms) and the 1 M ids crossing partitions (flush up to 10 ms per worker) bound it,
  not the superstep count.

**Verdict, Goose: partly.** It was never 111 supersteps (the BSP version already floods each partition to a local
fixpoint: 10 cross-partition waves). Removing those 10 barriers is worth ~1.3-1.5x at load ~20, but the limit on
8 workers is the partition: duplicated work across owners, one busy partition and a million ids through queues.

## What this adds to ATTACKS 3

The level count is a cost of *synchronization*, and only where a barrier waits on a descheduled thread (C p2par
with spinning waits; Goose's supersteps, ~1.4x). In Futhark it is not the cost: the flattened level's per-element
passes are, and a sequential worklist in the same language is 1.5-2x faster at 1 core and no slower than 8
threads. The hybrid shape (wide levels parallel, tail sequential) cuts levels 3x in Futhark but saves nothing,
because the tail held only 40% of the pops and the expensive part is the wide levels' per-element passes.
Still owed: the same runs on a quiet machine (every number above is at load 17-26).
