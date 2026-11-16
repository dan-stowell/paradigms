# Goose's ancestors and relatives: ideas, and kill tests for the grid

Problems: 1 resolve (hash join), 2 reach (flood), 3 layout+relocate (scan+scatter), 4 codegen by shape.
Data: MinIO, 5.2 M symbol slots, 9.4 M relocations, 2.7 M by-name references, 360,376 live symbols, 109 MB image.
Machine floors: DRAM read 92 GB/s, random loads 0.74 G/s into 512 MB. hlink is at ~4-5% of its floor.

Each "kill test" is a measurement on MinIO's column files; if the threshold is crossed the idea is dropped
for this workload. "Reference" means the hlink C program or the flat-array C baseline in the same harness.
Source links are the primary text I know of; I did not re-verify every URL on the day of writing (only
the Goose repository's own `docs/design/prior_art.md` was read in full: it is Goose's own survey and
ranks ML Kit, GNU obstacks, ParaSail, Ada/GNAT, C arenas, Cyclone as nearest).

## 1. Region / arena memory (bump pointer, free everything at scope exit)

- Idea: allocate by bumping a pointer in a region; free the whole region at once. Goose: N data stacks, freed at
  scope exit by one store, N computed by the compiler.
- Sources: Tofte and Talpin, region inference (POPL 1994) https://dl.acm.org/doi/10.1145/174675.177855 ; ML Kit
  https://elsman.com/mlkit/ ; GNU obstacks https://www.gnu.org/software/libc/manual/html_node/Obstacks.html ;
  Goose spec section 1.2-1.3 https://aardappel.github.io/goose/docs/goose_spec.html
- Problems: all four (phase-scoped memory: load, resolve, reach, layout, emit).
- Kill test: instrument hlink's allocations per phase. Compute (a) bytes whose lifetime is not nested inside
  the phase that allocated them (live past the end of the allocating phase to a non-ancestor phase). If more than 20% of peak
  bytes are not nested, regions degenerate to one global arena and the idea adds nothing. (b) Allocator
  time: if malloc/free is under 2% of any phase's time in hlink (it is expected to be: flat arrays are allocated once),
  the speed argument is dead and only the memory argument remains.

## 2. Lobster: compile-time reference counting / ownership inference (same author)

- Idea: infer a single owner for most values at compile time, so refcount operations vanish (Lobster's page reports that most RC
  operations are removed; exact figure not re-checked); the leftover are runtime counts. Goose is the step after: no RC at all,
  because roots plus data stacks replace it.
- Source: https://aardappel.github.io/lobster/memory_management.html (Lobster); the Goose repo
  `docs/design/ownership.md` for the relation.
- Problems: 4 (the shape emitter holds many short-lived template and patch objects), 2 (worklist nodes).
- Kill test: take the codegen-by-shape pipeline's 1.4 M expression nodes. At scope exit of each phase, count
  objects with more than one live owner (a node referenced by two shape groups, an operand shared by two
  templates). If shared-owner objects exceed 5% of nodes the single-owner model needs copies or RC;
  if copying them costs more than 5% of the emit phase time, kill.

## 3. Vale: generational references

- Idea: every object carries a generation number; a reference stores the generation it expects; a deref compares
  them. Gives memory safety for aliased mutable data without RC or borrow checking. Goose's relative `in pool`
  references and slot pools make the same trade (stale slot gives wrong-but-typed value, "a logic bug").
- Source: https://verdagon.dev/blog/generational-references
- Problems: 1 and 2 (links between symbols that can be reassigned during a winner-rule resolution).
- Kill test: (a) the linker's pipeline is built once from immutable columns; run the whole resolve+reach on
  MinIO with a generation counter on every symbol slot and count stale-link detections. If it is zero on all
  runs and phases (expected), generations only add cost: kill. (b) Cost: widen every symbol link from 32 to
  64 bits (32 index + 32 generation) in the resolve join and flood; if that costs more than 10% over 32-bit
  links on either, kill.

## 4. Cyclone and region-typed languages

- Idea: lexically nested regions with types that name the region of each pointer, so the compiler rejects
  dangling pointers; Cyclone added LIFO "dynamic regions" and unique pointers.
- Sources: Gay and Aiken, "Language support for regions", PLDI 2001; Grossman et al., "Region-based memory management
  in Cyclone", PLDI 2002 https://dl.acm.org/doi/10.1145/512529.512563 ; https://cyclone.thelanguage.org/
- Problems: 3 (the image buffer and its relocation patches have obvious nesting: image outlives per-object scratch).
- Kill test: write the layout+relocate phase with the phases as three nested regions (inputs, derived tables, image).
  Kill if more than one growable array must live at once in the same region at the same time on the critical path
  (Goose's one-growable-per-stack rule needs distinct stacks; the count of stacks above 8 means the
  region discipline is not buying structure). Count growable arrays alive across the relocate scatter in hlink;
  threshold: more than 8 simultaneously growing arrays kills it.

## 5. Rust arenas and index handles (bumpalo, typed-arena, slotmap, generational-arena)

- Idea: allocate nodes into an arena and refer to them by index (or arena-scoped reference) to escape the borrow
  checker and to cut allocator cost; slotmap adds a generation per slot.
- Sources: https://github.com/fitzgen/bumpalo ; https://github.com/thomcc/rust-typed-arena ;
  https://docs.rs/slotmap/ ; Goose benchmark `bintrees` vs "Rust arena" in bench/summary.md
  https://aardappel.github.io/goose/bench/summary.html
- Problems: 2 (flood over the symbol graph), 1 (join's chain entries).
- Kill test: build the 5.2 M symbol records as (a) one malloc per symbol, (b) bump arena, (c) one flat column set
  preallocated. Run the flood over all relocations. If (b) is not at least 3x faster to build than (a),
  or (c) is not within 5% of (b) at flood time, the arena idea is not what carries the performance: kill the
  arena framing and keep columns.

## 6. Data-oriented design (Mike Acton): think in transforms of data, hot/cold splitting, SoA

- Idea: structure code around the actual access pattern; separate fields touched together from fields that are not;
  arrays of columns beat arrays of structs when passes touch few fields. Goose's `soa.md` design (`T[||..]`
  column arrays) is not implemented, so columns are hand-made there.
- Sources: Mike Acton, "Data-Oriented Design and C++", CppCon 2014 https://www.youtube.com/watch?v=rX0ItVEVjHc ;
  Richard Fabian, Data-Oriented Design https://www.dataorienteddesign.com/dodbook/
- Problems: 1, 3 (the join touches name hash and flags; layout touches size and align; relocate touches offset
  and target).
- Kill test: on the 5.2 M symbol slots, run the resolve probe pass with the record as AoS (all fields, say 48 B) and as SoA
  (touched columns only). Report bytes moved and time. Kill if SoA is not at least 1.5x faster on the probe pass: then
  hot/cold splitting is not the lever on this data (the random probe, not the stream, dominates).

## 7. Zig compiler DOD (Andrew Kelley): struct-of-arrays, u32 indices instead of pointers, InternPool, extra arrays

- Idea: AST/IR nodes live in a MultiArrayList (one array per field); nodes refer to each other by 32-bit index;
  variable-length payloads go in a shared `extra: []u32` array pointed to by an index; strings and types are
  interned to u32 handles. The talk reports large memory and speed gains for the Zig compiler (numbers recalled from memory, not
  re-checked: verify against the talk before quoting).
- Sources: Andrew Kelley, "Practical Data-Oriented Design", Software You Can Love 2021 https://vimeo.com/649009599 ;
  Zig source `lib/std/multi_array_list.zig`, `src/InternPool.zig` https://github.com/ziglang/zig ;
  similar choices in Carbon's toolchain (Chandler Carruth, "Modernizing Compiler Design for Carbon Toolchain",
  CppNow 2023 https://www.youtube.com/watch?v=ZI198eFghJk ).
- Problems: 1 (names interned to u32, joined by index), 2 (relocations as `extra` runs), 4 (expressions as shape ids).
- Kill test: the shape grouping already has 1.4 M expressions over 4,096 shapes. Re-encode expression operand lists as
  runs in a shared u32 `extra` array with a u32 index per node, and compare against an Expr struct with 64-bit pointers
  on (a) peak bytes and (b) a full grouping pass. Kill if bytes drop by less than 30% or the pass is less than 1.2x faster.

## 8. Narrow typed links: 32-bit indices with static validation (Goose `u32 in arr`, index types)

- Idea: a link is a u32 that is statically known to be a valid index of one particular array; check it once where it
  enters the program, and indexing needs no bounds check (`checkfree fn` proves none remain). `u32 upto out` bounds
  prefix offsets.
- Source: Goose `docs/design/index_types.md` https://aardappel.github.io/goose/docs/design/index_types.html ; spec
  section 3.11 and 7.14; implemented in v2026.4 (bench `graph_csr.goose`).
- Problems: 1, 2, 3 (every gather in the pipeline: by package index, relocation target, symbol offset).
- Kill test: run the flood over 9.4 M relocations with (a) unchecked u32 indexing, (b) a bounds check on every access,
  (c) validate-once then unchecked. Kill if the cost of (b) over (a) is under 3% (the checks are already free,
  so the feature buys nothing here) or the validation pass in (c) is more than 5% of flood time.

## 9. Relative / position-independent references and byte-level save/load

- Idea: links stored as offsets relative to themselves or a pool base, so a structure can be written with one write
  and read back with one read (plus validation), no pointer fixup. Cap'n Proto, FlatBuffers, and Goose
  `T&<u32 in pool>`, `to_bytes()`, `from_bytes<T>()`.
- Sources: https://capnproto.org/encoding.html ; https://flatbuffers.dev/ ; Goose `docs/design/relative_references.md`
  and `serialization.md`.
- Problems: 3 (the 109 MB image is itself position independent data), and the column files the grid reads.
- Kill test: load MinIO's columns by `mmap` + validation scan versus parse into fresh arrays. Kill if the validation
  scan is more than 10x the 92 GB/s floor for the bytes (that is, load slower than ~9 GB/s), since then the
  "one read" promise does not hold on this data, or if mmap load is no faster than read into arrays.

## 10. Share-nothing threads with typed queues (Goose, Erlang, Pony, Go channels)

- Idea: no shared mutable memory; values are copied (flat) between isolated programs through queues; no data races by construction.
- Sources: Goose reference section 22 https://aardappel.github.io/goose/docs/reference.html ; Armstrong thesis
  https://erlang.org/download/armstrong_thesis_2003.pdf ; Pony https://www.ponylang.io/
- Problems: 1 (radix-partition both sides, ship partitions), 3 (workers build image segments), not 2.
- Kill test: partition the by-name references and definitions by hash radix across 8 workers, copy each partition in and
  the winners out. Kill if the copy-in/copy-out time is more than 30% of the 8-worker total, or the 8-worker time is
  not at least 4x faster than 1 worker. Preliminary evidence from hello8 (NOTES.md): 200 MB sent to 8 workers took
  about 119 ms against 95 ms of worker compute (send was 56% of send+compute time, above the 30% line);
  the 1.7 GB/s effective send speed is 2% of the DRAM floor.

## 11. Handles instead of pointers (generational or not)

- Idea: hand out small integer handles; look up through a table; the table can move, compact, or check generations.
- Source: Andre Weissflog, "Handles are the better pointers" https://floooh.github.io/2018/06/17/handles-vs-pointers.html ;
  Goose `slot_pool` and `slice_pool` in stdlib https://aardappel.github.io/goose/docs/stdlib.html
- Problems: 2 (worklist entries and visited state), 1 (winner links).
- Kill test: in the flood, route every symbol access through a handle table (one more dependent load) versus
  direct u32 index. Kill if the handle indirection costs more than 15% of flood time (random loads are
  already the floor; a second dependent miss would double it), which the 0.74 G/s random-load number predicts.

## 12. One growable value on top of each stack (obstack discipline) plus static stack assignment

- Idea: the single growing object lives at the top of its stack, so growth is a pointer bump with no capacity checks and
  no moves; references into it stay valid; the compiler assigns stacks like registers.
- Sources: GNU obstacks (link above); Goose spec sections 1.3 and 10.3.
- Problems: 3 (the output image and section buffers grow while patches reference them).
- Kill test: in the layout+relocate program, list every array that grows during the pass and whether a slice or
  reference into it is live across the growth. If the number of simultaneously live growers in hlink exceeds the
  8 stacks a Goose program of that shape would need, or growth happens while references are held on more than
  10% of relocations, kill.

## 13. Results constructed at their destination (Goose in-place construction, copy elision in Rust/C++, Zig result location semantics)

- Idea: a function building a variable-size value writes straight into the receiver's storage through any depth of calls.
- Sources: Goose spec section 4.3 and 7.3; Zig result location semantics https://ziglang.org/documentation/master/#Result-Location-Semantics
- Problems: 4 (emit each byte template straight into the output buffer, patch operands in place), 3 (relocation patches).
- Kill test: emit the 1.4 M shapes into the image directly versus into a scratch buffer and memcpy. Kill if the extra
  copy is under 2% of the codegen phase (109 MB at 92 GB/s is about 1.2 ms of copy; if the measured gap is that
  small, in-place construction is not worth a language feature).

## Measured (cells, 2026-11-16T16:25Z, load ~8-13)

- **Leave a shared-nothing output distributed.** p3par builds the 109 MB image in 17-24 ms on 8 workers (2 MB pages) when
  each worker keeps its address range; gathering it into main costs 142-171 ms more. For a linker the output is a file:
  each worker writing its range (pwrite at its offset) is the shape that keeps Goose's isolation and its speed. Kill test:
  per-worker pwrite of the segments to a hugetlbfs/tmpfs file; kill if it costs more than 2x the distributed 3b.
- **A data stack's page size as a language knob.** Remapping the pages above a builder's stack top to MAP_HUGETLB halves 3b
  (86.7-98.5 -> 40.8-42.2 ms, faults 26,731 -> 258-457). Goose assigns stacks statically, so it knows which stack holds
  the image: a per-stack (or per-`let`) "huge" attribute would make this idiomatic. Kill test: the same 2x on another
  big write (P1's 200 MB table first touch: 213 ms first rep vs 79-115 later).

## G-P4. The output file as the meeting point of shared-nothing workers (measured on P4)

- Idea: workers that cannot share memory still share the file system. A linker's workers write their ranges of the
  output file in place; main only hands out bases. Goose has no positioned write, so it is a 20-line C sidecar
  (flatio.c `flat_create`, `flat_write_at`).
- Evidence (P4, MinIO, 18:39Z, load 1.9): joining 60.7 MB of code + 1.78 M relocations into one result costs 226-260 ms
  (1 worker) / 120-152 ms (8) through the queue, and 54-63 / 37-61 ms through the files (ext4, page cache).
- Next: the same for P3b, whose gather into main is 142-171 ms on 8 workers (Surprise 1). Kill test: if 8 workers
  writing the 109 MB image to a file are not at least 2x faster than the queue gather, drop it.
