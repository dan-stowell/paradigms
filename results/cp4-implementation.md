# CP4 implementation and initial checks

`ref/p4/gen_cp4.py` reads the original byte-template table, ranks templates by descending
node count with ascending template ID as the tie breaker, and emits `build/cp4_generated.h`.
It needs only Python's standard library. Generated data is intentionally not committed.
There are 4,096 template functions and a sparse 108,833-entry function-pointer table.
A multi-segment template has a switch on segment number; each arm is straight-line:
constant-length memcpy of literal bytes followed by constant-offset integer/jump stores
or relocation appends. Single-segment functions have no segment dispatch. Unaligned stores
use constant-size memcpy, avoiding aliasing/alignment UB.

`ref/p4/p4cp4.c` starts from p4par's layout, output allocation, function partition,
private relocation vectors, concatenation, and oracle check. Only hot node-segment emission
uses generated functions; cold node segments retain the generic loop and glue is unchanged.
The generator checks that no hole straddles a segment, and generation rejects unknown
encodings or widths. Runtime checks template-table cardinality; regenerate for each dataset.
The complete oracle remains the final dataset-specific validity check.

Reproduce (from repository root):

```sh
prlimit --as=12884901888 python3 ref/p4/gen_cp4.py /home/exedev/paradigms/data/minio-p4
/usr/bin/time -f 'compile_wall_seconds %e\ncompile_peak_kib %M' \
  prlimit --as=12884901888 gcc -O2 -march=native -fopenmp -o build/p4cp4 ref/p4/p4cp4.c
env OMP_NUM_THREADS=1 OMP_WAIT_POLICY=passive P4_REPS=2 P4_CP4_COVERED=1 \
  prlimit --as=12884901888 build/p4cp4 /home/exedev/paradigms/data/minio-p4
env OMP_NUM_THREADS=8 OMP_WAIT_POLICY=passive P4_REPS=2 P4_CP4_COVERED=1 \
  prlimit --as=12884901888 build/p4cp4 /home/exedev/paradigms/data/minio-p4
```

All three initial configurations passed all 60,733,909 code bytes and 1,780,281 relocations:
1 thread, 8 threads, and 1-thread `P4_CP4_GENERIC=1 P4_HUGE=0` fallback. The latter also
exercises the covered generic benchmark. Logs are `cp4-check-1.log`, `cp4-check-8.log`,
and `cp4-check-generic.log`. These short checks are not benchmark evidence.

Generated C: **2,916,263 bytes**. GCC 13.3.0 compile: **52.50 seconds**, peak RSS
**458,844 KiB** (`cp4-compile.log`); concurrent host builders were active, and the nearby
uptime snapshot was 2026-11-16 17:33:06 UTC, load **17.48, 14.52, 10.57**.
ELF `size`: text 1,225,920; data 871,556; bss 32 bytes.

The generator independently counts routing from item columns (`cp4-generation.log`):

| Path | Items | Copied bytes |
|---|---:|---:|
| Generated hot node segments | 1,141,371 | 21,718,974 |
| Generic cold node segments | 499,045 | 21,441,281 |
| Generic glue | 740,836 | 17,573,654 |
| Total | 2,381,252 | 60,733,909 |

Hot templates cover **837,365 / 1,122,309 nodes (74.61%)**, and 50.32% of node bytes.
Zero-length node segments count as items and execute the relevant function/loop.

For the covered-node kill test, set `P4_CP4_COVERED=1`. After the complete normal run and
its oracle comparison, the program gathers a list of hot item IDs in original order,
then times only these items using the same emitted code buffer and metadata. Gathering
is outside timing; all hot template lookup, indirect calls, copy/patch and relocation
appends are inside timing. Worker-local vectors are reset each iteration; there is no
marker/glue work and no final relocation concatenation in this diagnostic. One warmup
is excluded, then P4_REPS iterations report minimum and measured total. Code bytes are
rechecked afterward; covered-only relocation vectors are not compared to the full oracle.
The full emission that precedes this diagnostic *does* compare every relocation.

Set `P4_CP4_GENERIC=1` to disable generated functions in both full and covered phases.
The covered pair uses identical gathered items/traversal and differs only in generated
versus generic segment emission. This directly measures the 1.5× covered-node threshold;
whole emission versus p4/p4par should be reported separately. Sustained, interleaved
measurements and verdict are coordinated by the lead in `codex-cp.md`.
