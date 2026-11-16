# Wildcard prior art: systems outside the grid

Written from memory of papers and code, not re-fetched; items marked (verify) are claims to check before relying on them.
Machine: 8 cores, no GPU, Linux x86_64. Data: 5.2 M slots, 9.4 M relocs, 2.7 M name refs, 360,376 live, 109 MB image.
Problems: 1 resolve (join + winner rule), 2 reach (flood + conditional method rule), 3 layout (scan) + relocate (gather/scatter), 4 codegen by shape.
"T" in thresholds = hlink's wall time for the same phase, measured on this VM (the brief gives only the floor ratio of 4-5%).
duckdb 1.x is already at /usr/local/bin/duckdb, so DuckDB's install cost is zero.

## Rankable candidates

### DuckDB (vectorized SQL)  -- fits 1, 3 (scan), partly 4
- Resolve is a hash join plus a window/arg-max: `QUALIFY row_number() OVER (PARTITION BY name_abi ORDER BY is_def DESC, NOT dupok DESC, has_content DESC, size DESC, idx)=1`, or `arg_max` with a struct key. Layout is `sum(size_aligned) OVER (PARTITION BY section ORDER BY idx)`. Relocate is a join plus a column the harness scatters.
- Axis the grid lacks: a mature vectorized, morsel-parallel, cost-optimised relational engine you do not write the join for (Soufflé is relational too but is Datalog-compiled, with its own index structures). It tests "does an off-the-shelf engine at ~GB/s beat a hand join". Out-of-core spill and radix-partitioned joins come free.
- Weak at 2 (recursive CTEs run iteratively with UNION dedup; USING KEY recursion added in 1.3 (verify) helps, but a 360 k-node flood with ~10^6 rounds-worth of edges is the known bad case) and at the conditional method rule. Plain scatter is not expressible; it is an UPDATE or a join.
- Install: none. Kill test: load the columns as Parquet or read_csv of flat binaries (`read_blob` + cast is awkward; write columns as Parquet from Python, not timed). Run the resolve query over 5.2 M slots / 2.7 M refs. Kill if wall (excluding load) > 20x T for resolve, or if the recursive-CTE reach over 9.4 M relocs from the entry points exceeds 5 s (hlink's flood should be tens of ms). Success: resolve <= 3x T and layout scan <= 2x T.
- Verdict: cheapest column to add; answers "how far does a general SQL engine get".

### Umbra / HyPer compiled queries  -- fits 1, 4
- Data-centric code generation (Neumann, VLDB 2011) plus Umbra's adaptive compilation and Flying Start low-latency codegen (Kersten et al., VLDB 2018; Funke et al. 2020 on tailored codegen, verify). The interesting hit is problem 4: Umbra compiles each query to machine code via its own IR in ms, which is "emit by shape from templates" done in a DB. Morsel-driven parallelism (Leis, SIGMOD 2014) is the scheduler that DuckDB and Umbra share.
- Axis: compile-the-plan (pipeline fusion; no materialised intermediates). In the grid only Halide fuses, and only over arrays.
- Install: Umbra is a binary from the TUM team (free for research, via request or a Docker image, verify), not on apt. HyPer is closed. Open reimplementation to read, not run: `cwida/duckdb` has no JIT; `tum-db/umbra-playground` Docker (verify).
- Kill test: if the Umbra binary is not obtainable in one working session, drop it and read it only for the design. Use DuckDB as proxy and measure the fusion win by hand: a single fused C loop for resolve (probe + winner rule) versus DuckDB's pipeline; kill if fusion gives < 1.5x on the 2.7 M-ref probe.
- Verdict: a reading column, not a running one.

### Differential Dataflow / Timely (Rust)  -- fits 2 and incrementality
- Axis the grid lacks: incremental maintenance. `reach` as an iterative `iterate` over (src,dst) edges; resolve as `join`; both update in time proportional to the delta. A relink after changing one package reuses the join state. Soufflé is batch only (its incremental mode is experimental, verify), Futhark and Halide are batch.
- Likely slow on cold start: arrangements build sorted batches (LSM-like) and per-record overhead is large; McSherry's COST paper (HotOS 2015) shows these systems often need many cores to match one-thread hand code, and that is the exact claim the charter wants to test. Method-rule conditional (needs aggregate over interface-converted types x method names) is a join with a negation; DD supports `antijoin`.
- Install: `cargo` is not on the VM (checked); rustup is a single script download, ~300 MB. Build of differential-dataflow with deps is ~1-2 min.
- Kill test: reach over 9.4 M relocs from the entry points, cold. Kill if > 50x T on 8 workers. Then the real test, which is its reason to exist: change one package's relocs (~1 % of edges), and require the update to finish in < T/20. If cold is slow but the delta is that fast, record it as the "incremental" axis finding, not as a failure.
- Verdict: the one candidate that adds a genuinely new axis (time/incrementality).

### Ascent / Egglog / Flix  -- fits 2, 1 (Datalog family)
- Ascent (Sahebolamri et al., CC 2022) is Datalog embedded in Rust via macros with Rayon parallelism (ascent_par); Egglog adds e-graphs + Datalog (Zhang et al., PLDI 2023) and is aimed at equality saturation, not at 100 M-row joins. Flix is JVM, lattice-valued Datalog.
- Axis: lattices in rules (Ascent and Flix), which fits "winner rule" as a lattice meet (max over ordered tuple). Soufflé has no lattice aggregation in the normal path, and handles winners with `min`/`max` aggregates. Marginal over Soufflé otherwise.
- Install: Rust again. Kill test: same as Soufflé's reach; kill unless it beats Soufflé on 9.4 M edges by 2x or expresses the winner rule without a post-pass. Low priority.

### BQN / APL / Co-dfns (Aaron Hsu)  -- fits 3, 1 (partly), 4
- Co-dfns (Hsu, PhD thesis Indiana 2019; "A data parallel compiler hosted on the GPU") compiles APL to GPU/CPU with the compiler itself written as array code: no pointers, trees as depth vectors / parent vectors, passes as scans and gathers. It is the strongest prior art that a whole compiler can be flat arrays: a parent-vector AST, the same trick for a flat reloc table. Exactly "problem 4 without trees". Note: Co-dfns dependency was ArrayFire, so it runs on CPU too (verify build state).
- BQN (CBQN) has SIMD primitives hand-tuned (bit-packed bools, grade / sort, `⊔` group, `⊏` select): Marshall Lochbaum's notes describe sort and hash lookup at near-memory speed on AVX2/AVX-512 and are readable design documents for the kernels the grid will need.
- Axis: array-language notation with "no names for loop indices". Overlaps Futhark; difference is dynamic typing and the idiom base (depth vectors, scans for trees).
- Install: CBQN builds with `make` in seconds; Co-dfns needs Dyalog (commercial, free non-commercial licence, verify).
- Kill test: express layout+relocate (scan then gather/scatter over 9.4 M relocs) in CBQN. Kill if > 10x T (interpreter overhead should vanish at these sizes; if not, its primitives are not at memory speed). Also measure CBQN's `⊔` group and `⍋` grade on 5.2 M u64 keys against 3 ns/key.
- Verdict: use as reading list for problem 4 and as a cheap running column for 3.

### Zig self-hosted linker and `wild`  -- all, as existing-code comparators
- `wild` (David Lattimer, Rust, ELF, multi-threaded): the best open data-parallel linker; design notes describe a symbol resolution phase using parallel hash tables, layout in a single pass with per-section prefix sums, and writing the output with sized per-thread slices. It is a direct prior-art for the 3 row and reports, e.g., a few times faster than mold on big inputs (verify numbers). mold (Rui Ueyama) is the other reference: parallel hash tables, TBB, no relocation stalls.
- Zig's incremental linker (Andrew Kelley, `zig/src/link/Elf.zig`, MachO.zig): data-oriented design (struct-of-arrays, index handles instead of pointers); the interesting axis is in-place incremental patching with a free-list over the output file, similar to the Differential Dataflow axis but by hand.
- Install: wild via `cargo install --locked --git https://github.com/davidlattimer/wild` (verify repo path); zig is a single tarball. They link ELF, not Go objects, so they cannot be a column on MinIO's data. Use them for calibration only: link a comparable amount of ELF (e.g. Go's own `go build -ldflags=-linkmode=external` object set or a Chrome-sized `.o` corpus) with mold/wild/lld and compute the per-symbol rate (ns/symbol).
- Kill test: rate = wall / symbols. If wild or mold already reach < 100 ns/symbol on 8 cores for 5 M symbols, then hlink at 4-5 % of floor is out of line with practice; if they are 1 us/symbol or worse, the floor is not what real linkers achieve and this should be said in the report.
- Verdict: calibration rather than a column.

### Arrow / Polars / DataFusion compute kernels  -- fits 1, 3
- Polars (Rust, streaming engine, hash joins, `arg_max` over groups, cum_sum) and DataFusion do what DuckDB does with a DataFrame API in a program rather than SQL; the extra point is Arrow's flat columnar layout, which is the file layout the project already uses (flat little-endian columns). pyarrow `compute` kernels are single-op, not fused.
- Axis: dataframe-as-IR (same as DuckDB, nothing new). Kill if DuckDB already beats it; skip unless DuckDB loses by a lot.
- Install: `pip install polars` (check pip availability and a venv). Kill test: the resolve join in Polars; kill if > 1.5x DuckDB.

### SuiteSparse:GraphBLAS (and LAGraph)  -- fits 2, 1
- Reach is BFS as `v<m> = v lor.land A` per level, with a mask for the visited set: GraphBLAS is built for exactly this, and its direction-optimising push/pull BFS (Beamer 2012) lives in LAGraph (Davis, ACM TOMS 2019; LAGraph BFS push/pull, verify). The conditional method rule is a masked vxm with the interface-converted mask. Fits "flood with a rule" better than any other system on the list and is a C library with a JIT for user-defined operators (v8+).
- Axis: the semiring / masked-sparse-matrix-algebra. Futhark has no sparse graph primitive; Soufflé has no direction optimisation.
- Install: build from source with cmake (~5-10 min with OpenMP, JIT needs a C compiler present, which gcc is). Dense bitmap/hypersparse formats chosen automatically.
- Kill test: BFS from the entry points on the 360 k-live graph (9.4 M edges). Kill if > 5x T for the flood. Also record rounds (depth) and per-level frontier size, which hlink's flood must report.
- Verdict: the best fit for problem 2, and a likely win for problem 2 on direction-optimisation.

### ISPC  -- fits 3, 4, and 1's inner loops
- SPMD-on-SIMD compiler (Pharr & Mark, InPar 2012) for AVX2/AVX-512 CPUs: write scalar-looking code with `foreach`, get masked vector code; gathers and scatters are first class (`varying` pointers), and tasks give multicore. The charter already has Halide, Futhark (both able to target CPU) but not an explicit programming-model that makes SIMD gathers/scatters visible. Memory-bound work will not win from SIMD itself; it wins from gather-issue rate and prefetching, which is the question.
- Axis: the SIMD lane as the unit of the algorithm, explicit masks, `unmasked`, `foreach_active`. Lets one test "is gather/scatter throughput the real limit" separately from parallelism. Use as the direct implementation of the layout scan and relocation scatter.
- Install: single tarball release from GitHub (`ispc-v1.2x-linux.tar.gz`) plus `llvm` not required. Easy.
- Kill test: relocate over 9.4 M relocs in ISPC with 8 tasks. Kill if > 1.3x hlink's scalar relocate. Compare against the 0.74 G/s random-load figure: need to reach > 70 % of it.
- Verdict: cheap, and tests the "SIMD is not the bottleneck" claim.

### Kokkos / Chapel / Taichi / Triton / Mojo / Accelerate / Dex / SaC / Bend (HVM)
Briefly, with why each is lower priority:
- **Kokkos** (C++ performance portability, OpenMP backend): parallel_for / parallel_reduce / parallel_scan with views and layouts; the axis is "memory layout as a template parameter". Overlaps Halide's scheduling idea and Futhark's primitives; adds only layout-as-type. Install: cmake build, ~10 min. Low.
- **Chapel**: PGAS, `forall` over distributed arrays, `+ scan`, atomics; the axis is locality. On one node it gives a friendly forall/scan/reduce on CPU, comparable to Futhark's multicore backend with a better imperative escape hatch. Install: Chapel binary release or brew; building from source is ~30 min. Medium-low.
- **Taichi**: Python-embedded, sparse data structures (SNode trees: pointer, bitmasked, hash, dynamic) compiled to LLVM CPU/GPU. The axis is sparse hierarchical containers declared apart from the kernel, closest to Halide's separation; spatially sparse frames, not graphs. Install: `pip install taichi` (check support for the Python version). Low.
- **Triton**: GPU-only tile language; no CPU path in practice. Skip (no GPU).
- **Mojo**: SIMD types, autotuning `@parameter`, MLIR; compiler is closed and not self-hosted, installs via pip/magic (verify license/version). Axis (compile-time parametric SIMD width) is covered by ISPC. Low.
- **Accelerate (Haskell)**: embedded array language with fusion and an LLVM CPU backend; same family as Futhark with a weaker fusion story and no sort/scatter combine primitive. Install: GHC + stack, heavy. Skip.
- **Dex** (Paszke et al.): typed index sets and effects; research prototype, not tuned. Skip.
- **SaC (Single Assignment C)**: with-loops, shape-polymorphic arrays, auto-parallelisation on pthreads; the most mature data-parallel functional language on CPUs besides Futhark. Axis overlap with Futhark is large. Install: `sac2c` from source, the FPGA/CUDA backends not needed (~20 min). Low-medium.
- **Bend / HVM2** (HigherOrderCO): interaction-nets runtime that parallelises all pure functions automatically (CPU C runtime, CUDA); the axis is automatic parallelism by a graph reduction machine rather than data parallelism. Known to be 10-100x slower per core than hand loops. Great as a counterexample column, not a speed column. Install: `cargo install hvm bend-lang`. Kill test: resolve a 1 M-ref by hand-built tree-map; kill if > 100x T. Almost certainly killed; that is the finding.

## Not-a-paradigm research worth reading
- Hsu's Co-dfns thesis (above): compiler as a depth-vector / scan program.
- Neumann (VLDB 2011), Leis (SIGMOD 2014, morsels), Kersten (VLDB 2018 "Everything you always wanted to know about compiled and vectorized queries"): the direct answer to "vectorized or compiled" for this project's join and scan operators.
- Balkesen et al. (ICDE 2013), Schuh et al. (SIGMOD 2016), "main-memory hash joins on multi-core": radix join numbers (hundreds of M tuples/s per socket) are the floor for problem 1's join; compare to the 2.7 M refs x 5.2 M table: should be < 30 ms on 8 cores, and anything near that is "hardware speed".
- McSherry et al., "Scalability! But at what COST?" (HotOS 2015): the test every column must pass, a single-thread reference.
- Kelley, "A Practical Guide to Applying Data-Oriented Design" (Zig talk) and the `zig/src/link/` code: the by-hand version of struct-of-arrays plus index handles.

## Ranked top 3 columns to add

1. **SuiteSparse:GraphBLAS** (problem 2, also 1 via masked mxm). Adds the sparse-linear-algebra / semiring axis with direction-optimising BFS; the only candidate whose primitive IS the flood with a mask, so the conditional method rule has a natural form. Install is a 10-minute cmake, kill test is one BFS with a threshold of 5x T.
2. **DuckDB** (problems 1, 3 scan). Zero install, and it answers the most obvious question a reader would ask: "can an off-the-shelf vectorized SQL engine do the join and the scan at hardware speed?". Winner rule as QUALIFY, layout as window sum. Expected to lose on 2, which is itself the finding about where relational falls down.
3. **Differential Dataflow** (problem 2, plus a relink after one-package change). The only candidate adding a new axis (incrementality) rather than a variation on data-parallel batch; the COST test makes the result honest either way. Costs a Rust toolchain install.

Honourable mention: **ISPC** as a fourth column if Dan wants an explicit-SIMD axis for the relocate scatter: it is the cheapest to install and isolates gather throughput from parallelism. `wild`/mold are calibration, not columns.
