# Futhark: problems 1-4 on MinIO (cells), then the install notes

Futhark 0.29.1, `c` backend (sequential) for 1 core, `multicore` backend for 8. All compute is in `p1.fut`,
`p2.fut`, `p3.fut` (+ `msort.fut`); the C drivers (`p1.c p2.c p3.c`, shared `drv.h`) only mmap the flat files,
hand them to the generated library (`futhark_new_*`, which copies), call the entries, time them, and compare
the outputs with the oracles. Build: `systems/futhark/build.sh` (`YIELD=1` also builds the non-spinning runtime,
`build/*_mcy.exe`). Rerun every timing: `systems/futhark/bench.sh` (`HUGE=1` for 2 MB pages).

## Surprises

0. **A sequential worklist written in Futhark beats Futhark's data-parallel flood, and halving the levels does
   nothing** (ATTACKS 3, results/levelfree.md, 17:40-17:51Z, load 17-26). The flood has **45 levels** (not 111:
   that is the C level-by-level count; this flood checks methods every level), **1,211 parallel operations**.
   Cell `2seq` (p2.fut `flood_hybrid` with the drain always on: a `loop` in host code, in-place updates, 45
   parallel ops of setup only): 1 core min 73-79 ms vs 137-141 for the original, 8 threads 67-130 vs 156-258
   (ranges apart, first window). The hybrid (`2hyb`: parallel levels while the frontier is >= 4 K, else a
   sequential drain; 15 levels, 437 ops, 40% of pops sequential) is within noise of the original. So the cost is
   the ~26 passes per element of a flattened level, not the number of levels. Second attempt, batched levels
   (`2bat`, a worklist loop inside a map over frontier chunks): 35-45 rounds, duplicate expansions, 1.5-2.5x
   slower. Trap on the way: a growable (`++`, existential-size) stack in the drain cost several memory-block
   reference-count updates per push in the generated C, 3x slower than a fixed-size stack.
0b. **(P4) Futhark does have a bulk copy into a slice, and it is 3-5x faster than the scatter shape 3b uses.**
   `row with [a : a + m] = xs[f : f + m]` in a sequential loop compiles to a memcpy (through the runtime's
   `lmad_copy`). Put that loop inside a `map` over 64 equal blocks of the output and the copy is data-parallel
   again: P4's copy of 60.7 MB costs ~20-35 ms of CPU at 1 core this way vs ~110 ms as 32-byte rows of
   (index, byte) scattered (PARTS=1 minus PARTS=0, cpu/rep, 17:56Z, load ~13). 3b's note "no bulk copy into a
   slice" holds only *inside a map*; a loop inside a map has one. **Applied to 3b (`3b_blk`): 2.7x at 1 thread,
   3.6x at 8**, see "3b in blocks" below. Use the multicore backend at 1 thread for it: the c backend compiles
   the same map over blocks to a copy 3.8x slower (98 vs 26 ms for 3b's copy alone).

1. **2 MB pages for the whole runtime are a 1.2-1.7x win on every cell, with no code change.**
   `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` makes glibc back its large mallocs with MAP_HUGETLB, and the Futhark
   runtime gets every array from malloc. Intermediate arrays are allocated per entry call and part of that
   memory goes back to the kernel between calls, so with 4 KB pages each warm rep pays 16,589 (1a) to 33,183 (1c) minor faults;
   with huge pages 18-67. Interleaved, 3 rounds, 1 core, 16:33Z, load 5.3-6.3 (min-max of the 3 rounds' medians):

   | cell | 4 KB pages, ms (faults/rep) | 2 MB pages, ms (faults/rep) |
   |---|---|---|
   | 1a | 143-158 (16,589) | 89-111 (38) |
   | 1b_scan | 74-94 (19,285) | 47-48 (37) |
   | 1c | 197-225 (33,183) | 115-133 (67) |
   | 2 | 122-157 (9,453) | 103-119 (24) |
   | 2lazy | 196-224 (3,785) | 167-200 (18) |

   End to end it matters more (the uploads of the inputs fault too): 3b process 0.59-0.61 s -> 0.38-0.42 s on 1
   core, 0.41-0.44 -> 0.17-0.22 s on 8 (104,142 -> 2,456 faults per process). This is ATTACKS 2's observation
   made general: for a runtime that allocates per operation, the page size is a first-order knob.
2. **The stock multicore runtime spins, and on this shared machine that costs more than the parallelism wins.**
   The join loop of every parallel operation and the scans' decoupled look-back busy-wait; Futhark has no
   wait-policy knob. `YIELD=1 build.sh` patches the generated C (sed, 7-26 `sched_yield()` per program) so a
   waiting thread yields. Interleaved P2 at 8 threads, load 10-12 (16:21Z): eager flood medians 156-310 ms
   spinning vs 120-297 yielding; lazy 235-459 vs 106-181. Under load 13 (bench at 16:30Z) neither helps:
   8-thread P2 is slower than 1 core. At 45 levels x ~26 parallel operations per level (1,211 per flood,
   counted: see Surprise 0), each one a join that waits for the slowest descheduled thread. Read all 8-thread numbers below as
   upper bounds measured on an oversubscribed machine.
3. **The binary search a data-parallel program reaches for first is the slow part.** "Which row owns item j" as
   a binary search per item (1b, 3b) vs a scatter of row starts + a max-scan: 1b 455-471 -> 47-103 ms on 1 core;
   3b 177-183 -> 111-120 ms (interleaved, 16:31Z, load 10). 9.4 M searches of 23 steps over a 20 MB offset
   array are cache misses; the scan is two streaming passes.

## Correctness (all OK, 2026-11-16, MinIO)

    cd systems/futhark && ./build.sh
    D=/home/exedev/paradigms/data/minio
    build/p1_c.exe $D 1 1 1a; build/p1_c.exe $D 1 1 1b_scan; build/p1_c.exe $D 1 1 1c    # -> P1a OK, P1b_scan OK, P1c OK
    build/p2_c.exe $D 1 1 2; build/p2_c.exe $D 1 1 2lazy                                  # -> P2 OK, P2lazy OK
    build/p3_c.exe $D 1 1 3a; build/p3_c.exe $D 1 1 3b                                    # -> P3a OK, P3b OK

(args: DATADIR THREADS REPS CELL; the same with `_multicore.exe $D 8 ...`.) Each driver compares the output
arrays element by element with `oracle/p1_canon.u32`, `p1_builtin.u32`, `p1_reltarget.u32`, `p1c_canon.u32`,
`p2_live.u32`, `p3_addr.u32`, and every placed symbol's bytes with `p3_image.bin`; 3b also counts relocation
errors (0). Variants 1b (binary search), 2lazy_look, 3a_loop, 3b_bytes also pass. As in the C references, 1b
and 1c take 1a's canon from its oracle, 3b takes 3a's addresses and `p3_addr_all`, 2lazy takes 1a's canon; each
cell stands alone.

## Timings

`bench.sh`, 2026-11-16T16:26-16:30Z (load 5.5 rising to 13.2) and `HUGE=1 bench.sh` 16:32-16:33Z (load 4.9-7.0),
MinIO. warm = entry only, inputs resident, repeated in one process (>= ~1 s); e2e = whole process, 5 runs.
min / median ms. 1 = `c` backend; 8 = `multicore`, stock (spinning) runtime unless marked yield. The 8-thread
rows were taken at load 5-13 on 8 cores: they measure the machine as much as Futhark (Surprise 2).

| cell | 1 core warm | 1 core e2e | 8 thr warm | 8 thr e2e | 1 core warm, 2 MB | 8 thr warm, 2 MB | 8 thr e2e, 2 MB | plain C 1 core / tuned C 8 |
|---|---|---|---|---|---|---|---|---|
| 1a join by name | 152 / 178 | 275 / 308 | 60 / 84 (yield 61 / 77) | 208 / 248 | 79 / 88 | 24 / 28 | 68 / 70 | 195 / 8.6-10.5 |
| 1b targets (scan) | 76 / 103 | 215 / 226 | 59 / 71 | 211 / 235 | 47 / 49 | 18 / 24 | 64 / 71 | 44 / 5.3-6.3 |
| 1b targets (binary search) | 459 / 471 | 628 / 645 | 86 / 96 | 267 / 300 | 455 / 465 | 70 / 83 | 136 / 142 | |
| 1c content join | 202 / 210 | 332 / 349 | 127 / 165 (yield 107 / 144) | 267 / 268 | 111 / 115 | 38 / 46 | 83 / 87 | 57-61 / 7.2-8.9 |
| 2 reach (p2_edge CSR) | 140 / 188 | 303 / 349 | 155 / 259 | 347 / 519 | 97 / 105 | 58 / 102 | 135 / 142 | 39-43 / 13-17 |
| 2lazy (decode in flood) | 303 / 346 | 996 / 1343 * | 241 / 412 | 1171 / 1499 * | 165 / 173 | 90 / 137 | 362 / 440 * | p2lazy 52.6 / 13.6-15.4 |
| 3a layout (scan) | 22 / 25 | 117 / 137 | 10 / 31 | 98 / 128 | 22 / 22 | 12 / 30 | 25 / 37 | 8-19 / - |
| 3a layout (loop) | 14 / 21 | 40 / 40 | 8.5 / 12 (yield 7.3 / 10.5) | 36 / 50 | 13 / 14 | 14 / 17 | 49 / 56 | |
| 3b relocate | 108 / 119 | 342 / 368 | 40 / 48 | 286 / 332 | 107 / 109 | 41 / 50 | 114 / 119 | 48.6-56 (4 KB) 23.9 (2 MB) / 13.4-14 (4 KB) 5.4-6.8 (2 MB) |

\* 2lazy's e2e includes its untimed-in-warm setup (aux_edges: the funcdata and kind 1-3 rows, 260-530 ms), as
ref/flat/p2lazy.c also excludes from its flood time.

Best 8-core warm (min, 2 MB pages): 1a 24, 1b 18, 1c 38, 2 58, 2lazy 90, 3a 7.3-12, 3b 41 ms (3b_blk: 23.5 ms,
below).

### 3b in blocks (`3b_blk`, IDEAS I18 applied, 2026-11-16T18:24Z)

`copy_blocks` in p3.fut: the 109 MB image cut into 64 equal blocks; a map over the blocks, in each a sequential
loop of slice updates (`row with [a:a+m] = dbin[f:f+m]`, a memcpy). Symbols are not in address order, so each
(block, symbol) overlap is listed by an `expand` over the symbols (a symbol spans `nblk` blocks), grouped by a
6-bit radix sort on the block number, and each block's range found by a scatter + min-scan. The patch is unchanged.
+20 lines. Correct: `build/p3_c.exe $D 1 1 3b_blk` and `build/p3_multicore.exe $D 8 1 3b_blk` -> P3b_blk OK.

`systems/futhark/bench3b.sh` (5 interleaved rounds, one process per (cell, config) per round, 8 warm reps each,
2 MB pages), 18:24:37-18:25:28Z, load 5.4 falling to 4.6; best min | per-process medians min / median / max, ms
(raw: `p3b-blocks-bench-2026-11-16T1824Z.txt`):

| cell | c backend, 1 core | multicore, 1 thread | multicore, 8 threads |
|---|---|---|---|
| 3b (64-byte chunk scatter) | 159.7 \| 161.0 / 166.7 / 169.1 | 175.5 \| 178.7 / 181.8 / 186.0 | 96.4 \| 107.8 / 110.9 / 113.6 |
| **3b_blk** (blocked copy) | 117.4 \| 119.5 / 123.0 / 137.1 | **60.4 \| 60.8 / 62.0 / 65.0** | **23.5 \| 29.8 / 30.5 / 40.3** |
| copy alone, chunks (`3b_copy`) | 120.5 \| 122.4 / 122.8 / 129.0 | 110.7 \| 111.4 / 116.1 / 118.9 | 51.9 \| 64.5 / 68.6 / 71.3 |
| copy alone, blocks (`3b_blkcopy`) | 97.9 \| 98.5 / 101.0 / 113.3 | 26.0 \| 26.4 / 27.7 / 38.9 | 11.8 \| 16.4 / 19.4 / 21.2 |

The ranges do not overlap: 3b 166.7 -> 62.0 ms median at 1 thread (best backend each: c for 3b, multicore for
3b_blk; 2.7x), 110.9 -> 30.5 at 8 threads (3.6x). The copy itself 116 -> 28 ms (1) and 69 -> 19 (8). (Old 3b ran
slower in this window than in the 16:3xZ table above: 160 vs 107 ms at 1 core; only the interleaved comparison
counts.) Against C (ref/flat/p3.c 3b on 2 MB pages: 23.9 ms, 1 core; tuned C 5.4-6.8, 8): 2.5x at 1, ~4x at 8,
was 4.5x / 7x. On the c backend the blocked copy is only 1.2x better: its generated code for the map over blocks
copies 4x slower than multicore's (98 vs 26 ms), not investigated further.

## ATTACKS 1 (reach counting its own decode)

- **Can Futhark express "decode a reference when the flood reaches it"? Yes.** `reach_lazy`'s `edges` function
  expands each frontier source's relocation range (`expand` over `slot_rel`) and maps the decode
  (package-base table, then canon) over the expanded relocations, inside the level loop; the graph is never
  materialized. The R_USETYPE skip rule (type 26 skips the next two) becomes a segmented scan of a 3-state
  machine's transition functions (function composition is associative), or a bounded look-back
  (`2lazy_look`, the second attempt: not faster, 108-130 vs 96-118 ms at 8 threads, 16:21Z).
- **Does it beat the eager C pipeline (108.7 ms 1 thread / 30.1 ms 8)? No.** 1 core: lazy 165-200 ms (2 MB
  pages) / 196-224 (4 KB). The eager Futhark pipeline would be its own 1b (47-48 ms) + a CSR build (not written)
  + the flood over p2_edge (97-119 ms), > 144 ms: also above 108.7. 8 threads: lazy 90-137 ms median, eager flood
  58-102, at load 5-7.
- **Why.** Futhark's flood is level-synchronous: 45 levels (counted, `reach_levels`), each ~26 data-parallel operations (filters,
  expands = scan + scatter + segmented iota, scatters, the dedup), each a full pass with its own allocation. At
  1 core the eager flood costs ~2 ms per level (45 levels, not the 111 assumed earlier); the lazy flood adds an expand and a
  segmented scan per level, so in Futhark lazy *loses* to eager (165 vs 97 ms) where in C it wins. What makes
  the lazy shape cheap in C (a sequential walk of a row, decode and mark in the same loop) is a nested,
  irregular loop with writes to shared state, which Futhark must flatten into separate passes.
- First tuning pass (before these numbers): the method rule checked every one of the 106 K methods every level
  (`filter call (iota M)`, the largest item in the profile); now a pending list holds the candidate methods
  that have not fired.

## ATTACKS 2 (relocate on huge pages)

- **Can the runtime put 3b's output on 2 MB pages? Yes, without touching the code**:
  `GLIBC_TUNABLES=glibc.malloc.hugetlb=2` (with `vm.nr_hugepages=512` reserved here). Futhark exposes no
  allocator hook; the C API offers no way to hand it a buffer (`futhark_new_*` copies).
- **Minor faults of 3b**: per warm rep 4,623-6,833 (4 KB) vs 18-80 (2 MB); per process (load + 1 run) 104,142
  (1 core) / 168,488 (8 threads) vs 2,456 / 2,798. The C reference faults 19,109 per rep (a fresh 109 MB image
  each time); Futhark's warm reps fault less because the image's memory is recycled (even with `FUT_FRESH=1`,
  which empties the runtime's free list, so glibc keeps it), so the warm number does not pay the image's first
  touch; the first rep of each process does.
- **Times**, 5 interleaved process runs each, 3 reps per process, 16:31Z, load 5.8-6.1:
  first rep (fresh image) 1 core 181-190 ms (4 KB) vs 119-150 (2 MB); 8 threads 126-137 vs 52-64; later reps
  1 core 112-126 vs 107-116 (within noise), 8 threads 44-67 vs 29-55.
  3b itself is 2-4.5x the C reference (C 1 core 49 / 24 ms on 4 KB / 2 MB pages): the copy and the patch are
  scatters of (index, byte) pairs (one per byte of data, 8 per relocation), which is how a scatter must be
  shaped in Futhark: there is no memcpy of a run into a slice of a shared array from inside a map.

## ATTACKS 3 (is the level count the cost?)

Answer: **no** (full write-up, counts and both timing windows: results/levelfree.md). Cells, all P2 OK
(`build/p2_c.exe $D 1 1 2hyb` -> P2hyb OK; `2seq`, `2bat` the same; knobs in env, p2.c):

| cell | levels / rounds | parallel ops | 1 core min / med / max | 8 threads min / med / max |
|---|---|---|---|---|
| 2 (original) | 45 | 1,211 | 137 / 171 / 313 | 156 / 201 / 258 |
| 2hyb lo 4 K hi 16 K | 15 + 2 drains | 437 | 161 / 197 / 280 | 129 / 171 / 335 |
| 2hyb lo 1 K hi 4 K | 16 + 2 drains | 463 | 116 / 119 / 168 | 102 / 178 / 339 |
| 2seq (one worklist) | 0 + 1 drain | 45 | 79 / 156 / 175 | **67 / 102 / 130** |
| 2bat C 64 B 1 K | 35-42 rounds | 1,576 | 220 / 340 / 555 | 211 / 370 / 490 |

(17:40Z, 5 interleaved rounds, load 17-20, 4 KB pages; `systems/futhark/levelfree.sh time`.) Easy: the drain is
a worklist `loop` with `with` updates, outside any map, so it compiles to plain sequential C; the frontier
handoff is one `ex` array. Forced: a fixed-size stack (bounded by hi + widest row + 3 x methods), because an
existential-size one costs refcount traffic per push (3x); the batched version's per-chunk buffers must be
initialised every round (no uninitialised arrays). Impossible: a shared visited set inside a parallel map
(the chunks of `2bat` each keep their own table and duplicate work). Lines: hybrid 155 (with comments), batch 131.

```futhark
              let c = stack[sp - 1]
              let sp = sp - 1
              let nd = if gotype c && iface[c] then 2u8 else 1u8
              in if nd <= ex[c] then (live, iface, ex, stack, sp, sig_seen, named, reflect, pbuf, np, pops, true)
              else
              let ex = ex with [c] = nd
              ...
                loop (live, iface, stack, sp, sig_seen, named) for e in i64.u32 eoff[c]..<i64.u32 eoff[c + 1] do
                  let t = i64.u32 edst[e]
                  let k = ekind[e]
                  in if k == 0 then
                       let (live, stack, sp) = mark_live live stack sp t
```

## Per cell: easy / forced / impossible

- **1a** (49 lines incl. the hash table). Easy: the winner rule as `reduce_by_index u64.max` over a packed
  `rank << 32 | ~id`. Forced: the hash table itself (no hash maps in the language; built by rounds of linear
  probing where `reduce_by_index i64.max` claims empty slots, no atomics). Impossible: nothing.
- **1b** (18 lines for the scan version). Easy: the decode is a pure map. Forced: each relocation's owner slot
  must be computed (scatter + max-scan); the first shape, a binary search per relocation, was 6-10x slower.
- **1c** (14 lines, reusing 1a's table). Easy, same shape as 1a with a different packed key.
- **2** (74 lines eager, +63 lazy). Easy: each rule is a scatter of `true` into a flag array (all monotone, so
  races are benign). Forced: the frontier as a getter function (a function argument cannot be size-polymorphic),
  the expand of rows each level, dedup by scatter-the-index-and-check; the R_USETYPE skip rule as a segmented scan
  of state-transition functions. Impossible: a cheap lazy decode (see ATTACKS 1).
- **3a** (86 lines + 26 msort). Easy: layout as a scan of "align then add" maps, which compose (adv_then; the
  scan is exact against the oracle). Forced: the typelink sort (the library's merge sort is GPU-shaped, 13 ms for
  33 K keys; a CPU-shaped bottom-up merge sort is 2 ms). The sequential cursor loop (`3a_loop`, in-place
  updates under uniqueness types) is as fast as the data-parallel scan here: 360 K symbols is too small.
- **3b** (60 lines; +20 for `3b_blk`). Easy: patch values as a `match` over kinds. Forced: the copy, either as a
  scatter of (index, byte) pairs in 64-byte chunks (per-byte binary search, `3b_bytes`, 480 ms) or, 2.7-3.6x
  faster, as a map over 64 image blocks with a sequential memcpy loop inside (`3b_blk`), which needs the
  symbols grouped by block (expand + radix sort). Impossible:
  choosing the output's page size from the language (done from outside, via glibc).

## Core code (excerpts)

The layout walk as a scan (p3.fut): an advance is `x -> rup(x + c, a) + d`; two compose into one.

```futhark
def adv_then ((c1, a1, d1): adv) ((c2, a2, d2): adv) : adv =
  let e = d1 + c2
  in if a2 <= a1 then (c1, a1, rup e a2 + d2) else (c1 + rup e a1, a2, d2)
...
  let ends = segmented_scan adv_then adv_id (map (\i -> i % (nunits * nc) == 0) (iota ncells)) steps
```

The lazy reach's edges of a frontier (p2.fut): expand the rows, decode where followed.

```futhark
    let rs = expand (\c -> if decodes c then i64.u32 (slot_rel[c + 1] - slot_rel[c]) else 0)
                    (\c j -> (c, i64.u32 slot_rel[c] + j)) proc
    let fol = followed (map (\(c, r) -> r == i64.u32 slot_rel[c]) rs) (map (\(_, r) -> rel_type[r]) rs)
    let rt = map2 (\(c, r) f -> if f then decode canon builtin pkg_off pkg (i64.u32 slot_obj[c]) rel_pkg[r] rel_idx[r] else 0) rs fol
```

The 1a winner (p1.fut):

```futhark
  let value g = u64.u32 rank[g] << 32 | u64.u32 (!(u32.i64 g))
  let win = reduce_by_index (replicate m 0u64) u64.max 0 (drop W slot :> [nn]i64) (map value named)
```

Lines (non-blank, non-comment): p1.fut 103, p2.fut 137, p3.fut 168, msort.fut 26; drivers p1.c 65, p2.c 57,
p3.c 87, drv.h 67 (loading, timing, checking only).

## Tools

`FUT_PROFILE=1 build/p2_multicore.exe ... 2> prof.json; python3 prof.py prof.json` sums the runtime's
per-operation report by source location. `FUT_TUNING=file` sets tuning parameters (`name=value` lines).
`FUT_FRESH=1` (p3) empties the runtime's free list before each rep.

# P4: code generation by shape (2026-11-16)

`p4.fut` (one entry, `emit`) + `p4.c` (driver: loads `data/minio-p4`, times, checks). Rebuilds all 60,733,909 bytes
of 136,634 bodies from templates + operands + glue, the function offsets, and the 1,780,281 relocations.

**Check** (every shape passes; the relocations are compared as a sorted list, as ref/p4/p4.c does):

    cd systems/futhark && ./build.sh p4
    D=/home/exedev/paradigms/data/minio-p4
    for c in 4 4g 4f 4b 4bf; do build/p4_c.exe $D 1 1 $c; done        # -> P4 OK, P4g OK, P4f OK, P4b OK, P4bf OK
    build/p4_multicore.exe $D 8 1 4bf                                    # -> P4bf OK

It compares the image with `oracle/p4_code.bin` byte for byte (memcmp), the offsets with `oracle/p4_fn_off.u64`,
the relocations with `oracle/p4_rel_*`, and counts holes with an encoding/width the encoder cannot write (0).

**The shape.** All in Futhark; nothing but loading and checking in C.
- Lay out: `item_src` per item (a map: from glue or a template, offset, length), `at = exscan lens`.
- Holes: a flattened map over (node, hole) pairs, made by `expand` over the 1.12 M nodes with each template's
  hole list (`class_holes`: a per-template filter of its holes, 8-byte immediates split into two 4-byte parts).
  A node's segments are items (checked: 1,640,416 = 1,640,416), so `seg_item[nseg_off[r] + s]` (one scatter) gives
  the item of segment s; position `P = at[item] + hole_at - seg_lo`, operand `node_op[r] + (h - tmpl_hole_off[t])`;
  rel32 = `at[v] + a - (P + 4)`, a gather. Each hole is a row of 4 (index, byte) writes scattered into the image.
- Relocations: three `expand`s concatenated (reloc holes over nodes, glue relocations over items, markers over
  functions), returned unsorted. In-language, natural.
- The copy, five shapes (`how`, cell names in p4.c):

  | cell | copy + patch | 1 core warm (2 MB pages) |
  |---|---|---|
  | `4` | scatter of 32-byte (index, byte) rows (3b's shape), then the patch scatter | 411 / 458 |
  | `4g` | gather: owner of every output byte by scatter + max-scan over 60.7 M, then the patch scatter | 535 / 569 |
  | `4f` | one scatter of 32-byte rows, holes written into each row by a loop | ~2x `4` (1.0-1.1 s CPU vs 0.5) |
  | `4b` | map over 64 blocks, sequential loop of slice copies (memcpy) inside; then the patch scatter | **348 / 376** |
  | `4bf` | map over 64 blocks; each item's slice copy then its holes, in the block's loop (C's loop, per block) | 357 / 424 (c), **326 / 374** (multicore 1 thread) |

**Timings.** `HUGE=1 CFGS="c:1:1 multicore:1:1mc multicore:8:8spin mcy:8:8yield" CELLS="4 4b 4bf 4g" bench.sh`,
2026-11-16T18:00:37-18:02:48Z, load 12.1 falling to 10.7; then 4 KB pages 18:02:48-18:03:43Z, load 9.8-10.7. Raw
output: `p4-bench-2026-11-16T1800Z.txt`. min / median / max ms, warm = the entry with inputs resident (5 reps at
1 core, 10 at 8), e2e = whole process (load 300 MB of flat files into Futhark + one run + copy out), 5 runs. The C
references ran interleaved after each cell (best lay out + best emit of 10 warm reps, 5 processes).

| cell | 1 core warm | 1 core e2e | mc 1 thr warm | 8 spin warm | 8 yield warm | 8 thr e2e (best cfg) |
|---|---|---|---|---|---|---|
| 4 | 411 / 458 / 578 | 508 / 522 / 545 | 426 / 435 / 481 | 220 / 245 / 337 | 229 / 272 / 348 | 383 / 396 / 440 |
| 4g | 535 / 569 / 593 | 576 / 638 / 675 | 513 / 556 / 628 | 374 / 434 / 547 | 254 / 304 / 346 | 398 / 464 / 530 |
| 4b | 348 / 376 / 417 | 440 / 455 / 475 | 355 / 396 / 426 | 189 / 210 / 249 | 200 / 228 / 294 | 310 / 359 / 373 |
| 4bf | 357 / 424 / 466 | 476 / 505 / 606 | **326 / 374 / 439** | 185 / 204 / 251 | **161 / 187 / 216** | 254 / 269 / 298 |
| 4bf, 4 KB pages | 515 / 554 / 811 | 882 / 924 / 931 | | 208 / 251 / 290 | | 507 / 533 / 563 |
| C ref/p4/p4.c (1) | 102-110 / 106-115 | | | | | |
| C ref/p4/p4par.c (8) | | | | 28-43 / 30-46 | | |

So: **3.2x the plain C at 1 core** (326 vs 102 ms min), **5.3x the OpenMP C at 8 threads** (161 vs 30), both on a
machine at load 10-12 (8-thread numbers are upper bounds, as for P1-P3: cpu/rep 525-750 ms at 8 threads means the
threads got ~3-4 cores). 2 MB pages: 1.4-1.6x at 1 core, 1.2x at 8, faults per warm rep 95 K -> 265 (Surprise 1 again).

**Where the time goes** (1 core, `PARTS` switches pieces off; cpu/rep, 17:46Z, load ~20, so only the proportions
mean anything): everything but the copy, patch and relocations (lay out, hole tables, the static check, zeroing
the image) ~100-120 ms; the 32-byte-row copy ~110; the patch scatter ~130; the relocations ~90-100. The blocked
copy takes the copy to ~20-35. gprof of the generated C (`-pg`, line-level, mapped back through the generated
`// p4.fut:N` comments) agrees: in `4`, the copy rows 28%, the patch rows 18%, expand's scans 12%, relocations 6%.
What is left is passes: every expand is a scan + scatter + max-scan + map over its output, and there are four
(patched holes 6.33 M, reloc holes 1.35 M, glue relocs 0.34 M, markers 0.1 M), each with its tuples materialized.

**Tuning that did not matter** (all within noise at load 15-20): chunk 8 / 16 / 32 / 64 for the row copy;
`--unsafe` (bounds checks off); multicore at 1 thread vs the c backend (equal within noise).

**First-version mistakes worth recording** (each measured, 1 core):
- The patch rows and a `reduce` counting bad holes were fused into one redomap, which materialized all 8 x 6.33 M
  (index, byte) pairs: removing the fusion (the check moved to the template table) took the entry 1.1 s -> 0.6 s.
- A `filter` over the 7.68 M (item, hole) pairs to pick the relocation holes was the single biggest operation
  (a scan of wide tuples); per-template class lists (a filter over 1.86 M template holes, done once) replaced it.
- The segment of each template hole by a linear walk over the template's segments: calls have hundreds; a binary
  search instead.

**The "commonest K templates, rest as glue" variant does not change the shape**: in this program a glue item and
a node segment differ only in where their bytes come from and whether they have holes; making rare-template nodes
glue moves bytes from one source array to the other. It needs a new extraction (the glue would hold their patched
bytes and their holes' relocations), not a different Futhark program, so it was not built.

**Easy / forced / impossible.**
- Easy: lengths by map, places by exclusive scan, jumps by gathering `at[v]`, relocations as expands (no sort
  needed: compared as a set). Every hole is independent, so the patch is one flat map.
- Forced: the copy. As 3b found, a map cannot write a run of bytes into a slice of a shared array, so the natural
  copy is a scatter of (index, byte) pairs, one per byte; the fast copy is a map over blocks with a *sequential*
  loop of slice updates inside (C's loop, 64 times in parallel), which needs the first item of each block (a binary
  search) and items clipped at block edges. Also forced: hole -> segment -> item needs three tables (per-template
  hole segments, per-node segment offsets, segment -> item scatter) that C gets from its loop position.
- Impossible: nothing. Slow: the flat version is ~4.5x C at 1 core; the blocked one 3.2x.

**Lines** (non-blank, non-comment): p4.fut 159 with all five shapes (the `4bf` path alone ~95: lay out 12, hole
tables 25, patch_item 12, copy_blocks 18, relocations 15, entry plumbing ~13); p4.c 122 (loading, timing, checking).

**Core code** (p4.fut). The holes as a flattened map over (node, hole) pairs:

```futhark
  let holes_of (idx, start) =
    expand (\r -> let t = i64.u32 node_tmpl[r] in start[t + 1] - start[t])
           (\r j -> let (h, k) = idx[start[i64.u32 node_tmpl[r]] + j] in (r, h, k)) (iota NN)
  let place r h =
    let (s, lo, _) = hseg[h]
    in (at[seg_item[nseg_off[r] + s]] + i64.u32 hole_at[h] - lo, i64.u32 node_op[r] + h - i64.u32 tmpl_hole_off[i64.u32 node_tmpl[r]])
  let value h k p o =
    if hole_enc[h] == 2 then (p, 4, at_of op_val[o] + op_aux[o] - (p + 4))
    else (p + 4 * k, i64.u8 hole_w[h] - 4 * k, op_val[o] >> (32 * k))
  ...
  let (wi, wv) = holes_of patched |> map patch |> map bytes |> flatten |> unzip
  in scatter code wi wv
```

The blocked copy (the fast shape): a map over blocks, C's loop inside each.

```futhark
def blit [S] (row: *[S]u8) (to: i64) (xs: []u8) (from: i64) (m: i64) : *[S]u8 =
  row with [to : to + m] = (xs[from : from + m] :> [to + m - to]u8)
  ...
  let block b =
    let lo = b * S
    let hi = i64.min total (lo + S)
    let (i0, _) = loop (l, h) = (0, n) while h - l > 1 do let m = (l + h) / 2 in if at[m] <= lo then (m, h) else (l, m)
    let (row, _) =
      loop (row, i) = (replicate S 0u8, i0) while i < n && at[i] < hi do
        let (fromt, o, l) = src[i]
        let a = i64.max at[i] lo
        let e = i64.min (at[i] + l) hi
        let m = i64.max 0 (e - a)
        let from = o + a - at[i]
        let row = if fromt then blit row (a - lo) tmpl_bin from m else blit row (a - lo) glue_bin from m
        let row = if fromt then fix i lo row else row
        in (row, i + 1)
    in row
  in take total (flatten (map block (iota P))) :> [total]u8
```

Reading: the flat version reads like the problem statement (a scan, a map over pairs, a scatter); the blocked
one reads like C wrapped in a map, and is the one that is fast. That is the P4 finding for this column.

## P4 from the flattened stream (`4D`, ATTACKS 4's control D, 2026-11-16T18:40Z)

Hypothesis (lead, from results/codex-cp7.md): flattening the emit stream (each item's bytes and its holes resolved
and rebased once, so the hot loop does no item -> template -> segment -> hole lookups) is 2.2x for C and should help
Futhark more. **Confirmed: 4.5x for Futhark (1 thread), 4.2x (8 threads), and the Futhark emit from the flat stream
beats the plain C template emit.**

- `prepare` (in Futhark, natural: it is the old emit's tables without the final scatter): the blob (the image
  with all holes zero, by the blocked copy), one patch record per integer or jump hole (absolute position, width,
  enc, value or target item, aux; 8-byte integers pre-split into two 4-byte parts), the relocations complete, `at`,
  the function offsets. Returned as an opaque record (`type stream`) that stays inside Futhark between the entries.
- `emit_flat`: `scatter (copy blob)` of one flat map over the 6.33 M patch records (rows of 4 (index, byte); a
  jump's rel32 by gathering `at[v]`), and the relocations copied out. 8 lines. Unlike p4runs D the rel32 targets
  still go through `at` (as D does too) and the relocations are copied, not pushed.
- Correct: `build/p4_c.exe /home/exedev/paradigms/data/minio-p4 1 1 4D` and `build/p4_multicore.exe ... 8 1 4D`
  -> P4D OK (same full check as every P4 cell).

`systems/futhark/bench4D.sh` (5 interleaved rounds; in each one process per program and thread count; Futhark 10
warm reps on 2 MB pages, prepare 3 reps; C `P4_REPS=32`, best of), 18:40:42Z, load 2.7, quiet. Best | min / median
/ max of the per-process medians (C: of the per-process bests), ms. Raw: `p4-flat-bench-2026-11-16T1840Z.txt`.

| | 1 thread | 8 threads |
|---|---|---|
| Futhark `4bf` (template emit, incl. lay out + relocations) | 283.8 \| 296.3 / 303.3 / 313.2 (mc), 310.5 \| 318.5 / 326.9 / 332.7 (c) | 95.3 \| 108.0 / 112.8 / 120.6 |
| **Futhark `4D` emit** | **65.1 \| 65.8 / 67.8 / 71.9** (mc), 72.5 \| 73.8 / 74.0 / 76.5 (c) | **23.4 \| 24.6 / 26.9 / 28.1** |
| Futhark `prepare` (the flattening) | 311.9 \| 317.8 / 323.8 / 331.8 (mc), 296.1 \| 297.6 / 304.7 / 376.2 (c) | 121.1 \| 130.8 / 138.4 / 151.6 |
| C ref/p4/p4.c (lay out + emit) | 106.2 / 107.5 / 108.8 | |
| C ref/p4/p4runs.c D emit | 48.4 / 48.7 / 48.8 | 10.4 / 14.2 / 14.9 |
| C p4runs D prep (single-threaded in both) | 343.5 / 359.7 / 369.7 | 342.3 / 347.1 / 385.0 |

- Flattening gains: Futhark 303 -> 68 ms (4.5x) at 1 thread, 113 -> 27 (4.2x) at 8; C p4 107.5 -> D 48.7 (2.2x).
- Gap to C: template emit 3.0x (1 thread, vs p4) / ~3.5x (8, vs p4par in earlier windows); flat emit 1.4x (1) /
  1.9x (8) vs D. Futhark's flat emit (68 ms) is faster than C's template emit (107).
- The flattening's cost: 324 ms (1 thread) / 138 ms (8) in Futhark, i.e. the old emit's work, done once; cheaper
  than C's prep (347-360 ms, serial). It pays off only if the stream is reused (or stored) over ~1.3 emits at 1
  thread; as one step per build, prepare + emit (392 ms) is no faster than 4bf (303). The stream is also big:
  blob 60.7 MB + patch records 6.33 M x 26 B = 165 MB + relocations 41 MB (C's D: 416 MB of columns).
- Why Futhark gains more: in the template emit its costs were the lookups' passes (hole -> segment -> item tables,
  four expands with materialized tuples), not the copies; flattening deletes exactly those passes and leaves a
  memcpy plus one flat map + scatter, Futhark's best shape. C's lookups were cheap pointer chasing inside one loop.

# Install notes (Futhark on this VM)

## Install (done 2026-11-16)

```
mkdir -p /home/exedev/tools/dl && cd /home/exedev/tools/dl
curl -sLO https://github.com/diku-dk/futhark/releases/download/v0.29.1/futhark-0.29.1-linux-x86_64.tar.xz
mkdir -p ../futhark && tar xf futhark-0.29.1-linux-x86_64.tar.xz -C ../futhark --strip-components=1
ln -sf /home/exedev/tools/futhark/bin/futhark ~/.local/bin/futhark     # ~/.local/bin is on PATH
```

- Version: `Futhark 0.29.1` (git 06a06af), prebuilt static binary, no Haskell toolchain needed.
- Backends used: `c` (sequential) and `multicore` (pthreads), both just need gcc (gcc present at /usr/bin/gcc).
  No GPU here, so cuda/opencl/hip are irrelevant.
- Also on disk: `/home/exedev/tools/fb/futhark-benchmarks-master` (futhark-benchmarks tarball, includes
  `lib/github.com/diku-dk/{segmented,sorts,containers,...}`), scratch benches in `/home/exedev/tools/fut-scratch`.

## Hello world: `hello/`

`hello.fut`: entry `hello (xs: [n]u32) : [3]u64` = inclusive prefix sum (scan, u64) + 65,536-bin histogram of
the low 16 bits (`reduce_by_index`) + checksums.  `driver.c` mmaps a raw little-endian u32 file
(MAP_POPULATE), hands the pointer to the generated library, runs 3 reps, prints a checksum.
`build.sh` builds both backends; `run.sh [N]` generates N random u32 (xorshift64*) and times.

How to call from C with raw arrays (the path for MinIO's columns):

```
futhark multicore --library -o hello hello.fut      # -> hello.c hello.h (+ hello.json manifest)
gcc -O3 -march=native -DMULTICORE -I. driver.c hello.c -lm -lpthread
```
```c
struct futhark_context_config *cfg = futhark_context_config_new();
futhark_context_config_set_num_threads(cfg, 8);                      // multicore only
struct futhark_context *ctx = futhark_context_new(cfg);
struct futhark_u32_1d *a = futhark_new_u32_1d(ctx, mmap_ptr, n);     // COPIES the data
struct futhark_u64_1d *out;
futhark_entry_hello(ctx, &out, a); futhark_context_sync(ctx);
futhark_values_u64_1d(ctx, out, host_buf);                           // copy out (also async: sync after)
```

## Timings (50 M random u32 = 200 MB; 8 cores; `date -u` = Mon Nov 16 15:22:41 UTC 2026)

Entry time only (input already resident), ms, 3 reps; upload = the copy in `futhark_new_u32_1d`.

| backend | threads | upload copy | run 0 | run 1 | run 2 |
|---|---|---|---|---|---|
| c | 1 (sequential) | 118.7 | 229.3 | 237.0 | 218.5 |
| multicore | 1 | 118.4 | 539.6 | 557.0 | 573.5 |
| multicore | 8 | 120.5 | 180.0 | 159.0 | 186.2 |

All three agree: sum=107369272683654617 ck_scan=3657172974595629368 ck_hist=1638382427737.

Reading: 8 threads give only 1.3-1.4x over the sequential C backend (and 3.3x over multicore at 1 thread).
Hardware floor for this work is ~200 MB read at ~90 GB/s = a few ms per pass, so Futhark is >30x off
the memory floor even in the best case. The multicore 1-thread penalty (2.4x vs `c`) is the cost of the
chunked scan (two passes) plus scheduler. Fused map/scan/reduce is not the problem; scan and
reduce_by_index with 65,536 bins (per-thread subhistograms, then merge) dominate (not profiled further).

Baselines from the library code (20 M elements, multicore, entry time only, 2 reps, same day):

| program | 1 thread | 8 threads |
|---|---|---|
| `radix_sort_int 32 u32.get_bit` (diku-dk/sorts), 20 M u32 | 1.95 s, 1.86 s | 0.71 s, 0.68 s |
| `containers/array` hash `dedup`, 20 M i32 (~4 M distinct) | 1.79 s, 1.74 s | 0.81 s, 1.12 s |

That is ~30 M keys/s sorted, ~20 M/s hash-inserted: two orders of magnitude below the 0.74 G random loads/s
the machine does. Sort-based and hash-based joins in stock Futhark libraries are not competitive; any Futhark
entry in the grid should be written with scan/scatter/reduce_by_index over flat columns, not these libs.

## Gotchas

- Tuple results of an entry become an opaque struct (`futhark_opaque_tup3_...`) in 0.29; the simple route is
  to return one array (`[3]u64`) or a record and use the generated projections.
- `futhark_new_u32_1d` copies (no zero-copy wrap of a host pointer on c/multicore); budget the copy
  (~120 ms for 200 MB here, ~1.7 GB/s; cause not investigated).
  Do inputs once, keep arrays resident, call entries repeatedly.
- Indices are `i64`; bounds checks are on by default (`--unsafe` turns them off)
  and may matter in gathers; untested here.
- Library mode needs `-lm -lpthread`; multicore `-DMULTICORE` is only our driver's guard around set_num_threads.
- `futhark bench`/`-t` report microseconds of entry time excluding input parsing; our driver measures wall clock
  around entry + sync.
- Threads default = number of cores; override with config API or `--num-threads` in the generated executable.
- `reduce_by_index` on multicore with a large bin count (65,536) is the likely cost; with up to ~4 M bins
  (symbol slots 5.2 M) the per-thread subhistogram strategy (O(H * T) memory) would hurt; test before using.
- `/tmp`-style reminder: `build/` holds the 200 MB data file and is gitignored.
