# Independent hole-experiment review

Reviewed `ref/p4/p4holes.c` on 2026-11-16. No blocking correctness issue found.
Only this clone was written; the original flat data and oracle were read-only.

## Reproducible independent counts

`python3 results/holes-review.py /home/exedev/paradigms/data/minio-p4`
reads the original flat columns directly, reconstructs function-relative hole
positions independently of the C prepared representation, and counts reuse,
widths, neighboring pairs, and greedy H2 groups. Output is
`results/codex-holes-independent-counts.log`.

Every result matches the C counter: 8,018,921 records, comprising 5,633,606
integers, 700,229 rel32s and 1,685,086 relocation records. Reuse is 1,135,878
previous equal values and 456,215 original-template byte matches, with union
1,452,746. There are 3,454,252 neighboring writable pairs whose entire span fits
in eight bytes with an eight-byte window still inside the function; 3,044,116
of these pairs contain only integers. H2's disjoint greedy grouping merges
3,527,790 integer holes into 1,756,297 groups, leaving 6,247,428 records.

These pair counts overlap and must not be presented as a count of distinct
holes removed. `gap_le_8` alone does not mean both holes fit in eight bytes.
Original templates have zeroed integer fields, so template-byte matches mean
low-width operand bytes already equal those zero bytes, not discovery of a
nonzero modal constant. Previous-value matches use exact full operand values
and the same template-hole ID within the function.

## Correctness and bounds

Built independently with:

```sh
mkdir -p build/holes-review
gcc -O1 -g -fsanitize=undefined -fno-sanitize-recover=all -fopenmp \
  -o build/holes-review/p4holes ref/p4/p4holes.c
for t in 1 8; do
  for m in H0 H1 H2; do
    OMP_NUM_THREADS=$t P4_REPS=1 P4_HUGE=0 \
      build/holes-review/p4holes /home/exedev/paradigms/data/minio-p4 "$m"
  done
done
```

All six runs pass without UBSan diagnostics; each checks 60,733,909 bytes,
136,635 function offsets, and 1,780,281 sorted relocations. Output:
`results/codex-holes-ubsan.log`. Independently ran H2 under each
`P4_FAULT=byte`, `offset`, `relocation`; every run returns nonzero and reports
`P4 FAIL`. Fault logs are under `build/holes-review/fault-*.log`.

H1 patches integer bytes only into prepared blobs and removes those records;
rel32s remain dynamic. H2's window never crosses a function boundary; masks
preserve non-hole bytes, including previously patched bytes if windows overlap.
Unaligned 64-bit accesses use memcpy, avoiding C alignment/aliasing violations.
In-place record compaction writes only at or behind the current input record,
so it does not overwrite future groups. Distinct workers own disjoint functions.

## Timing interpretation

The comparison retains B's output allocation, original-item quantile partitions,
relocation collection/concatenation, excluded warmup, and minimum-over-reps
protocol. H1/H0 move integer work into preparation, and H2 computes masks and
values there; report preparation separately. Original mappings and allocation
capacity remain resident despite compacted logical record counts.

H0 is a useful checked diagnostic, but its skip also removes width dispatch and
operand access, and its switch differs from baseline B. A B-to-H0 improvement
cannot isolate hardware store latency from dispatch/metadata effects. Neither
B nor these variants retain the original nested template lookups in the hot
loop. Without working counters, use "metadata traversal/dispatch and memory
traffic" rather than claiming measured branch misprediction or store throughput.
The count-enabled constructor includes per-hole counting overhead; only the
later adjacency analysis is outside its printed preparation total. Performance
runs must disable `P4_COUNTS` and not use the count-run preparation number.
