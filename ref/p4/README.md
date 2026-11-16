# Problem 4: code generation by shape (the flat files)

Every system in the grid reads these and nothing else. They are written by `ref/p4/extract.sh` into
`data/minio-p4/` (git-ignored, about 340 MB for MinIO, plus the raw dump), from a copy of simd-lang's compiler
fc (`ref/fc/src` = simd-lang `driver/fc` at `a99d791`) with one added file, `ref/fc/src/p4dump.go`, and
one-line hooks in the walker and the encoder (`ref/fc/src/*.go` against simd-lang's: `diff -r`). That copy
builds MinIO to a program byte-identical to the one it builds with the dump off, and that program runs
(`--version`); extract.sh checks both every time.

`ref/p4/check.sh` builds a plain single-core C program (`ref/p4/p4.c`) that reads only these files and
reproduces the oracle exactly: that is the proof that the files say everything the problem needs. Read it
next to this file. Conventions (raw little-endian arrays, CSR, strings, `meta.txt`) are `ref/flat/README.md`'s.

## The problem

fc compiles a Go function with one walk over its typed syntax tree, writing x86 bytes as it goes; every
local lives in a frame slot below BP, a value is in AX (or X0) only between two steps of an expression. Each
expression node (SHAPES.md's definition: an operator, a call, a selector, an index, a slice, a conversion, a
literal, a closure; not a variable, a constant, a type or `pkg.Name`, which are its parent's operands) is the
interval of bytes written while the walker was inside it. Its **own bytes** are that interval minus its
children's intervals: segment 0, child 1, segment 1, ..., child n, segment n.

A node's own bytes, with the operand-dependent fields zeroed, are its **template**; the zeroed fields are
**holes**; the values they had are the node's **operands**. Problem 4 is: given each node's template and
operands, and the bytes no node owns (**glue**: statements, prologue-free bodies of wrappers, epilogues),
emit every function body exactly as fc did. Two steps: a scan (the length of every piece, then where it
goes) and a scatter (copy the template bytes, patch each hole, including jumps, whose rel32 depends on where
their target landed).

The templates come from fc's own output, so the files hold every template the program needs; the measure of
the idea is how few there are and how much the commonest cover (NOTES.md). A system may emit only the nodes
of the commonest K templates and take the rest as glue; the oracle is the same.

## Files

| file | what |
|---|---|
| `meta.txt` | `nfn nitem nglue nnode ntmpl nshape nmode nsym nop nthole ntseg nmark ngrel code_bytes nrel` |
| `fn_name.*` | per function: its symbol name (`<1>` appended for an ABIInternal-vs-ABI0 twin) |
| `fn_item.u32` | CSR (`nfn+1`): the items of function `f`, in output order, are `[fn_item[f], fn_item[f+1])` |
| `item_kind.u8 item_ref.u32 item_seg.u32` | per item: kind 0 glue (`ref` = glue id), kind 1 segment `seg` of node `ref` |
| `glue.off.u64 glue.bin` | per glue id: its bytes, verbatim |
| `glue_rel_off.u32` + `grel_off.u32 grel_siz.u8 grel_type.u16 grel_sym.u32 grel_add.i64` | CSR per glue id: its relocations (offset in the glue run, width, cmd/internal/objabi type, symbol, addend) |
| `fn_mark_off.u32` + `mark_off.u32 mark_type.u16 mark_sym.u32 mark_add.i64` | CSR per function: its zero-width marker relocations (R_USEIFACE and the like), at `mark_off` from the body's start |
| `node_tmpl.u32` | per node: its template |
| `node_shape.u32 node_mode.u8` | per node: its typed shape (`shape.*`: explore/wholeprog's key, e.g. `bin+ local:i64 K:i64 ->i64`) and the walker's entry it came through (`mode.*`: expr, exprAs, exprTo, cond, call, lvalue, multi, elem, addr) |
| `node_parent.u32 node_fn.u32` | per node: its parent node (0xFFFFFFFF: a statement's) and its function |
| `node_op.u32` + `op_val.i64 op_aux.i64` | CSR per node: one operand per hole of its template, in hole order |
| `tmpl_shape.u32 tmpl_mode.u8` | per template: the (shape, mode) all its nodes have |
| `tmpl.off.u64 tmpl.bin` | per template: its bytes (the segments back to back, holes zeroed; relocated fields as fc left them, zero) |
| `tmpl_seg_off.u32` + `tseg_end.u32` | CSR per template: the end offset of each segment in its bytes (`n_children + 1` segments) |
| `tmpl_hole_off.u32` + `hole_at.u32 hole_w.u8 hole_enc.u8 hole_kind.u8 hole_rtype.u16` | CSR per template: its holes, sorted by offset: offset in the template bytes, width, encoding, kind, relocation type |
| `shape.* mode.* sym.*` | strings; symbol 0 is "none" (the TLS relocation) |

Hole kinds (what the operand is): 1 a BP displacement (a local, a temporary, a parameter's home), 2 an SP
displacement (an outgoing argument), 3 a displacement from another base (a field or element offset through a
pointer), 4 an immediate, 5 the frame size, 6 a jump target, 7 a symbol.

**4. Emit.** Item `i`'s length is its glue run's length, or segment `s` of its node's template:
`tseg_end[s] - tseg_end[s-1]` (`tseg_end[-1] = 0`). Items are laid back to back from 0 in item order
(functions follow one another): `at[i]`. Function `f`'s body is `[at[fn_item[f]], at[fn_item[f+1]])`. Copy
each item's bytes to `at[i]`; for a node segment, patch each of its template's holes with offset `h` in the
segment's range `[lo, hi)`, at `P = at[i] + h - lo`, with its operand `(v, a)`:

| encoding | what is written |
|---|---|
| 1 integer | the low `w` bytes of `v` (little-endian) |
| 2 rel32 | `int32(at[v] + a - (P + 4))`: `v` is the item the jump lands in (same function), `a` the offset in it |
| 3 relocation | nothing (the template's bytes stay); a relocation `(P, w, rtype, symbol v, addend a)` |

Glue relocations go at `at[i] + grel_off`, markers at the function's start `+ mark_off`.
Oracle: `oracle/p4_code.bin` (every body fc wrote, functions in `fn_name` order; `oracle/p4_fn_off.u64` its
`nfn+1` offsets) must equal the output byte for byte, and the relocations, as a sorted list of
`(offset, type, symbol, addend, width)`, must equal `oracle/p4_rel_{off.u64,siz.u8,type.u16,sym.u32,add.i64}`.

## What the extraction did for the systems

The body is fc's before the prologue goes in front (the prologue needs the final frame size; everything in
the body is BP-relative, so no body byte depends on it except the frame-size holes) and before the object
writer: relocations are fc's own list, with symbol names interned. Jumps fc took back (a `jmp` to the next
instruction, `place` in gen.go) are gone from the bytes and from the nodes. Which template a node gets is
given: fc chose it (registers, encodings, whether a value was spilled); a system that picks templates itself
from `(shape, mode)` and operand classes is the next problem, and NOTES.md says how far apart the two are.

## Prepared runs (CP7)

`p4runs.c DATA A|B|C|D` prepares a run representation once, times that preparation
separately, then times repeated emission with `P4_REPS`. A absorbs hole-free
neighbours; B makes one zeroed-hole blob per function; C deduplicates B by template
sequence and byte/hole shape; D keeps the original item boundaries as a prepared
control. These trade additional prepared input for less runtime lookup work.
`check-runs.sh DATA` checks bytes, every function offset and sorted relocations;
`bench-runs.sh DATA` compares them with p4/p4par in five interleaved rounds at 1/8
threads. Counts, input sizes, timing scope and conclusions: `results/codex-cp7.md`.

`p4holes.c DATA H1|H2` starts from B. H1 moves integer writes into preparation;
H2 keeps them in emission and combines consecutive integer holes whose full span
fits in eight bytes into a masked store. `H0` is a checked diagnostic: pre-bake
integers but retain their records and traversal. `P4_COUNTS=1` prints reuse, width,
and adjacency counts (disable it for timing). `check-runs.sh` also invokes
`check-holes.sh`, including deliberately planted byte/offset/relocation faults.
`bench-holes.sh` compares B, p4par, H1/H2/H0 in five rotated rounds at 1/8 threads.
Scope, measured tradeoffs and reproduction: `results/codex-holes.md`.

## Second dataset: kube-apiserver (2026-11-16T18:53Z)

`TARGET=/home/exedev/targets/kubernetes PKG=./cmd/kube-apiserver GOCACHE=data/kube-p4/gocache ref/p4/extract.sh
data/kube-p4` (the dump-on binary is byte-identical to the dump-off one; it runs `--version`). 202,980 functions,
74.5 MB of body (MinIO 136,634 / 60.7 MB), 2.92 M items, 1.36 M nodes, 93,112 byte templates (top 4,096 cover
83.0% of nodes), 9.32 M operands, 2.21 M relocations. `ref/p4/check.sh data/kube-p4`: p4 and p4par (1, 8 threads)
P4 OK; p4par emit 108 ms (1) / 38 ms (8), one run, load 4.4.
