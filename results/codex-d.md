# Eager relocation decode + CSR + reach

Written: `Mon Nov 16 16:02:18 UTC 2026`. Measurement window: Mon Nov 16 16:00–16:01 UTC 2026; exact `date -u` stamps per configuration/round are in [raw runs](codex-d-runs.txt).

**No win for this implementation on MinIO:** best-of-five warm totals are 108.657 ms (1 thread) and 30.063 ms (8 threads), versus the supplied hlink lazy-flood measurements of 98–103 / 22–23 ms. Even this partial eager pipeline loses; a faster prebuilt-CSR flood alone does not establish an end-to-end win. This is evidence about this count/prefix/fill implementation, not a lower bound on eager decoding. hlink was not rerun in this window.

| Dataset | Threads | 1b decode ms | CSR count + prefix + fill ms | Flood ms | Total ms | Interleaved p2par ms |
|---|---:|---:|---:|---:|---:|---:|
| minio | 1 | 31.675 | 48.602 | 28.380 | 108.657 | 26.711 |
| minio | 8 | 5.831 | 11.714 | 12.519 | 30.063 | 12.073 |
| kube-apiserver | 1 | 61.227 | 94.482 | 41.583 | 200.290 | 39.483 |
| kube-apiserver | 8 | 8.928 | 23.746 | 16.042 | 53.244 | 15.331 |

Each column is its own minimum of five measured samples; phase minima need not sum to the minimum total. Each total is an actual contiguous decode + build + flood measurement. Kube-apiserver totals are 200.290 / 53.244 ms; no kube hlink baseline was supplied, so these do not establish its relative hlink performance.

## Inputs, work and exclusions

[Implementation](../ref/flat/p2pipe.c) reads `oracle/p1_canon.u32` and `oracle/p1_builtin.u32`: **1a is not computed or timed**. 1b independently decodes all 9,403,582 MinIO / 19,824,324 kube relocations, including relocations of sources without CSR rows, using `slot_rel`, `slot_obj`, `obj_pkg_off`, `obj_pkg`, `rel_pkg`, `rel_idx`, then canon. It never uses `oracle/p1_reltarget.u32` to construct targets; that file is comparison-only. `obj_base`, relocation offset, size and addend are unnecessary for this plain-edge decode/filter; package bases already provide slot ids. No raw Go objects are read.

CSR sources are canonical definitions with flag 32, matching dumpflat. Weak relocations are skipped before type dispatch; METHODOFF skips its triple; USETYPE, INITORDER, USEIFACE, USEIFACEMETHOD, USENAMEDMETHOD and target zero do not produce plain relocation edges. Count and fill run in parallel, with a serial prefix over source counts. The measured fill also copies inherited edges into the new CSR. The flood is copied from p2par, with its algorithm and scheduling unchanged.

| Dataset | Rebuilt plain relocation edges | Inherited funcdata kind 0 | Inherited kind 1 | Kind 2 | Kind 3 | Total inherited edges | Merged edges | Inherited method records |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| MinIO | 7,446,678 | 704,023 | 131,247 | 38,588 | 337 | 874,195 | 8,320,873 | 106,079 |
| kube-apiserver | 14,008,368 | 1,301,437 | 207,601 | 76,192 | 7,174 | 1,592,404 | 15,600,772 | 192,391 |

The flat CSR lacks explicit aux provenance. dumpflat emits relocations first, then funcdata aux records. Setup counts the eligible nonzero relocation targets per row; the first that-many kind-0 entries are the relocation-edge comparison set, and the remaining kind-0 entries are the inherited funcdata suffix. An assertion requires inherited funcdata to belong to text rows. Kinds 1–3, signature/name tables, flags, roots and method tables come from existing p2 files. The oracle comparison thus relies on the reviewed dumpflat ordering and relocation filter for the aux boundary; it cannot independently establish aux provenance from a separate label that the format lacks.

Excluded from all reported pipeline totals: 1a, decoding auxiliary references and special kinds, extracting method tables/interning strings, mmap/file loading, allocation, setup to isolate inherited edges, validation, and flood-state clearing/initialization. p2par also excludes its flood-state reset. Consequently this is the requested partial eager pipeline, **not a full raw-object reach implementation** or a direct replacement for dumpflat's 133–139 ms P2 conversion (which does additional work). Arrays are allocated before timing; one complete pipeline iteration warms allocated outputs and frontier storage. Nothing is read from or written to the data directories except read-only mappings.

## Verification and independent check

Every pipeline invocation checks all decoded targets against `oracle/p1_reltarget.u32`; checks rebuilt kind-0 relocation destinations per source as multisets (including duplicates); checks the final materialized merged CSR per source as multisets of `(kind,destination)`; and checks sorted live ids against `oracle/p2_live.u32`. All five rounds at both thread counts passed: MinIO 663,630 live, kube-apiserver 926,486 live. The final CSR comparison is after the timed runs, so count/prefix/fill errors cannot be hidden by checking only the temporary relocation lists.

An independent reviewer inspected filtering and aux ordering against dumpflat and ran a separate 8-thread kube build/run at 15:59 UTC: all target, relocation-multiset and live checks passed. The reused p2par flood contains existing concurrent plain reads alongside atomic writes and same-value plain writes to signature/name marks; observed oracle success is not a proof of C data-race freedom. This experiment preserves that baseline rather than changing the flood algorithm. `-Wall -Wextra` compilation is clean.

## Reproduction and shared-host noise

Run from this worktree:

```sh
python3 ref/flat/p2pipe-bench.py
```

The [runner](../ref/flat/p2pipe-bench.py) compiles with `gcc -O2 -march=native -fopenmp`, sets `OMP_NUM_THREADS=1` or `8`, and caps each run using `prlimit --as=8589934592` (8 GiB). It instruments only a temporary copy of p2par to print each repetition, leaving the tracked baseline unchanged. Each invocation runs two iterations: the first is warmup, **only the second is a measured sample**. Five rounds provide exactly five samples per configuration. Pipeline and baseline alternate order across rounds. No affinity setting is imposed. VM: 8 logical CPUs, AMD EPYC 9554P, GCC toolchain on the shared host. Load is recorded using `cut -d' ' -f1 /proc/loadavg` before each pair. The baseline has a 119.570 ms outlier at MinIO/8 in round 4, reinforcing the need to interleave and report noise.

All measured samples, ms (first iteration discarded):

| Round | Dataset | Threads | Load | Decode | CSR | Flood | Total | p2par |
|---:|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | minio | 1 | 0.77 | 31.675 | 48.602 | 28.380 | 108.657 | 30.035 |
| 1 | minio | 8 | 0.77 | 5.831 | 11.714 | 12.519 | 30.063 | 12.073 |
| 1 | kube-apiserver | 1 | 0.77 | 61.227 | 94.482 | 44.581 | 200.290 | 39.483 |
| 1 | kube-apiserver | 8 | 1.35 | 8.928 | 29.477 | 21.572 | 59.978 | 20.289 |
| 2 | minio | 1 | 1.35 | 32.674 | 48.826 | 32.127 | 113.627 | 30.060 |
| 2 | minio | 8 | 1.35 | 6.245 | 15.697 | 16.493 | 38.435 | 15.899 |
| 2 | kube-apiserver | 1 | 1.48 | 64.765 | 101.010 | 43.901 | 209.676 | 41.452 |
| 2 | kube-apiserver | 8 | 1.48 | 9.903 | 23.746 | 19.595 | 53.244 | 24.945 |
| 3 | minio | 1 | 1.48 | 32.563 | 50.116 | 30.409 | 113.087 | 32.727 |
| 3 | minio | 8 | 1.52 | 8.781 | 17.440 | 22.116 | 48.337 | 33.141 |
| 3 | kube-apiserver | 1 | 1.52 | 114.465 | 169.458 | 55.148 | 339.072 | 44.211 |
| 3 | kube-apiserver | 8 | 1.52 | 9.538 | 28.114 | 21.500 | 59.152 | 21.437 |
| 4 | minio | 1 | 1.48 | 31.788 | 49.832 | 29.287 | 110.907 | 26.711 |
| 4 | minio | 8 | 1.48 | 6.439 | 13.569 | 14.744 | 34.751 | 119.570 |
| 4 | kube-apiserver | 1 | 1.48 | 61.880 | 99.103 | 46.253 | 207.236 | 42.100 |
| 4 | kube-apiserver | 8 | 1.44 | 14.468 | 26.096 | 20.563 | 61.128 | 21.078 |
| 5 | minio | 1 | 1.44 | 45.600 | 54.932 | 31.120 | 131.652 | 27.729 |
| 5 | minio | 8 | 1.44 | 8.070 | 15.823 | 15.577 | 39.471 | 17.638 |
| 5 | kube-apiserver | 1 | 1.44 | 61.950 | 98.653 | 41.583 | 202.187 | 41.427 |
| 5 | kube-apiserver | 8 | 1.41 | 13.907 | 29.859 | 16.042 | 59.809 | 15.331 |
