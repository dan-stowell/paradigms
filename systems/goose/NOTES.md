# Goose: install, status, hello-world, judgment

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
