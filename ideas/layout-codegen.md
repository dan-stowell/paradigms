# Prior art for problems 3 (layout + relocate) and 4 (codegen by shape)

Format: idea / source / problem / column / kill test. Sizes from BRIEF.md (360,376 live symbols, 9.4 M relocs,
109 MB image, 1.4 M exprs, 4,096 shapes). "Floor" = DRAM-bound time (92 GB/s read, 0.74 G random loads/s).
Links marked (unread) were identified by search or memory and not read in full; the rest were opened.

## Problem 3: layout

### L1. Fix layout first, then copy and relocate in parallel (mold)
- Source: https://github.com/rui314/mold/blob/main/docs/design.md ; https://maskray.me/blog/2021-12-19-why-isnt-ld.lld-faster
- Idea: compute section sizes and offsets before any bytes move. Then each input chunk is copied and relocated independently into its own disjoint output range. mold aims for about 2x `cp` time. lld has few parallel passes.
- Problem: 3. Column: C (the hlink reference), and the model for every other column.
- Kill test: time a plain parallel `memcpy` of the 109 MB image from a shuffled 360 k-chunk source list, with no relocs. Call this T_cp. Kill the "copy+reloc is memory-bound" claim if the full copy+patch in any column is more than 3x T_cp. That would mean the relocs, not bytes, are the cost. Check hlink first.

### L2. Segmented exclusive scan with alignment as a monoid
- Source: Blelloch, "Prefix Sums and Their Applications", CMU-CS-90-190 (https://www.cs.cmu.edu/~guyb/papers/Ble93.pdf) (unread); Sengupta et al., "Scan primitives for GPU computing" (unread).
- Idea: address = align_up(prev_end, a) is not plain addition, but it is associative as a function composition. Represent each symbol as the map x -> align_up(x, a) + size. Composition of such maps stays in the form "max(c, align_up-chain)" for power-of-two alignments, because align_up(align_up(x,a),b) = align_up(x, max(a,b)) when a divides b, and so on. A scan over those pairs gives exact addresses. Segment by section.
- Problem: 3. Column: Futhark (scan with a custom operator), Halide (poor fit), Goose (probably a plain loop is fine).
- Kill test: compute the 360 k addresses with (a) a serial loop and (b) the parallel scan with the alignment-monoid operator. Require bit-identical addresses to hlink's and (b) no more than 2x faster than (a) at 360 k. Honest expectation: 360 k is about 3 MB of data, so the serial loop takes about 1 ms. Kill the scan idea on this input if (a) takes under 2 ms. Only the ordering rules (sort by section, then by size/name) could matter. Measure those separately in L3.

### L3. Order-by-key layout as radix sort + scan
- Source: Adinets and Merrill, "Onesweep: A Faster LSD Radix Sort Algorithm for GPUs", https://arxiv.org/abs/2206.01784 (unread); Go's linker sorts data symbols by size/name within section in `cmd/link/internal/ld/data.go` (dodataSect).
- Idea: encode (section, alignment class, size, name-hash) into one 64-bit key. Radix sort the 360 k live symbols, then scan the sizes.
- Problem: 3. Column: Futhark (radix_sort in the library), Soufflé (poor fit, sorting is outside Datalog), C.
- Kill test: a 360 k-element 64-bit key radix sort on 8 cores. Threshold: under 3 ms, and under 10% of total layout+reloc time. If sorting is above 10%, the ordering rule must be fused with the previous stage (reachability order).

### L4. Layout from the reachability order, with no sort at all
- Source: lld/mold "section ordering" and Go's linker emit in input order (https://maskray.me/blog/2021-12-19-why-isnt-ld.lld-faster).
- Idea: if the winner/flood stage already emits live symbols in a stable order (object, then index), a per-section counting pass (histogram by section, then scan over 8-16 sections) gives each symbol its slot. This replaces the sort.
- Problem: 3. Column: Futhark (reduce_by_index/hist), Goose, C.
- Kill test: byte-compare the image layout against the Go linker's own addresses for the sections where Go's ordering is by input order. If more than 1% of data symbols differ in address, the order rule really needs the sort (L3). Otherwise L3 is dead on MinIO.

## Problem 3: relocation

### L5. Relocation application as sorted scatter vs random scatter
- Source: mold design.md (relocations scanned in parallel, applied per output chunk) (https://github.com/rui314/mold/blob/main/docs/design.md); Onesweep for the sorting variant (https://arxiv.org/abs/2206.01784).
- Idea: each reloc does gather(target_addr[sym]) then write 4/8 bytes at (site). If relocs are stored in site order within each symbol (they are, in Go objects), the writes are near-sequential. The gathers into target_addr (360 k x 8 B = 2.9 MB) fit in L2/L3. So the real cost should be well below the 0.74 G/s random-load figure.
- Problem: 3. Column: all.
- Kill test: microbenchmark the 9.4 M-reloc patch loop with the live addresses table. Threshold: under 30 ms (about 3x the 9.4 M x 16 B read floor, 150 MB / 92 GB/s = 1.6 ms, plus writes). If measured above 100 ms, it is latency-bound by dependent loads and software prefetch or sorting by target is needed. Run it with reloc order shuffled to measure the gain from locality.

### L6. Writer/reader split: patch in place into an mmap'd output with page-parallel ownership
- Source: mold design.md; maskray lld post (https://maskray.me/blog/2021-12-19-why-isnt-ld.lld-faster). The truncate+mmap output is from a secondary source (unread).
- Idea: partition the output by page, with each worker owning a contiguous range, so no atomics. Copy the symbol bytes and apply its relocs in the same pass (data is hot in cache), instead of "copy everything, then patch everything".
- Problem: 3. Column: C, Goose.
- Kill test: A/B on MinIO: fused copy+patch vs two-pass. Require fused to be at least 25% faster, else keep the simpler two-pass form.

### L7. Go-specific: relocation kinds are a small enum, so branch-free per-kind passes
- Source: Go's `cmd/link/internal/ld/data.go` (relocsym), https://cs.opensource.google/go/go/+/master:src/cmd/link/internal/ld/data.go (unread); SHAPES.md.
- Idea: histogram the 9.4 M relocs by kind (ADDR, PCREL, CALL, ADDROFF, METHODOFF, TLS...). Run one tight loop per kind over a partitioned list (a stable partition, itself a scan + scatter) instead of one switch per reloc.
- Problem: 3. Column: Futhark (partition), Halide (one kernel per kind), Goose.
- Kill test: report the histogram. Kill the idea if one kind is above 90% (a branch predictor handles it, partitioning adds cost). Otherwise measure branchy loop vs partition+loops; need at least 1.3x.

## Problem 4: codegen by shape

### C1. Copy-and-patch: templates are precompiled stencils with holes
- Source: Xu and Kjolstad, https://arxiv.org/abs/2011.13127 (read abstract); CPython's JIT, PEP 744, https://peps.python.org/pep-0744/ (unread); Deegen, https://github.com/luajit-remake/luajit-remake (unread).
- Idea: for each of the 4,096 shapes, hold a byte template plus a list of (offset, operand-kind, width) holes. Emission is memcpy(template) then the hole writes. This is exactly the relocation-patching machinery of problem 3, so it can reuse the same scatter kernel (a hole is a relocation with a symbolic target).
- Problem: 4 (and 3). Column: C, Goose, Futhark.
- Kill test: emit all 1.4 M expressions, whose output size is the sum of template lengths. Threshold: at least 10 GB/s of emitted code (about 1/9 of DRAM read), or the total emit time under 1.4 M x 20 ns / 8 cores = 3.5 ms for small templates. If the per-expression cost is above 100 ns single-threaded, the loop is dominated by operand lookup, not copying.

### C2. Stencil size distribution decides fixed-width vs ragged output
- Source: Copy-and-patch paper section on stencil sizes (https://arxiv.org/abs/2011.13127) (unread beyond the abstract).
- Idea: if template lengths are mostly equal (e.g. at most 32 B), emit with fixed-stride SIMD stores and compact afterwards. If ragged, a scan over lengths gives the offsets (this reuses L2).
- Problem: 4. Column: Futhark, Halide (fixed-stride is a natural Halide pipeline), Goose.
- Kill test: compute template lengths over the 4,096 shapes, weighted by occurrence. If p90 <= 64 B and max/p90 <= 2, use fixed stride. Otherwise scan. Measure the compaction pass and kill fixed-stride if compaction costs more than the scan.

### C3. Tree-pattern instruction selection is the grid's "shape" lookup (BURS / iburg)
- Source: Fraser, Hanson, Proebsting, "Engineering a Simple, Efficient Code-Generator Generator", LOPLAS 1992, https://doi.org/10.1145/151640.151642 (unread); Pelegri-Llopart and Graham, "Optimal Code Generation for Expression Trees", POPL 1988 (unread).
- Idea: iburg compiles tree patterns to tables, so each node costs a table lookup keyed by (op, child states). The 4,096 shapes are the pre-seen states. If the expressions are already in a shape-id column, the BURS "labelling" pass is just a gather, and the 3.3% remainder falls back to a slow path.
- Problem: 4. Column: Soufflé (patterns as relations), C.
- Kill test: confirm 96.7% by counting. Build the shape key as a hash of (op, shape(child)...) bottom-up. Count distinct keys and the coverage of the top 4,096. Kill if coverage is below 95% on MinIO data, or if the fallback path costs more than the fast path total (fallback_count x fallback_ns > total fast time).

### C4. Hash-consing / bottom-up tree hashing as the shape-grouping step
- Source: Maziarz et al., "Hashing Modulo Alpha-Equivalence", PLDI 2021, https://arxiv.org/abs/2105.02856 (unread); Merkle-style structural hashing; Filliatre and Conchon, "Type-safe modular hash-consing" (unread).
- Idea: assign each node a hash of (op, child hashes) in postorder levels. Nodes of equal height are independent, so process by level (a level-synchronous pass over the expression forest). Then the shape id = a gather from a hash table of 4,096 entries (small, fits in L1/L2).
- Problem: 4. Column: Futhark (level-wise map + hist), Halide (per-level stage), Soufflé (recursion by stratum).
- Kill test: hash 1.4 M expression trees (count total nodes first). Threshold: at least 200 M nodes/s on 8 cores, and below 10 ms in total. Compare to sort-based grouping (C5). If the level count is above 64 (deep trees), level-sync parallelism is poor and the kill is "use per-tree parallelism".

### C5. Sort-based grouping vs hash-table grouping of expressions by shape
- Source: Onesweep (https://arxiv.org/abs/2206.01784) (unread); Futhark `reduce_by_index` / `hist` (https://futhark-lang.org, docs) (unread); Soufflé specialised B-trees and Brie, Jordan et al., CAV 2019, https://doi.org/10.1007/978-3-030-25543-5_23 (unread).
- Idea: with 4,096 shapes, a counting sort on shape id is one 12-bit pass. Grouping ends with a contiguous run of each shape, so emission becomes one tight loop per shape (the template stays in registers, with no per-expression template lookup).
- Problem: 4. Column: Futhark, Goose, C. Soufflé would do it via an index and probably lose.
- Kill test: A/B: (a) ungrouped loop with a template lookup per expression, in original order, vs (b) one 12-bit counting sort then per-shape loops. Need (b) including the sort to beat (a) by at least 1.2x. Else drop grouping. The ordering must be restored for output, so also count the inverse scatter.

### C6. Operand patching is a gather, and the operands may be locality-friendly
- Source: derived from copy-and-patch (https://arxiv.org/abs/2011.13127); Hillis and Steele, "Data Parallel Algorithms", CACM 1986, https://doi.org/10.1145/7902.7903 (unread).
- Idea: each hole reads one operand (a reg number, an immediate, a symbol address) from an operand column. If operands are stored column-per-hole-index per shape (SoA within the group), the reads are streaming.
- Problem: 4. Column: Futhark, Halide, Goose.
- Kill test: bandwidth. Bytes read (operands) + bytes written (code) over time. Threshold: at least 30% of 92 GB/s. If under 10%, the cost is in index chasing and the layout needs re-engineering.

## Compilers as data-parallel code (Problems 3 and 4)

### G1. Co-dfns: a compiler written as array operations over a flat tree
- Source: Aaron Hsu, "A data parallel compiler hosted on the GPU", PhD thesis, Indiana University 2019, https://scholarworks.iu.edu/dspace/items/3ab772c9-92c9-4f59-bd95-40aff99e8c7a/full (read abstract; the thesis is long); background https://aplwiki.com/wiki/Co-dfns
- Idea: the AST is stored as parallel vectors (depth, parent, kind) and every pass is a scan, gather or scatter with no pointers and no explicit loops. The same style works on the CPU, and it is exactly the columnar style the grid already uses. Take from it: parent-vector trees with depth vectors, and the "scatter by parent index" idiom, as the expression format for 1.4 M expressions.
- Problem: 4. Column: Futhark (closest in spirit; a native fit), new ("APL-style" column, optionally via BQN/Dyalog on CPU as a measured baseline).
- Kill test: represent the expressions as (parent, kind, operand) vectors. Compute depth by pointer jumping (log N rounds) vs level-by-level. Kill the pointer-jumping form if its total traffic (rounds x 1.4 M x 8 B) times 2 exceeds the serial pass time. Expect the serial pass to win at this size. If so, record that Co-dfns style pays only on GPUs.

### G2. Pareas: lexing, parsing, semantic analysis and RISC-V backend on the GPU
- Source: Voetter, Huijben, Rietveld, "Compilation on the GPU? A Feasibility Study", ACM CF 2022, https://doi.org/10.1145/3528416.3530249 ; code https://github.com/snektron/pareas ; Huijben thesis (backend) https://theses.liacs.nl/pdf/2020-2021-HuijbenMarcel.pdf
- Idea: instruction selection by tree pattern is done as a data-parallel pass: the backend assigns per-node instruction counts, a scan gives offsets, and a scatter writes instructions. This is the shape-emission pipeline of problem 4 (count, scan, emit). It also uses the same scan-for-address trick as problem 3.
- Problem: 4 and 3. Column: Futhark (the Pareas compiler is itself written in Futhark, which is a direct comparison), Goose.
- Kill test: implement their three-step backend (count, scan, scatter) for the 1.4 M expressions, using exact per-shape instruction counts. Threshold: under 15 ms on CPU (8 cores). Compare to C1 (direct copy-and-patch loop). If the three-step version is not within 1.5x of C1, a fused loop wins and the extra column buys nothing.

### G3. Data-parallel lexing/parsing and finite-state machines (relevant only for text inputs)
- Source: Mytkowicz, Musuvathi, Schulte, "Data-Parallel Finite-State Machines", ASPLOS 2014, https://doi.org/10.1145/2541940.2541988 (unread).
- Idea: speculative per-chunk FSM execution plus composition of transition maps (a scan over maps). The inputs here are binary columns, so the lexing step is empty. Its relevance is the technique: any serial scan with a small state (e.g. the alignment padding in L2, or Go's varint-delta pcln tables) can be parallelised as a scan over state-to-state functions.
- Problem: 3 (L2) and any pcln/varint decoding. Column: Futhark.
- Kill test: count how many MinIO columns need serial varint decoding (pcdata/pcln/funcdata). Skip the idea if under 5% of output bytes. If more, a 2-pass chunked decode must reach at least 3 GB/s per core.

## Practice from real linkers

### P1. Go's own linker: parallel relocation by symbol blocks and the loader's index design
- Source: Go linker rewrite design (Cherry Zhang), https://go.googlesource.com/proposal/+/master/design/ (search "linker") (unread); `cmd/link/internal/ld/data.go`; https://github.com/golang/go/tree/master/src/cmd/link/internal/ld
- Idea: Go's linker already splits data into blocks and runs relocsym in parallel per block (`writeBlocks`), 100K-byte blocks. This is the production baseline to compare any grid column against; it tells which structure is cheap to change.
- Problem: 3. Column: C baseline.
- Kill test: time `go tool link` phases on MinIO with `-v` timing (or `GOFLAGS=-ldflags=-bench=...` style). Threshold: report the relocation+write phase time. If hlink is already below it by at least 10x, the grid targets (not the Go baseline) are the right floor comparison.

### P2. Wild / Rust parallel linkers (layout in parallel, then write)
- Source: https://github.com/davidlattimore/wild (unread); mold docs above.
- Idea: Wild assigns addresses in a parallel "layout" phase with per-thread resolution of section sizes, then a separate write phase. It reports per-phase timings, which can be used to calibrate where the 4-5% of floor goes.
- Problem: 3. Column: C, new (Rust).
- Kill test: link a comparable Rust/C binary with mold and wild at 8 threads and compute bytes-of-output/sec. If either linker exceeds 30% of the DRAM floor, the "4-5% of floor" in FLOOR.md may be an artifact of Go-specific passes, not layout, so profile before blaming layout.

## Cross-cutting kill tests (run first, cheap)
1. Reloc kind histogram and per-symbol reloc count distribution (L7, L5): 10 minutes, decides several entries.
2. Shape-coverage recount from raw expressions (C3): confirms the 96.7% before building anything.
3. L5 microbenchmark: the 9.4 M-reloc patch loop alone, as the floor for all of problem 3.
4. Expression tree depth distribution (C4, G1): decides level-synchronous vs per-tree parallelism.
