# Goose: the cells (problems 1-3 on MinIO), then install notes

Cells measured 2026-11-16T16:22-16:25Z under **load 8-13** (all builders running): every number below is a range
over 5 interleaved rounds of `systems/goose/bench.sh` (min / median / max), each round repeating its kernel in-process
(P1_REPS=5, P2REPS=10-20, P3_REPS=20) and taking the min of its reps. Treat them as provisional; the lead can rerun
`systems/goose/bench.sh` on a quiet machine. Goose's queues block on a pthread condition variable (no spin-wait), so
the 8-worker numbers have no wait policy to set (RESUME.md's load warning does not apply as such; a wake-up per
message round still costs more under load). Build: `goose build X.goose -o bin/X` (release v2026.4, gcc 13.3 -O2).
Memory cap: `systemd-run --user --scope -p MemoryMax=8G` (Goose reserves 2 GB of address space per data stack, so
`prlimit --as=6G` makes it die at start with "cannot reserve data stack address space").

## Surprises

1. **The shared-nothing tax is the gather, not the compute.** 3b on 8 workers builds its image in 17-24 ms (2 MB pages)
   when each worker keeps its address range; gathering the 109 MB into one array in main costs **142-171 ms** more
   (two copies through the queue plus main's page faults: 57-83 k faults). Same in 1b: 9.4 M targets computed in
   parallel and sent to main take 42-67 ms on 8 workers vs 39-57 ms on 1 thread. The workloads parallelize; moving
   their outputs between isolated programs is 5-8x the compute.
2. **Goose's runtime pins the output to 4 KB pages, but a 20-line C sidecar can swap the pages under a builder's stack
   top for hugetlb pages, and it halves 3b** (ATTACKS 2): 1 thread 86.7-98.5 ms -> **40.8-42.2 ms**, minor faults per
   rep 26,731 -> 258-457; 8 workers 32.8-40.0 -> **17.4-23.5 ms**. The runtime reserves each data stack as a NORESERVE
   4 KB mapping and madvises MADV_NOHUGEPAGE on its first 2 MB; the language has no word for page size.
3. **P1's first repetition is 2.5x its later ones** (1a 213 ms once vs 79-115 ms in reps 2-5): the 200 MB open-addressing
   table (8 M groups x 24 B) and the 1c table are first-touched in rep 1; later reps reuse the same stack pages (a
   resizable cleared and regrown on the same data stack never returns its pages). The warm P1 numbers below are those
   later reps; the C references' single-run numbers include their table faults. One run of p1 (all of 1a+1b+1c, first
   touch included) is 560 ms of compute (16:15Z, load 2).

## ATTACKS 3 (no supersteps: is the level count the cost?)

Full write-up with counts and both windows: results/levelfree.md. `p2async.goose` keeps p2par's owner partition and
p2rules.goose's local rules and drops the supersteps: a worker floods its part to a local fixpoint (or until
`P2FLUSH` ids wait), posts ids straight into the owners' inboxes and any new global fact (signature, method name,
reflect) to every worker, takes what has arrived with `qpoll`, and goes on; idle, it reports its per-peer sent and
received batch counts to main and blocks on its inbox. Main stops it when every latest report is idle and
sent[i][j] == received[j][i] for every pair (a worker only reports when idle and only wakes on a message, so an
in-flight or unreported message always shows as a mismatch). `systems/goose/levelfree.sh 7` reruns it.

- **The premise was wrong for Goose: p2par has 10 supersteps, not 111.** Each superstep already drains the
  partition to a local fixpoint, method rule included; the steps are cross-partition waves (221,907, 416,622,
  126,815, 120,375, 68,606, 79,820, 1,658, 387, 60, 0 ids).
- Counts, 8 workers: BSP 10 barriers, 560 inbox batches + 160 status/go, 1,036,250 ids; async 0 barriers, 482-687
  batches + 102-174 idle reports, the same ~1,036 K ids.
- Times (best flood, min / median / max, 5 and 7 interleaved rounds, load 17-23): 8 workers **async 33.4 / 38.9 /
  44.2 vs BSP 48.1 / 52.9 / 62.5** (ranges apart), then 28.2 / 37.4 / 46.6 vs 33.8 / 48.0 / 80.0; lazy decode
  60.4 / 64.6 / 76.6 vs 66.7 / 95.4 / 103.0, then 47.0 / 52.9 / 63.7 vs 64.0 / 84.7 / 119.8; 1 worker the same.
- Verdict: dropping the barriers is worth ~1.3-1.5x under this load (no worker waits for a descheduled one), but
  8 workers stay only ~1.3-1.9x faster than 1: per worker 4-20 ms local work, up to 10 ms bucketing and posting
  ids, 7-23 ms idle; summed work ~70-80 ms against 43 ms on one worker. The partition (busy owner, a million ids
  through queues) is the limit, not the superstep count.

## ATTACKS 1 (reach, counting the decode)

- **Expressible: yes, trivially.** The flood's drain loop decodes a live symbol's relocations from rel_pkg/rel_idx/
  rel_type through its object's package-base table and canon right where it pops the symbol (p2lazy.goose `drain` and
  `target`, p2rules.goose for workers). Goose is imperative; nothing forces the graph to be materialized. What is
  forced: the p2 files mix relocation edges with aux edges, so load "cuts" the relocation edges out of each row
  (outside the timed region; it is why p2lazy's end to end is 731-859 ms).
- **Numbers** (interleaved with the C references, 16:24-16:25Z, load 7.5-8.5): 1 thread Goose lazy **70-84 ms** vs C
  p2lazy 61-71 (1 outlier 229) in the same window, vs the eager C pipeline 108.7 (quiet). 8 workers Goose lazy **39-59 ms**
  vs C p2lazy 21-31 (OMP_WAIT_POLICY=passive) and eager C 30.1 (quiet). So: it beats the eager C pipeline on 1 thread,
  and is near the lazy C one there (1.1-1.2x); on 8 workers it does not beat even the eager C pipeline.
- **Against Goose's own eager shape** it wins at both widths: eager = 1b decode (39-57 ms 1 thread, 42-67 on 8) + a CSR
  build (not written: the extractor's p2 files are that CSR) + the CSR flood (41-48 ms 1 thread, 22-32 on 8), so >= 80 /
  64 ms before the CSR build, vs 74-79 / 37-49 lazy.
- Why 8 workers lose to C: the flood (lazy or not) scales only ~1.7x from 1 to 8 workers (CSR 41 -> 22-32; lazy 66 -> 38-48,
  16:16Z): 10 bulk-synchronous supersteps with W x W inbox messages per step, each a condition-variable wake-up, and each
  worker drains only 9-20 ms of its 37-47 ms. Lazy costs ~1.7x the CSR flood at every W (three columns read per
  relocation instead of one, plus the canon gather into a per-worker 20 MB copy: shared-nothing gives every worker its
  own canon, 160 MB on 8, where C's threads share one).

## ATTACKS 2 (relocate on huge pages)

Yes, but under the language: `flatio.c` `fresh_pages(builder, n, huge)` mmaps MAP_FIXED|MAP_HUGETLB over the 2 MB-
aligned part of the next n bytes above the builder's stack top (the head below the first 2 MB boundary stays on 4 KB
pages, hence 258-457 faults rather than C's 39), then Goose's own `img.resize(n, 0)` writes the image there. With
`huge=false` it maps fresh 4 KB pages, so every rep pays its faults (P3_REPS, like ref/flat/p3.c). `minor_faults()` is
getrusage. Interleaved 16:24-16:25Z (load ~8), 1 thread P3_REPS=20 min per run:

| | 4 KB pages | 2 MB pages |
|---|---|---|
| C `build/p3` | 56.4-60.3 ms, 19,109 faults/rep | 24.5-26.0 ms, 39 faults/rep |
| Goose `bin/p3` | 86.7-98.5 ms (bench 16:22Z), 26,731 faults/rep | 39.3-43.2 ms, 258-457 faults/rep |
| Goose `bin/p3par` 8 workers, image distributed | 32.8-40.0 ms, 26,736 faults/rep in workers | 17.4-23.5 ms, 824-1,336 faults/rep |

Goose stays 1.6x C at either page size: `img.resize(n, 0)` writes 109 MB of zeros C's mmap gets free, and `put` stores a
relocation byte by byte (the language has no unaligned u32/u64 store into a u8 array).

## The cells

All checks: `systems/goose/bin/<cell> /home/exedev/paradigms/data/minio [8]` prints `P1 OK` / `P2 OK` / `P3 OK`
(`P3 pipeline OK` with `--pipeline`); each compares its output arrays with the oracle files element by element
(`check32` in flat.goose; the image symbol by symbol against oracle/p3_image.bin). Lines are whole files (flat.goose 150
and par.goose 36 shared; flatio.c 102 is the C escape: file reads, page remapping, getrusage).

| cell | 1 thread warm | 8 workers warm | 1 / 8 e2e | lines |
|---|---|---|---|---|
| 1a join by name | 79-115 (first rep 213) | 37-57 | 1a+1b+1c: 852-1066 / 518-687 | p1 195, p1par 411 |
| 1b relocation targets | 39-57 | 42-67 (incl. sending 9.4 M targets to main) | (above) | |
| 1c content-hash join | 97-118 | 39-47 | (above) | |
| 2 reach, CSR flood | 41-49 (p2par W=1; p2 single run 53) | 22-32 | 205-269 / 225-316 | p2 187, p2par 241 + p2rules 341 |
| 2' reach, lazy decode | 74-79 | 37-49 | 731-859 / 411-807 | p2lazy 286, p2par lazy mode |
| 3a layout | 15-26 | 17-31 | 3a+3b: 338-392 / 595-730 | p3 251, p3par 386 |
| 3b relocate, 4 KB | 87-99 | 33-40 distributed; +142-171 gathered | (above) | |
| 3b relocate, 2 MB | 41-42 | 17-24 distributed | | |

e2e is one process with one rep, wall from `date +%s%N` around it, including the oracle self-check's reads (~10-20%).
3a on 8 workers is no faster than 1 thread: main's serial scan over 366 k (unit, bucket, class) counts and the typelink
sort, plus three message rounds, are most of it.

### Per cell: easy / forced / impossible

- **1a** easy: open-addressing table of inline `Group {Key, val}` structs, winner rule as `rank << 32 | ~id` max. Forced:
  on 8 workers a shuffle join; a worker's inbox must be a distinct type (`Mail<u8[j+1], M>`, an 8-arm match) because
  queues are per type. Impossible: one shared table (no shared memory).
- **1b** easy: a gather per relocation through the object's package slice. Forced: on 8, an allgather of canon (every
  worker gets all 21 MB) and shipping targets to main. Impossible: nothing.
- **1c** easy: same shape as 1a. Forced: same shuffle. Impossible: nothing.
- **2 CSR** easy: `checkfree fn drain` over index-typed columns (`u32 in live`, `u32 upto edge_dst`), validated once by
  `from_bytes` at load: zero bounds checks in the flood. Forced: on 8, owner-partitioned BSP with per-owner inbox types and
  main merging the global facts (signatures, reflect, names). Impossible: a shared visited set.
- **2 lazy** easy: the decode is ten lines inside the drain. Forced: cutting relocation edges out of the p2 rows at load;
  every worker holds its own canon. Impossible: nothing.
- **3a** easy: count per (unit, bucket, class), scan, assign; `sort` with a block comparator for typelinks. Forced: on 8,
  counts to main and bases back (three message rounds). Impossible: nothing.
- **3b** easy: `img[base..].move_from(bytes)` and a match on relocation kind. Forced: byte-wise stores; the hugetlb
  output only through C under the runtime. Impossible: one image written by 8 workers in place (they can't share it);
  the gathered 8-worker image is slower than 1 thread.

### Falls down

- **8-core 3b with one output array**: two attempts (workers build segments and main gathers: 3b 53-150 ms gathered;
  then the image left distributed, which is fast, 17-24 ms, but is not one array). Reason: no shared memory, and a queue
  hop copies twice. Distributed is the idiomatic answer and the one reported as the cell.
- **8-core P2 near C**: CSR and lazy floods both scale ~1.7x over 8 workers (two attempts: one queue type per owner with
  batched inboxes and BSP; the lazy variant; a third, asynchronous with no supersteps, p2async: 1.3-1.5x faster than BSP
  under load, still ~1.3-1.9x over 1 worker). Reason: per-superstep message rounds; Goose has no shared-memory
  `par for` (a design note only).

### Core code excerpt (p2lazy.goose, the lazy part of the flood)

```
        if decodes(c) {
            // the lazy part: this symbol's relocations, decoded now
            var skip = 0;     // type 26 hides the next two relocations
            for r in slot_rel[c]..slot_rel[c as i64 + 1] {
                if skip > 0 { skip--; continue; }
                let ty = rel_type[r];
                if ty == 26 { skip = 2; continue; }
                if plain(ty) {
                    let t = target(c, r);
                    if t != 0 { plain_edge(t as Sym, uii, work); }
                }
            }
        }
```

and 3b's loop (p3.goose): `img[base..base + bytes.len].move_from(bytes);` then per relocation
`match rkind[r] { 1, 2 => { v = ta + radd[r]; } 3, 4 => { ... v = (ta - sect[k]) + radd[r]; } 5, 6, 7 => { v = ta + radd[r] - (P + sz); } ... }`
and `put(img, base + off, v, sz)`.

## Install, status, hello-world, judgment (installer, 15:24Z)

Measured 2026-11-16 (`date -u`: Mon Nov 16 15:24 UTC 2026), Linux x86_64, 8 cores, gcc 13.3.0, glibc 2.39.

## What Goose is

A new (2026) memory-safe systems language by Wouter van Oortmerssen (Lobster's author). Site
https://aardappel.github.io/goose/ , repo https://github.com/aardappel/goose (Apache 2.0). Design: no heap, no GC, no RC,
no destructors, no lifetime annotations. Dynamic data lives on N compiler-assigned "data stacks" (bump pointers,
freed at scope exit with one store; at most one resizable value live per stack, always on top, so growth never moves
and references stay valid). Every reference has a static root checked by the compiler. Nested data is inline (arrays
of strings are one block; variable-size enums). Narrow links: `T&<u32>` self-relative, `T&<u32 in pool>`, and index
types `u32 in arr` (validated once, indexing then needs no check; `checkfree fn` enforces zero bounds checks).
Threads: each `thread_fn` is a separate program with its own memory; flat values move through typed queues by memcpy.
Compiles to one C file (whole-program, monomorphizing); `goose run` JITs it with built-in TinyCC, `goose build` calls a
system C compiler (gcc here, -O2).

The README states it was "produced almost entirely with AI, from a human language design" (compiler implementation by
Claude Fable; design by the author). About 30 k lines of C++ is the site's claim; the checked-out `src/*.h|cpp`
is 74 k lines.

## Is there a compiler / interpreter / transpiler

- A compiler that transpiles Goose to C (`goose -o out.c file.goose`) and then either
  - runs it in-process through TinyCC (`goose file.goose`, no optimization, no threads, extern C only for `<math.h>`), or
  - builds an executable with the machine's C compiler (`goose build file.goose -o exe`, `-O2`).
- No interpreter, no LLVM, no direct native backend. Also: formatter, LSP, package fetcher (git tags), `checkfree` and `--bce-lines` diagnostics.

## Install (what I did)

Exact commands (also `systems/goose/hello/build.sh`, `build_from_source.sh`):

```
mkdir -p /home/exedev/tools/goose && cd /home/exedev/tools/goose
curl -sL -o rel.tgz https://github.com/aardappel/goose/releases/download/v2026.4/goose-v2026.4-linux-x64.tar.gz
mkdir rel && tar xzf rel.tgz -C rel          # -> rel/goose-v2026.4-linux-x64/goose
git clone https://github.com/aardappel/goose src   # HEAD 3a0aa2eb (2026-11-15), docs and benchmarks read from here
```

- Binary used: release v2026.4 (published 2026-11-06), `goose v2026.4 (runtime dce889723a23)`, linux-x64 (needs glibc >= 2.35;
  this VM has 2.39). `goose toolchain` finds gcc 13.3.0. Release bundles gfx/audio/ui/physics.
- Source build: needs cmake >= 3.27 (not installed on the VM). I used a pip venv (`cmakevenv`, cmake 4.4.5) and
  `-DGOOSE_BUILTIN_LIBS=none`. See "Source build" below for the result.
- Both `goose run` and `goose build` work with the VM's own gcc and libc headers. First `build` compiles the runtime once ("compiling the runtime for gcc 13.3.0").

## What is implemented vs on paper

Read from the shipped docs (spec Appendix B, implementation.md section 10, bench/summary.md) and the source:

Implemented and exercised by the test suite and benchmarks (per the docs; I used a subset myself):
data stacks and the stack invariant, static roots / reference safety, slices, inline nested arrays
(`T[k]`, `T[]`, `T[..]`, `T[>..]`, `T[>..<]`), fixed and variable enums, relative references and
`slot_pool`/`slice_pool`, `to_bytes`/`from_bytes`, in-place construction, `return from` errors, generics
(monomorphized), function blocks, index types (`u32 in a`, `u32 upto a`) and `checkfree fn`, `simd fn`, thread_fn + typed
queues + contained workers, C interop (`extern fn`, `extern "lib" {}`, sidecar `.build` for own C), packages, LSP.

On paper / open (spec Appendix B, design/ notes), verified absent:
- Column arrays (`T[||..]`, struct of arrays): design/soa.md says "The feature is not implemented."
- Child threads / `par for` (fork-join over shared memory): design/child_threads.md is a proposal; `par for` appears
  in no spec section, reference, or source file.
- Move for resizables, more than one resizable per struct (root structs are the workaround), labeled break,
  mixed-type pools and pool GC, wasm backend, queue select/fairness, SIMD alignment, standard-library access beyond
  file/stdin/time/env (no subprocess, no network, no mmap).
- Bounds-check elimination: indirect indexing keeps its check unless the stored values have an index type; `u64`
  values lose range info (TODO 0a, 0f).

## Hello-world: what was written

`hello/hello1.goose`: `read_file` the whole file into a `u8[>..]` (the only file API is whole-file append: no mmap,
no pread, no seek, no partial read), decode u32 LE with `binary::read_u32le` (checked, per element; there is no
bitcast/reinterpret of a byte array to u32), inclusive prefix sum into `u64[>..]`, 256-bin histogram of the low 8 bits,
checksums. `hello/hello8.goose`: same across N workers, chunk copied to each over a typed queue, worker returns
(len, total, sum of local prefixes, histogram); main folds `base*len + lps`. `hello/gen.c` writes the input file and prints
the oracle; `hello/c_ref.c` is a C reference. Both Goose programs print identical checksums to the oracle.

Input: 50,000,000 xorshift64* u32 (200 MB, /home/exedev/tools/goose/u32_50M.bin, page-cached).
Oracle: `total=107384557821208332 prefix_checksum=2059448577320109362 hist_checksum=10908785116829868540` (all match).

### Timings (ms, wall by Goose `clock()`, three consecutive runs each, 2026-11-16 15:24 UTC, warm page cache)

| program | read | decode | scan+hist (+checksum) | send+spawn | workers+collect | total |
|---|---|---|---|---|---|---|
| C reference (gcc -O2, direct u32 reads) | 94 / 100 / 101 | n/a | 194 / 187 / 200 | | | 287 / 287 / 301 |
| Goose 1 thread (`hello1`, gcc -O2) | 103 / 114 / 117 | 171 / 169 / 175 | 226 / 213 / 236 | | | 501 / 496 / 528 |
| Goose 1 worker (`hello8 1`) | 132 / 108 / 106 | | | 218 / 223 / 225 | 537 / 526 / 526 | 887 / 858 / 856 |
| Goose 2 workers | 95 / 98 / 101 | | | 182 / 171 / 180 | 301 / 251 / 296 | 577 / 521 / 578 |
| Goose 4 workers | 141 / 130 / 104 | | | 223 / 185 / 161 | 188 / 166 / 166 | 552 / 480 / 431 |
| Goose 8 workers | 113 / 107 / 108 | | | 119 / 119 / 117 | 96 / 95 / 77 | 329 / 321 / 302 |

Reading: the 8-worker run is 1.6x faster than 1-thread Goose end to end (500 to ~320 ms) and about equal to C
single-threaded (287). The compute part alone (workers+collect) scales 533 to 77-96 ms (5.6-6.9x on 8). Everything else is serial in main:
the 100 ms file read and the 119 ms of copying chunks into queues (200 MB in ~119 ms = 1.7 GB/s, about 2% of the DRAM floor,
and it includes thread spawn). The checksum phase of `hello1` is 17 ms (a second pass over the prefix array).
Decode via `binary::read_u32le` costs about 170 ms per 50 M values (3.4 ns each), roughly as much as the whole scan:
Goose has no zero-copy view of bytes as u32, so a flat binary column costs a decode pass.

### Gotchas

- `goose run` (TinyCC) refuses programs with threads; use `goose build`.
- File API is whole-file only (`read_file` appends all bytes to a `u8[>..]`; `write_file`). No mmap, pread, seek. I did not add a C FFI
  workaround because the workload fit; the documented way is the program's own C via a sidecar `foo.build` plus `extern fn` (see docs/packages.md section 3), with C pointers passed as `u64` (Goose cannot read through them without `ffi::bytes`, which copies).
- No shared memory between threads, so a prefix sum cannot materialize one global array in parallel; I send chunks, return per-chunk
  totals and fold. Writing the final prefix would need a second queue round trip per chunk (and queues are per type,
  not per worker, so a worker can't be addressed without a distinct message type per worker).
- Queue elements must be flat; a 25 MB `u8[]` chunk worked fine as one message.
- Unsigned overflow wraps silently in release builds; `--debug` adds checks. `u32` converts implicitly to `u64`
  (a redundant `as u64` is a warning); `u64` against signed needs casts.
- Thread count is `hardware_threads()` (8 here); `hello8` takes an optional one-character count.

## Source build

`hello/build_from_source.sh`: cmake 4.4.5 from a pip venv, `-DGOOSE_BUILTIN_LIBS=none`, `-j8`: 1m48s wall on 8 cores, binary
`/home/exedev/tools/goose/build/goose`, `goose v2026.4-43-g3a0aa2eb (runtime a9ca9ac6fac6)` (43 commits past the v2026.4 tag).
Compiled `hello1.goose` with it: identical checksums, total 487 ms (same as the release binary within noise). The release
binary is what the timings above used.

## Judgment: Goose's memory model against a linker's data

For the flat, big, long-lived columns (5.2 M symbols, 9.4 M relocations) Goose's model fits well and costs nothing: each
column is a global `T[>..]` grow-only array (one stack each, bump pointer, no moves), links are `u32 in column` index types
validated once at load, and `checkfree fn` proves the flood and relocation loops have no bounds checks, which is the
Zig-compiler "indices not pointers" design with the compiler checking it (the bench `graph_csr` is exactly a CSR flood).
Struct-of-arrays has to be done by hand since column arrays are unimplemented, and decoding the columns from files costs a
pass (about 3.4 ns per u32), because there is no mmap or byte-to-u32 view. A hash join is hand-written open addressing in
arrays, bounds-check-free only if the slot values are typed as indices (the bench `words` keeps four checks); the 0.74 G/s
random-load floor is untouched by the language, and the winner rule is ordinary code. The graph flood is a single-program
algorithm: the visited set cannot be shared among workers, so it runs on one core unless it is partitioned by owner with per-level message exchange
(the only parallel construct Goose has today is isolated programs; shared-memory `par for` is a design note). The scatter into
the 109 MB image has the same constraint: one array in one program, so parallel scatter means workers build disjoint segments and
send them back as copies, and my measured queue throughput (about 1.7 GB/s including spawn) puts a 109 MB hop near 60 ms
against a floor of about 1.2 ms. So the memory model maps well onto layout, resolve and reach as single-thread, bounds-check-free
column code and badly onto the 8-core goal; expect Goose to land near the C single-thread number with less code, and
not near hlink's parallel result, unless child threads arrive.
