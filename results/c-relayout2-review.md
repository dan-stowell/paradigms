# Independent review of attempt 2 (2026-11-16)

Reviewed `ref/flat/p3holes.c` separately from its implementer, including the
allocation rule, reverse-list update, full-history replay, exports, and timed
fixed-point checks. The review identified a real zero-size retention bug:
`alloccap == 0` was being treated as absent allocation even for a retained
zero-sized common symbol. The implementation now uses the retention predicate,
and the regression case below fails before the fix and passes afterwards.
The datasets have 293 / 293 / 290 zero-size placed symbols in A / B1 / B2.

Additional review changes: spare-extent alignment gaps are returned to the
owning arena's hole list; the unchanged typelink table is not scanned on every
addition; copies, relocation writes and table entries read maintained addresses
rather than stale per-version caches. History replay checks exact logical hole
lists, tails and shared-spare cursors as well as addresses, allocation capacities,
full zero-filled images and sorted offset tables. Timed repeated cycles finish
at a checked allocator fixed point; every intermediate endpoint of a further
cycle is checked again by full history replay. Fixed-point hash comparison is
an additional check, not a replacement for the full image/address comparisons.

## Independently executed allocator tests

```sh
mkdir -p build-relayout2/tmp
export TMPDIR="$PWD/build-relayout2/tmp"
gcc -O1 -g -fsanitize=undefined \
  -o build-relayout2/review-allocator ref/flat/check-p3holes-allocator.c
prlimit --as=8589934592 build-relayout2/review-allocator
```

Result: `REVIEW allocator: 20000 random allocate/free steps, alignment, first fit
splitting, coalescing, live/live and live/hole disjointness OK`.

The deterministic cases also check shrink/regrow address retention, necessary
movement on growth beyond capacity, first-fit hole reuse before tail/spare,
overflow coalescing, arena ownership of freed overflow, and zero-size retention.
The random test uses a fixed seed and inspects every active allocation and free
list after each operation. UBSan reported no errors. An earlier version of the
random/split/coalesce test also passed ASan+UBSan without an address-space cap
(ASan reserves a large virtual shadow); the reproducible capped command above
uses UBSan only.

Independent input inspection finds exactly one growing common allocation in
edit 1: unified ID 877763, text size 266 -> 532, alignment 32. Edit 2 has no growing
common allocations. The observed 1 / 0 forward moved addresses therefore match
the allocator rule rather than a hidden compaction pass.

## Scope and remaining limits

The full-history oracle shares the deterministic allocation primitives with the
incremental path, as permitted by the history-defined rule, but rebuilds allocator
state from compact A, discovers changes by a full symbol scan, and renders every
symbol into a fresh zero image. The separate `p3inc_check.c` evaluator checks
relocation semantics, alignment, overlap, and complete image bytes; the separate
table checker reconstructs ordering from raw strings and exported addresses.
Those are complementary checks, not three independent allocator implementations.

No arbitrary-edit fuzzing of relocation graphs or end-to-end executable linker
claim is made. The synthetic test exercises allocator operations, not random
normalized symbol replacement records. New packages and exhausted fixed spare
extents remain outside the measured workload; exhaustion is an assertion rather
than a moving fallback. Free overflow remains owned by its arena, so it is not
available to other arenas. `bytes_changed` counts the P3 image; sorted-table
stores are separately counted by `table_bytes` and included in `written`.
