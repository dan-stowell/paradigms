# Halide on this VM

## Install (done, 2026-11-16)
- C++ API + AOT generators: official release tarball **Halide 21.0.0**. v21 is the newest tag that ships
  binaries; the v22.x GitHub releases have NO attached tarballs, only PyPI wheels.
  ```
  mkdir -p /home/exedev/tools/halide && cd /home/exedev/tools/halide
  curl -sL -o h21.tar.gz https://github.com/halide/Halide/releases/download/v21.0.0/Halide-21.0.0-x86-64-linux-b629c80de18f1534ec71fddd8b567aa7027a0876.tar.gz
  mkdir v21 && tar xzf h21.tar.gz -C v21 --strip-components=1     # 665 MB; include/ lib/ bin/ share/
  ```
  Host target detected by `v21/bin/get_host_target`: x86-64-linux, avx2, avx512 (skylake/cannonlake), zen4, fma, f16c.
  No cmake on the VM; g++ 13.3 is enough (see build.sh).
- Python: `python3 -m venv /home/exedev/tools/halide/venv && venv/bin/pip install halide==22.1.0 numpy` (JIT sanity-checked).
  Note: C++ AOT is 21.0.0, Python is 22.1.0.
- Data: `/home/exedev/tools/halide/data/{idx.u32,table.u32}` (raw LE u32; idx = 50M values in [0,2^24), table = 2^24 values; made by hello/mkdata.cpp).

## AOT/generator path (hello/)
1. `gen.cpp` defines three `Generator`s (gather, hist, scan) and links `libHalide.so` + `libHalide_GenGen.a` (supplies `main`; the tarball has no GenGen.cpp).
2. `./gen -g NAME -f FUNC -o . target=host variant=K` emits `FUNC.a` + `FUNC.h` (one invocation per variant).
3. `driver.cpp` mmaps the files (MAP_POPULATE), wraps them zero-copy in `Halide::Runtime::Buffer`, calls the AOT functions, checks each result against plain C++, reports best of 5.
`hello/build.sh` then `hello/run.sh` reproduce everything (data generated on first run). Raw output: `hello/run.out.txt`.

## Timings (run 2026-11-16T15:25:36Z, best of 5, N=50,000,000 u32, table 64 MB, 2^20 bins; ms)
| pipeline | 1 thread | 8 threads |
|---|---|---|
| (a) gather out[i]=table[idx[i]] | 488 | 91 (68 in an earlier run) |
| (b) hist, RDom scatter, serial update (default) | 66 | 65 |
| (b) hist, `update().atomic().parallel(r)` | 698 | 1584 |
| (b) hist, `rfactor` private hists + merge, parallel over 8 chunks | 150 | 184 |
| (c) scan, serial RDom `out(r)=out(r-1)+in(r)` | 46 | 44 |
| (c) scan, hand-blocked 3-phase, parallel | 244 | 60 |
| plain C++ gather / hist / scan (1 thread) | 416 / 61 / 20 | n/a |

All Halide outputs verified equal to the C++ reference. Gather: 8 threads give 5-7x (DRAM random-load bound, about 0.55-0.75 G/s).

## What Halide does with these
- **Gather**: pure definition, trivial, parallel over chunks, matches C++ and scales. A data-dependent index REQUIRES `clamp` (else "Buffer may be accessed in an unbounded way"). Halide's model is that it must bound every access footprint.
- **Scatter / histogram**: an RDom update `h(f(in(r))) += 1`. Default is serial. `parallel(r)` is refused with "may introduce a race condition" (tested). Options: `atomic()` (correct but 10-25x slower here; slower with more threads), or `rfactor` (private partial histograms per chunk; did not beat serial for 2^20 bins; I tried one schedule only, untuned, so this is not a verdict). Halide has no sort, radix, or partition primitive.
- **Scan**: no scan primitive. A scan is an RDom update with a loop-carried dependence; `parallel` on it is refused (same race error, tested), and `rfactor` needs an associative reduction, which a scan is not as a whole. The only route is a hand-written blocked scan as separate Funcs (what variant 1 does: in-block scan parallel over blocks, serial scan of block totals, add offsets). That makes about 3 passes over memory: 60 ms at 8 threads vs a 20 ms single-thread C++ loop, so no win at 50M u32 (the serial scan already moves ~9 GB/s).
- Scatter output bounds cannot be inferred; the extent is pinned with `set_bounds`.

## Gotchas
- v22.x has no binary tarball; use v21.0.0 or the pip wheel.
- Arithmetic with `GeneratorParam<int>` and `0u` is ambiguous as Expr; use `cast<uint32_t>(0)`. `RDom(0, buf.dim(0))` does not compile; use `.extent()`.
- `Var`s used in `schedule()` must be the same Vars as in `generate()` (make them generator members), else "could not find split dimension".
- A uint32 index into a Buffer needs an explicit `cast<int>`; no implicit unsigned-to-signed.
- `HL_NUM_THREADS` controls the runtime thread pool (works as expected).
- Compile errors from `.parallel()` are thrown as `Halide::CompileError` at schedule time.
