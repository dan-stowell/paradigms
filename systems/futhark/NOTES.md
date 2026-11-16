# Futhark on this VM

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
