# Prior art for problem 4: copy-and-patch, CPython's JIT, Deegen

Reader: Sonnet subagent, 2026-11-16. Data: `/home/exedev/paradigms/data/minio-p4` (files in `ref/p4/README.md`).
An idea from a paper is a hypothesis until a cell measures it.

## What was read, and how

| source | status |
|---|---|
| Xu & Kjolstad, Copy-and-Patch Compilation, OOPSLA 2021, https://arxiv.org/abs/2011.13127 | Read in full: the PDF's text, extracted with ghostscript (2.2 MB PDF; figures are partly garbled in the extraction, so the figure numbers and code listings below were checked against the text). Not read: the supplemental Appendix B (extern-symbol macro code), which the paper refers to. |
| Xu & Kjolstad, Deegen: A JIT-Capable VM Generator for Dynamic Languages, https://arxiv.org/abs/2411.11469 | Read in full: Sec 7 (baseline JIT) and Appendix B (the Add bytecode pipeline) read closely. Sections 4-6 and 8 skimmed through targeted searches, not line by line. No stencil counts are reported in the sections I read. |
| CPython: PEP 744 (https://peps.python.org/pep-0744/) | Read only through the fetch tool's summarizer. It is high level; it has no hole, relocation or variant details. Treat as not read in full. |
| CPython: `Tools/jit/README.md` | Read through the summarizer. It is build instructions only. |
| CPython: `Tools/jit/_stencils.py`, `_schema.py`, `_optimizers.py` (parts), `template.c` (head), `Python/jit.c` (grep plus the `_PyJIT_Compile` loop), `InternalDocs/jit.md` | Read on `main` via raw.githubusercontent.com. Not read in full: `_targets.py` (777 lines, grepped), most of `jit.c`'s patch helpers. |
| luajit-remake repo docs; Haoran Xu's blog https://sillycross.github.io/2023/05/12/2023-05-12/ | NOT read. The Deegen paper is the primary source I used. CPython's `jit.md` points to the blog. |
| The 2023 LLVM dev meeting talk on Deegen | NOT read (only a search snippet). |

No source reports the stencil count of CPython's JIT. `jit_stencils.h` is generated, not in the repo, and I did not build it.

## Facts extracted (the answers to the brief)

**Stencil selection per (operation, operand location/kind).**
- The library maps a *configuration* to a stencil: node type, operand types, whether each operand is a constant, a register or a stack slot, whether the result spills, and the number of pass-through registers (Sec 3, 5.1; Fig 15). Selection at run time is a tree-pattern match plus one hash lookup (Sec 5.2, p16). For WebAssembly it is one array index from `(opcode, numInRegInts, numInRegFloats, spillOutput)`, with the other values as patch operands (Fig 16: "not even a switch case on the opcode or on the register configuration").
- Variants are the Cartesian product of "metavars" (C++ template parameters) cut by a hand-written filter function (Sec 5.1: `numPt > numMaxPassthroughs - 2` is rejected; void-typed operands are rejected). The paper says plainly that the product "can grow to a significant number" and that there are "too many stencils to write by hand" (Sec 3).
- Bounding tricks:
  1. Pass-through values are always typed as `uint64_t` or `double`, whatever their real type. This removes the type cross-product, since a stencil only cares about its own inputs (Sec 3, Fig 8).
  2. A hard cap on the number of live register temporaries; beyond it the value spills (Sec 4, Sethi-Ullman-style budget).
  3. Supernodes (fused AST subtrees) are optional. Without them WebAssembly has 1,666 stencils in 35 kB (20 B each). With them the high-level compiler has 98,831 stencils in 17.5 MB (177 B each), from 25 stencil generators, built in 14 min on 6 threads (Sec 7.6).
- Registers are a calling-convention trick: a stencil is a C function compiled with the GHC calling convention, so "parameter i" is a fixed physical register. The function prototype is the register-allocation protocol (Sec 3). The code generator never knows an instruction encoding.

**Holes.**
- A hole is an `extern` symbol the C++ stencil refers to through a macro (`DEF_CONSTANT_k`, `DEF_CONTINUATION_k`). Clang then must emit a linker relocation for it. Hole ordinal = the symbol's number (Sec 5.1, 5.2; Fig 12).
- Extraction reads the object file's relocation records (offset, type, symbol). Every hole is a 32- or 64-bit scalar; the patch is "initialize with a constant, optionally subtract the address, optionally add a symbol" (Sec 5.2). The stencil struct (Fig 9) is the binary plus three vectors: `pc32Patches` (offsets; subtract dst), `symbol32Patches` and `symbol64Patches` (offset, ordinal; add `patches[ord]`). Patching is `+=`, not an overwrite.
- Because Clang is told the extern address fits in a signed 32 bits, every stack offset and constant hole is a full 32-bit field. In their Fig 12 the stack offset 8 is written `41 39 85 08 00 00 00` (disp32 with mod=10), not disp8. **They never vary the encoding width: the width is fixed to 32 bits by construction.** That is how they avoid our disp8/disp32 split. The cost is code size.
- Deegen (App B, B.7-B.11; Sec 7.2): a hole is `@__deegen_cp_stencil_hole_k`, and the value is an arbitrary expression of bytecode operands (e.g. `slot*8+1`). Relocation `R_X86_64_32S hole-0x1` is how the `+1` offset is carried. The code generator replays the expression at codegen time. Deegen must prove statically that each expression fits the 32-bit range the ABI assumes (`[1, 2^31-2^24)`); the user can annotate ranges (`AnnotateRange`), and if the range's length fits but its endpoints do not, an adjustment constant is added and subtracted again in the IR. Otherwise it falls back to computing the value at run time.

**Sizes and speeds reported.**
- WebAssembly C&P: 1,666 stencils, 35 kB. More than 300 MB/s of machine code per core on AutoCAD and clang.wasm (Sec 6.2, Fig 19); 0.6 ms for a 30 kB module, of which 0.2 ms is allocator start-up; 4.9x-6.5x lower start-up delay than Liftoff.
- High-level C&P: code generation for TPC-H Q5 takes 178 us, less than the 326 us it takes to build the AST (Sec 7.3). Compilation is linear; two AST traversals plus one call-graph traversal (Sec 7.4). Jump removal and light-weight register allocation account for the bulk of the run-time gain from optimizations (Sec 7.5).
- Deegen baseline JIT for Lua: 19.1 M bytecodes/s, 1.62 GiB/s of machine code, 91 bytes per bytecode (about 52 ns per bytecode), averaged over 44 benchmarks (Sec 8.3). The generated code generator is "branchless by design": straight-line `movups` copies of the stencil plus `movl`/`addl` patches, with the stencil's constant bytes baked into the code generator's own instructions (Fig 19).
- CPython: 3 to 60 s of build time, 10-20% more memory. The JIT is "about as fast as the interpreter" (PEP 744). No emission speed is reported anywhere I read.

**CPython's design (a counterpoint).**
- One stencil per micro-op, with no operand-location variants. Operands live in the interpreter's stack frame / registers by a fixed convention; only `oparg`, `operand0`, `operand1`, `target`, `jump_target`, `error_target`, and code/data base addresses are holes (`HoleValue`, `_stencils.py`). There are 16-bit and 32-bit operand holes so the compiler can use short immediates (`SUPPORTS_SMALL_CONSTS`). The variant explosion is avoided by moving the variation to run time (a fixed ABI) rather than into stencils.
- Size pass first (`state.instruction_starts[i] = code_size` over the trace, then allocate), then emit by `group->emit(code, data, executor, instruction, &state)`, then patch jump targets from `instruction_starts[]` (`jit.c`, `_PyJIT_Compile`). This is the same scan/scatter split as ours.
- The build step rewrites each stencil's assembly so that short branches become long branches and jump-to-next is deleted (`_optimizers.py`: `SHORT_BRANCH`/`LONG_BRANCH`, fall-through logic). On x86-64 it also relaxes `GOTPCRELX` loads in place at patch time (`patch_x86_64_32rx` in `jit.c`): a variable-form encoding repaired by the patcher after the fact.

## What data says before any idea (measured here, scripts in the scratchpad)

All numbers are over the 1,122,309 nodes of `data/minio-p4`; method: `python3 -I` over the flat files, nothing in the repo changed.

1. **Disp width is a pure function of the operand.** For every encoder hole of kind 1/2/3 (BP, SP, other displacement), encoding 1 (integer), width 1 or 4: 4,863,909 holes in nodes; the rule "width is 1 iff the operand value is in [-128, 127]" holds with **0 exceptions**. Width counts: BP 2,586,863 w1 / 1,326,786 w4; other 834,460 w1 / 77,760 w4; SP 35,909 w1 / 2,891 w4. So the width is not extra information; the 108,833 templates carry it redundantly.
2. **Masking only the width (hole bytes deleted, mod bits of the byte before a disp hole cleared) gives 66,592 distinct templates, not 108,833** (top-4,096 covers 82.1% of nodes vs 74.6%; top-1,024 66.3% vs 59.9%; 95% of nodes needs 23,332 templates, 99% needs 55,369). This used only the byte before the hole, so it does not merge the SIB form (SP-based), 35,909 holes. It is a lower bound on the merge.
3. **The key `(shape, mode, hole kinds)` does not fix the bytes without widths** (commonest template is right for 78.0% of nodes). Adding the hole widths: 102,385 keys and 97.3%. After width masking, the commonest normalized template of each `(shape, mode, hole kinds)` group covers **96.9%** of nodes (60,931 groups, 69.8% of nodes in groups with a single normalized variant). So the width, not the operand location class, is the main source of variants; location classes (heap / AX / spilled pointer) are in the remaining 3%.
4. **Library size vs C&P:** our `tmpl.bin` is 10.5 MB for 108,833 templates (96 B average); C&P's high-level library is 17.5 MB for 98,831 stencils (177 B). The same order of library, so a template table is not out of scale.
5. **Speed context:** our emit is 60.7 MB in 139-147 ms on one core (about 0.42 GB/s, about 130 ns per node, 61 ns per item); layout is 12.6-16.6 ms (`NOTES.md`). C&P reports more than 0.3 GB/s; Deegen 1.62 GiB/s (about 1.74 GB/s), 52 ns per bytecode. Ours is already above C&P's headline and about 4x below Deegen's.

## Ideas

### CP1. Width-relaxed templates: one template per (shape, mode, ..) with the displacement width derived from the operand
- Source: C&P Sec 5.1-5.2 and Fig 12 (fixed 32-bit holes via the small code model; Appendix B not read); Deegen Sec 7.2 (range proof); CPython `jit.c` `patch_x86_64_32rx` (relaxation at patch time).
- What it says: the stencil carries the widest (disp32) form, the patcher shrinks it. C&P/Deegen avoid the problem by always using disp32 and proving the value fits; CPython repairs the form at patch time. Our own data say the choice is a function of the operand (data point 1), so fc's disp8 is exactly "relax disp32 when the value fits".
- Kill test (templates, on `data/minio-p4`): build the template table keyed by `(shape, mode, hole kinds, normalized bytes)` with every kind 1/2/3 disp hole stored as disp32 and with the mod bits of its modrm byte (and the SIB case) taken from a rule on the operand. Rules to be checked: (a) every node's reconstructed bytes equal the oracle (`ref/p4/check.sh` style, P4 OK); (b) the number of distinct templates is **at most 66,592** (the lower bound above) and the top-4,096 coverage is **at least 82%**. Kill the idea if (b) cannot beat 85,000 templates with (a) satisfied, or if the length scan needs more than 1.3x the current 12.6-16.6 ms (length is now `base - 3 * #{holes that fit}`, not a table lookup).

### CP2. Always-disp32 (C&P's own way): fixed width, new oracle
- Source: C&P Sec 5.2, Fig 12 (disp32 always); Deegen Sec 7.2 (`R_X86_64_32S`, `[1, 2^31-2^24)` range proof).
- What it says: if every displacement hole is a fixed 4 bytes, template lengths are constants, the layout scan is a pure gather of per-template lengths with no per-operand work, and the template count drops by the width split (66.6 k or fewer). The cost is bytes: the program is bigger and the oracle differs, so this is a new oracle (the program must run, as `NOTES.md` says of the "operands first brought to fixed places" idea).
- Kill test: from the 3,457,232 width-1 holes in nodes (BP 2,586,863, other 834,460, SP 35,909), the node bytes would grow by 3 bytes each = **10.37 MB, +17% of the 60.7 MB**. Kill if (a) the growth, counted in glue too (same rewrite on the glue's displacement holes, which are not in the template files), is above 25%; or (b) the rewritten MinIO (disp32 everywhere; run `--version`) does not run; or (c) the layout scan is not at least 30% faster than 12.6 ms, since that is the only benefit.

### CP3. Key = (shape, mode, hole kinds) plus per-operand location class bits, table indexed directly
- Source: C&P Sec 3, 4, 5.2 and Fig 15-16 (configuration = node type, operand types, const/register/stack for each operand, spill flag, pass-through count; selection is a hash or array index, no switch).
- What it says: the stencil is chosen by a small tuple of "where is each operand" classes. They bound it with a filter and a pass-through cap, not by listing all products.
- Kill test: define the class of each operand from the hole kinds plus the width bit (slot near/far, other-disp near/far, imm, jump, symbol). Build `key -> template id` and test the **selection** problem (which template, not just the count): the key `(shape, mode, hole kinds, widths)` picks the node's template for 97.3% (measured), at 102,385 keys. Kill the "key by operand location class gives a small library" claim unless a key that is *not* the template itself gets at least 99% of nodes with at most 30,000 keys. The 3% not explained is where the real location classes (heap pointer load, value left in AX, spilled pointer) hide; list the 20 commonest templates in the residual to see which class bit would split them. Needs one new column, `node_loc`, that the extraction does not write yet (`NOTES.md` suggests it).

### CP4. Straight-line per-template emitter for the hot templates (Deegen Fig 19)
- Source: Deegen Sec 7.1, Fig 19, 8.3 (the generated code generator is branchless: fixed-size 16-byte copies of the stencil bytes and a few `mov`/`add` patches, 1.62 GiB/s, 52 ns per bytecode).
- What it says: the per-stencil copy-and-patch routine is itself generated code, so the copy length is a constant, the hole offsets are immediates, and there is no loop over a hole vector. We currently run a generic loop over `tmpl_hole_off` per node segment.
- Kill test: take the top 4,096 templates by node count (74.6% of nodes; 82.1% after CP1's merge). Generate C for each: `memcpy` with a constant length, then one store per hole at a constant offset, taking operands from the node's operand array. Switch on the template id via a function-pointer table. Measure emit (copy + patch) on one core, same pinning as `NOTES.md` (`taskset -c 5`, three runs, state the load). Require the oracle to still be matched byte for byte. **Kill if the emit time on the covered nodes is less than 1.5x faster than the generic loop** (the current 139-147 ms total; the target for the whole emit is at most 80 ms, which would put us near Deegen's 1.7 GB/s). Compile-time cost to report: size of the generated C and compile time.

### CP5. Holes as per-kind SoA lists, patch by `+=` of the addend, with relative holes separated
- Source: C&P Fig 9 (three vectors `pc32Patches`, `symbol32Patches`, `symbol64Patches`; patching is `+=`), Sec 5.2; CPython `_PATCH_FUNCS` (one patch function per relocation type, `patch_32`, `patch_32r`, `patch_64`).
- What it says: they separate holes by *how* they are patched (pc-relative subtract, 32-bit add, 64-bit add), so the patch loop has no per-hole branch on type.
- Kill test: reorder each template's hole list by `(encoding, width)` in the table (holes of encoding 1 width 1, then width 4, then 8; rel32; relocations) and give each group its own tight loop. Time the patch part of emit on the full MinIO input (7.68 M operands, 6.33 M encoder holes; `NOTES.md` has the 139-147 ms emit). Kill if the speedup is under 10%. Note that our relocation holes (encoding 3, 1.35 M symbols) write nothing and only add a relocation record, so they can leave the loop entirely (collect them from a separate per-template list).

### CP6. Late jump patching with a start table, and jump-to-next elision (already ours)
- Source: C&P Sec 4 (the copy step elides jumps to the next stencil; remaining jumps are patched after copying), Deegen Sec 7.1 (compute total size, allocate, generate, then fix branch targets); CPython `instruction_starts[]` in `jit.c`.
- What it says: sizes first, then emission, then jump fix-up as a separate pass over only the jump holes. fc already removes jumps to the next instruction (`place` in `gen.go`), so only the second part applies.
- Kill test: split the 1.08 M rel32 jump holes into their own pass after all other patching (their targets need `at[]` of the target item, which the scan already has) and time emit with and without the split. Kill if the split is not at least 5% faster or if it breaks the oracle. (Low expected value: the layout already runs before emit, so jumps can be patched in place.)

### CP7. Fuse a node with its commonest child into one template (supernodes)
- Source: C&P Sec 3 and Sec 4 (supernodes: "if (a[i] <op> b[j])" or an array access as one stencil; Fig 11b's If-Leq supernode), 7.5 (supernodes help most where memory access and arithmetic dominate); Sec 6.4 (WebAssembly has none and stays at 1,666 stencils).
- What it says: fusing across a node boundary removes a boundary's overhead and gives the compiler (here, fc's bytes) a fixed sequence. For us the "supernode" is a free choice because the bytes are already fixed by fc, so fusing only saves *items* in scan and scatter, not code quality.
- Kill test: from `node_parent.u32`, count (parent template, child template) pairs and, for the top 4,096 pairs by count, the number of items removed if each pair is emitted as one item (the child's segments and the parent's segments between them merge into one contiguous run only when the child is the first or last child, so count only those). Kill if the fusion removes fewer than 20% of the 2,381,252 items (the layout is 12.6-16.6 ms and emit does one copy per item, so a smaller reduction cannot give a measurable speedup) or if the pair library exceeds 60,000 extra templates.

### CP8. Pass-through registers / calling-convention register protocol
- Source: C&P Sec 3 (Fig 8), Sec 4 (register budget), Sec 5.1 (`numPassThroughs`).
- What it says: they let the C compiler do register allocation for each stencil with the continuation's prototype as the protocol, and cap the number of pass-throughs to bound the variant count.
- Why it mostly does not apply: fc's byte stream is fixed (a value is in AX only between two steps; every local is a frame slot), so there are no register assignments to choose. It matters only if a cell wants to emit *different* bytes with the same program semantics (the "operands first brought to fixed places" idea in `NOTES.md`).
- Kill test (only if that idea is attempted): count, over the nodes, how many have a child value left in AX at entry (a template whose first instruction reads AX without loading it; from `tmpl.bin` bytes plus `hole_kind`). If it is more than 30% of nodes and the new oracle's code is not within 10% of fc's size, the "all operands in fixed places" design loses against fc's output; otherwise drop CP8.

## Not found / open
- No source gives a measured count of how many distinct stencils per opcode survive filtering; only the totals above. The ratio of total stencils to stencil generators (98,831 / 25, about 3,950 each) is the only handle.
- Variable-width x86 encodings are not addressed in C&P or Deegen except by avoiding them (disp32 always, immediate 32-bit via `R_X86_64_32S`) and by CPython's relax-at-patch GOT trick. I found no paper that merges disp8 and disp32 templates; CP1 is ours to test.
- Deegen's emission-speed claim (1.62 GiB/s) is for a bytecode-per-bytecode stream with constant-size stencils and no variable-length encodings; it is an upper bound for what we can expect, not a prediction.

### CP7 run-fusion experiment (2026-11-16)
The requested A/B/C limit experiment, plus an unmerged prepared control, is complete:
`results/codex-cp7.md`. A removes 56.6% of copies; B removes 94.3%, but B is only
1.08× faster than the same prepared representation without merging (one thread).
Preparation itself gives the large benefit; all oracle checks pass. C deduplicates
136,634 function runs to 55,389 shapes / 49.0 MB, a 19.3% blob-byte saving. This is
not the original top-4,096 parent/child-pair selection, so that library-size kill
test remains open. Fewer pieces alone are not a proportional emit-cost model.
