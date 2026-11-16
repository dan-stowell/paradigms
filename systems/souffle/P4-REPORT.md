# Soufflé on P4 (code generation by shape): report

Branch `worktree-agent-af694f1a3652305ad`. Code: `systems/souffle/emit.dl` (attempt 1), `emit2.dl` (attempt 2,
tuned), the `code=` writer in `flatio.h` (`writeCode`), `check_p4.c`. Long form: NOTES.md ("4 code generation by
shape", Surprises, Timings); cell block: end of CELLS.md; raw bench: `bench-p4-20261116T1919Z.txt`.

## Correct

Yes, both attempts, at 1 and 8 threads, every run of the bench: the 60,733,909 code bytes equal
`oracle/p4_code.bin`, the 136,635 function offsets equal `oracle/p4_fn_off.u64`, and the 1,780,281 relocations,
both sides sorted as (offset, type, symbol, addend, width), equal `oracle/p4_rel_*`.

    cd systems/souffle; P4=/home/exedev/paradigms/data/minio-p4
    ./build.sh emit.dl; ./build.sh emit2.dl; gcc -O2 -o build/check_p4 check_p4.c
    OMP_WAIT_POLICY=passive build/emit2 -j8 -F $P4 -D out && build/check_p4 $P4 out     # P4 OK (also build/emit, -j1)

The check can fail: the first version lost 37,703 relocations to set semantics (below), which check_p4's count
comparison reports.

## What Datalog computes and what C does

Rules (all of it relational):
- each item's length: a glue run's `goff(g + 1) - goff(g)`, a node segment's `hi - lo` from its template's segment
  ends (`seg(t, s, lo, hi)` from `tseg_end`);
- each item's place `at(i)`: a prefix sum, which Datalog has no operator for, and an aggregate cannot recurse.
  Shape: a radix-16 tree. `bsum(L, parent, c, n)`: block c of level L (items `[c 16^L, (c+1) 16^L)`), level 0 the
  lengths, levels 1-5 a range aggregate over `len` (non-recursive). `start(L, c) = start(L + 1, parent) + sum of
  the earlier siblings' bsum` (at most 15; an index range under the (level, parent) prefix), a recursion over the 6
  levels whose aggregate reads only the lower stratum; `at` is level 0. Cost 0.8 s of 7.2 s on 1 thread;
- each function's offset `at(fn_item[f])` (the oracle's nfn+1 entries come from an end item with length 0);
- each hole's segment (`h >= lo, h < hi`) and offset in it; per encoding, each node segment's holes with its
  operand: integer = the operand, rel32 = `at(v) + a - (P + 4)` (a join with `at` on the target item), relocation =
  a `reloc` tuple; glue relocations at `at(i) + off`; markers at the function's start + off;
- the copy list: `emit(0|1, at, src offset, n)`.

C (flatio `code="p4_code.out.bin"`, 42 lines): read the `emit(kind, at, a, b)` tuples into a vector; for kind 0/1
memcpy b bytes from glue.bin / tmpl.bin at offset a to position at; then for kind 2 store the low b bytes of a at
position at; write the file. No lookups, no arithmetic on positions. Relocations and offsets are written as columns
by flatio's ordinary writer.

## Times (ms, min / median / max of 5 interleaved rounds, 2026-11-16T19:19-19:23Z, load 7.2-9.8)

warm = flatio's compute (rules only: inputs already bulk-loaded, output writing and the C writer excluded);
e2e = process. 8 threads with `OMP_WAIT_POLICY=passive`. The load came from the other builders: 8-thread numbers
are upper bounds.

| | 1 thread warm | 1 thread e2e | 8 threads warm | 8 threads e2e |
|---|---|---|---|---|
| Soufflé attempt 2 (emit2.dl) | 7246 / 8316 / 9046 | 9642 / 11097 / 12282 | 2722 / 3814 / 4741 | 5056 / 6161 / 8002 |
| Soufflé attempt 1 (emit.dl) | 8781 / 9507 / 10096 | 11292 / 12254 / 12957 | 3328 / 3598 / 3858 | 5369 / 6100 / 6539 |
| C writer (copies + stores, inside e2e) | 85 / 107 / 112 | | 43 / 58 / 66 | |
| p4.c (lay out + emit) | 107 / 111 / 155 | 1062 / 1270 / 1491 | | |
| p4par | 106 / 112 / 134 | 1015 / 1168 / 1248 | 21.6 / 27.0 / 46.9 | 508 / 567 / 842 |

So attempt 2 is ~68x p4.c on 1 thread (min to min) and ~126x p4par on 8. The C e2e includes P4_REPS=5 iterations,
sorting and checking the relocations against the oracle, so it overstates the C. Soufflé e2e = load 1.2-1.6 s
(14.7 M tuples, 7.68 M of them operands) + compute + writing ~0.6-1.0 s (tuples to a vector 0.3-0.4 s) + exit.
Rerun: `ONLY='4|4-tuned|p4c' systems/souffle/bench.sh` (needs `ref/p4/check.sh` for build/p4, build/p4par).

Where attempt 2's time goes (souffleprof, 1 thread, 19:15Z): integer holes 1.74 s (5.63 M tuples), relocation holes
0.95 s (1.34 M), rel32 0.93 s (0.70 M), nseg 0.65, template copies 0.44, holeseg 0.43, at 0.41, bsum 0.30, len 0.24,
npos 0.25, glue copies 0.25, glue relocations 0.23, start 0.07. The engine's floor is the 8.65 M output tuples plus
1.78 M relocations, each a B-tree insert (~0.3 µs measured on the glue-copy rule, 726 K tuples in 0.25 s): ~3 s.

## The two attempts, and what lost

1. **emit.dl**: the problem written down, the hole join once, `inline`, so each of its three users (integer, rel32,
   relocation) re-runs item -> at -> template -> template holes -> operand from the item scan.
2. **emit2.dl**: node item -> template range joined once (`nseg`, `npos` with its place); each encoding's holes a scan
   of `npos` in output order with the encoding as an index key of `holeseg(t, s, enc, ...)`; `at` read off the tree
   directly. 1.15-1.2x faster than 1 on 1 thread (median 8316 vs 9507, min 7246 vs 8781 above); on 8 threads within noise
   at this load (min 2722 vs 3328, median 3814 vs 3598).

Tried and dropped, all P4 OK:
- Prefix tree without an explicit parent column (`p = c bshr 4` in the body): Soufflé scans every block per parent;
  did not finish in 100 s. With the parent as an index prefix: 0.07 s for `start`.
- Holes driven from the operand list (one scan of 7.68 M operands for all encodings) or from each encoding's template
  holes: random probes into `at`/`op`/places, 1.5x slower overall (rel32 rule 2.0 vs 0.93 s, integer 5.4 vs 1.7 s).
- Every hole materialized once (`hole`, arity 7): equal on 1 thread, 2.5x slower on 8 (7.5-8.3 vs 3.0-3.1 s):
  Soufflé keeps arity >= 7 relations behind an indirect index (flatio's bulk load does not even compile for it).
- emit and reloc as bries: 1 thread compute 14.9 vs 10.1 s, writing 4.3 vs 0.55 s (same window, load 23).
- Knobs already in use: compiled mode (`souffle -g` + g++ -O3 -march=native; `-c` is the same path), -j1/-j8, passive
  OpenMP wait, bulk-loaded indexes. Not applicable: subsumption and choice-domain (nothing to choose or subsume: every
  value is a function of its key), functors (a C functor holding a running sum would be C doing the scan).

## Verdict

Correct, slow; does not fall down. Datalog says the whole computation in ~60-75 lines, rel32 included, and the one
idiom that has to be invented, the prefix sum, is cheap (radix tree, 11%). What it cannot do is cheap per-tuple
output: every byte-run and every hole is a tuple inserted into a B-tree, and that, not the scan, is the ~70x / ~130x.

## Surprises

- The prefix sum is not the cost; the output tuples are.
- Set semantics vs a list oracle: 37,703 of 95,195 markers repeat another exactly and would vanish (flatio's loader
  dedups too: 57,492 loaded); the marker's id travels as a column.
- Join order by locality beats join count: three in-order scans beat one random-order scan.

## Lines

emit.dl 60 (81 with comments), emit2.dl 74 (99), the C writer 42, check_p4.c 59. Non-blank, non-comment.
