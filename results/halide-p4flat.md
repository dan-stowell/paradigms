# Halide P4 over the flattened emit stream (ATTACKS 4 step 1)

Builder, 2026-11-16T19:28-19:45Z. Code: `systems/halide/cells/gen_p4f.cpp` (EmitFlat, 59 lines),
`systems/halide/cells/p4f.cpp` (C prep + driver + checks, 213 lines). Timing: `systems/halide/bench4f.sh [ROUNDS [DATA]]`.
Raw output: [halide-p4flat-minio.txt](halide-p4flat-minio.txt), [halide-p4flat-kube.txt](halide-p4flat-kube.txt).

**Check.** `HL_NUM_THREADS=8 systems/halide/cells/build/p4f DATA all` checks every variant on both streams: the
code against oracle/p4_code.bin byte for byte, all nfn + 1 function offsets against p4_fn_off.u64, and the sorted
relocations against p4_rel_*. **P4 OK on data/minio-p4 and data/kube-p4, at 1 and 8 threads**:

- MinIO: 136,635 offsets, 60,733,909 bytes, 1,780,281 relocations, 0 differences.
- kube: 202,981 offsets, 74,522,346 bytes, 2,211,317 relocations, 0 differences.
- No FAIL in any of the 2 x 5 benchmark rounds.

## The representation

The prep is in C, serial and timed apart (as Futhark's 4D `prepare` is). It is p4runs' construct loop, which
resolves item -> template -> segment -> holes once. It produces:

- **blob**: the image with every patched field zeroed. Each item's run is at its own place, as in p4runs D.
- **Patch records** `(pos, width <= 4, enc, value, addend)`. An 8-byte integer becomes two 4-byte records, as in
  Futhark's 4D. A rel32 record keeps its target item. MinIO has 5,636,373 integer parts and 700,229 rel32.
- **The relocations**, complete: markers, glue and template relocation holes, 1,780,281.

**H1** (codex-holes) bakes the integers into the blob, which leaves only the 700,229 rel32 records.

The emit (EmitFlat) has no lookups:
- `code(x) = blob(x)`: a pure Func, vectorized 64 wide and parallel over 64 KB chunks.
- The patch: one update over (byte 0..3, record), a byte store each. Bytes past a record's width go to 4 scratch
  bytes past the code. A rel32 gathers `at[target]`. A second form, `_r32`, works when every record is 4 bytes
  (H1): one dense 4-lane store per record.
- `rel(k)`: the prepared columns, copied (as Futhark's 4D does).

Because the copy is one flat copy, **D and B are the same input for this emit**: per-item runs and per-function
runs tile the same blob. p4runs copies run by run, which is where its D and B differ.

## Times

`bench4f.sh 5`: five interleaved rounds, each round one process per (program, threads).
- Halide: wall time, bench.h min over 5 runs of >= 1 s, output on 2 MB pages.
- C: P4_REPS=32, best of, passive OpenMP wait.
- Futhark: 4D, multicore backend, min of 10.
- MinIO 19:36-19:40Z, load 3.8-6.3. kube 19:40-19:45Z, load 5.6-9.1.

Emit ms, min / median / max over the 5 rounds:

| program | MinIO 1 thr | MinIO 8 thr | kube 1 thr | kube 8 thr |
|---|---|---|---|---|
| **Halide flat, D stream** | **41.1 / 41.9 / 43.1** | **13.2 / 15.8 / 25.5** | 46.6 / 48.4 / 52.4 | 16.6 / 18.3 / 26.1 |
| **Halide flat, H1 stream** | 12.8 / 13.9 / 15.0 | 5.8 / 6.8 / 7.1 | 17.1 / 18.3 / 18.7 | 6.8 / 8.1 / 8.9 |
| **Halide flat, H1, `_r32` store** | **12.2 / 12.8 / 14.1** | **5.3 / 6.0 / 8.4** | 16.4 / 17.1 / 20.9 | 6.9 / 8.3 / 13.3 |
| Futhark 4D | 65.0 / 67.4 / 90.3 | 18.8 / 22.7 / 28.8 | 75.8 / 78.5 / 81.8 | 21.4 / 31.2 / 32.3 |
| C p4runs D | 46.1 / 46.3 / 47.7 | 13.3 / 13.8 / 14.3 | 52.0 / 53.2 / 53.8 | 14.5 / 18.1 / 23.1 |
| C p4runs B | 39.8 / 41.1 / 43.9 | 9.4 / 11.8 / 14.8 | 46.0 / 49.7 / 50.9 | 12.0 / 14.9 / 16.1 |
| C p4holes H1 | 26.9 / 27.6 / 29.2 | 6.1 / 7.7 / 10.4 | 33.0 / 33.2 / 35.2 | 8.2 / 11.2 / 11.4 |
| C p4par (lay out + emit, from items) | 102.7 / 103.8 / 112.5 | 20.3 / 24.7 / 33.1 | 114.8 / 122.8 / 131.8 | 26.5 / 28.7 / 36.3 |

For comparison, Halide's template emit (`emit_*_i2_v32_p1`, CELLS.md, 18:31Z, load 4-5) was 157-162 ms on 1 thread
(CPU time) and 23.0-25.1 on 8. **Flattening gives Halide 3.8x on 1 thread and 1.5x on 8** on MinIO. Futhark gained
4.5x and 4.2x.

**Prep** (serial C, not part of the emit): D 280-336 ms and H1 248-316 ms (MinIO: 280 / 248 on 1 thread,
296 / 316 in the 8-thread process; kube: 336 / 268). The emit is 14% of D's prep and 5% of H1's. As in codex-holes,
H1 moves work into the prep and does not speed up end to end. Here the H1 prep is no slower than D's, because
baking an integer costs the same as recording it.

Where the time goes (MinIO, `p4f DATA all`, 19:30Z, load 2.7; the diagnostic variants are not checked):

| | 1 thr | 8 thr |
|---|---|---|
| copy alone (`_copy`) | 7.1 | 2.6 |
| D: copy + 6.34 M records (`_norel`) | 40.4 | 14.5 |
| D: + relocation copy (full) | 42.5 | 16.4 |
| H1: copy + 0.70 M rel32 (`_norel`) | 13.0 | 5.8 |
| H1: full (`_r32`) | 14.2 | 6.1 |

On D the patch is **33 ms of 42**, about 5.3 ns per record on 1 thread (4 unrolled byte stores, a select per
byte). The copy is 7 ms (8.6 GB/s) and the relocation copy 2 ms.

## Reading

- **Flattening fits Halide even better than it fits Futhark.** The flat emit is a pure copy plus one scatter, with no
  dependent loads. Halide's D emit is 1.6x faster than Futhark's 4D on 1 thread (41.9 vs 67.4 median) and 1.4x on 8
  (15.8 vs 22.7). It matches C on the same stream: 1 thread 41.9 vs p4runs D 46.3 and B 41.1, 8 threads 15.8 vs
  13.8 / 11.8 (ranges overlap with D).
- This is not quite like for like with C. p4runs and p4holes collect the relocations during the emit (a push per
  relocation, per run) and copy run by run. Halide and Futhark copy prepared relocation columns and one flat blob.
  That moves part of C's emit work into the prep.
- **H1 changes Halide's picture much more than C's.** Baking the integers removes 89% of the records, and Halide's
  emit drops **3.0-3.3x on 1 thread** (41.9 -> 12.8-13.9 median MinIO; 48.4 -> 17.1-18.3 kube) and **2.3-2.6x on
  8** (15.8 -> 6.0-6.8). C's drop was 1.55x (codex-holes). The reason: Halide's flat patch costs about 5 ns per
  record and is 80% of the D emit, so the emit scales with the records. Once they are gone, what is left is
  bandwidth: the copy (7 ms), 0.7 M rel32 gathers (about 6 ms) and the relocation copy. With H1 the Halide emit on
  1 thread is 2x C's p4holes H1 (12.8 vs 27.6 median), and C does more inside its timed region (see above). On 8
  threads they are within noise (6.0 vs 7.7, ranges overlap).
- The `_r32` form (an exact 4-byte vector store per record, legal because every H1 record is a whole rel32) is
  4-8% faster than the general byte form.
- Easy: everything. Three Funcs, 59 lines, the schedules are split + vectorize + parallel. Forced: the scratch
  bytes for records narrower than 4, which is 3b's trick (a non-innermost where is not a loop bound), and the
  untaken select side reading `at[0]`. Impossible: nothing.
- On kube everything is 1.1-1.3x MinIO's time, in proportion to its 1.23x bytes and 1.2x records. The order is the
  same.
