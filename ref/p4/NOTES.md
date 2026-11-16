# Problem 4: notes (2026-11-16)

## The oracle, and why it is per node

fc (simd-lang `driver/fc`, `a99d791`) writes a function's machine code in one walk over the typed tree
into one byte buffer (`asm.b`, x86.go). The buffer is only ever appended to, with two exceptions: rel32
fields patched in place when a label is placed, and the frame size patched into `LEAQ n(SP), BP` at the end;
and one truncation (`place`, gen.go: a `jmp` to the very next instruction is taken back). Children are
compiled inside their parent's call. So a node's bytes are an interval of the buffer, its children's
intervals nest inside it, and emission **is** separable per node, the preferred oracle:

- per node, the bytes fc's walker wrote for it given its operands' locations (its own segments, between its
  children), and
- the whole bodies, which a system's output is compared with byte for byte (plus the relocation list).

No per-statement or per-function fallback was needed. What could not be made per node is recorded as such:
the glue (bytes written outside every expression node: statement code, the epilogue, wrappers with no
syntax; 17.6 MB of 60.7 MB) is given verbatim, and the zero-width marker relocations (R_USEIFACE...,
95,195) are per function, because fc appends them to the list without a position.

## The instrumented copy (ref/fc)

`ref/fc/src` is simd-lang's `driver/fc` with `p4dump.go` added and 60 lines of hooks (`diff -r` against
`/home/exedev/targets/simd-lang/driver/fc`; the only removed lines are gofmt re-aligning a struct's comments):

- the encoder records a hole wherever it writes an operand-dependent field: a disp8/disp32 (by base: BP, SP,
  other), an immediate (mov, alu, store, imul, shift count), a jump's rel32, the frame size;
- `defer g.p4leave(g.p4enter(e, mode))` at the top of the walker's 17 expression entries (expr, exprAs,
  exprTo, condJump, addressOf, lvalue, multi, call, compositeTo, elemTo, selector, index, sliceExpr, unary,
  binary, typeAssert, funcLit). Entering the node the walker is already in (expr then binary on the same
  node) is the same node; the outermost entry is its mode. Leaves (identifiers, constants, types, pkg.Name,
  parens) make no node: their few bytes (a heap variable's pointer load) are the parent's;
- `place` tells the dump when it takes a jump back (the holes in it are dropped, node ends clipped);
- `finish` writes the body, its relocations, holes and nodes after the frame size is patched in, before
  the prologue goes in front.

`ref/fc/build.sh` builds it into `build/fcp4` in a GOROOT of our own (`data/goroot-p4`: the stock one with
simd-lang's mkcompiler.sh/mkfc.sh patches applied by running its patch scripts on our copy; nothing is
written in simd-lang). `ref/fc/run.sh` builds MinIO with it (`fc build -j 4 ... -link`).

The typed shape is explore/wholeprog's key (operator, each operand as leaf kind or `t`, with type class,
result class), computed by the same rules on fc's types with the instance's type arguments applied.

## Checks (each rerunnable)

- **The dump changes nothing.** The MinIO binary built with `FC_P4DUMP` set is byte-identical to the one
  built without it, and to the one built by the unmodified copy of fc (124,286,144 bytes, `cmp`); it runs
  (`--version`). `ref/p4/extract.sh` repeats the first comparison every time.
- **The files are deterministic.** A dump made with `-j 8` and one with `-j 4` give identical flat files and
  oracle (every file `cmp`-equal; functions are sorted by name, no name repeats).
- **The files are complete.** `ref/p4/check.sh`: `build/p4` rebuilds all 60,733,909 bytes of 136,634 bodies
  from templates + operands + glue and they equal `oracle/p4_code.bin`; the 1,780,281 relocations equal the
  oracle's. P4 OK.
- **The check can fail.** Adding 1 to one operand (`op_val.i64`), or flipping one bit of `tmpl.bin`, makes it
  print P4 FAIL (tried on a symlinked copy of the directory).

## Counts (MinIO, fc a99d791)

| | |
|---|---|
| functions emitted | 136,634 (fc: "70.6 MB of code" with prologues and morestack tails; bodies 60.7 MB) |
| expression nodes | 1,122,309 (SHAPES.md: 1,423,275 in the source) |
| bytes in nodes / glue | 43.2 MB (71.1%) / 17.6 MB |
| typed shapes / + mode / byte templates | 19,871 / 28,199 / 108,833 |
| operands (holes in nodes) | 7.68 M: 6.33 M encoder holes + 1.35 M symbols. Encoder holes, glue's included (2.81 M): BP 5.81 M, SP 53 k, other disp 1.23 M, imm 0.95 M, frame size 25 k, jumps 1.08 M |
| nodes that write no bytes of their own | 274,966 (24.5%): `p.f` where p is a local pointer gives the parent a location |

Fewer nodes than SHAPES.md because fc compiles by demand (dead functions are not compiled) and because some
syntax is compiled by statement code without an expression entry (range clauses, `x op= y` targets, switch
tags, `i++`: those bytes are glue). Generic instances count once per instance.

Coverage, nodes in the top K (and node bytes in the top K templates by count):

| key | distinct | top 256 | top 1,024 | top 4,096 | top 16,384 | top 65,536 |
|---|---|---|---|---|---|---|
| typed shape (SHAPES.md's key) | 19,871 | 68.0% | 87.4% | **96.6%** | 99.7% | |
| typed shape + mode | 28,199 | 56.4% | 78.2% | 92.7% | 98.8% | |
| byte template, nodes | 108,833 | 43.3% | 59.9% | **74.6%** | 86.8% | 96.1% |
| byte template, node bytes | | 19.5% | 33.2% | 50.3% | 68.2% | 87.8% |
| shape + mode + hole kinds (bytes, widths ignored) | 61,806 | 48.2% | 67.5% | 83.2% | 93.7% | |

The typed-shape curve reproduces SHAPES.md's (96.6% vs 96.7% at 4,096) on fc's own nodes. **But the typed
shape does not fix the bytes:** a node's template is the commonest of its (shape, mode) for 64.2% of nodes;
10.9% of nodes have a (shape, mode) with a single template, 46% one with more than ten. The 4,096 commonest
byte templates cover 74.6% of nodes and only half of the node bytes (the big, rare templates are the calls).
What splits them (`python3 -I ref/p4/stats.py data/minio-p4` prints the commonest keys with their templates):

- **disp8 vs disp32.** A frame slot within 128 bytes of BP is a different encoding (mod bits, length) from
  one further away: `x != nil` on an interface local is 41 bytes in 48% of cases, 50 or 47 in most others.
  A template with variable-width holes would merge these; keying on hole kinds without widths and bytes
  gives 61,806 keys and 83.2% at 4,096.
- **Where the operands came from.** The shape says `local:ptr`, but fc's location may be the slot, a heap
  variable (load the pointer first), a value left in AX by the child, or memory through a spilled pointer;
  each is a different instruction sequence.
- **Calls** carry their whole argument marshalling: `call t ->iface @exprTo` has 376 templates, the
  commonest 36%.

So: a template table keyed by typed shape is at the kill line for counting, but a code generator that
emits *these* bytes from a (shape, operand class) key needs either variable-width holes and a location
class per operand in the key, or operands first brought to fixed places (JOURNAL's untested idea), which
would change the bytes and so needs a new oracle (the program runs, the smoke test).

## Time

`build/p4 data/minio-p4`, 1 core (`taskset -c 5`), warm (files mapped with MAP_POPULATE, page cache hot),
three runs, machine load 3.5 from the other builders: lay out (item lengths + prefix sum, 2.38 M items)
**12.6-16.6 ms**, emit (copy 60.7 MB, patch 7.68 M holes, collect relocations) **139-147 ms**; sorting the
relocations for the comparison 105-115 ms (checking only). Extraction: the instrumented build 6.6 s on 4
cores (4.0 s without the dump), `p4mkflat` 7 s and 3.2 GB.

## Caveats and next steps

- Which template a node gets is given (fc chose it). A system that chooses templates from (shape, mode,
  operand classes) needs the operands' location classes; they are implicit in the hole kinds and the
  template, not a separate column. Worth adding if a cell wants to try.
- One operand per hole: two holes with the same value (one slot loaded and stored) are two operands.
  Linking a hole to the child or variable whose location it is was not done.
- Fields the encoder does not treat as operands (register numbers, condition codes, the scale of an index)
  stay in the template bytes, so they split templates rather than being patched.
- kube-apiserver: extracted to data/kube-p4 (README.md, "Second dataset"); the MinIO path in `ref/fc/run.sh` is now
  the default of TARGET/PKG.
